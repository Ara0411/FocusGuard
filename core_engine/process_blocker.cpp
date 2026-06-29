#include "process_blocker.h"

// RAII 패턴을 적용한 안전한 핸들 관리 클래스
struct AutoHandle {
    HANDLE h;
    AutoHandle(HANDLE h) : h(h) {}
    ~AutoHandle() { 
        if (h != NULL && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h); 
        }
    }
    operator HANDLE() const { return h; }
};

//대소문자 무시 문자열 비교 함수
bool iequals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](char a, char b) {
            return tolower(a) == tolower(b);
        });
}

void process_blocker_func()
{
    std::cout << "[process_blocker] 앱 차단 모니터링 스레드가 시작되었습니다." << std::endl;
    while (true) {
        // 0.2초마다 검사 (타임 갭 최소화)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<std::string> current_blacklist;
        {
            // 화이트리스트/블랙리스트가 핫 리로드 될 수 있으므로 Read Lock 걸고 가져오기
            std::shared_lock<std::shared_mutex> r_lock(g_wl_mutex);
            current_blacklist = blacklist_process;
        }
        if (current_blacklist.empty()) continue; // 차단할 앱이 없으면 패스

        // 프로세스 스냅샷 생성
        AutoHandle hProcessSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (hProcessSnap == INVALID_HANDLE_VALUE) continue;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        // 첫 번째 프로세스 정보 가져오기
        if (!Process32First(hProcessSnap, &pe32)) {
            continue;
        }

        // 전체 프로세스 순회
        do {
            std::string exeName = pe32.szExeFile;
            bool should_block = false;

            // 블랙리스트 앱 중 일치하는 것이 있는지 검사
            for (const auto& blocked_app : current_blacklist) {
                if (iequals(exeName, blocked_app)) {
                    should_block = true;
                    break;
                }
            }

            if (should_block) {
                // 해당 PID의 프로세스 강제 종료를 위해 핸들 열기
                AutoHandle hProcess(OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID));
                if (hProcess != NULL) {
                    if (TerminateProcess(hProcess, 0)) {
                        std::cout << "\n[Process Block] ⛔ 블랙리스트 앱 차단됨: " << exeName << " (PID: " << pe32.th32ProcessID << ")" << std::endl;
                    }
                }
            }

        } while (Process32Next(hProcessSnap, &pe32));
    }
}
