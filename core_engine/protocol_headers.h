#pragma once

#ifndef PROTOCOL_HEADERS_H
#define PROTOCOL_HEADERS_H

#include <winsock2.h>
#include <windows.h>

// MAC Address
typedef struct mac_address {
    u_char byte1, byte2, byte3, byte4, byte5, byte6;
} mac_address;

// Ethernet Header
struct pkt_eth_header {
    mac_address dest_mac;
    mac_address src_mac;
    u_short eth_type;
};

// IPv4 Address
typedef struct ip_address {
    u_char byte1, byte2, byte3, byte4;
} ip_address;

// IPv4 Header
struct pkt_ip_header {
    u_char ver_ihl;
    u_char tos;
    u_short tlen;
    u_short identification;
    u_short flags_fo;
    u_char ttl;
    u_char proto;
    u_short crc;
    ip_address saddr;
    ip_address daddr;
};

// TCP Header
struct pkt_tcp_header {
    u_short sport;
    u_short dport;
    u_int seq;
    u_int ack;
    u_char data_offset;
    u_char flags;
    u_short window;
    u_short checksum;
    u_short urg_ptr;
};

// Pseudo Header for Checksum
struct pkt_pseudo_header {
    ip_address src_addr;
    ip_address dst_addr;
    u_char placeholder;
    u_char protocol;
    u_short tcp_length; // TCP, UDP 공용으로 사용 가능
};

// UDP Header
struct pkt_udp_header {
    u_short sport;
    u_short dport;
    u_short len;
    u_short checksum;
};

// DNS Header
struct pkt_dns_header {
    u_short transaction_id;
    u_short flags;
    u_short questions;
    u_short answer_rrs;
    u_short authority_rrs;
    u_short additional_rrs;
};
// ICMP Header
struct pkt_icmp_header {
    u_char type;
    u_char code;
    u_short checksum;
    u_int unused;
};

#endif // PROTOCOL_HEADERS_H
