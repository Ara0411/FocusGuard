#include "packet_processor.h"
#include "stats_logger.h"

// HTTP Host 추출 함수
std::string extract_http_host(const u_char* payload, int payload_len) {
    std::string data((const char*)payload, payload_len);
    size_t host_pos = data.find("Host: ");
    if (host_pos != std::string::npos) {
        size_t end_pos = data.find("\r\n", host_pos);
        if (end_pos != std::string::npos) {
            return data.substr(host_pos + 6, end_pos - (host_pos + 6));
        }
    }
    return "";
}

// HTTPS TLS SNI (Server Name Indication) 확장 필드 추출 함수
std::string extract_sni(const u_char* payload, int payload_len) {
    // 최소한의 TLS ClientHello 크기 확인
    if (payload_len < 43) return "";
    if (payload[0] != 0x16) return ""; // 0x16: Handshake
    if (payload[5] != 0x01) return ""; // 0x01: Client Hello

    // TLS Header(5) + Handshake Header(4) + Client Version(2) + Random(32) = 43 bytes
    int offset = 43;
    
    // Session ID
    if (offset >= payload_len) return "";
    int session_id_len = payload[offset];
    offset += 1 + session_id_len;

    // Cipher Suites
    if (offset + 2 > payload_len) return "";
    int cipher_suites_len = (payload[offset] << 8) | payload[offset+1];
    offset += 2 + cipher_suites_len;

    // Compression Methods
    if (offset >= payload_len) return "";
    int comp_methods_len = payload[offset];
    offset += 1 + comp_methods_len;

    // Extensions
    if (offset + 2 > payload_len) return "";
    int extensions_len = (payload[offset] << 8) | payload[offset+1];
    offset += 2;

    int ext_end = offset + extensions_len;
    if (ext_end > payload_len) ext_end = payload_len;

    // 확장 필드 순회하며 SNI (Type 0x0000) 찾기
    while (offset + 4 <= ext_end) {
        int ext_type = (payload[offset] << 8) | payload[offset+1];
        int ext_len = (payload[offset+2] << 8) | payload[offset+3];
        offset += 4;

        if (ext_type == 0x0000) { // Server Name Indication 타입 발견!
            if (offset + 5 <= ext_end) {
                int name_type = payload[offset+2];
                if (name_type == 0) { // Hostname 타입
                    int name_len = (payload[offset+3] << 8) | payload[offset+4];
                    offset += 5;
                    if (offset + name_len <= ext_end) {
                        return std::string((const char*)(payload + offset), name_len);
                    }
                }
            }
            break;
        }
        offset += ext_len;
    }
    return "";
}

void load_whitelist() {
    try {
        std::ifstream f("whitelist.json");
        if (f.is_open()) {
            json data = json::parse(f);
            std::vector<std::string> new_wl;
            std::vector<std::string> new_bl;
            std::vector<std::string> new_bl_process;
            if (data.contains("allowed_domains")) {
                for (auto& el : data["allowed_domains"]) {
                    new_wl.push_back(el.get<std::string>());
                }
            }
            if (data.contains("blocked_domains")) {
                for (auto& el : data["blocked_domains"]) {
                    new_bl.push_back(el.get<std::string>());
                }
            }
            if (data.contains("blocked_process")) {
                for (auto& el : data["blocked_process"]) {
                    new_bl_process.push_back(el.get<std::string>());
                }
            }
            
            // 파싱이 성공적으로 끝난 뒤에만 원본 리스트를 교체 (안전한 업데이트)
            {
                std::unique_lock<std::shared_mutex> lock(g_wl_mutex); // 리스트 보호용 뮤텍스 공용 사용
                whitelist_domains = std::move(new_wl);
                blacklist_domains = std::move(new_bl);
                blacklist_process = std::move(new_bl_process);
            }
            std::cout << "[Info] 필터 로드 완료: 허용 " << whitelist_domains.size() << "개 / 도메인 차단 " << blacklist_domains.size() << "개 / 앱 차단 " << blacklist_process.size() << "개" << std::endl;
        } else {
            std::cerr << "[Warning] whitelist.json 파일을 찾을 수 없습니다. 모든 통신이 차단될 수 있습니다. " << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Error] 화이트리스트 로드 실패: " << e.what() << std::endl;
    }
}

// ============================================================
// [Step 4-A] packet_handler: Producer 역할만 담당
//
// 변경 전 (단일 스레드):
//   packet_handler = 수신 + SNI 분석 + 화이트리스트 검사 + RST 발사
//   → 모든 작업이 직렬로 실행되어 처리 중 다음 패킷 수신 불가
//
// 변경 후 (멀티스레드):
//   packet_handler = 수신 + Queue에 push (3줄로 단순화)
//   → 수신 직후 복사본을 Queue에 넣고 즉시 반환
//   → 다음 패킷을 지체 없이 바로 수신할 수 있음
//
// 주의: param으로 넘어오는 adhandle은 더 이상 여기서 사용하지 않음
//       RST 발사는 consumer_func에서 전역 adhandle을 통해 수행함
// ============================================================
void packet_handler(u_char* /*param*/, const struct pcap_pkthdr* header, const u_char* pkt_data) {
    total_captured_cnt++; // 캡처될 때마다 1씩 증가

    try {
        // 1. 메모리 풀에서 객체 하나 빌려오기
        CapturedPacket* pkt = g_packet_pool.Acquire();
        
        // 2. 패킷 데이터를 객체에 복사
        pkt->SetData(header, pkt_data);

        // 3. IOCP 큐에 패킷 포인터를 넘기고 즉시 반환 (비동기 통지)
        // OS 커널이 Consumer 스레드들을 관리하며 패킷을 전달해 줍니다.
        // 진정한 Overlapped I/O 패러다임에 맞춰 LPOVERLAPPED 인자를 통해 객체를 전달합니다.
        PostQueuedCompletionStatus(g_hIocp, 0, 0, (LPOVERLAPPED)pkt);
    } catch (...) {
        // 메모리 풀 고갈 등 예외 발생 시 현재 패킷 드랍 (안전 무시)
    }
}

// ============================================================
// consumer_func: IOCP 기반 병렬 패킷 처리 워커(Worker) 스레드
//
// IOCP 큐(g_hIocp)에서 대기하며 OS로부터 패킷을 할당받아
// SNI 분석 후 허용/차단 여부를 판정하고 RST 패킷을 발사합니다.
// ============================================================
void consumer_func(pcap_t* adhandle) {
    while (true) {
        try {
            DWORD bytesTransferred = 0;
            ULONG_PTR completionKey = 0;
            LPOVERLAPPED pOverlapped = NULL;

            // IOCP 큐에서 패킷 대기 (여러 스레드가 대기하더라도 OS가 알아서 충돌 없이 분배함)
            BOOL bRet = GetQueuedCompletionStatus(g_hIocp, &bytesTransferred, &completionKey, &pOverlapped, INFINITE);
            
            // 에러나 널 포인터는 무시 (이제 객체는 OVERLAPPED 포인터로 들어옵니다)
            if (!bRet || pOverlapped == NULL) continue;

            // LPOVERLAPPED 포인터를 CapturedPacket 객체로 캐스팅
            CapturedPacket* pkt = (CapturedPacket*)pOverlapped;

            // RAII 래퍼: 이 스코프 내에서 continue; 로 빠져나가더라도 
            // 무조건 풀에 반환되도록 하여 메모리 누수(풀 고갈) 방지
            struct PktReleaser {
                CapturedPacket* p;
                ~PktReleaser() { if (p) g_packet_pool.Release(p); }
            } releaser{pkt};

            // 꺼낸 패킷의 원시 바이트 포인터와 헤더 참조
            const u_char* pkt_data = pkt->raw_data.data();
            const struct pcap_pkthdr* header = &pkt->header;

            // ── 이하 로직은 기존 packet_handler에서 그대로 이동 ──

            // 1. 이더넷 헤더 매핑
            struct pkt_eth_header* eth = (struct pkt_eth_header*)pkt_data;

            // IPv4 패킷인지 확인 (0x0800)
            if (ntohs(eth->eth_type) != 0x0800) continue;

            // 2. IP 헤더 매핑
            struct pkt_ip_header* ip = (struct pkt_ip_header*)(pkt_data + sizeof(struct pkt_eth_header));

            // ── 위조 패킷 자가 수신(루프백) 방지 ──
            // 우리가 발송한 위조 RST/ICMP 패킷은 고유 ID를 가짐. 이를 무시하지 않으면
            // 방금 쏜 위조 RST 패킷이 곧바로 이 스레드로 다시 들어와 차단 캐시를 지워버리는 치명적 버그가 발생함.
            if (ip->identification == htons(54321) || 
                ip->identification == htons(12345) || 
                ip->identification == htons(12346)) {
                continue;
            }

            // IP 주소 추출
            uint32_t src_ip = *(uint32_t*)&ip->saddr;
            uint32_t dst_ip = *(uint32_t*)&ip->daddr;

            // UDP 패킷인지 확인 (proto == 17)
            if (ip->proto == 17) {
                int ip_len = (ip->ver_ihl & 0xf) * 4;
                struct pkt_udp_header* udp = (struct pkt_udp_header*)((u_char*)ip + ip_len);
                int dest_port = ntohs(udp->dport);
                
                // UDP 443 (QUIC) 포트인 경우 ICMP 도달 불가 메시지를 쏘아 TCP로 Fallback 유도
                if (dest_port == 443) {
                    send_spoofed_icmp_unreachable(adhandle, pkt_data);
                }
                continue;
            }

            // TCP 패킷인지 확인 (proto == 6)
            if (ip->proto != 6) continue;

            int ip_len = (ip->ver_ihl & 0xf) * 4;
            struct pkt_tcp_header* tcp = (struct pkt_tcp_header*)((u_char*)ip + ip_len);

            int dest_port = ntohs(tcp->dport);
            // 80(HTTP), 443(HTTPS) 포트만 처리
            if (dest_port != 80 && dest_port != 443) continue;

            // 커넥션 ID 생성 (Server IP + Client Port)
            // src_ip 대신 dst_ip를 사용하여 포트 재사용 시 다른 목적지와의 충돌 방지
            uint64_t conn_id = ((uint64_t)dst_ip << 32) | ntohs(tcp->sport);

            // ── 연결 종료(FIN, RST) 감지 시 캐시 즉시 삭제 (포트 재사용 꼬임 방지) ──
            if ((tcp->flags & 0x01) || (tcp->flags & 0x04)) {
                std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                allowed_conns.erase(conn_id);
                blocked_conns.erase(conn_id);
                pending_conns.erase(conn_id);
                if (tcp->flags & 0x04) continue; // 내가 쏜 위조 RST 패킷 무한루프 방지
            }

            // ── 커넥션 목록 읽기 (shared_lock: 여러 스레드 동시 읽기 허용) ──
            {
                std::shared_lock<std::shared_mutex> r_lock(g_ip_mutex);
                if (allowed_conns.count(conn_id) > 0) continue; // 허용된 커넥션 → 즉시 통과
            }

            // ── SYN 패킷 처리 (새 연결 시도 감지) ──
            // SYN=0x02, ACK=0x10 → SYN만 있고 ACK 없으면 새 연결
            if ((tcp->flags & 0x02) && !(tcp->flags & 0x10)) {
                // ── 커넥션 목록 쓰기 (unique_lock: 단독 접근) ──
                std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                // blocked/allowed 목록에 없을 때만 pending에 등록
                if (blocked_conns.count(conn_id) == 0 && allowed_conns.count(conn_id) == 0) {
                    pending_conns[conn_id] = time(nullptr); // 삽입 시각 저장
                }
                continue; // SYN 패킷 자체는 통과시킴
            }

            // ── 이미 차단된 커넥션이라면 즉시 RST 주입 ──
            {
                std::shared_lock<std::shared_mutex> r_lock(g_ip_mutex);
                if (blocked_conns.count(conn_id) > 0) {
                    send_spoofed_rst_packet(adhandle, pkt_data, header->caplen, "Blocked Connection");
                    continue;
                }
            }

            int tcp_len = ((tcp->data_offset >> 4) * 4);
            int payload_len = ntohs(ip->tlen) - ip_len - tcp_len;

            // 페이로드 없는 핸드쉐이크 패킷 (ACK 등) → 통과
            if (payload_len <= 0) continue;

            const u_char* payload = (const u_char*)tcp + tcp_len;
            std::string domain = "";
            
            char src_ip_str[16];
            char dst_ip_str[16];
            sprintf_s(src_ip_str, "%d.%d.%d.%d", ip->saddr.byte1, ip->saddr.byte2, ip->saddr.byte3, ip->saddr.byte4);
            sprintf_s(dst_ip_str, "%d.%d.%d.%d", ip->daddr.byte1, ip->daddr.byte2, ip->daddr.byte3, ip->daddr.byte4);

            if (dest_port == 80) {
                domain = extract_http_host(payload, payload_len);
            } else if (dest_port == 443) {
                domain = extract_sni(payload, payload_len);
            }

            if (!domain.empty()) {
                // ── 필터링 (블랙리스트 우선 검사, 이후 화이트리스트) ──
                bool is_blacklisted = false;
                bool is_whitelisted = false;
                {
                    std::shared_lock<std::shared_mutex> r_lock(g_wl_mutex);
                    // 1. 블랙리스트 검사
                    for (const auto& b : blacklist_domains) {
                        if (domain == b) { is_blacklisted = true; break; }
                        std::string suffix = "." + b;
                        if (domain.length() > suffix.length() &&
                            domain.compare(domain.length() - suffix.length(), suffix.length(), suffix) == 0) {
                            is_blacklisted = true; break;
                        }
                    }
                    
                    // 2. 화이트리스트 검사 (블랙리스트에 없을 때만)
                    if (!is_blacklisted) {
                        for (const auto& w : whitelist_domains) {
                            if (domain == w) { is_whitelisted = true; break; }
                            std::string suffix = "." + w;
                            if (domain.length() > suffix.length() &&
                                domain.compare(domain.length() - suffix.length(), suffix.length(), suffix) == 0) {
                                is_whitelisted = true; break;
                            }
                        }
                    }
                } // g_wl_mutex 해제

                if (is_blacklisted) {
                    // ── 명시적 블랙리스트 도메인 차단 ──
                    {
                        std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                        if (blocked_conns.count(conn_id) == 0) {
                            pending_conns.erase(conn_id);
                            blocked_conns[conn_id] = time(nullptr);
                            std::cout << "\n[Thread " << std::this_thread::get_id() << "] [BLACKLIST HIT] ⛔ 명시적 차단 | " << src_ip_str << " -> " << dst_ip_str << " | 도메인: " << domain << std::endl;
                            StatsLogger::GetInstance().LogBlockedConnection(domain);
                        }
                    }
                    send_spoofed_rst_packet(adhandle, pkt_data, header->caplen, domain);
                }
                else if (is_whitelisted) {
                    // ── 허용 커넥션 등록 (쓰기 잠금) ──
                    std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                    if (allowed_conns.count(conn_id) == 0) {
                        pending_conns.erase(conn_id); // pending에서 제거
                        allowed_conns[conn_id] = time(nullptr); // 허용 시각 저장
                        std::cout << "\n[Thread " << std::this_thread::get_id() << "] [Whitelist] ✅ OK 허용 | " << src_ip_str << " -> " << dst_ip_str << " | 도메인: " << domain << std::endl;
                        StatsLogger::GetInstance().LogNormalConnection();
                    }
                } else {
                    // ── 화이트리스트에 없는 미등록 도메인 처리 ──
                    bool is_whitelist_active = false;
                    {
                        std::shared_lock<std::shared_mutex> r_lock(g_wl_mutex);
                        // 화이트리스트에 1개 이상의 도메인이 있을 때만 "나머지 전체 차단(White-list 모드)" 작동
                        is_whitelist_active = !whitelist_domains.empty();
                    }

                    if (is_whitelist_active) {
                        {
                            std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                            if (blocked_conns.count(conn_id) == 0) {
                                pending_conns.erase(conn_id); // pending에서 제거
                                blocked_conns[conn_id] = time(nullptr); // 차단 시각 저장
                                std::cout << "\n[Thread " << std::this_thread::get_id() << "] [Unknown] ❌ 미등록 차단 | " << src_ip_str << " -> " << dst_ip_str << " | 도메인: " << domain << std::endl;
                                StatsLogger::GetInstance().LogBlockedConnection(domain);
                            }
                        } // g_ip_mutex 해제 후 RST 발사
                        send_spoofed_rst_packet(adhandle, pkt_data, header->caplen, domain);
                    } else {
                        // 화이트리스트가 비어있다면 (JSON 파싱 실패 등), 미등록 도메인을 기본 허용 (Fail-Open)
                        std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                        if (allowed_conns.count(conn_id) == 0) {
                            pending_conns.erase(conn_id);
                            allowed_conns[conn_id] = time(nullptr);
                            // 너무 많은 로그 출력을 방지하기 위해 콘솔 출력 생략
                            StatsLogger::GetInstance().LogNormalConnection();
                        }
                    }
                }
            } else {
                // SNI/Host 없는 데이터 패킷
                // pending 목록에 있는 커넥션이면 → 도메인 미확인 상태이므로 RST 차단
                bool is_pending = false;
                {
                    std::shared_lock<std::shared_mutex> r_lock(g_ip_mutex);
                    is_pending = (pending_conns.count(conn_id) > 0);
                }
                if (is_pending) {
                    send_spoofed_rst_packet(adhandle, pkt_data, header->caplen, "Pending Block");
                }
                // pending에도 없으면 → 기존 연결, 일단 통과
            }
        } catch (const std::exception& e) {
            std::cerr << "\n[Error] Worker Thread 내부 예외 발생 (무시하고 계속 실행): " << e.what() << std::endl;
        } catch (...) {
            // 알 수 없는 예외 무시
        }
    } // while(true) 끝
} // consumer_func 끝

// ============================================================
// [Step 6] Watcher 스레드 (관리 전용)
//
// 1. 화이트리스트 핫 리로드 (파일 수정 시 자동 재적용)
// 2. 오래된 IP 캐시 삭제 (TTL 만료 시 제거하여 메모리 절약)
// ============================================================
time_t get_file_mod_time(const char* path) {
    struct stat result;
    if (stat(path, &result) == 0) {
        return result.st_mtime;
    }
    return 0;
}

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
