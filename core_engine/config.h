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
#include <deque>
#include <condition_variable>
#pragma warning(push)
#pragma warning(disable: 26495) // nlohmann/json 외부 라이브러리 내부 공용체 경고 무시
#include "json.hpp"
#pragma warning(pop)

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
        // snaplen 65535 적용: 프레임 전체 복사
        int copy_len = hdr->caplen;
        raw_data.assign(pkt_data, pkt_data + copy_len);
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

//위조 패킷을 담아 둘 구조체
struct RawPacket {
    int len{ 0 };
    u_char data[256]{};

    RawPacket() : len(0), data{} {
        memset(data, 0, sizeof(data));
    }

    RawPacket(const u_char* p_data, int p_len) : len(p_len), data{} {
        if (len > 256) len = 256;
        if (len < 0) len = 0;
        if (p_data && len > 0) {
            memcpy(data, p_data, len);
        }
    }
};

//RawPacket을 담아 둘 아웃바운드큐
class PacketTxQueue {
private:
    std::deque<RawPacket> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stopped_{false};
    size_t max_capacity_;

public:
    PacketTxQueue(size_t max_capacity = 4096) : max_capacity_(max_capacity) {}

    void Push(const RawPacket& pkt) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (queue_.size() >= max_capacity_) {
                queue_.pop_front(); // 버스트 시 오래된 패킷 드랍하여 지연 방지
            }
            queue_.push_back(pkt);
        }
        cv_.notify_one();
    }

    bool Pop(RawPacket& out_pkt) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !queue_.empty() || stopped_.load(); });
        if (stopped_.load() && queue_.empty()) return false;
        out_pkt = queue_.front();
        queue_.pop_front();
        return true;
    }

    void Stop() {
        stopped_.store(true);
        cv_.notify_all();
    }

    size_t Size() {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
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
extern PacketTxQueue g_outbound_queue;
extern std::atomic<bool> g_is_paused; // GUI 휴식 모드 플래그 (pause.flag 파일로 제어)
extern std::string g_target_adapter; // GUI에서 지정한 우선 캡처 NIC 어댑터명

// ── 신규 추가: 정상 종료 및 미등록 차단 기능 제어용 전역 변수 ──
extern std::atomic<bool> g_is_running; // 엔진 실행 상태 (false 시 모든 스레드 정상 종료 루틴 진입)
extern std::atomic<bool> g_block_unregistered; // 미등록 사이트 차단 여부 (true: 화이트리스트 모드, false: 블랙리스트 전용 모드)
extern pcap_t* g_adhandle; // pcap 핸들 전역 참조 (pcap_breakloop 용)
constexpr ULONG_PTR EXIT_COMPLETION_KEY = 0xDEADBEEF; // IOCP 워커 스레드 정상 종료용 키

// ── 진단 통계 카운터 (재조립 및 프레임 잘림 감시용) ──
extern std::atomic<uint64_t> g_stat_truncated_frames;
extern std::atomic<uint64_t> g_stat_reassembly_starts;
extern std::atomic<uint64_t> g_stat_reassembly_success;
extern std::atomic<uint64_t> g_stat_reassembly_expired;
extern std::atomic<uint64_t> g_stat_reassembly_seq_err;
extern std::atomic<uint64_t> g_stat_reassembly_oom;

// ── TLS 세그먼트 재조립 버퍼 구조체 ──
struct TlsReassemblyEntry {
    uint32_t next_expected_seq = 0;      // 다음 기대 TCP 시퀀스 번호
    uint32_t expected_total_len = 0;     // 기대 총 TLS 레코드 크기 (헤더 5바이트 포함)
    time_t created_at = 0;               // 세션 생성 시각 (초 단위)
    std::vector<u_char> accumulated;     // 누적된 페이로드 데이터
};

extern std::mutex g_reassembly_mutex;
extern std::unordered_map<uint64_t, TlsReassemblyEntry> g_reassembly_map;
constexpr size_t MAX_REASSEMBLY_ENTRIES = 2000;    // 동시 재조립 최대 세션 수 상한
constexpr size_t MAX_REASSEMBLY_BUF_SIZE = 16384;  // 세션당 최대 버퍼 크기 (16KB)
