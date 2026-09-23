#include "BackendManager.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
//  경로 탐색: 실행 파일 위치에서 상위 폴더를 타고 올라가
//  'core_engine/FocusGuard.vcxproj' 또는 'core_engine/' 폴더가 있는
//  FocusGuard/ 워크스페이스 루트를 찾아 반환합니다.
//  찾지 못하면 빈 문자열 반환.
// ─────────────────────────────────────────────────────────────────────────────
QString BackendManager::resolveWorkspacePath() const
{
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {   // 최대 8단계 상위 탐색
        if (d.exists("core_engine")) {
            return d.absolutePath();
        }
        if (!d.cdUp()) break;
    }
    // 못 찾으면 하드코딩 fallback (개발 머신 전용)
    return "C:/Users/SHIN/Desktop/FG-Work-Space/FocusGuard";
}

BackendManager::BackendManager(QObject *parent)
    : QObject(parent)
{
    // 워크스페이스 루트 자동 탐색
    QString ws = resolveWorkspacePath();
    m_statsFilePath    = ws + "/core_engine/FocusGuard_stats.json";
    m_whitelistFilePath = ws + "/core_engine/whitelist.json";
    m_pauseFlagPath    = ws + "/core_engine/pause.flag";
    m_settingsFilePath = ws + "/qt_gui/user_settings.json";

    qDebug() << "[BackendManager] workspace =" << ws;
    qDebug() << "[BackendManager] stats     =" << m_statsFilePath;

    // 기본 타임라인 데이터 초기화 (24시간)
    for (int i = 0; i < 24; ++i)
        m_timelineData.append(0);

    // 기본 주간 데이터 초기화 (7일: 월~일)
    for (int i = 0; i < 7; ++i)
        m_weeklyMinutes.append(0);

    loadSettings();
    loadWhitelistFile();

    // 1초마다 통계 파일 폴링
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &BackendManager::reloadStats);
    m_timer->start(1000);

    reloadStats();
}

// ─────────────────────────────────────────────────────────────────────────────
//  설정 로드 / 저장
// ─────────────────────────────────────────────────────────────────────────────
void BackendManager::loadSettings()
{
    std::ifstream file(m_settingsFilePath.toStdString());
    if (!file.is_open()) return;
    try {
        json j;
        file >> j;
        if (j.contains("userName"))
            m_userName = QString::fromStdString(j["userName"].get<std::string>());
        if (j.contains("targetTime"))
            m_targetTime = QString::fromStdString(j["targetTime"].get<std::string>());
        if (j.contains("bestRecord"))
            m_bestRecord = j["bestRecord"].get<int>();
        if (j.contains("blockMode"))
            m_blockMode = j["blockMode"].get<int>();
        if (j.contains("rstRetry"))
            m_rstRetry = j["rstRetry"].get<int>();
        if (j.contains("pomodoroFocus"))
            m_pomodoroFocus = j["pomodoroFocus"].get<int>();
        if (j.contains("pomodoroRest"))
            m_pomodoroRest = j["pomodoroRest"].get<int>();
        if (j.contains("toastEnabled"))
            m_toastEnabled = j["toastEnabled"].get<bool>();
        if (j.contains("autoStart"))
            m_autoStart = j["autoStart"].get<bool>();
        if (j.contains("trayOnClose"))
            m_trayOnClose = j["trayOnClose"].get<bool>();
        if (j.contains("weeklyMinutes") && j["weeklyMinutes"].is_array()) {
            m_weeklyMinutes.clear();
            for (auto &el : j["weeklyMinutes"])
                m_weeklyMinutes.append(el.get<int>());
        }
        while (m_weeklyMinutes.size() < 7)
            m_weeklyMinutes.append(0);

        emit userNameChanged();
        emit targetTimeChanged();
        emit bestRecordChanged();
        emit blockModeChanged();
        emit rstRetryChanged();
        emit pomodoroFocusChanged();
        emit pomodoroRestChanged();
        emit toastEnabledChanged();
        emit autoStartChanged();
        emit trayOnCloseChanged();
        emit weeklyMinutesChanged();
    } catch (...) {
        qDebug() << "[BackendManager] user_settings.json parse error";
    }
}

void BackendManager::saveSettings()
{
    json j;
    j["userName"]      = m_userName.toStdString();
    j["targetTime"]    = m_targetTime.toStdString();
    j["bestRecord"]    = m_bestRecord;
    j["blockMode"]     = m_blockMode;
    j["rstRetry"]      = m_rstRetry;
    j["pomodoroFocus"] = m_pomodoroFocus;
    j["pomodoroRest"]  = m_pomodoroRest;
    j["toastEnabled"]  = m_toastEnabled;
    j["autoStart"]     = m_autoStart;
    j["trayOnClose"]   = m_trayOnClose;

    json arrWeek = json::array();
    for (const auto &v : m_weeklyMinutes)
        arrWeek.push_back(v.toInt());
    j["weeklyMinutes"] = arrWeek;

    QSaveFile sFile(m_settingsFilePath);
    if (sFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QByteArray data = QByteArray::fromStdString(j.dump(4));
        sFile.write(data);
        if (sFile.commit()) {
            qDebug() << "[BackendManager] user_settings.json saved successfully:" << m_settingsFilePath;
        } else {
            qWarning() << "[BackendManager] failed to commit user_settings.json:" << sFile.errorString();
        }
    } else {
        qWarning() << "[BackendManager] failed to open user_settings.json for writing:" << sFile.errorString();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  화이트리스트 로드 / 저장
// ─────────────────────────────────────────────────────────────────────────────
void BackendManager::loadWhitelistFile()
{
    std::ifstream file(m_whitelistFilePath.toStdString());
    if (!file.is_open()) return;
    try {
        json j;
        file >> j;
        
        // 1. allowed_domains (화이트리스트)
        QVariantList wlist;
        if (j.contains("allowed_domains")) {
            for (auto &el : j["allowed_domains"])
                wlist.append(QString::fromStdString(el.get<std::string>()));
        }
        if (m_whitelistDomains != wlist) {
            m_whitelistDomains = wlist;
            emit whitelistDomainsChanged();
        }

        // 2. blocked_domains (블랙리스트)
        QVariantList blist;
        if (j.contains("blocked_domains")) {
            for (auto &el : j["blocked_domains"])
                blist.append(QString::fromStdString(el.get<std::string>()));
        }
        if (m_blacklistDomains != blist) {
            m_blacklistDomains = blist;
            emit blacklistDomainsChanged();
        }

        // 3. blocked_process (차단 프로그램/앱)
        QVariantList plist;
        if (j.contains("blocked_process")) {
            for (auto &el : j["blocked_process"])
                plist.append(QString::fromStdString(el.get<std::string>()));
        }
        if (m_blockedProcesses != plist) {
            m_blockedProcesses = plist;
            emit blockedProcessesChanged();
        }

    } catch (...) {
        qDebug() << "[BackendManager] whitelist.json parse error";
    }
}

void BackendManager::saveWhitelistFile()
{
    qDebug() << "[BackendManager] saveWhitelistFile called for:" << m_whitelistFilePath;

    // 1. 기존 파일 읽기 (스코프를 제한하여 ifstream이 즉시 close 되도록 보장)
    json j;
    {
        std::ifstream rfile(m_whitelistFilePath.toStdString());
        if (rfile.is_open()) {
            try {
                rfile >> j;
            } catch (const std::exception &e) {
                qWarning() << "[BackendManager] read existing whitelist error:" << e.what();
            }
            rfile.close();
        }
    }

    // 2. allowed_domains
    json arrWhite = json::array();
    for (const auto &v : m_whitelistDomains) {
        QString s = v.toString().trimmed().toLower();
        if (!s.isEmpty()) {
            arrWhite.push_back(s.toStdString());
        }
    }
    j["allowed_domains"] = arrWhite;

    // 3. blocked_domains
    json arrBlack = json::array();
    for (const auto &v : m_blacklistDomains) {
        QString s = v.toString().trimmed().toLower();
        if (!s.isEmpty()) {
            arrBlack.push_back(s.toStdString());
        }
    }
    j["blocked_domains"] = arrBlack;

    // 4. blocked_process (차단 앱)
    json arrProc = json::array();
    for (const auto &v : m_blockedProcesses) {
        QString s = v.toString().trimmed();
        if (!s.isEmpty()) {
            arrProc.push_back(s.toStdString());
        }
    }
    j["blocked_process"] = arrProc;

    // 5. QSaveFile을 사용한 안전하고 원자적인 파일 쓰기
    QSaveFile sFile(m_whitelistFilePath);
    if (sFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QByteArray data = QByteArray::fromStdString(j.dump(4));
        sFile.write(data);
        if (sFile.commit()) {
            qDebug() << "[BackendManager] whitelist.json successfully saved to" << m_whitelistFilePath
                     << "(whitelist:" << m_whitelistDomains.size()
                     << ", blacklist:" << m_blacklistDomains.size()
                     << ", blocked_process:" << m_blockedProcesses.size() << ")";
        } else {
            qWarning() << "[BackendManager] failed to commit whitelist.json:" << sFile.errorString();
        }
    } else {
        qWarning() << "[BackendManager] failed to open whitelist.json for writing:" << sFile.errorString();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  화이트리스트 / 블랙리스트 / 앱 차단 CRUD (QML 호출용)
// ─────────────────────────────────────────────────────────────────────────────
void BackendManager::addToWhitelist(const QString &domain)
{
    QString d = domain.trimmed().toLower();
    qDebug() << "[BackendManager] addToWhitelist requested for:" << d;
    if (d.isEmpty()) return;

    // 만약 블랙리스트에 있으면 블랙리스트에서 제거
    int blRemoved = m_blacklistDomains.removeAll(d);
    if (blRemoved > 0) {
        emit blacklistDomainsChanged();
    }

    if (!m_whitelistDomains.contains(d)) {
        m_whitelistDomains.append(d);
        emit whitelistDomainsChanged();
    }
    saveWhitelistFile();
}

void BackendManager::removeFromWhitelist(const QString &domain)
{
    int removed = m_whitelistDomains.removeAll(domain.trimmed().toLower());
    if (removed > 0) {
        emit whitelistDomainsChanged();
        saveWhitelistFile();
    }
}

void BackendManager::addToBlacklist(const QString &domain)
{
    QString d = domain.trimmed().toLower();
    if (d.isEmpty() || m_blacklistDomains.contains(d)) return;
    m_blacklistDomains.append(d);
    emit blacklistDomainsChanged();
    saveWhitelistFile();
}

void BackendManager::removeFromBlacklist(const QString &domain)
{
    int removed = m_blacklistDomains.removeAll(domain.trimmed().toLower());
    if (removed > 0) {
        emit blacklistDomainsChanged();
        saveWhitelistFile();
    }
}

void BackendManager::addBlockedProcess(const QString &processName)
{
    QString p = processName.trimmed();
    if (p.isEmpty()) return;
    if (!p.endsWith(".exe", Qt::CaseInsensitive)) {
        p += ".exe";
    }
    qDebug() << "[BackendManager] addBlockedProcess requested for:" << p;

    // 대소문자 무시 중복 체크
    for (const auto &v : m_blockedProcesses) {
        if (v.toString().compare(p, Qt::CaseInsensitive) == 0) {
            return;
        }
    }

    m_blockedProcesses.append(p);
    emit blockedProcessesChanged();
    saveWhitelistFile();
}

void BackendManager::removeBlockedProcess(const QString &processName)
{
    QString p = processName.trimmed();
    qDebug() << "[BackendManager] removeBlockedProcess requested for:" << p;
    bool removed = false;
    for (int i = m_blockedProcesses.size() - 1; i >= 0; --i) {
        if (m_blockedProcesses[i].toString().compare(p, Qt::CaseInsensitive) == 0) {
            m_blockedProcesses.removeAt(i);
            removed = true;
        }
    }
    if (removed) {
        emit blockedProcessesChanged();
        saveWhitelistFile();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  통계 JSON 폴링 (1초마다 호출)
// ─────────────────────────────────────────────────────────────────────────────
void BackendManager::reloadStats()
{
    std::ifstream file(m_statsFilePath.toStdString());
    if (!file.is_open()) return;

    // 빈 파일 방어
    file.seekg(0, std::ios::end);
    if (file.tellg() == 0) return;
    file.seekg(0, std::ios::beg);

    try {
        json j;
        file >> j;

        // 1. 집중 점수
        if (j.contains("focus_score_percent")) {
            double v = j["focus_score_percent"].get<double>();
            if (m_focusScore != v) { m_focusScore = v; emit focusScoreChanged(); }
        }

        // 2. 경과 시간 (elapsedSecs + focusTime 문자열)
        if (j.contains("start_time")) {
            time_t st = j["start_time"].get<time_t>();
            if (st > 0) {
                int secs = qMax(0, (int)(time(nullptr) - st));
                if (m_elapsedSecs != secs) {
                    m_elapsedSecs = secs;
                    emit elapsedSecsChanged();

                    // 주간 요일별 집중 시간 실시간 갱신 (오늘 요일에 실시간 누적)
                    time_t now = time(nullptr);
                    struct tm *lt = localtime(&now);
                    if (lt) {
                        int dayIdx = (lt->tm_wday == 0) ? 6 : (lt->tm_wday - 1);
                        int curMins = m_elapsedSecs / 60;
                        if (dayIdx >= 0 && dayIdx < m_weeklyMinutes.size()) {
                            if (m_weeklyMinutes[dayIdx].toInt() != curMins) {
                                m_weeklyMinutes[dayIdx] = curMins;
                                emit weeklyMinutesChanged();
                            }
                        }
                    }

                    // 최고 기록 자동 갱신 및 영구 저장
                    if (m_elapsedSecs > m_bestRecord) {
                        m_bestRecord = m_elapsedSecs;
                        emit bestRecordChanged();
                        saveSettings();
                    }
                }
                QString ts = QString("%1:%2:%3")
                    .arg(secs / 3600, 2, 10, QChar('0'))
                    .arg((secs % 3600) / 60, 2, 10, QChar('0'))
                    .arg(secs % 60, 2, 10, QChar('0'));
                if (m_focusTime != ts) { m_focusTime = ts; emit focusTimeChanged(); }
                m_startTime = st;
            }
        }

        // 3. 차단 횟수
        if (j.contains("raw_stats")) {
            auto &rs = j["raw_stats"];
            int blocked = rs.value("blocked_connections", 0);
            int killed  = rs.value("killed_processes", 0);
            int total   = blocked + killed;
            if (m_blockedAttempts != total) {
                m_blockedAttempts = total;
                emit blockedAttemptsChanged();
            }
        }

        // 4. 최초 차단 이후 경과 시간 (blockedSecs + blockedTime)
        if (j.contains("first_block_time")) {
            time_t ft = j["first_block_time"].get<time_t>();
            if (ft > 0) {
                int bsecs = qMax(0, (int)(time(nullptr) - ft));
                if (m_blockedSecs != bsecs) {
                    m_blockedSecs = bsecs;
                    emit blockedSecsChanged();
                }
                QString bt = QString("%1:%2:%3")
                    .arg(bsecs / 3600, 2, 10, QChar('0'))
                    .arg((bsecs % 3600) / 60, 2, 10, QChar('0'))
                    .arg(bsecs % 60, 2, 10, QChar('0'));
                if (m_blockedTime != bt) { m_blockedTime = bt; emit blockedTimeChanged(); }
            }
        }

        // 5. 24시간 타임라인
        if (j.contains("timeline_24h")) {
            QVariantList tl;
            for (auto &it : j["timeline_24h"])
                tl.append(it.get<int>());
            if (m_timelineData != tl) { m_timelineData = tl; emit timelineDataChanged(); }
        }

        // 6. 실시간 차단 로그 (recent_logs)
        if (j.contains("recent_logs")) {
            QVariantList logs;
            for (auto &el : j["recent_logs"]) {
                QVariantMap m;
                QString timeStr   = QString::fromStdString(el["time"].get<std::string>());
                QString domainStr = QString::fromStdString(el["domain"].get<std::string>());
                
                // 카테고리 자동 추론
                QString cat = "기타";
                QString dLower = domainStr.toLower();
                if (dLower.contains("youtube") || dLower.contains("googlevideo") || dLower.contains("tiktok") || dLower.contains("twitch") || dLower.contains("netflix")) {
                    cat = "동영상";
                } else if (dLower.contains("instagram") || dLower.contains("facebook") || dLower.contains("twitter") || dLower.contains("x.com") || dLower.contains("fbcdn")) {
                    cat = "SNS";
                } else if (dLower.contains("discord") || dLower.contains("kakao") || dLower.contains("telegram")) {
                    cat = "메신저";
                } else if (dLower.contains("steam") || dLower.contains("nexon") || dLower.contains("riot") || dLower.contains("blizzard")) {
                    cat = "게임";
                } else if (dLower.contains("spotify") || dLower.contains("melon")) {
                    cat = "음악";
                }

                m["time"]   = timeStr;
                m["domain"] = domainStr;
                m["action"] = "접근 차단!";
                m["proto"]  = "TLS";
                m["reason"] = "화이트리스트 미등록";
                m["cat"]    = cat;
                logs.append(m);
            }
            if (m_recentLogs != logs) { m_recentLogs = logs; emit recentLogsChanged(); }
        }

        // 7. Top 차단 도메인 (top_blocked_domains → [{name, count, color}] 리스트 및 1위 추출)
        if (j.contains("top_blocked_domains")) {
            QVariantList top;
            QString bestDomain = "없음";
            int bestCount = 0;

            for (auto &[k, v] : j["top_blocked_domains"].items()) {
                QVariantMap m;
                QString d = QString::fromStdString(k);
                int cnt   = v.get<int>();
                m["name"]  = d;
                m["count"] = cnt;
                top.append(m);

                if (cnt > bestCount) {
                    bestCount = cnt;
                    bestDomain = d;
                }
            }

            // 차단 횟수 내림차순 정렬
            std::sort(top.begin(), top.end(), [](const QVariant &a, const QVariant &b) {
                return a.toMap().value("count").toInt() > b.toMap().value("count").toInt();
            });

            // UI 도넛 차트 및 리스트용 테마 컬러 배정
            const QStringList colors = { "#F43F5E", "#A855F7", "#7B61FF", "#38BDF8", "#10B981", "#F59E0B" };
            for (int i = 0; i < top.size(); ++i) {
                QVariantMap m = top[i].toMap();
                m["color"] = colors[i % colors.size()];
                top[i] = m;
            }

            if (m_topBlockedDomains != top) {
                m_topBlockedDomains = top;
                emit topBlockedDomainsChanged();
            }
            if (m_topDomain != bestDomain) {
                m_topDomain = bestDomain;
                emit topDomainChanged();
            }
            if (m_topDomainCount != bestCount) {
                m_topDomainCount = bestCount;
                emit topDomainCountChanged();
            }
        }

    } catch (const std::exception &e) {
        qDebug() << "[BackendManager] JSON parse error:" << e.what();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Setter
// ─────────────────────────────────────────────────────────────────────────────
void BackendManager::setUserName(const QString &name)
{
    if (m_userName != name) { m_userName = name; emit userNameChanged(); saveSettings(); }
}

void BackendManager::setTargetTime(const QString &time)
{
    if (m_targetTime != time) { m_targetTime = time; emit targetTimeChanged(); saveSettings(); }
}

void BackendManager::setIsPaused(bool paused)
{
    if (m_isPaused == paused) return;
    m_isPaused = paused;
    qDebug() << "[BackendManager] setIsPaused called:" << m_isPaused;

    if (m_isPaused) {
        // 1. core_engine/pause.flag 생성
        QFile f(m_pauseFlagPath);
        if (f.open(QIODevice::WriteOnly)) { f.write("1"); f.close(); }

        // 2. 현재 디렉터리 pause.flag 생성 (안전 fallback)
        QFile f2("pause.flag");
        if (f2.open(QIODevice::WriteOnly)) { f2.write("1"); f2.close(); }
        
        qDebug() << "[BackendManager] pause.flag created at:" << m_pauseFlagPath;
    } else {
        QFile::remove(m_pauseFlagPath);
        QFile::remove("pause.flag");
        qDebug() << "[BackendManager] pause.flag removed!";
    }
    emit isPausedChanged();
}

void BackendManager::togglePause()
{
    setIsPaused(!m_isPaused);
}

void BackendManager::setBlockMode(int v)
{
    if (m_blockMode != v) { m_blockMode = v; emit blockModeChanged(); saveSettings(); }
}

void BackendManager::setRstRetry(int v)
{
    if (m_rstRetry != v) { m_rstRetry = v; emit rstRetryChanged(); saveSettings(); }
}

void BackendManager::setPomodoroFocus(int v)
{
    if (m_pomodoroFocus != v) { m_pomodoroFocus = v; emit pomodoroFocusChanged(); saveSettings(); }
}

void BackendManager::setPomodoroRest(int v)
{
    if (m_pomodoroRest != v) { m_pomodoroRest = v; emit pomodoroRestChanged(); saveSettings(); }
}

void BackendManager::setToastEnabled(bool v)
{
    if (m_toastEnabled != v) { m_toastEnabled = v; emit toastEnabledChanged(); saveSettings(); }
}

void BackendManager::setAutoStart(bool v)
{
    if (m_autoStart != v) { m_autoStart = v; emit autoStartChanged(); saveSettings(); }
}

void BackendManager::setTrayOnClose(bool v)
{
    if (m_trayOnClose != v) { m_trayOnClose = v; emit trayOnCloseChanged(); saveSettings(); }
}

void BackendManager::saveAppSettings(int blockMode, int rstRetry, int pomodoroFocus, int pomodoroRest,
                                     bool toastEnabled, bool autoStart, bool trayOnClose,
                                     const QString &userName, const QString &targetTime)
{
    m_blockMode = blockMode;
    m_rstRetry = rstRetry;
    m_pomodoroFocus = pomodoroFocus;
    m_pomodoroRest = pomodoroRest;
    m_toastEnabled = toastEnabled;
    m_autoStart = autoStart;
    m_trayOnClose = trayOnClose;
    if (!userName.trimmed().isEmpty()) m_userName = userName.trimmed();
    if (!targetTime.trimmed().isEmpty()) m_targetTime = targetTime.trimmed();

    emit blockModeChanged();
    emit rstRetryChanged();
    emit pomodoroFocusChanged();
    emit pomodoroRestChanged();
    emit toastEnabledChanged();
    emit autoStartChanged();
    emit trayOnCloseChanged();
    emit userNameChanged();
    emit targetTimeChanged();

    saveSettings();
}

