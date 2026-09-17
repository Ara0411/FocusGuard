#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <ctime>

class StatsLogger {
public:
    static StatsLogger& GetInstance();

    void Init();
    void LogNormalConnection();
    void LogBlockedConnection(const std::string& domain);
    void LogProcessKilled(const std::string& exeName);
    void SaveToFile(const std::string& filepath);

private:
    bool IsNoiseDomain(const std::string& domain);
    std::string GetRootDomain(const std::string& domain);

    StatsLogger();
    ~StatsLogger() = default;

    std::mutex mtx_;
    time_t start_time_;
    time_t first_block_time_;
    
    int conn_normal_cnt_;
    int conn_blocked_cnt_;
    int process_killed_cnt_;

    std::unordered_map<std::string, int> domain_block_counts_;
    std::unordered_map<std::string, time_t> last_log_time_;
    std::vector<int> hourly_blocks_;

    void RecordBlockTime();
};
