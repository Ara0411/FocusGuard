#pragma once
#include <iostream>
#include <string>
#include <winsock2.h>
#include <pcap.h>
#include <set>
#include <vector>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <sys/stat.h>
#include <ctime>
#include <atomic>
#include "json.hpp"

using json = nlohmann::json;

#pragma comment(lib, "wpcap.lib")
#pragma comment(lib, "ws2_32.lib")

#include "protocol_headers.h"

struct CapturedPacket : public OVERLAPPED {
    struct pcap_pkthdr header;
    std::vector<u_char> raw_data;

    CapturedPacket() {
        memset(static_cast<OVERLAPPED*>(this), 0, sizeof(OVERLAPPED));
        raw_data.reserve(2048);
    }

    void SetData(const struct pcap_pkthdr* hdr, const u_char* pkt_data) {
        memset(static_cast<OVERLAPPED*>(this), 0, sizeof(OVERLAPPED));
        header = *hdr;
        raw_data.assign(pkt_data, pkt_data + hdr->caplen);
    }
};

class PacketPool {
private:
    std::vector<CapturedPacket*> pool_;
    std::mutex mtx_;

public:
    PacketPool(size_t initial_size = 10000) {
        pool_.reserve(initial_size);
        for (size_t i = 0; i < initial_size; i++) {
            pool_.push_back(new CapturedPacket());
        }
    }

    ~PacketPool() {
        for (auto pkt : pool_) delete pkt;
        pool_.clear();
    }

    CapturedPacket* Acquire() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (pool_.empty()) {
            return new CapturedPacket();
        }
        CapturedPacket* pkt = pool_.back();
        pool_.pop_back();
        return pkt;
    }

    void Release(CapturedPacket* pkt) {
        std::lock_guard<std::mutex> lock(mtx_);
        pool_.push_back(pkt);
    }
};

extern std::atomic<uint64_t> total_captured_cnt;
extern std::shared_mutex g_ip_mutex;
extern std::shared_mutex g_wl_mutex;
extern std::unordered_map<uint64_t, time_t> blocked_conns;
extern std::unordered_map<uint64_t, time_t> allowed_conns;
extern std::unordered_map<uint64_t, time_t> pending_conns;
extern std::vector<std::string> whitelist_domains;
extern std::vector<std::string> blacklist_domains;
extern std::vector<std::string> blacklist_process;
extern PacketPool g_packet_pool;
extern HANDLE g_hIocp;

