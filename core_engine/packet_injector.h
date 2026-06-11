#pragma once
#include "config.h"

unsigned short calculate_checksum(unsigned short* ptr, int nbytes);
void send_spoofed_rst_packet(pcap_t* adhandle, const u_char* orig_pkt, int caplen, const std::string& domain);
void send_spoofed_icmp_unreachable(pcap_t* adhandle, const u_char* orig_pkt);
