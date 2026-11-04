#include "watcher.h"
#include "config.h"
#include "packet_processor.h"
#include "stats_logger.h"

// ============================================================
// Watcher 스레드 (관리 전용)
//
// 1. pause.flag 감지 → g_is_paused 토글 (GUI 휴식 모드 연동)
// 2. 화이트리스트 핫 리로드 (파일 수정 시 자동 재적용)
// 3. 오래된 IP 캐시 삭제 (TTL 만료 시 제거하여 메모리 절약)
// 4. 통계 JSON 저장 (UI 렌더링용)
// ============================================================
namespace {
time_t get_file_mod_time(const char* path) {
    struct stat result;
    if (stat(path, &result) == 0) {
        return result.st_mtime;
    }
    return 0;
}
} // namespace

void watcher_func() {
    time_t last_time = get_file_mod_time("whitelist.json");
    bool was_paused = false; // 이전 상태 추적

    std::cout << "[Watcher] 상태 감시 및 통계 로깅 스레드가 시작되었습니다." << std::endl;

    while (g_is_running.load()) {
        // 1초에 한 번씩 검사 (UI 반응성 향상)
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!g_is_running.load()) break;

        // 1. pause.flag 확인 → 휴식/집중 모드 전환 (상대 경로 및 core_engine 서브경로 동시 검사)
        struct stat buffer;
        bool is_paused_now = (stat("pause.flag", &buffer) == 0) || (stat("core_engine/pause.flag", &buffer) == 0);

        // 집중 → 휴식 전환 감지
        if (!was_paused && is_paused_now) {
            std::cout << "\n[Watcher] ☕ 휴식 모드 전환 감지: 패킷 차단 일시중지 (유튜브 등 모든 접속 허용)" << std::endl;
        }

        // 휴식 → 집중 복귀 감지: 커넥션 캐시 전부 초기화
        if (was_paused && !is_paused_now) {
            {
                std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                allowed_conns.clear();
                blocked_conns.clear();
                pending_conns.clear();
            }
            {
                std::lock_guard<std::mutex> r_lock(g_reassembly_mutex);
                g_reassembly_map.clear();
            }
            std::cout << "\n[Watcher] ▶ 집중 모드 복귀: 커넥션 캐시 초기화 완료. 차단 재개!" << std::endl;
        }

        was_paused = is_paused_now;
        g_is_paused.store(is_paused_now);

        // 2. 와이어샤크 비교용 패킷 캡처 통계 출력 (5초마다)
        static int tick = 0;
        if (++tick % 5 == 0) {
            std::cout << "\n[Status] 누적 캡처 패킷 수 (Wireshark 비교용): " << total_captured_cnt.load() << std::endl;
        }

        // 3. 화이트리스트 핫 리로드
        time_t current_time = get_file_mod_time("whitelist.json");
        if (current_time == 0) {
            current_time = get_file_mod_time("core_engine/whitelist.json");
        }
        if (current_time != 0 && current_time != last_time) {
            last_time = current_time;
            std::cout << "\n[Watcher] 필터 파일(whitelist.json) 변경 감지! 핫 리로드를 진행합니다..." << std::endl;
            load_whitelist();
        }

        // 4. IP 캐시 TTL 정리 (쓰기 잠금 필요)
        time_t now = time(nullptr);
        {
            std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);

            // allowed_conns (30초 초과 시 제거)
            for (auto it = allowed_conns.begin(); it != allowed_conns.end(); ) {
                if (now - it->second > 30) it = allowed_conns.erase(it);
                else ++it;
            }

            // blocked_conns (30초 초과 시 제거)
            for (auto it = blocked_conns.begin(); it != blocked_conns.end(); ) {
                if (now - it->second > 30) it = blocked_conns.erase(it);
                else ++it;
            }

            // pending_conns (15초 초과 시 제거)
            for (auto it = pending_conns.begin(); it != pending_conns.end(); ) {
                if (now - it->second > 15) it = pending_conns.erase(it);
                else ++it;
            }
        }

        // 4-1. 재조립 세션 만료 정리 (3초 초과 시 삭제)
        {
            std::lock_guard<std::mutex> r_lock(g_reassembly_mutex);
            for (auto it = g_reassembly_map.begin(); it != g_reassembly_map.end(); ) {
                if (now - it->second.created_at > 3) {
                    g_stat_reassembly_expired++;
                    it = g_reassembly_map.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // 5. 통계 데이터 JSON 저장 (UI 렌더링용)
        StatsLogger::GetInstance().SaveToFile("FocusGuard_stats.json");

        // 6. 진단 통계 출력 (잘린 프레임 발생 시)
        static int diag_tick = 0;
        if (++diag_tick % 10 == 0) {
            uint64_t trunc = g_stat_truncated_frames.load();
            if (trunc > 0) {
                std::cout << "[Diagnostic Stats] 잘린프레임(caplen < len): " << trunc << std::endl;
            }

            // ── 재조립 통계 출력: starts > 0이고 값이 이전과 변했을 때만 출력 ──
            uint64_t starts = g_stat_reassembly_starts.load();
            if (starts > 0) {
                uint64_t succ = g_stat_reassembly_success.load();
                uint64_t exp = g_stat_reassembly_expired.load();
                uint64_t seq_err = g_stat_reassembly_seq_err.load();
                uint64_t oom = g_stat_reassembly_oom.load();

                static uint64_t last_starts = 0, last_succ = 0, last_exp = 0, last_seq_err = 0, last_oom = 0;
                if (starts != last_starts || succ != last_succ || exp != last_exp || seq_err != last_seq_err || oom != last_oom) {
                    std::cout << "[Diagnostic Stats] TLS재조립: 시작 " << starts 
                              << " | 성공 " << succ 
                              << " | 만료 " << exp 
                              << " | seq오류 " << seq_err 
                              << " | 맵초과 " << oom << std::endl;
                    last_starts = starts;
                    last_succ = succ;
                    last_exp = exp;
                    last_seq_err = seq_err;
                    last_oom = oom;
                }
            }
        }
    }

    // 스레드 종료 시 최종 저장
    StatsLogger::GetInstance().SaveToFile("FocusGuard_stats.json");
    std::cout << "[Watcher] 상태 감시 스레드가 종료되었습니다." << std::endl;
}
