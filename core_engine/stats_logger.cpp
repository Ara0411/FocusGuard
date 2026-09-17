#include "stats_logger.h"
#include "config.h" // For nlohmann::json
#include <fstream>
#include <iostream>
#include <algorithm>

StatsLogger& StatsLogger::GetInstance() {
    static StatsLogger instance;
    return instance;
}

StatsLogger::StatsLogger() {
    start_time_ = time(nullptr);
    first_block_time_ = 0;
    conn_normal_cnt_ = 0;
    conn_blocked_cnt_ = 0;
    process_killed_cnt_ = 0;
    hourly_blocks_.resize(24, 0);
}

void StatsLogger::Init() {
    std::lock_guard<std::mutex> lock(mtx_);
    start_time_ = time(nullptr);
}

bool StatsLogger::IsNoiseDomain(const std::string& domain) {
    // 흔한 윈도우/시스템 텔레메트리 도메인 목록 및 위젯용 도메인 보강
    const std::vector<std::string> noise_keywords = {
        "azure", "microsoft", "windows", "msedge", "aka.ms", "visualstudio.com",
        "msn.com", "skype.com", "live.com", "office.com", "bing.com", "weather.com",
        "nelreports.net", "pki.goog", "digicert.com", "telemetry", "ocsp", "googleapis.com",
        "cloudflare-ech.com", "clarity.ms", "footprintdns.com", "office.net", 
        "asus.com", "scorecardresearch.com"
    };
    
    // 소문자로 변환하여 검사 (단순 비교)
    std::string lower_domain = domain;
    std::transform(lower_domain.begin(), lower_domain.end(), lower_domain.begin(), ::tolower);
    
    for (const auto& keyword : noise_keywords) {
        if (lower_domain.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string StatsLogger::GetRootDomain(const std::string& domain) {
    std::string d = domain;
    std::transform(d.begin(), d.end(), d.begin(), ::tolower);
    
    if (d.find("googlevideo.com") != std::string::npos || d.find("youtube") != std::string::npos || d.find("youtu.be") != std::string::npos || d.find("ytimg.com") != std::string::npos) {
        return "youtube.com";
    }
    if (d.find("googleusercontent.com") != std::string::npos) {
        return "google.com";
    }
    if (d.find("spotify.com") != std::string::npos) {
        return "spotify.com";
    }
    if (d.find("facebook") != std::string::npos || d.find("fbcdn") != std::string::npos) {
        return "facebook.com";
    }
    if (d.find("instagram") != std::string::npos) {
        return "instagram.com";
    }
    if (d.find("twitter.com") != std::string::npos || d.find("x.com") != std::string::npos || d.find("twimg.com") != std::string::npos) {
        return "x.com";
    }
    if (d.find("netflix") != std::string::npos || d.find("nflxvideo") != std::string::npos) {
        return "netflix.com";
    }
    
    // 기본적으로 서브도메인 www. 제거
    if (d.rfind("www.", 0) == 0) {
        return d.substr(4);
    }
    return d;
}

void StatsLogger::RecordBlockTime() {
    time_t now = time(nullptr);
    if (first_block_time_ == 0) {
        first_block_time_ = now;
    }
    
    struct tm ltm;
#ifdef _WIN32
    localtime_s(&ltm, &now);
#else
    localtime_r(&now, &ltm);
#endif
    int hour = ltm.tm_hour;
    if (hour >= 0 && hour < 24) {
        hourly_blocks_[hour]++;
    }
}

void StatsLogger::LogNormalConnection() {
    std::lock_guard<std::mutex> lock(mtx_);
    conn_normal_cnt_++;
}

void StatsLogger::LogBlockedConnection(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    std::string root_domain = GetRootDomain(domain);
    
    // 백그라운드 노이즈인 경우 통계 집계에서 제외
    if (IsNoiseDomain(root_domain) || IsNoiseDomain(domain)) {
        return;
    }
    
    // 10초 쿨타임 (Debouncing) 적용
    time_t now = time(nullptr);
    if (last_log_time_.count(root_domain) > 0) {
        if (now - last_log_time_[root_domain] < 10) {
            return; // 10초 이내의 동일 도메인 차단 시도는 무시 (집중도 삭감 방어)
        }
    }
    last_log_time_[root_domain] = now;
    
    conn_blocked_cnt_++;
    domain_block_counts_[root_domain]++;
    RecordBlockTime();
}

void StatsLogger::LogProcessKilled(const std::string& exeName) {
    std::lock_guard<std::mutex> lock(mtx_);
    process_killed_cnt_++;
    // exeName 은 추후 프로세스별 통계가 필요하면 기록할 수 있음
    RecordBlockTime();
}

void StatsLogger::SaveToFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    try {
        json j;
        j["start_time"] = start_time_;
        j["first_block_time"] = first_block_time_;
        
        // 집중도 계산 (Score)
        // Score = (1 - (blocked + killed) / (normal + blocked + killed)) * 100
        int total_activities = conn_normal_cnt_ + conn_blocked_cnt_ + process_killed_cnt_;
        double score = 100.0;
        if (total_activities > 0) {
            int bad_activities = conn_blocked_cnt_ + process_killed_cnt_;
            score = (1.0 - (double)bad_activities / total_activities) * 100.0;
        }
        j["focus_score_percent"] = score;
        
        // Top 5 Domains
        std::vector<std::pair<std::string, int>> domain_vec(domain_block_counts_.begin(), domain_block_counts_.end());
        std::sort(domain_vec.begin(), domain_vec.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        
        json top_domains = json::object();
        for (size_t i = 0; i < domain_vec.size() && i < 5; ++i) {
            top_domains[domain_vec[i].first] = domain_vec[i].second;
        }
        j["top_blocked_domains"] = top_domains;
        
        j["timeline_24h"] = hourly_blocks_;
        
        json raw_stats;
        raw_stats["normal_connections"] = conn_normal_cnt_;
        raw_stats["blocked_connections"] = conn_blocked_cnt_;
        raw_stats["killed_processes"] = process_killed_cnt_;
        j["raw_stats"] = raw_stats;
        
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
        }
    } catch (...) {
        // 예외 무시
    }
}
