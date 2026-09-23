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

// DNS QNAME(질의 도메인) 파싱 함수(DNS패킷에서 도메인 추출하여 리턴)
std::string extract_dns_query(const u_char* payload, int payload_len) {
    //dns 해더의 크기는 항상 12바이트이기때문에 이를 검사
    if (payload_len <= 12) return "";

    int offset = 12; // DNS 헤더만큼 건너뛰기
    std::string domain = "";

    while (offset < payload_len) {
        uint8_t len = payload[offset++];
        if (len == 0) break; // 질의 이름 종료
        if ((len & 0xC0) == 0xC0) break; // 압축 포인터 예외 처리
        if (offset + len > payload_len) return ""; // 오버플로우 방지

        if (!domain.empty()) domain += ".";
        domain.append((const char*)(payload + offset), len);
        offset += len;
    }
    return domain;
}

//                  화이트리스트 자동 등록
// 바로 json파일에 등록하면 병목 현상이 발생하므로 현재 메모리에
// 로드해놓은 리스트와 비교 후 새로운 것이면 즉시 메모리에 추가
// 이후 따로 json파일에 추가
static std::mutex g_whitelist_file_mtx;

void add_to_whitelist_safe(const std::string& new_domain) {
    if (new_domain.empty()) return;

    // 1. 메모리 중복 체크 (화이트리스트뿐 아니라 블랙리스트도 검사)
    {
        std::shared_lock<std::shared_mutex> r_lock(g_wl_mutex);
        for (const auto& d : whitelist_domains) {
            if (d == new_domain) return;
        }
        for (const auto& b : blacklist_domains) {
            if (b == new_domain) return;
        }
    }

    // 2. whitelist.json 파일 갱신 (전용 뮤텍스로 멀티스레드 파일 경합 보호)
    try {
        std::lock_guard<std::mutex> file_lock(g_whitelist_file_mtx);
        json config_data;
        {
            std::ifstream in_f("whitelist.json");
            if (!in_f.is_open()) return;
            config_data = json::parse(in_f);
        }

        if (!config_data.contains("allowed_domains")) {
            config_data["allowed_domains"] = json::array();
        }

        for (const auto& item : config_data["allowed_domains"]) {
            if (item.get<std::string>() == new_domain) return;
        }
        config_data["allowed_domains"].push_back(new_domain);

        {
            std::ofstream out_f("whitelist.json");
            if (out_f.is_open()) {
                out_f << config_data.dump(2);
            }
        }

        // 3. 메모리에 즉시 반영
        {
            std::unique_lock<std::shared_mutex> w_lock(g_wl_mutex);
            whitelist_domains.push_back(new_domain);
        }

        std::cout << "\n[Auto Whitelist] 💾 연관 도메인 자동 허용 등록: " << new_domain << std::endl;
    }
    catch (...) {}
}

void load_whitelist() {
    try {
        std::ifstream f("whitelist.json");
        if (!f.is_open()) {
            f.open("core_engine/whitelist.json");
        }
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

            // ── 패킷 기본 길이 유효성 검사 ──
            if (pkt->raw_data.size() < sizeof(struct pkt_eth_header) + sizeof(struct pkt_ip_header)) continue;

            // 일시정지 상태(휴식 모드)면 모든 패킷 통과
            if (g_is_paused.load()) {
                continue;
            }

            // 1. 이더넷 헤더 매핑
            struct pkt_eth_header* eth = (struct pkt_eth_header*)pkt_data;

            // IPv4 패킷인지 확인 (0x0800)
            if (ntohs(eth->eth_type) != 0x0800) continue;

            // 2. IP 헤더 매핑
            struct pkt_ip_header* ip = (struct pkt_ip_header*)(pkt_data + sizeof(struct pkt_eth_header));
            int ip_len = (ip->ver_ihl & 0xf) * 4;
            if (ip_len < 20 || pkt->raw_data.size() < sizeof(struct pkt_eth_header) + ip_len) continue;

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

            char src_ip_str[16];
            char dst_ip_str[16];
            sprintf_s(src_ip_str, "%d.%d.%d.%d", ip->saddr.byte1, ip->saddr.byte2, ip->saddr.byte3, ip->saddr.byte4);
            sprintf_s(dst_ip_str, "%d.%d.%d.%d", ip->daddr.byte1, ip->daddr.byte2, ip->daddr.byte3, ip->daddr.byte4);

            // UDP 패킷인지 확인 (proto == 17)
            if (ip->proto == 17) {
                if (pkt->raw_data.size() < sizeof(struct pkt_eth_header) + ip_len + sizeof(struct pkt_udp_header)) continue;
                struct pkt_udp_header* udp = (struct pkt_udp_header*)((u_char*)ip + ip_len);
                int dest_port = ntohs(udp->dport);
                int udp_len = ntohs(udp->len);
                
                // UDP 443 (QUIC) 포트인 경우 ICMP 도달 불가 메시지를 쏘아 TCP로 Fallback 유도
                if (dest_port == 443) {
                    queue_spoofed_icmp_unreachable(pkt_data, header->caplen);
                    continue;
                }

                // UDP 53 (DNS) 처리: 차단 없이 도메인 탐지 로그 출력
                if (dest_port == 53) {
                    int udp_header_len = 8;
                    int dns_payload_len = udp_len - udp_header_len;

                    if (dns_payload_len > 12) {
                        const u_char* dns_payload = (const u_char*)udp + udp_header_len;
                        std::string queried_domain = extract_dns_query(dns_payload, dns_payload_len);

                        if (!queried_domain.empty()) {
                            std::cout << "\n[Thread " << std::this_thread::get_id() << "] [DNS Sniff] 🔍 "
                                << src_ip_str << " -> " << dst_ip_str
                                << " | 도메인: " << queried_domain << std::endl;

                            // ── 화이트리스트 파생 도메인 자동 수집 ── ###알고리즘 이상한걸로 되어있어요###
                            std::string matched_parent = "";
                            {
                                std::shared_lock<std::shared_mutex> r_lock(g_wl_mutex);
                                for (const auto& w : whitelist_domains) {
                                    // 1. 서브도메인 검사 (play.google.com -> google.com)
                                    std::string suffix = "." + w;
                                    if (queried_domain.length() > suffix.length() &&
                                        queried_domain.compare(queried_domain.length() - suffix.length(), suffix.length(), suffix) == 0) {
                                        matched_parent = w;
                                        break;
                                    }

                                    // 2. 국가별 TLD 검사 (google.co.kr -> google.com)
                                    std::string root_name = w.substr(0, w.find('.'));
                                    if (queried_domain.find(root_name + ".") != std::string::npos) {
                                        matched_parent = w;
                                        break;
                                    }
                                }
                            }

                            // 등록된 화이트리스트 사이트의 파생 도메인이면 자동 그룹 저장
                            if (!matched_parent.empty() && queried_domain != matched_parent) {
                                add_to_whitelist_safe(queried_domain);
                            }
                        }
                    }
                    continue; // DNS 패킷은 차단하지 않고 정상 통과시킴
                }
                continue;
            }

            // TCP 패킷인지 확인 (proto == 6)
            if (ip->proto != 6) continue;
            if (pkt->raw_data.size() < sizeof(struct pkt_eth_header) + ip_len + sizeof(struct pkt_tcp_header)) continue;

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
                    queue_spoofed_rst_packet(pkt_data, header->caplen, "Blocked Connection");
                    continue;
                }
            }

            int tcp_len = ((tcp->data_offset >> 4) * 4);
            int payload_len = ntohs(ip->tlen) - ip_len - tcp_len;

            // 페이로드 없는 핸드쉐이크 패킷 (ACK 등) → 통과
            if (payload_len <= 0) continue;

            const u_char* payload = (const u_char*)tcp + tcp_len;
            std::string domain = "";

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
                    queue_spoofed_rst_packet(pkt_data, header->caplen, domain);
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
                        queue_spoofed_rst_packet(pkt_data, header->caplen, domain);
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
                    queue_spoofed_rst_packet(pkt_data, header->caplen, "Pending Block");
                } else {
                    // 캐시에 없는 기존 연결(SNI 없는 데이터 패킷)
                    // → 집중 모드에서는 RST로 강제 종료 (휴식 중 allowed된 소켓 즉시 차단)
                    {
                        std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                        if (blocked_conns.count(conn_id) == 0) {
                            blocked_conns[conn_id] = time(nullptr);
                        }
                    }
                    queue_spoofed_rst_packet(pkt_data, header->caplen, "Stale Connection");
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "\n[Error] Worker Thread 내부 예외 발생 (무시하고 계속 실행): " << e.what() << std::endl;
        } catch (...) {
            // 알 수 없는 예외 무시
        }
    } // while(true) 끝
} // consumer_func 끝
