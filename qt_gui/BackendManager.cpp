#include "BackendManager.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QProcess>
#include <QRegularExpression>
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
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

    // 코어 엔진 실행 파일 경로 탐색
    QString engineCandidate = ws + "/core_engine/x64/Debug/FocusGuard.exe";
    if (QFile::exists(engineCandidate)) {
        m_engineExePath = engineCandidate;
    } else {
        m_engineExePath = ws + "/core_engine/x64/Release/FocusGuard.exe";
    }

    qDebug() << "[BackendManager] workspace  =" << ws;
    qDebug() << "[BackendManager] stats      =" << m_statsFilePath;
    qDebug() << "[BackendManager] engine exe =" << m_engineExePath;

    // 기본 타임라인 데이터 초기화 (24시간)
    for (int i = 0; i < 24; ++i)
        m_timelineData.append(0);

    // 기본 주간 데이터 초기화 (7일: 월~일)
    for (int i = 0; i < 7; ++i)
        m_weeklyMinutes.append(0);

    // NIC 장치 초기 수집
    refreshNetworkAdapters();

    loadSettings();
    loadWhitelistFile();

    // 1초마다 통계 파일 폴링
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &BackendManager::reloadStats);
    m_timer->start(1000);

    reloadStats();

    // 코어 감시 엔진(FocusGuard.exe) 자동 기동 (UAC 관리자 권한 상속)
    QTimer::singleShot(500, this, [this]() {
        startEngine();
    });
}

BackendManager::~BackendManager()
{
    stopEngine();
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

        // 4. target_adapter
        if (j.contains("target_adapter")) {
            m_targetAdapter = QString::fromStdString(j["target_adapter"].get<std::string>()).trimmed();
            if (m_targetAdapter.isEmpty()) m_targetAdapter = "AUTO";
        } else {
            m_targetAdapter = "AUTO";
        }
        emit targetAdapterChanged();

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

    if (!m_targetAdapter.isEmpty() && m_targetAdapter != "AUTO") {
        j["target_adapter"] = m_targetAdapter.toStdString();
    } else {
        j["target_adapter"] = "AUTO";
    }

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

void BackendManager::removeSelectedSites(int tabIndex, const QStringList &targets)
{
    if (targets.isEmpty()) return;
    qDebug() << "[BackendManager] removeSelectedSites: tabIndex=" << tabIndex << ", count=" << targets.size();

    bool changed = false;
    if (tabIndex == 0) {
        // 화이트리스트
        for (const QString &t : targets) {
            QString d = t.trimmed().toLower();
            if (m_whitelistDomains.removeAll(d) > 0) {
                changed = true;
            }
        }
        if (changed) {
            emit whitelistDomainsChanged();
            saveWhitelistFile();
        }
    } else if (tabIndex == 1) {
        // 블랙리스트
        for (const QString &t : targets) {
            QString d = t.trimmed().toLower();
            if (m_blacklistDomains.removeAll(d) > 0) {
                changed = true;
            }
        }
        if (changed) {
            emit blacklistDomainsChanged();
            saveWhitelistFile();
        }
    } else if (tabIndex == 2) {
        // 차단 앱/프로세스
        for (const QString &t : targets) {
            QString p = t.trimmed();
            for (int i = m_blockedProcesses.size() - 1; i >= 0; --i) {
                if (m_blockedProcesses[i].toString().compare(p, Qt::CaseInsensitive) == 0) {
                    m_blockedProcesses.removeAt(i);
                    changed = true;
                }
            }
        }
        if (changed) {
            emit blockedProcessesChanged();
            saveWhitelistFile();
        }
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

                    // 최고 기록 자동 갱신 및 영구 저장 (I/O 부하 방지를 위해 30초 간격으로 디스크 저장)
                    if (m_elapsedSecs > m_bestRecord) {
                        m_bestRecord = m_elapsedSecs;
                        emit bestRecordChanged();
                        static int lastSavedBest = 0;
                        if (m_elapsedSecs - lastSavedBest >= 30) {
                            lastSavedBest = m_elapsedSecs;
                            saveSettings();
                        }
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
    if (m_autoStart != v) {
        m_autoStart = v;
        emit autoStartChanged();
        saveSettings();
    }

#ifdef _WIN32
    QSettings bootSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    const QString appName = "FocusNetGuardian";
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    if (v) {
        bootSettings.setValue(appName, QString("\"%1\"").arg(appPath));
        qDebug() << "[BackendManager] Windows registry autostart registered:" << appPath;
    } else {
        bootSettings.remove(appName);
        qDebug() << "[BackendManager] Windows registry autostart removed";
    }
#endif
}

void BackendManager::setTrayOnClose(bool v)
{
    if (m_trayOnClose != v) { m_trayOnClose = v; emit trayOnCloseChanged(); saveSettings(); }
}

void BackendManager::showTrayNotification(const QString &title, const QString &message)
{
    qDebug() << "[BackendManager] Tray Notification:" << title << "-" << message;
    emit trayNotificationRequested(title, message);
}

void BackendManager::requestShowWindow()
{
    emit showWindowRequested();
}

void BackendManager::quitApp()
{
    QCoreApplication::quit();
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

void BackendManager::requestAiUserAnalysis()
{
    if (m_isAiAnalyzing) return;
    m_isAiAnalyzing = true;
    emit isAiAnalyzingChanged();

    // 0.8초 딜레이를 두어 자연스러운 AI 실시간 분석 연출
    QTimer::singleShot(800, this, [this]() {
        int videoCount = 0;
        int snsCount = 0;
        int msgCount = 0;
        int gameCount = 0;
        int etcCount = 0;
        int totalBlocked = m_blockedAttempts;

        for (const auto &v : m_topBlockedDomains) {
            QVariantMap m = v.toMap();
            QString name = m.value("name").toString().toLower();
            int cnt = m.value("count").toInt();

            if (name.contains("youtube") || name.contains("googlevideo") || name.contains("tiktok") || name.contains("twitch") || name.contains("netflix")) {
                videoCount += cnt;
            } else if (name.contains("instagram") || name.contains("facebook") || name.contains("twitter") || name.contains("x.com")) {
                snsCount += cnt;
            } else if (name.contains("discord") || name.contains("kakao") || name.contains("telegram")) {
                msgCount += cnt;
            } else if (name.contains("steam") || name.contains("nexon") || name.contains("riot") || name.contains("blizzard") || name.contains("league")) {
                gameCount += cnt;
            } else {
                etcCount += cnt;
            }
        }

        // 인터넷 사용 유형(페르소나) 추론 엔진
        m_aiActionPlan.clear();

        if (totalBlocked == 0 && m_elapsedSecs >= 1800) {
            // 차단 시도 없이 장시간 몰입
            m_aiUserType = "🧘 초지일관 고요한 수도승형";
            m_aiTypeIcon = "shield-check";
            m_aiTypeColor = "#10B981";
            m_aiAnalysisReport = QString("오늘 %1 동안 단 1회의 방해 시도 없이 순도 높은 학습 몰입을 유지하고 있습니다. 유혹 신호에 뇌가 반응하지 않는 최상의 딥워크(Deep Work) 상태입니다.")
                                    .arg(m_focusTime);
            m_aiActionPlan << "현재의 훌륭한 집중 리듬을 유지하되 50분마다 5분의 스트레칭을 병행하세요."
                           << "최대 집중 상태가 90분을 넘어가면 인지 피로가 누적되므로 휴식을 권장합니다."
                           << "지금 학습한 핵심 내용을 5분간 복습 노트에 인출해 장기 기억으로 전환하세요.";
        } else if (videoCount >= 5 || (totalBlocked > 0 && (double)videoCount / totalBlocked >= 0.4)) {
            // 영상/스트리밍 편중
            m_aiUserType = "🎬 충동적 도파민 탐험가형";
            m_aiTypeIcon = "zap";
            m_aiTypeColor = "#F43F5E";
            m_aiAnalysisReport = QString("동영상 및 스트리밍 플랫폼 차단 비율이 전체의 %1%를 차지하고 있습니다. 뇌가 지루함을 느끼는 순간 무의식적으로 쇼츠/유튜브를 클릭하는 '시각적 도파민 보상 추구' 패턴이 감지되었습니다.")
                                    .arg(totalBlocked > 0 ? (int)((double)videoCount / totalBlocked * 100) : 60);
            m_aiActionPlan << "유튜브 관련 서브 도메인(googlevideo.com 등)을 사이트 관리에서 완전 차단 유지하세요."
                           << "집중 시작 전 '오늘 볼 영상'을 메모장에 적어두고 학습 종료 후에만 열람하세요."
                           << "공부 중 졸리거나 피로할 때는 화면 대신 일어나 1분간 가벼운 스트레칭을 하세요.";
        } else if (msgCount >= 5 || (totalBlocked > 0 && (double)msgCount / totalBlocked >= 0.35)) {
            // 메신저/연결 집착
            m_aiUserType = "💬 사회적 연결 갈망형";
            m_aiTypeIcon = "user";
            m_aiTypeColor = "#38BDF8";
            m_aiAnalysisReport = QString("카카오톡·디스코드 등 실시간 소통 앱 차단이 집중 흐름을 반복해서 끊고 있습니다. 타인의 연락에 즉각 반응해야 한다는 '연결 불안(FOMO)'이 무의식적인 앱 실행을 유발하고 있습니다.");
            m_aiActionPlan << "뽀모도로 25분 집중 시간 동안에는 스마트폰을 뒤집어 시야 밖으로 치우세요."
                           << "메신저 앱은 작업표시줄에서 완전히 종료하고 '휴식 시간 5분'에만 확인하세요."
                           << "주요 대화 상대에게 '집중 시간 중에는 답장이 늦을 수 있다'고 미리 공유하세요.";
        } else if (gameCount >= 3 || (totalBlocked > 0 && (double)gameCount / totalBlocked >= 0.3)) {
            // 게임/런처 접근
            m_aiUserType = "🎮 스트레스 탈출 게이머형";
            m_aiTypeIcon = "cpu";
            m_aiTypeColor = "#A855F7";
            m_aiAnalysisReport = QString("난이도 높은 과제나 코딩 중 뇌가 과부하를 느낄 때 스트레스 회피성 게임 클라이언트 실행이 감지되었습니다. 단기 탈출 충동을 통제하는 보호 장치가 효과적으로 작동 중입니다.");
            m_aiActionPlan << "게임 런처는 백그라운드 프로세스까지 FocusNet Guardian으로 완벽 차단하세요."
                           << "과제가 막힐 때는 게임 대신 5분간 찬물을 마시거나 심호흡을 권장합니다."
                           << "오늘의 목표 달성률 100% 달성 시 스스로에게 게임 1시간의 보상을 부여하세요.";
        } else if (totalBlocked >= 10) {
            // 다중 도메인 산발 분산
            m_aiUserType = "⚡ 멀티태스킹 브라우징형";
            m_aiTypeIcon = "refresh";
            m_aiTypeColor = "#F59E0B";
            m_aiAnalysisReport = QString("총 %1회의 다양한 사이트 차단이 감지되었습니다. 한 가지 화면에 10분 이상 머물지 못하고 여러 웹페이지 탭을 오가며 주의력이 분산되는 '팝콘 브레인' 패턴을 보입니다.")
                                    .arg(totalBlocked);
            m_aiActionPlan << "브라우저에서 지금 작업 중인 탭 단 1개만 남겨두고 나머지는 모두 닫으세요."
                           << "엄격 모드(Default-Deny)를 활성화하여 화이트리스트 외 접속을 철저히 차단하세요."
                           << "타이머 목표를 15분 단위로 작게 쪼개어 단기 완수 성공 경험을 쌓으세요.";
        } else {
            // 기본 상태
            m_aiUserType = "🌱 잠재력 발휘 준비형";
            m_aiTypeIcon = "target";
            m_aiTypeColor = "#818CF8";
            m_aiActionPlan << QString("일일 집중 목표 시간(현재 %1)을 달성할 때까지 세션을 이어가세요.").arg(m_targetTime)
                           << "자주 방문하는 학습 필수 사이트만 화이트리스트에 꼼꼼히 등록하세요."
                           << "최소 30분 이상 집중 후 AI 분석을 다시 요청하면 더욱 정밀한 피드백을 제공합니다.";
        }

        m_isAiAnalyzing = false;
        emit isAiAnalyzingChanged();
        emit aiAnalysisFinished();
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Npcap 엔진 제어 및 터미널 로그 스트리밍 (QProcess)
// ─────────────────────────────────────────────────────────────────────────────
void BackendManager::startEngine()
{
    if (m_isEngineRunning) return;

    if (!QFile::exists(m_engineExePath)) {
        QString ws = resolveWorkspacePath();
        QString candidate = ws + "/core_engine/x64/Debug/FocusGuard.exe";
        if (QFile::exists(candidate)) m_engineExePath = candidate;
        else m_engineExePath = ws + "/core_engine/x64/Release/FocusGuard.exe";
    }

    if (!QFile::exists(m_engineExePath)) {
        m_engineLogs.append(QString("[%1] [Error] 코어 엔진 실행 파일을 찾을 수 없습니다: %2")
                            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), m_engineExePath));
        emit engineLogsChanged();
        return;
    }

    if (!m_engineProcess) {
        m_engineProcess = new QProcess(this);

        connect(m_engineProcess, &QProcess::readyReadStandardOutput, this, [this]() {
            QByteArray out = m_engineProcess->readAllStandardOutput();
            QString s = QString::fromUtf8(out).trimmed();
            if (!s.isEmpty()) {
                QStringList lines = s.split('\n', Qt::SkipEmptyParts);
                for (const QString &line : lines) {
                    m_engineLogs.append(line.trimmed());
                }
                while (m_engineLogs.size() > 500) {
                    m_engineLogs.removeFirst();
                }
                emit engineLogsChanged();
            }
        });

        connect(m_engineProcess, &QProcess::readyReadStandardError, this, [this]() {
            QByteArray err = m_engineProcess->readAllStandardError();
            QString s = QString::fromUtf8(err).trimmed();
            if (!s.isEmpty()) {
                QStringList lines = s.split('\n', Qt::SkipEmptyParts);
                for (const QString &line : lines) {
                    m_engineLogs.append(QString("[STDERR] %1").arg(line.trimmed()));
                }
                while (m_engineLogs.size() > 500) {
                    m_engineLogs.removeFirst();
                }
                emit engineLogsChanged();
            }
        });

        connect(m_engineProcess, &QProcess::started, this, [this]() {
            m_isEngineRunning = true;
            m_engineLogs.append(QString("[%1] [System] 🚀 코어 엔진(FocusGuard.exe)이 정상 기동되었습니다.")
                                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            emit isEngineRunningChanged();
            emit engineLogsChanged();
        });

        connect(m_engineProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
            m_isEngineRunning = false;
            m_engineLogs.append(QString("[%1] [System] 🛑 코어 엔진이 종료되었습니다 (Code: %2, Status: %3)")
                                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(exitCode).arg(exitStatus));
            emit isEngineRunningChanged();
            emit engineLogsChanged();
        });
    }

    m_engineLogs.append(QString("[%1] [System] 엔진 구동 요청: %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), m_engineExePath));
    emit engineLogsChanged();

    // 엔진 실행 (작업 디렉터리는 core_engine 폴더로 설정하여 whitelist.json 정상 접근)
    QFileInfo fi(m_engineExePath);
    QString workDir = fi.absolutePath();
    // core_engine 폴더가 한 단계 위인 경우
    QDir d(workDir);
    if (d.dirName() == "Debug" || d.dirName() == "Release") {
        d.cdUp(); // x64
        d.cdUp(); // core_engine
        workDir = d.absolutePath();
    }
    m_engineProcess->setWorkingDirectory(workDir);

    m_engineProcess->start(m_engineExePath, QStringList());

    if (!m_engineProcess->waitForStarted(2000)) {
        // 권한 상승(740) 등으로 일반 QProcess 시작 실패 시 ShellExecuteEx runas 비상 Fallback 실행
        qWarning() << "[BackendManager] QProcess start failed, falling back to ShellExecuteEx runas";
#ifdef _WIN32
        std::wstring exeW = QDir::toNativeSeparators(m_engineExePath).toStdWString();
        std::wstring dirW = QDir::toNativeSeparators(workDir).toStdWString();

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exeW.c_str();
        sei.lpDirectory = dirW.c_str();
        sei.nShow = SW_SHOWNORMAL;

        if (ShellExecuteExW(&sei)) {
            m_isEngineRunning = true;
            m_engineLogs.append(QString("[%1] [System] 🛡️ UAC 권한 상승(ShellExecuteEx)으로 엔진이 독립 실행되었습니다.")
                                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            emit isEngineRunningChanged();
            emit engineLogsChanged();
        } else {
            m_engineLogs.append(QString("[%1] [Error] 엔진 권한 상승 실행 실패 (Windows Error: %2)")
                                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(GetLastError()));
            emit engineLogsChanged();
        }
#endif
    }
}

void BackendManager::stopEngine()
{
    if (m_engineProcess && m_engineProcess->state() != QProcess::NotRunning) {
        m_engineLogs.append(QString("[%1] [System] 엔진 종료 시그널 전달 중...").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        emit engineLogsChanged();
        m_engineProcess->terminate();
        if (!m_engineProcess->waitForFinished(2000)) {
            m_engineProcess->kill();
        }
    } else {
#ifdef _WIN32
        // ShellExecuteEx 등으로 구동된 외부 FocusGuard.exe 프로세스 강제 종료
        system("taskkill /F /IM FocusGuard.exe >nul 2>&1");
        m_isEngineRunning = false;
        m_engineLogs.append(QString("[%1] [System] 외부 FocusGuard.exe 프로세스를 종료했습니다.")
                            .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        emit isEngineRunningChanged();
        emit engineLogsChanged();
#endif
    }
}

void BackendManager::restartEngine()
{
    stopEngine();
    QTimer::singleShot(800, this, [this]() {
        startEngine();
    });
}

void BackendManager::clearEngineLogs()
{
    m_engineLogs.clear();
    emit engineLogsChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  네트워크 인터페이스(NIC) 어댑터 수집 및 선택
// ─────────────────────────────────────────────────────────────────────────────
void BackendManager::refreshNetworkAdapters()
{
    QStringList rawList;
    QStringList displayList;

    // 1. 항상 최상단 0번 항목은 '자동 감지 (권장)'
    rawList.append("AUTO");
    displayList.append("🌐 자동 감지 (권장 - 활성 네트워크 자동 감지)");

#ifdef _WIN32
    // Windows PowerShell을 통해 물리 랜카드 위주로 수집
    // 가상 네트워크(VMware, VirtualBox, vEthernet, WSL 등), 루프백(Loopback), 블루투스(Bluetooth) 장치는 목록에서 숨김
    QProcess p;
    p.start("powershell", QStringList() << "-NoProfile" << "-Command"
            << "Get-NetAdapter | Where-Object { $_.InterfaceDescription -notmatch 'Virtual|VMware|VirtualBox|vEthernet|WSL|Loopback|Bluetooth|TAP-Windows' } "
               "| Sort-Object @{Expression={ if ($_.Status -eq 'Up') { 0 } else { 1 } }} "
               "| Select-Object -ExpandProperty InterfaceDescription");
    if (p.waitForFinished(3000)) {
        QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) {
            QStringList lines = out.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                QString item = line.trimmed();
                if (!item.isEmpty() && !rawList.contains(item)) {
                    rawList.append(item);

                    // 복잡한 칩셋 명칭 대신 직관적인 아이콘과 연결 유형 함께 표기
                    QString lower = item.toLower();
                    if (lower.contains("wi-fi") || lower.contains("wireless") || lower.contains("wlan") ||
                        lower.contains("802.11") || lower.contains("8852") || lower.contains("ax2") || lower.contains("wifi")) {
                        displayList.append(QString("📶 Wi-Fi (%1)").arg(item));
                    } else if (lower.contains("ethernet") || lower.contains("gbe") || lower.contains("gigabit") ||
                               lower.contains("pcie") || lower.contains("family controller") || lower.contains("lan")) {
                        displayList.append(QString("🔌 이더넷 (%1)").arg(item));
                    } else {
                        displayList.append(QString("🌐 %1").arg(item));
                    }
                }
            }
        }
    }
#endif

    // Fallback: 물리 랜카드가 감지되지 않은 경우의 기본 목록
    if (rawList.size() <= 1) {
        rawList.append("Realtek 8852CE WiFi 6E PCI-E NIC");
        displayList.append("📶 Wi-Fi (Realtek 8852CE WiFi 6E PCI-E NIC)");
        rawList.append("Realtek PCIe GbE Family Controller");
        displayList.append("🔌 이더넷 (Realtek PCIe GbE Family Controller)");
    }

    m_nicRawList = rawList;
    if (m_nicList != displayList) {
        m_nicList = displayList;
        emit nicListChanged();
    }

    // 2. 역매칭(Reverse Matching) 및 Edge Case 방어
    if (m_targetAdapter.isEmpty() || m_targetAdapter == "AUTO" || m_targetAdapter == "자동 감지") {
        m_nicIndex = 0;
        m_targetAdapter = "AUTO";
        emit nicIndexChanged();
        emit targetAdapterChanged();
    } else {
        int idx = m_nicRawList.indexOf(m_targetAdapter);
        if (idx != -1) {
            m_nicIndex = idx;
            emit nicIndexChanged();
        } else {
            // ⚠️ [Edge Case 방어]: 사용자의 환경 변화(외장 랜카드 분리, USB 테더링 해제 등)로 이전 장치를 못 찾으면
            // 인덱스 오류가 나지 않도록 무조건 0번(자동 감지)으로 Fallback 처리!
            qWarning() << "[BackendManager] Saved adapter not found in current hardware. Falling back to AUTO (0):" << m_targetAdapter;
            m_nicIndex = 0;
            m_targetAdapter = "AUTO";
            emit nicIndexChanged();
            emit targetAdapterChanged();
            saveWhitelistFile(); // 갱신된 AUTO 상태 저장
        }
    }
}

void BackendManager::selectNetworkAdapter(int index)
{
    if (index >= 0 && index < m_nicRawList.size()) {
        m_nicIndex = index;
        m_targetAdapter = m_nicRawList[index]; // 0번이면 "AUTO", 1번 이상이면 실제 하드웨어 식별자
        emit nicIndexChanged();
        emit targetAdapterChanged();
        saveWhitelistFile();
        qDebug() << "[BackendManager] NIC adapter selected:" << m_targetAdapter << "(" << m_nicList[index] << ")";

        // 엔진이 구동 중이면 새 어댑터 적용을 위해 재시작 안내/자동 재기동
        if (m_isEngineRunning) {
            restartEngine();
        }
    }
}

void BackendManager::setNicIndex(int v)
{
    if (m_nicIndex != v) {
        selectNetworkAdapter(v);
    }
}

void BackendManager::setTargetAdapter(const QString &v)
{
    if (m_targetAdapter != v) {
        m_targetAdapter = v;
        emit targetAdapterChanged();
        saveWhitelistFile();
    }
}

