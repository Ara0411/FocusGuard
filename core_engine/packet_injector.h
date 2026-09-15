#pragma once
#include "config.h"

unsigned short calculate_checksum(unsigned short* ptr, int nbytes);
void queue_spoofed_rst_packet(const u_char* orig_pkt, int caplen, const std::string& domain);
void queue_spoofed_icmp_unreachable(const u_char* orig_pkt, int caplen);
void packet_sender_func(pcap_t* adhandle);

// 하위 호환용 래퍼 함수
void send_spoofed_rst_packet(pcap_t* adhandle, const u_char* orig_pkt, int caplen, const std::string& domain);
void send_spoofed_icmp_unreachable(pcap_t* adhandle, const u_char* orig_pkt);
