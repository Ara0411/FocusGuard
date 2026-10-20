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
    // dns 해더의 크기는 항상 12바이트이기 때문에 이를 검사
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

// 화이트리스트 자동 등록 전용 뮤텍스
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
    std::lock_guard<std::mutex> file_lock(g_whitelist_file_mtx);

    // JSON 파일 읽기
    std::string filename = "whitelist.json";
    std::ifstream in_f(filename);
    if (!in_f.is_open()) {
        filename = "core_engine/whitelist.json";
        in_f.open(filename);
    }
    if (!in_f.is_open()) {
        std::cerr << "[Warning] add_to_whitelist_safe: whitelist.json 열기 실패" << std::endl;
        return;
    }

    // JSON 파싱 전 유효성 검증
    std::string raw_json((std::istreambuf_iterator<char>(in_f)), std::istreambuf_iterator<char>());
    in_f.close();
    if (!json::accept(raw_json)) {
        std::cerr << "[Error] add_to_whitelist_safe: whitelist.json 이 유효하지 않은 JSON 형식입니다." << std::endl;
        return;
    }
    json config_data = json::parse(raw_json);

    if (!config_data.contains("allowed_domains")) {
        config_data["allowed_domains"] = json::array();
    }

    for (const auto& item : config_data["allowed_domains"]) {
        if (item.is_string() && item.get<std::string>() == new_domain) return;
    }
    config_data["allowed_domains"].push_back(new_domain);

    std::ofstream out_f(filename);
    if (!out_f.is_open()) {
        std::cerr << "[Error] add_to_whitelist_safe: whitelist.json 쓰기 실패" << std::endl;
        return;
    }
    out_f << config_data.dump(2);
    out_f.close();

    // 3. 메모리에 즉시 반영
    {
        std::unique_lock<std::shared_mutex> w_lock(g_wl_mutex);
        whitelist_domains.push_back(new_domain);
    }

    std::cout << "\n[Auto Whitelist] 💾 연관 도메인 자동 허용 등록: " << new_domain << std::endl;
}

void load_whitelist() {
    // JSON 파일 열기
    std::ifstream f("whitelist.json");
    if (!f.is_open()) {
        f.open("core_engine/whitelist.json");
    }
    if (!f.is_open()) {
        std::cerr << "[Warning] whitelist.json 파일을 찾을 수 없습니다. 모든 통신이 차단될 수 있습니다. " << std::endl;
        return;
    }

    // JSON 파싱 전 유효성 검증 (json::accept으로 예외 없이 파싱 실패 감지)
    std::string raw_json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    if (!json::accept(raw_json)) {
        std::cerr << "[Error] 화이트리스트 로드 실패: whitelist.json 이 유효하지 않은 JSON 형식입니다." << std::endl;
        return;
    }
    json data = json::parse(raw_json);

    std::vector<std::string> new_wl;
    std::vector<std::string> new_bl;
    std::vector<std::string> new_bl_process;

    if (data.contains("allowed_domains")) {
        for (auto& el : data["allowed_domains"]) {
            if (el.is_string()) new_wl.push_back(el.get<std::string>());
        }
    }
    if (data.contains("blocked_domains")) {
        for (auto& el : data["blocked_domains"]) {
            if (el.is_string()) new_bl.push_back(el.get<std::string>());
        }
    }
    if (data.contains("blocked_process")) {
        for (auto& el : data["blocked_process"]) {
            if (el.is_string()) new_bl_process.push_back(el.get<std::string>());
        }
    }
    if (data.contains("target_adapter") && data["target_adapter"].is_string()) {
        g_target_adapter = data["target_adapter"].get<std::string>();
    }
    if (data.contains("block_unregistered") && data["block_unregistered"].is_boolean()) {
        g_block_unregistered.store(data["block_unregistered"].get<bool>());
    } else if (data.contains("block_unknown") && data["block_unknown"].is_boolean()) {
        g_block_unregistered.store(data["block_unknown"].get<bool>());
    }

    // 파싱이 성공적으로 끝난 뒤에만 원본 리스트를 교체 (안전한 업데이트)
    {
        std::unique_lock<std::shared_mutex> lock(g_wl_mutex); // 리스트 보호용 뮤텍스 공용 사용
        whitelist_domains = std::move(new_wl);
        blacklist_domains = std::move(new_bl);
        blacklist_process = std::move(new_bl_process);
    }
    std::cout << "[Info] 필터 로드 완료: 허용 " << whitelist_domains.size() << "개 / 도메인 차단 " << blacklist_domains.size() << "개 / 앱 차단 " << blacklist_process.size() << "개";
    if (!g_target_adapter.empty()) {
        std::cout << " / 지정 어댑터: " << g_target_adapter;
    }
    std::cout << " / 미등록 사이트 차단: [" << (g_block_unregistered.load() ? "ON (화이트리스트 모드)" : "OFF (블랙리스트 모드)") << "]";
    std::cout << std::endl;
}

// ── 미등록 사이트 차단 모드 설정 및 토글 함수 ──
void set_block_unregistered(bool enable, bool save_to_file) {
    bool prev = g_block_unregistered.exchange(enable);
    if (prev == enable) {
        std::cout << "\n[Info] 미등록 사이트 차단 모드가 이미 [" << (enable ? "ON" : "OFF") << "] 상태입니다." << std::endl;
        return;
    }

    std::cout << "\n========================================================" << std::endl;
    if (enable) {
        std::cout << "[모드 변경] 🛡️ 미등록 사이트 차단: [ ON ] (화이트리스트 모드)" << std::endl;
        std::cout << "           -> 허용 목록(whitelist)에 등록된 사이트만 접속 가능합니다." << std::endl;
        // 허용되었던 기존 세션 캐시 정리하여 즉시 차단 적용
        std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
        allowed_conns.clear();
        pending_conns.clear();
    } else {
        std::cout << "[모드 변경] 🔓 미등록 사이트 차단: [ OFF ] (블랙리스트 전용 모드)" << std::endl;
        std::cout << "           -> 차단 목록(blacklist) 외 모든 사이트 접속이 허용됩니다." << std::endl;
        // 차단되었던 미등록 세션 캐시 정리하여 즉시 통신 재개
        std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
        blocked_conns.clear();
        pending_conns.clear();
    }
    std::cout << "========================================================\n" << std::endl;

    if (save_to_file) {
        std::lock_guard<std::mutex> file_lock(g_whitelist_file_mtx);
        std::string filename = "whitelist.json";
        std::ifstream in_f(filename);
        if (!in_f.is_open()) {
            filename = "core_engine/whitelist.json";
            in_f.open(filename);
        }
        if (in_f.is_open()) {
            std::string raw_json((std::istreambuf_iterator<char>(in_f)), std::istreambuf_iterator<char>());
            in_f.close();
            if (json::accept(raw_json)) {
                json config_data = json::parse(raw_json);
                config_data["block_unregistered"] = enable;
                std::ofstream out_f(filename);
                if (out_f.is_open()) {
                    out_f << config_data.dump(2);
                    out_f.close();
                    std::cout << "[Info] whitelist.json에 'block_unregistered': " << (enable ? "true" : "false") << " 설정이 저장되었습니다." << std::endl;
                }
            }
        }
    }
}

void toggle_block_unregistered() {
    bool current = g_block_unregistered.load();
    set_block_unregistered(!current, true);
}

// ============================================================
// packet_handler: Producer 역할만 담당
// 수신 직후 메모리 풀에서 객체를 할당받아 IOCP 큐에 넣고 즉시 반환
// ============================================================
void packet_handler(u_char* /*param*/, const struct pcap_pkthdr* header, const u_char* pkt_data) {
    if (!g_is_running.load()) return;

    total_captured_cnt++; // 캡처될 때마다 1씩 증가

    // 1. 메모리 풀에서 객체 하나 빌려오기
    CapturedPacket* pkt = g_packet_pool.Acquire();
    if (pkt == nullptr) {
        std::cerr << "[Error] packet_handler: 메모리 풀 고갈 (패킷 드랍)" << std::endl;
        return;
    }

    // 2. 패킷 데이터를 객체에 복사
    pkt->SetData(header, pkt_data);

    // 3. IOCP 큐에 패킷 포인터를 넘기고 즉시 반환 (비동기 통지)
    if (!PostQueuedCompletionStatus(g_hIocp, 0, 0, (LPOVERLAPPED)pkt)) {
        std::cerr << "[Error] packet_handler: PostQueuedCompletionStatus 실패, 패킷 드랍" << std::endl;
        g_packet_pool.Release(pkt);
    }
}

// ============================================================
// consumer_func: IOCP 기반 병렬 패킷 처리 워커(Worker) 스레드
// ============================================================
void consumer_func(pcap_t* adhandle) {
    while (g_is_running.load()) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED pOverlapped = NULL;

        // IOCP 큐에서 패킷 대기 (종료 감지를 위해 500ms 타임아웃 지정)
        BOOL bRet = GetQueuedCompletionStatus(g_hIocp, &bytesTransferred, &completionKey, &pOverlapped, 500);
        
        // 정상 종료 키 감지 또는 프로그램 종료 상태 시 워커 루프 종료
        if (completionKey == EXIT_COMPLETION_KEY || !g_is_running.load()) {
            if (pOverlapped != NULL) {
                g_packet_pool.Release((CapturedPacket*)pOverlapped);
            }
            break;
        }

        // 타임아웃이나 빈 포인터인 경우 계속 대기
        if (!bRet || pOverlapped == NULL) continue;

        // LPOVERLAPPED 포인터를 CapturedPacket 객체로 캐스팅
        CapturedPacket* pkt = (CapturedPacket*)pOverlapped;

        // RAII 래퍼: 풀 반환 보장
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

                        // ── 화이트리스트 파생 도메인 자동 수집 ──
                        std::string matched_parent = "";
                        {
                            std::shared_lock<std::shared_mutex> r_lock(g_wl_mutex);

                            // 먼저 블랙리스트 도메인의 서브도메인인지 확인 (오염 방지)
                            bool is_blacklist_subdomain = false;
                            for (const auto& b : blacklist_domains) {
                                std::string bl_suffix = "." + b;
                                if (queried_domain.length() > bl_suffix.length() &&
                                    queried_domain.compare(queried_domain.length() - bl_suffix.length(), bl_suffix.length(), bl_suffix) == 0) {
                                    is_blacklist_subdomain = true;
                                    break;
                                }
                                if (queried_domain == b) {
                                    is_blacklist_subdomain = true;
                                    break;
                                }
                            }

                            // 블랙리스트와 무관한 경우에만 화이트리스트 서브도메인 검사
                            if (!is_blacklist_subdomain) {
                                for (const auto& w : whitelist_domains) {
                                    std::string suffix = "." + w;
                                    if (queried_domain.length() > suffix.length() &&
                                        queried_domain.compare(queried_domain.length() - suffix.length(), suffix.length(), suffix) == 0) {
                                        matched_parent = w;
                                        break;
                                    }
                                }
                            }
                        }

                        // 등록된 화이트리스트 사이트의 직접 서브도메인이면 자동 등록
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
        uint64_t conn_id = ((uint64_t)dst_ip << 32) | ntohs(tcp->sport);

        // ── 연결 종료(FIN, RST) 감지 시 캐시 즉시 삭제 ──
        if ((tcp->flags & 0x01) || (tcp->flags & 0x04)) {
            {
                std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                allowed_conns.erase(conn_id);
                blocked_conns.erase(conn_id);
                pending_conns.erase(conn_id);
            }
            continue;
        }

        // ── 허용된 커넥션 목록 읽기 (shared_lock) ──
        {
            std::shared_lock<std::shared_mutex> r_lock(g_ip_mutex);
            if (allowed_conns.count(conn_id) > 0) continue; // 허용된 커넥션 → 즉시 통과
        }

        // ── SYN 패킷 처리 (새 연결 시도 감지) ──
        if ((tcp->flags & 0x02) && !(tcp->flags & 0x10)) {
            std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
            if (blocked_conns.count(conn_id) == 0 && allowed_conns.count(conn_id) == 0) {
                pending_conns[conn_id] = time(nullptr);
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

        // 페이로드 없는 핸드셰이크 패킷 (ACK 등) → 통과
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

            // CDN/정적 리소스 패스스루 검사
            bool is_cdn_passthrough = false;
            if (!is_blacklisted && !is_whitelisted) {
                static const std::vector<std::string> cdn_prefixes = {
                    "cdn.", "static.", "media.", "assets.", "asset.",
                    "img.", "images.", "image.", "video.", "videos.",
                    "fonts.", "font.", "files.", "file.",
                    "upload.", "uploads.", "s3.", "storage.", "cache.",
                    "res.", "resource.", "resources.", "content.",
                    "cloud.", "data.", "dl.", "download."
                };
                for (const auto& prefix : cdn_prefixes) {
                    if (domain.length() > prefix.length() &&
                        domain.compare(0, prefix.length(), prefix) == 0) {
                        is_cdn_passthrough = true;
                        break;
                    }
                }
            }

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
            else if (is_whitelisted || is_cdn_passthrough) {
                // ── 허용 커넥션 등록 ──
                std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                if (allowed_conns.count(conn_id) == 0) {
                    pending_conns.erase(conn_id);
                    allowed_conns[conn_id] = time(nullptr);
                    if (is_cdn_passthrough) {
                        std::cout << "\n[Thread " << std::this_thread::get_id() << "] [CDN Pass] 🌐 CDN 패스스루 | " << src_ip_str << " -> " << dst_ip_str << " | 도메인: " << domain << std::endl;
                    } else {
                        std::cout << "\n[Thread " << std::this_thread::get_id() << "] [Whitelist] ✅ OK 허용 | " << src_ip_str << " -> " << dst_ip_str << " | 도메인: " << domain << std::endl;
                    }
                    StatsLogger::GetInstance().LogNormalConnection();
                }
            } else {
                // ── 화이트리스트에 없는 미등록 도메인 처리 ──
                bool is_whitelist_active = false;
                {
                    std::shared_lock<std::shared_mutex> r_lock(g_wl_mutex);
                    // 화이트리스트에 1개 이상의 도메인이 있을 때만 "화이트리스트 모드" 조건 성립
                    is_whitelist_active = !whitelist_domains.empty();
                }

                // [핵심] 미등록 차단 토글(g_block_unregistered)이 ON이고 화이트리스트가 활성화되어 있을 때만 차단!
                if (g_block_unregistered.load() && is_whitelist_active) {
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
                    // 미등록 차단이 OFF(블랙리스트 모드)이거나 화이트리스트가 비어있는 경우 -> 미등록 사이트 정상 허용 (Pass)
                    std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                    if (allowed_conns.count(conn_id) == 0) {
                        pending_conns.erase(conn_id);
                        allowed_conns[conn_id] = time(nullptr);
                        StatsLogger::GetInstance().LogNormalConnection();
                    }
                }
            }
        } else {
            // SNI/Host 없는 데이터 패킷
            // 미등록 차단이 켜져 있을 때만 pending/stale 연결 차단 수행
            if (g_block_unregistered.load()) {
                bool is_pending = false;
                {
                    std::shared_lock<std::shared_mutex> r_lock(g_ip_mutex);
                    is_pending = (pending_conns.count(conn_id) > 0);
                }
                if (is_pending) {
                    queue_spoofed_rst_packet(pkt_data, header->caplen, "Pending Block");
                } else {
                    {
                        std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                        if (blocked_conns.count(conn_id) == 0) {
                            blocked_conns[conn_id] = time(nullptr);
                        }
                    }
                    queue_spoofed_rst_packet(pkt_data, header->caplen, "Stale Connection");
                }
            } else {
                // 미등록 차단이 꺼져 있는 블랙리스트 전용 모드에서는 SNI 없는 패킷도 통과
                std::unique_lock<std::shared_mutex> w_lock(g_ip_mutex);
                if (allowed_conns.count(conn_id) == 0) {
                    pending_conns.erase(conn_id);
                    allowed_conns[conn_id] = time(nullptr);
                }
            }
        }
    } // while(g_is_running.load()) 끝
} // consumer_func 끝
