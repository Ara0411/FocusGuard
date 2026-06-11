#include "packet_injector.h"

unsigned short calculate_checksum(unsigned short* ptr, int nbytes) {
    long sum = 0;
    short answer = 0;
    short oddbyte = 0;

    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = (short)~sum;
    return answer;
}

// TCP RST 패킷 위조 및 주입 함수
void send_spoofed_rst_packet(pcap_t* adhandle, const u_char* orig_pkt, int caplen, const std::string& domain) {
    // 1. 원본 패킷 구조체 매핑
    struct pkt_eth_header* orig_eth = (struct pkt_eth_header*)orig_pkt;
    struct pkt_ip_header* orig_ip = (struct pkt_ip_header*)(orig_pkt + sizeof(struct pkt_eth_header));
    int ip_len = (orig_ip->ver_ihl & 0xf) * 4;
    struct pkt_tcp_header* orig_tcp = (struct pkt_tcp_header*)((u_char*)orig_ip + ip_len);
    int tcp_len = ((orig_tcp->data_offset >> 4) * 4);

    // 원본 패킷의 페이로드(데이터) 길이 계산
    int orig_payload_len = ntohs(orig_ip->tlen) - (ip_len + tcp_len);
    if (orig_payload_len < 0) orig_payload_len = 0;
    if (orig_tcp->flags & 0x02) orig_payload_len += 1; // SYN
    if (orig_tcp->flags & 0x01) orig_payload_len += 1; // FIN

    // 2. 가짜 패킷(RST) 메모리 할당 (페이로드 없이 헤더만)
    int spoofed_pkt_len = sizeof(struct pkt_eth_header) + sizeof(struct pkt_ip_header) + sizeof(struct pkt_tcp_header);
    u_char spoofed_pkt_fwd[128] = {0}; // 정방향 (클라이언트->서버)
    u_char spoofed_pkt_bwd[128] = {0}; // 역방향 (서버->클라이언트)

    struct pkt_eth_header* eth_fwd = (struct pkt_eth_header*)spoofed_pkt_fwd;
    struct pkt_ip_header* ip_fwd = (struct pkt_ip_header*)(spoofed_pkt_fwd + sizeof(struct pkt_eth_header));
    struct pkt_tcp_header* tcp_fwd = (struct pkt_tcp_header*)(spoofed_pkt_fwd + sizeof(struct pkt_eth_header) + sizeof(struct pkt_ip_header));

    struct pkt_eth_header* eth_bwd = (struct pkt_eth_header*)spoofed_pkt_bwd;
    struct pkt_ip_header* ip_bwd = (struct pkt_ip_header*)(spoofed_pkt_bwd + sizeof(struct pkt_eth_header));
    struct pkt_tcp_header* tcp_bwd = (struct pkt_tcp_header*)(spoofed_pkt_bwd + sizeof(struct pkt_eth_header) + sizeof(struct pkt_ip_header));

    // ==============================================
    // [정방향(Forward) RST] 내가 클라이언트인 척하고 서버에게 연결 끊기 요청
    // ==============================================
    memcpy(&eth_fwd->src_mac, &orig_eth->src_mac, 6);
    memcpy(&eth_fwd->dest_mac, &orig_eth->dest_mac, 6);
    eth_fwd->eth_type = orig_eth->eth_type;

    ip_fwd->ver_ihl = 0x45; // IPv4, Header Length 20
    ip_fwd->tos = 0;
    ip_fwd->tlen = htons(sizeof(struct pkt_ip_header) + sizeof(struct pkt_tcp_header));
    ip_fwd->identification = htons(54321);
    ip_fwd->flags_fo = 0;
    ip_fwd->ttl = 64;
    ip_fwd->proto = 6; // TCP
    ip_fwd->saddr = orig_ip->saddr;
    ip_fwd->daddr = orig_ip->daddr;
    ip_fwd->crc = 0;
    ip_fwd->crc = calculate_checksum((unsigned short*)ip_fwd, sizeof(struct pkt_ip_header));

    tcp_fwd->sport = orig_tcp->sport;
    tcp_fwd->dport = orig_tcp->dport;
    tcp_fwd->seq = htonl(ntohl(orig_tcp->seq) + orig_payload_len); // 원본 시퀀스 + 페이로드 길이
    tcp_fwd->ack = orig_tcp->ack;
    tcp_fwd->data_offset = (sizeof(struct pkt_tcp_header) / 4) << 4;
    tcp_fwd->flags = 0x04; // RST 플래그
    tcp_fwd->window = htons(0);
    tcp_fwd->urg_ptr = 0;

    struct pkt_pseudo_header psh_fwd;
    psh_fwd.src_addr = ip_fwd->saddr;
    psh_fwd.dst_addr = ip_fwd->daddr;
    psh_fwd.placeholder = 0;
    psh_fwd.protocol = 6;
    psh_fwd.tcp_length = htons(sizeof(struct pkt_tcp_header));

    int psize = sizeof(struct pkt_pseudo_header) + sizeof(struct pkt_tcp_header);
    u_char pseudogram_fwd[64] = {0};
    memcpy(pseudogram_fwd, (char*)&psh_fwd, sizeof(struct pkt_pseudo_header));
    memcpy(pseudogram_fwd + sizeof(struct pkt_pseudo_header), tcp_fwd, sizeof(struct pkt_tcp_header));

    tcp_fwd->checksum = 0;
    tcp_fwd->checksum = calculate_checksum((unsigned short*)pseudogram_fwd, psize);

    // ==============================================
    // [역방향(Backward) RST] 내가 서버인 척하고 클라이언트에게 연결 끊기 요청
    // [핵심] Wi-Fi 환경 우회: Source MAC을 공유기 MAC이 아닌 내 PC의 MAC으로 설정!
    // (공유기가 외부 MAC 스푸핑을 감지하고 드랍하는 것을 방지하는 Hairpinning 기법)
    // ==============================================
    memcpy(&eth_bwd->src_mac, &orig_eth->src_mac, 6); // 원래 서버(공유기)여야 하지만 내 PC MAC으로 위장
    memcpy(&eth_bwd->dest_mac, &orig_eth->src_mac, 6); // 목적지도 내 PC
    eth_bwd->eth_type = orig_eth->eth_type;

    ip_bwd->ver_ihl = 0x45;
    ip_bwd->tos = 0;
    ip_bwd->tlen = htons(sizeof(struct pkt_ip_header) + sizeof(struct pkt_tcp_header));
    ip_bwd->identification = htons(12345);
    ip_bwd->flags_fo = 0;
    ip_bwd->ttl = 64;
    ip_bwd->proto = 6;
    ip_bwd->saddr = orig_ip->daddr;
    ip_bwd->daddr = orig_ip->saddr;
    ip_bwd->crc = 0;
    ip_bwd->crc = calculate_checksum((unsigned short*)ip_bwd, sizeof(struct pkt_ip_header));

    tcp_bwd->sport = orig_tcp->dport;
    tcp_bwd->dport = orig_tcp->sport;
    tcp_bwd->seq = orig_tcp->ack;
    tcp_bwd->ack = htonl(ntohl(orig_tcp->seq) + orig_payload_len);
    tcp_bwd->data_offset = (sizeof(struct pkt_tcp_header) / 4) << 4;
    tcp_bwd->flags = 0x14; // RST + ACK 플래그
    tcp_bwd->window = htons(0);
    tcp_bwd->urg_ptr = 0;

    struct pkt_pseudo_header psh_bwd;
    psh_bwd.src_addr = ip_bwd->saddr;
    psh_bwd.dst_addr = ip_bwd->daddr;
    psh_bwd.placeholder = 0;
    psh_bwd.protocol = 6;
    psh_bwd.tcp_length = htons(sizeof(struct pkt_tcp_header));

    u_char pseudogram_bwd[64] = {0};
    memcpy(pseudogram_bwd, (char*)&psh_bwd, sizeof(struct pkt_pseudo_header));
    memcpy(pseudogram_bwd + sizeof(struct pkt_pseudo_header), tcp_bwd, sizeof(struct pkt_tcp_header));

    tcp_bwd->checksum = 0;
    tcp_bwd->checksum = calculate_checksum((unsigned short*)pseudogram_bwd, psize);

    // 3. 만들어진 양방향 RST 패킷 전송
    pcap_sendpacket(adhandle, spoofed_pkt_fwd, spoofed_pkt_len);
    pcap_sendpacket(adhandle, spoofed_pkt_bwd, spoofed_pkt_len);

    //std::cout << "[Phase 5] 접속 차단(RST 주입) 완료: " << domain << std::endl;

}

// ICMP Destination Unreachable (Port Unreachable) 패킷 위조 및 주입 함수
void send_spoofed_icmp_unreachable(pcap_t* adhandle, const u_char* orig_pkt) {
    struct pkt_eth_header* orig_eth = (struct pkt_eth_header*)orig_pkt;
    struct pkt_ip_header* orig_ip = (struct pkt_ip_header*)(orig_pkt + sizeof(struct pkt_eth_header));
    int orig_ip_len = (orig_ip->ver_ihl & 0xf) * 4;
    
    // 원본 IP 헤더 + 처음 8바이트 데이터(UDP 헤더) 크기 계산
    int icmp_payload_len = orig_ip_len + 8;
    
    // 2. 가짜 패킷(ICMP) 메모리 할당
    int spoofed_pkt_len = sizeof(struct pkt_eth_header) + sizeof(struct pkt_ip_header) + sizeof(struct pkt_icmp_header) + icmp_payload_len;
    u_char spoofed_pkt_bwd[256] = {0}; // 서버->클라이언트로 위장하여 전송

    struct pkt_eth_header* eth_bwd = (struct pkt_eth_header*)spoofed_pkt_bwd;
    struct pkt_ip_header* ip_bwd = (struct pkt_ip_header*)(spoofed_pkt_bwd + sizeof(struct pkt_eth_header));
    struct pkt_icmp_header* icmp_bwd = (struct pkt_icmp_header*)(spoofed_pkt_bwd + sizeof(struct pkt_eth_header) + sizeof(struct pkt_ip_header));
    u_char* icmp_payload = spoofed_pkt_bwd + sizeof(struct pkt_eth_header) + sizeof(struct pkt_ip_header) + sizeof(struct pkt_icmp_header);

    // [역방향 ICMP] 내가 서버인 척하고 클라이언트에게 도달 불가 메시 전송
    memcpy(&eth_bwd->src_mac, &orig_eth->src_mac, 6); // 본인 MAC 유지 (Hairpinning)
    memcpy(&eth_bwd->dest_mac, &orig_eth->src_mac, 6);
    eth_bwd->eth_type = orig_eth->eth_type;

    ip_bwd->ver_ihl = 0x45;
    ip_bwd->tos = 0;
    ip_bwd->tlen = htons(sizeof(struct pkt_ip_header) + sizeof(struct pkt_icmp_header) + icmp_payload_len);
    ip_bwd->identification = htons(12346);
    ip_bwd->flags_fo = 0;
    ip_bwd->ttl = 64;
    ip_bwd->proto = 1; // ICMP
    ip_bwd->saddr = orig_ip->daddr;
    ip_bwd->daddr = orig_ip->saddr;
    ip_bwd->crc = 0;
    ip_bwd->crc = calculate_checksum((unsigned short*)ip_bwd, sizeof(struct pkt_ip_header));

    // ICMP 헤더 설정 (Type 3: Destination Unreachable, Code 3: Port Unreachable)
    icmp_bwd->type = 3;
    icmp_bwd->code = 3;
    icmp_bwd->checksum = 0;
    icmp_bwd->unused = 0;

    // ICMP 페이로드 채우기 (원본 IP 헤더 + 처음 8바이트)
    memcpy(icmp_payload, orig_ip, icmp_payload_len);

    // ICMP 체크섬 계산
    icmp_bwd->checksum = calculate_checksum((unsigned short*)icmp_bwd, sizeof(struct pkt_icmp_header) + icmp_payload_len);

    pcap_sendpacket(adhandle, spoofed_pkt_bwd, spoofed_pkt_len);
}
