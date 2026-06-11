#include "config.h"
#include "packet_processor.h"

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
    system("netsh advfirewall firewall delete rule name=\"Block_QUIC_UDP443\" >nul 2>&1");
    std::cout << "[Info] QUIC 방화벽 차단 규칙을 해제했습니다." << std::endl;
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        cleanup_firewall();
    }
    return FALSE; // 기본 핸들러 실행 (프로그램 종료)
}

int main() {
    SetConsoleOutputCP(CP_UTF8); // [안티그래비티 추가] 터미널 한글 깨짐 영구 방지
    SetConsoleCP(CP_UTF8);       // [안티그래비티 추가] 터미널 한글 깨짐 영구 방지

    // ============================================================
    // 관리자 권한 필수 체크 (QUIC 차단 방화벽 추가를 위함)
    // ============================================================
    if (!IsRunAsAdmin()) {
        std::cerr << "\n[Error] ⛔ 관리자 권한이 없습니다!" << std::endl;
        std::cerr << "유튜브 등 최신 사이트가 사용하는 QUIC(UDP 443) 통신을 차단하려면 방화벽 제어가 필요합니다." << std::endl;
        std::cerr << "Visual Studio를 [관리자 권한으로 실행]하거나, black_test.exe를 우클릭 후 관리자로 실행해 주세요.\n" << std::endl;
        system("pause");
        return -1;
    }

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    atexit(cleanup_firewall);
    // ============================================================
    // [보안/차단 강화] QUIC 프로토콜(UDP 443) 원천 차단 방화벽 규칙 등록
    // 크롬 등 최신 브라우저는 ICMP 위조 패킷을 무시하고 QUIC 연결을 고집할 수 있습니다.
    // 윈도우 방화벽 단에서 UDP 443을 차단하면 브라우저는 즉시 TCP(HTTPS)로 Fallback합니다.
    // ============================================================
    system("netsh advfirewall firewall add rule name=\"Block_QUIC_UDP443\" dir=out action=block protocol=UDP remoteport=443 >nul 2>&1");
    std::cout << "[Info] QUIC (UDP 443) 차단 방화벽 규칙을 적용했습니다." << std::endl;

   
    pcap_if_t* alldevs;
    pcap_if_t* d;
    pcap_if_t* target_dev = NULL;
    char errbuf[PCAP_ERRBUF_SIZE];

    // 1. 시스템의 모든 네트워크 어댑터 찾기
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        std::cerr << "어댑터를 찾을 수 없습니다: " << errbuf << std::endl;
        return -1;
    }

    // 2. 배포용 엔진: 사용자 개입 없이 활성 인터넷 어댑터 자동 선택
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
        // 가상 머신이나 루프백 등 실제 통신이 아닌 어댑터 배제
        if (desc.find("VMware") != std::string::npos || 
            desc.find("Virtual") != std::string::npos || 
            desc.find("Hyper-V") != std::string::npos || 
            desc.find("Loopback") != std::string::npos) {
            continue;
        }

        if (has_ipv4) {
            target_dev = d;
            break;
        }
    }

    if (target_dev == NULL && alldevs != NULL) {
        target_dev = alldevs; // 조건에 맞는 게 없으면 첫 번째 어댑터 강제 선택
    }

    if (target_dev == NULL) {
        std::cerr << "[Error] 사용 가능한 네트워크 인터페이스가 없습니다." << std::endl;
        pcap_freealldevs(alldevs);
        return -1;
    }

    // JSON 화이트리스트 로드
    load_whitelist();

    std::cout << "[Info] 캡처 장치 선택 완료: " << target_dev->description << std::endl;

    // 3. 어댑터 열기 (캡처 시작)
    pcap_t* adhandle = pcap_open_live(
        target_dev->name,   // 장치 이름
        65536,              // 패킷 최대 길이
        1,                  // Promiscuous 모드 (모든 패킷 캡처)
        1,                  // 읽기 타임아웃 (1000ms -> 1ms로 줄여서 RST 발사 지연 완벽 방지)
        errbuf              // 에러 버퍼
    );

    if (adhandle == NULL) {
        std::cerr << "어댑터를 열 수 없습니다: " << target_dev->description << std::endl;
        pcap_freealldevs(alldevs);
        return -1;
    }

    // Npcap 커널 버퍼 대폭 확장 (기본 1MB -> 50MB)
    // 엄청나게 빠른 새로고침(F5) 연타 시 패킷 병목으로 인한 유실(Bypass) 완벽 방지
    pcap_setbuff(adhandle, 50 * 1024 * 1024);

    pcap_freealldevs(alldevs);

    // ============================================================
    // BPF(Berkeley Packet Filter) 설정
    // ============================================================
    // 80, 443 포트와 관련된 TCP 트래픽 및 QUIC 트래픽(UDP 443) 수신하도록 커널 단 필터 적용
    struct bpf_program fcode;
    const char* filter_exp = "tcp port 80 or tcp port 443 or udp port 443";
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
    // [IOCP] 초기화 및 스레드 풀(Consumer) 기동
    // ============================================================
    // 1. 전역 IOCP 큐 생성
    g_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    // 2. CPU 코어 개수 파악
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4; // 코어 수를 못 가져올 경우 기본 4개

    // 3. 코어 수만큼 Consumer 스레드 기동
    for (unsigned int i = 0; i < num_threads; i++) {
        std::thread consumer_thread(consumer_func, adhandle);
        consumer_thread.detach();
    }
    std::cout << "[IOCP] " << num_threads << "개의 병렬 Worker 스레드가 기동되었습니다.\n" << std::endl;

    // ============================================================
    // Watcher 스레드 기동 (핫 리로드 및 메모리 누수 방지용 TTL 정리)
    // ============================================================
    std::thread watcher_thread(watcher_func);
    watcher_thread.detach();



    std::cout << "네트워크 패킷 캡처를 시작합니다... (종료하려면 콘솔 창을 닫아주세요)\n" << std::endl;

    // 4. 무한 루프로 패킷 캡처 수행 (packet_handler 호출)
    pcap_loop(adhandle, 0, packet_handler, (u_char*)adhandle);

    pcap_close(adhandle);
    return 0;
}
