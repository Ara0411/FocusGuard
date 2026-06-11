#pragma once
#include "config.h"
#include "packet_injector.h"

std::string extract_http_host(const u_char* payload, int payload_len);
std::string extract_sni(const u_char* payload, int payload_len);
void load_whitelist();
void packet_handler(u_char* /*param*/, const struct pcap_pkthdr* header, const u_char* pkt_data);
void consumer_func(pcap_t* adhandle);
time_t get_file_mod_time(const char* path);
void watcher_func();

