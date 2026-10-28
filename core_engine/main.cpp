#include "config.h"
#include "packet_processor.h"
#include "process_blocker.h"
#include "watcher.h"
#include "stats_logger.h"
#include <windows.h>
#include <conio.h>
#include <vector>

// 관리자 권한 확인 함수
bool IsRunAsAdmin() {
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            fRet = Elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
    return fRet == TRUE;
}

void cleanup_firewall() {
    static std::atomic<bool> cleaned(false);
    if (cleaned.exchange(true)) return;
    system("netsh advfirewall firewall delete rule name=\"Block_QUIC_UDP443\" >nul 2>&1");
    std::cout << "[Info] QUIC 방화벽 차단 규칙을 해제했습니다." << std::endl;
}

// ── 엔진 정상 종료 처리 함수 ──
void stop_engine() {
    static std::atomic<bool> stopping(false);
    if (stopping.exchange(true)) return;

    std::cout << "\n[System] 🛑 FocusGuard 정상 종료 시퀀스를 진행합니다..." << std::endl;
    g_is_running.store(false);

    // 1. pcap_loop 루프 탈출
    if (g_adhandle != nullptr) {
        pcap_breakloop(g_adhandle);
    }

    // 2. IOCP 워커 스레드 깨우기 (코어 수만큼 종료 패킷 전송)
    if (g_hIocp != NULL) {
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
        for (unsigned int i = 0; i < num_threads * 2; ++i) {
            PostQueuedCompletionStatus(g_hIocp, 0, EXIT_COMPLETION_KEY, NULL);
        }
    }

    // 3. 아웃바운드 큐 종료
    g_outbound_queue.Stop();

    // 4. QUIC 방화벽 규칙 해제
    cleanup_firewall();

    // 5. 최종 통계 저장
    StatsLogger::GetInstance().SaveToFile("FocusGuard_stats.json");
    std::cout << "[System] 최종 통계 데이터가 FocusGuard_stats.json에 저장되었습니다." << std::endl;
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        std::cout << "\n[Console] 종료 시그널 감지! 리소스를 정리합니다..." << std::endl;
        stop_engine();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return TRUE;
    }
    return FALSE; // 기본 핸들러 실행
}

// 콘솔창 마우스 클릭/드래그/스크롤 시 발생하는 출력 블로킹(스레드 멈춤) 방지 함수
void DisableConsoleQuickEdit() {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput == INVALID_HANDLE_VALUE) return;

    DWORD prev_mode;
    if (GetConsoleMode(hInput, &prev_mode)) {
        // ENABLE_QUICK_EDIT_MODE와 ENABLE_MOUSE_INPUT 비트를 해제
        // 확장 플래그(ENABLE_EXTENDED_FLAGS)를 함께 넘겨야 QuickEdit 해제가 정상 적용됨
        DWORD new_mode = (prev_mode & ~ENABLE_QUICK_EDIT_MODE & ~ENABLE_MOUSE_INPUT) | ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(hInput, new_mode);
    }
}

// ── 콘솔 키보드 단축키 감시 스레드 ──
void console_input_func() {
    while (g_is_running.load()) {
        if (_kbhit()) {
            int ch = _getch();
            // 특수키/확장키 2바이트 처리
            if (ch == 0 || ch == 224) {
                _getch();
                continue;
            }

            if (ch == 'q' || ch == 'Q') {
                std::cout << "\n[단축키 'Q'] 정상 종료가 요청되었습니다." << std::endl;
                stop_engine();
                break;
            } else if (ch == 'u' || ch == 'U') {
                toggle_block_unregistered();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8); // 터미널 한글 깨짐 영구 방지
    SetConsoleCP(CP_UTF8);       // 터미널 한글 깨짐 영구 방지

    // ============================================================
    // 관리자 권한 필수 체크 (QUIC 차단 방화벽 추가를 위함)
    // ============================================================
    if (!IsRunAsAdmin()) {
        std::cerr << "\n[Error] ⛔ 관리자 권한이 없습니다!" << std::endl;
        std::cerr << "유튜브 등 최신 사이트가 사용하는 QUIC(UDP 443) 통신을 차단하려면 방화벽 제어가 필요합니다." << std::endl;
        std::cerr << "프로그램을 [관리자 권한으로 실행]해 주세요.\n" << std::endl;
        return -1;
    }

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    atexit(cleanup_firewall);

    // ============================================================
    // [보안/차단 강화] QUIC 프로토콜(UDP 443) 원천 차단 방화벽 규칙 등록
    // ============================================================
    system("netsh advfirewall firewall add rule name=\"Block_QUIC_UDP443\" dir=out action=block protocol=UDP remoteport=443 >nul 2>&1");
    std::cout << "[Info] QUIC (UDP 443) 차단 방화벽 규칙을 적용했습니다." << std::endl;

    // 출력 블로킹(스레드 멈춤) 방지 함수 호출
    DisableConsoleQuickEdit();
   
    pcap_if_t* alldevs;
    pcap_if_t* d;
    pcap_if_t* target_dev = NULL;
    char errbuf[PCAP_ERRBUF_SIZE];

    // 1. 시스템의 모든 네트워크 어댑터 찾기
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        std::cerr << "어댑터를 찾을 수 없습니다: " << errbuf << std::endl;
        cleanup_firewall();
        return -1;
    }

    // JSON 화이트리스트 및 어댑터 설정 로드
    load_whitelist();

    // 2. 어댑터 선택: whitelist.json의 target_adapter가 명시적으로 지정되어 있으면 우선 매칭
    if (!g_target_adapter.empty() && g_target_adapter != "AUTO" && g_target_adapter != "자동 감지") {
        for (d = alldevs; d != NULL; d = d->next) {
            std::string dName = d->name ? d->name : "";
            std::string dDesc = d->description ? d->description : "";
            if (dName == g_target_adapter || dDesc == g_target_adapter || 
                dDesc.find(g_target_adapter) != std::string::npos ||
                dName.find(g_target_adapter) != std::string::npos) {
                target_dev = d;
                std::cout << "[Info] 지정된 사용자 네트워크 어댑터 선택됨: " << (d->description ? d->description : d->name) << std::endl;
                break;
            }
        }
    }

    // 2-B. 지정된 어댑터가 없거나 "AUTO"인 경우: 활성 인터넷 어댑터 자동 탐색
    if (target_dev == NULL) {
        if (!g_target_adapter.empty() && g_target_adapter != "AUTO" && g_target_adapter != "자동 감지") {
            std::cout << "[Warning] 지정된 어댑터(" << g_target_adapter << ")를 찾지 못하여 활성 네트워크 자동 감지로 전환합니다." << std::endl;
        } else {
            std::cout << "[Info] 🌐 네트워크 어댑터 자동 감지(AUTO) 모드 가동 중..." << std::endl;
        }
        for (d = alldevs; d != NULL; d = d->next) {
            if (d->flags & PCAP_IF_LOOPBACK) continue;
            
            bool has_ipv4 = false;
            for (pcap_addr_t* a = d->addresses; a != NULL; a = a->next) {
                if (a->addr && a->addr->sa_family == AF_INET) {
                    struct sockaddr_in* sa = (struct sockaddr_in*)a->addr;
                    // APIPA (169.254.x.x) 주소는 무시 (연결 안 된 어댑터)
                    if (!(sa->sin_addr.S_un.S_un_b.s_b1 == 169 && sa->sin_addr.S_un.S_un_b.s_b2 == 254)) {
                        has_ipv4 = true;
                        break;
                    }
                }
            }

            std::string desc = d->description ? d->description : "";
            // 가상 머신이나 루프백, 블루투스 등 실제 인터넷 통신이 아닌 어댑터 배제
            if (desc.find("VMware") != std::string::npos || 
                desc.find("Virtual") != std::string::npos || 
                desc.find("Hyper-V") != std::string::npos || 
                desc.find("Loopback") != std::string::npos ||
                desc.find("Bluetooth") != std::string::npos) {
                continue;
            }

            if (has_ipv4) {
                target_dev = d;
                std::cout << "[Info] 활성 인터넷 네트워크 어댑터 자동 선택됨: " << (d->description ? d->description : d->name) << std::endl;
                break;
            }
        }
    }

    if (target_dev == NULL && alldevs != NULL) {
        target_dev = alldevs; // 조건에 맞는 게 없으면 첫 번째 어댑터 강제 선택
        std::cout << "[Info] 기본 첫 번째 어댑터 선택됨: " << (target_dev->description ? target_dev->description : target_dev->name) << std::endl;
    }

    if (target_dev == NULL) {
        std::cerr << "[Error] 사용 가능한 네트워크 인터페이스가 없습니다." << std::endl;
        pcap_freealldevs(alldevs);
        cleanup_firewall();
        return -1;
    }

    std::cout << "[Info] 캡처 장치 선택 완료: " << (target_dev->description ? target_dev->description : target_dev->name) << std::endl;

    // 3. 어댑터 열기 (캡처 시작)
    pcap_t* adhandle = pcap_open_live(
        target_dev->name,   // 장치 이름
        1500,               // 패킷 최대 길이 (MTU 사이즈 슬라이싱으로 메모리/CPU 최적화)
        1,                  // Promiscuous 모드 (모든 패킷 캡처)
        1,                  // 읽기 타임아웃 (1ms로 줄여서 RST 발사 지연 방지)
        errbuf              // 에러 버퍼
    );

    if (adhandle == NULL) {
        std::cerr << "어댑터를 열 수 없습니다: " << target_dev->description << std::endl;
        pcap_freealldevs(alldevs);
        cleanup_firewall();
        return -1;
    }

    // 전역 핸들에 등록 (pcap_breakloop를 위해)
    g_adhandle = adhandle;

    // Npcap 커널 버퍼 대폭 확장 (기본 1MB -> 50MB)
    pcap_setbuff(adhandle, 50 * 1024 * 1024);
    pcap_freealldevs(alldevs);

    // ============================================================
    // BPF(Berkeley Packet Filter) 설정
    // ============================================================
    struct bpf_program fcode;
    const char* filter_exp = "tcp port 80 or tcp port 443 or udp port 443 or (udp dst port 53)";
    if (pcap_compile(adhandle, &fcode, filter_exp, 1, PCAP_NETMASK_UNKNOWN) < 0) {
        std::cerr << "[Error] BPF 필터 컴파일 에러: " << pcap_geterr(adhandle) << std::endl;
    } else {
        if (pcap_setfilter(adhandle, &fcode) < 0) {
            std::cerr << "[Error] BPF 필터 적용 에러: " << pcap_geterr(adhandle) << std::endl;
        } else {
            std::cout << "[Info] BPF 커널 필터 적용 성공: " << filter_exp << std::endl;
        }
    }

    // ============================================================
    // 스레드 풀 및 백그라운드 관리 스레드 기동
    // ============================================================
    std::vector<std::thread> worker_threads;

    // 1. 위조 패킷 발사 스레드
    worker_threads.emplace_back(packet_sender_func, adhandle);

    // 2. 전역 IOCP 큐 생성 및 Consumer 워커 스레드 풀 생성
    g_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    for (unsigned int i = 0; i < num_threads; i++) {
        worker_threads.emplace_back(consumer_func, adhandle);
    }
    std::cout << "[IOCP] " << num_threads << "개의 병렬 Worker 스레드가 기동되었습니다." << std::endl;

    // 3. 프로세스 차단 관리자 스레드 기동
    worker_threads.emplace_back(process_blocker_func);

    // 4. 상태 감시 및 통계 스레드 기동
    worker_threads.emplace_back(watcher_func);

    // 5. 콘솔 키보드 단축키 감시 스레드 기동
    worker_threads.emplace_back(console_input_func);

    std::cout << "네트워크 패킷 캡처를 시작합니다...\n" << std::endl;

    // 4. 패킷 캡처 루프 수행 (stop_engine()에서 pcap_breakloop() 호출 시 루프 탈출)
    pcap_loop(adhandle, 0, packet_handler, (u_char*)adhandle);

    // 루프 탈출 후 정상 종료 절차 수행
    stop_engine();

    // 모든 백그라운드 스레드 정상 종료 대기 (Join)
    std::cout << "[System] 모든 백그라운드 스레드 종료 대기 중..." << std::endl;
    for (auto& t : worker_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (g_hIocp != NULL) {
        CloseHandle(g_hIocp);
        g_hIocp = NULL;
    }

    pcap_close(adhandle);
    g_adhandle = nullptr;

    std::cout << "[System] ✅ FocusGuard가 안전하게 정상 종료되었습니다.\n" << std::endl;
    return 0;
}
