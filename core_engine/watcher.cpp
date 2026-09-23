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

    while (true) {
        // 1초에 한 번씩 검사 (UI 반응성 향상)
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 1. pause.flag 확인 → 휴식/집중 모드 전환 (상대 경로 및 core_engine 서브경로 동시 검사)
        struct stat buffer;
        bool is_paused_now = (stat("pause.flag", &buffer) == 0) || (stat("core_engine/pause.flag", &buffer) == 0);

        // 집중 → 휴식 전환 감지
        if (!was_paused && is_paused_now) {
            std::cout << "\n[Watcher] ☕ 휴식 모드 전환 감지: 패킷 차단 일시중지 (유튜브 등 모든 접속 허용)" << std::endl;
        }

        // 휴식 → 집중 복귀 감지: 커넥션 캐시 전부 초기화
        // (휴식 중 allowed 된 YouTube 등 기존 소켓을 즉시 차단 대상으로 만들기 위함)
        if (was_paused && !is_paused_now) {
            std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
            allowed_conns.clear();
            blocked_conns.clear();
            pending_conns.clear();
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

        // 5. 통계 데이터 JSON 저장 (UI 렌더링용)
        StatsLogger::GetInstance().SaveToFile("FocusGuard_stats.json");
    }
}
