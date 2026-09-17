#include "watcher.h"
#include "config.h"
#include "packet_processor.h"
#include "stats_logger.h"

// ============================================================
// Watcher 스레드 (관리 전용)
//
// 1. 화이트리스트 핫 리로드 (파일 수정 시 자동 재적용)
// 2. 오래된 IP 캐시 삭제 (TTL 만료 시 제거하여 메모리 절약)
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

    while (true) {
        // 5초에 한 번씩 검사
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // 0. 와이어샤크 비교용 패킷 캡처 통계 출력
        std::cout << "\n[Status] 누적 캡처 패킷 수 (Wireshark 비교용): " << total_captured_cnt.load() << std::endl;

        // 1. 리스트 핫 리로드
        time_t current_time = get_file_mod_time("whitelist.json");
        if (current_time != 0 && current_time != last_time) {
            last_time = current_time;
            std::cout << "\n[Watcher] 필터 파일(whitelist.json) 변경 감지! 핫 리로드를 진행합니다..." << std::endl;
            load_whitelist();
        }

        // 2. IP 캐시 TTL 정리 (쓰기 잠금 필요)
        time_t now = time(nullptr);
        {
            std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);

            // allowed_conns (30초 초과 시 제거 - OS의 TIME_WAIT 주기를 축소 적용시켜 포트 꼬임 완벽 방지)
            for (auto it = allowed_conns.begin(); it != allowed_conns.end(); ) {
                if (now - it->second > 30) it = allowed_conns.erase(it);
                else ++it;
            }

            // blocked_conns (30초 초과 시 제거)
            for (auto it = blocked_conns.begin(); it != blocked_conns.end(); ) {
                if (now - it->second > 30) it = blocked_conns.erase(it);
                else ++it;
            }

            // pending_conns (15초 초과 시 제거 - 인증 안 된 좀비 연결 정리)
            for (auto it = pending_conns.begin(); it != pending_conns.end(); ) {
                if (now - it->second > 15) it = pending_conns.erase(it);
                else ++it;
            }
        }

        // 3. 통계 데이터 JSON 저장 (UI 렌더링용)
        StatsLogger::GetInstance().SaveToFile("FocusGuard_stats.json");
    }
}
