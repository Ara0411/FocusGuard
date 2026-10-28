#include "config.h"

std::atomic<uint64_t> total_captured_cnt(0);
std::shared_mutex g_ip_mutex;
std::shared_mutex g_wl_mutex;
std::unordered_map<uint64_t, time_t> blocked_conns;
std::unordered_map<uint64_t, time_t> allowed_conns;
std::unordered_map<uint64_t, time_t> pending_conns;
std::vector<std::string> whitelist_domains;
std::vector<std::string> blacklist_domains;
std::vector<std::string> blacklist_process;
PacketPool g_packet_pool;
HANDLE g_hIocp = NULL;
PacketTxQueue g_outbound_queue;
std::atomic<bool> g_is_paused(false);
std::string g_target_adapter = "";

// ── 신규 추가: 정상 종료 및 미등록 차단 기능 전역 인스턴스 ──
std::atomic<bool> g_is_running(true);
std::atomic<bool> g_block_unregistered(true);
pcap_t* g_adhandle = nullptr;
