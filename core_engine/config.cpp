#include "config.h"

std::atomic<uint64_t> total_captured_cnt(0);
std::shared_mutex g_ip_mutex;
std::shared_mutex g_wl_mutex;
std::unordered_map<uint64_t, time_t> blocked_conns;
std::unordered_map<uint64_t, time_t> allowed_conns;
std::unordered_map<uint64_t, time_t> pending_conns;
std::vector<std::string> whitelist_domains;
std::vector<std::string> blacklist_domains;
PacketPool g_packet_pool;
HANDLE g_hIocp;

