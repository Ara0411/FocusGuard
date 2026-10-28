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
#include <QAbstractNativeEventFilter>
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

#define HOTKEY_TOGGLE_MODE_ID 0xBEEF

class HotkeyEventFilter : public QAbstractNativeEventFilter
{
public:
    HotkeyEventFilter(BackendManager *mgr) : m_mgr(mgr) {}
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        Q_UNUSED(result);
        if (eventType == "windows_generic_MSG") {
            MSG *msg = static_cast<MSG *>(message);
            if (msg->message == WM_HOTKEY && msg->wParam == HOTKEY_TOGGLE_MODE_ID) {
                if (m_mgr) {
                    QMetaObject::invokeMethod(m_mgr, "toggleBlockMode", Qt::QueuedConnection);
                }
                return true;
            }
        }
        return false;
    }
private:
    BackendManager *m_mgr;
};
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
    m_historyFilePath  = ws + "/qt_gui/history_daily.json";

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

    // 이전 세션 잔여 pause.flag 정리 (앱 시작 시 정상 집중 모드로 동기화)
    QFile::remove(m_pauseFlagPath);
    QFile::remove("pause.flag");

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
    loadHistory();

    m_netManager = new QNetworkAccessManager(this);

    // 1초마다 통계 파일 폴링
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &BackendManager::reloadStats);
    m_timer->start(1000);

    reloadStats();

#ifdef _WIN32
    m_hotkeyFilter = new HotkeyEventFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(m_hotkeyFilter);
    registerGlobalHotkey();
#endif

    // 코어 감시 엔진(FocusGuard.exe) 자동 기동 (UAC 관리자 권한 상속)
    QTimer::singleShot(500, this, [this]() {
        startEngine();
    });
}

BackendManager::~BackendManager()
{
    blockSignals(true);
#ifdef _WIN32
    unregisterGlobalHotkey();
    if (m_hotkeyFilter) {
        QCoreApplication::instance()->removeNativeEventFilter(m_hotkeyFilter);
        delete m_hotkeyFilter;
        m_hotkeyFilter = nullptr;
    }
#endif
    saveHistory();
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
        if (j.contains("pomodoroAutoEnabled"))
            m_pomodoroAutoEnabled = j["pomodoroAutoEnabled"].get<bool>();
        if (j.contains("toastEnabled"))
            m_toastEnabled = j["toastEnabled"].get<bool>();
        if (j.contains("autoStart"))
            m_autoStart = j["autoStart"].get<bool>();
        if (j.contains("trayOnClose"))
            m_trayOnClose = j["trayOnClose"].get<bool>();
        if (j.contains("toggleShortcut"))
            m_toggleShortcut = QString::fromStdString(j["toggleShortcut"].get<std::string>());
        if (j.contains("toggleShortcutEnabled"))
            m_toggleShortcutEnabled = j["toggleShortcutEnabled"].get<bool>();
        if (j.contains("geminiApiKey")) {
            m_geminiApiKey = QString::fromStdString(j["geminiApiKey"].get<std::string>()).trimmed();
        } else {
            QByteArray envKey = qgetenv("GEMINI_API_KEY");
            if (!envKey.isEmpty()) {
                m_geminiApiKey = QString::fromUtf8(envKey).trimmed();
            }
        }
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
        emit pomodoroAutoEnabledChanged();
        emit pomodoroStateChanged();
        emit pomodoroRoundChanged();
        emit toastEnabledChanged();
        emit autoStartChanged();
        emit trayOnCloseChanged();
        emit toggleShortcutChanged();
        emit toggleShortcutEnabledChanged();
        emit weeklyMinutesChanged();
        emit geminiApiKeyChanged();
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
    j["pomodoroAutoEnabled"] = m_pomodoroAutoEnabled;
    j["toastEnabled"]  = m_toastEnabled;
    j["autoStart"]     = m_autoStart;
    j["trayOnClose"]   = m_trayOnClose;
    j["toggleShortcut"] = m_toggleShortcut.toStdString();
    j["toggleShortcutEnabled"] = m_toggleShortcutEnabled;
    if (!m_geminiApiKey.isEmpty()) {
        j["geminiApiKey"] = m_geminiApiKey.toStdString();
    }

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

        // 5. block_unregistered (미등록 차단 모드: true = 엄격 0, false = 유연 1)
        if (j.contains("block_unregistered") && j["block_unregistered"].is_boolean()) {
            int newMode = j["block_unregistered"].get<bool>() ? 0 : 1;
            if (m_blockMode != newMode) {
                m_blockMode = newMode;
                emit blockModeChanged();
            }
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

    if (!m_targetAdapter.isEmpty() && m_targetAdapter != "AUTO") {
        j["target_adapter"] = m_targetAdapter.toStdString();
    } else {
        j["target_adapter"] = "AUTO";
    }

    // 5. block_unregistered (미등록 사이트 차단 여부: 엄격 모드(0)이면 true, 유연 모드(1)이면 false)
    j["block_unregistered"] = (m_blockMode == 0);

    // 6. QSaveFile을 사용한 안전하고 원자적인 파일 쓰기
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

    // 만약 블랙리스트에 있으면 블랙리스트에서 제거 (www. 포함)
    bool blChanged = false;
    if (m_blacklistDomains.removeAll(d) > 0) blChanged = true;
    if (m_blacklistDomains.removeAll("www." + d) > 0) blChanged = true;
    if (d.startsWith("www.") && m_blacklistDomains.removeAll(d.mid(4)) > 0) blChanged = true;
    if (blChanged) {
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

    // 만약 화이트리스트에 있으면 화이트리스트에서 제거 (www. 포함)
    bool wlChanged = false;
    if (m_whitelistDomains.removeAll(d) > 0) wlChanged = true;
    if (m_whitelistDomains.removeAll("www." + d) > 0) wlChanged = true;
    if (d.startsWith("www.") && m_whitelistDomains.removeAll(d.mid(4)) > 0) wlChanged = true;
    if (wlChanged) {
        emit whitelistDomainsChanged();
    }

    m_blacklistDomains.append(d);
    emit blacklistDomainsChanged();
    if (m_aiRecommendedDomains.contains(d)) {
        m_aiRecommendedDomains.removeAll(d);
        emit aiRecommendedDomainsChanged();
    }
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
    // ── 뽀모도로(Pomodoro) 집중/휴식 자동 루프 갱신 (1초마다 1회 수행) ──
    static qint64 lastPomodoroTickTime = 0;
    qint64 nowSec = QDateTime::currentSecsSinceEpoch();
    if (nowSec != lastPomodoroTickTime) {
        lastPomodoroTickTime = nowSec;

        if (m_pomodoroAutoEnabled) {
            if (!m_isPaused) {
                // [집중 진행 모드]
                m_isPomodoroBreak = false;
                m_pomodoroElapsedSecs++;
                int targetFocusSecs = qMax(1, m_pomodoroFocus) * 60;
                if (m_pomodoroElapsedSecs >= targetFocusSecs) {
                    // 집중 시간 도달 -> 자동으로 휴식 모드로 전환
                    m_pomodoroElapsedSecs = 0;
                    m_isPomodoroBreak = true;
                    m_pomodoroRound++;
                    emit pomodoroRoundChanged();
                    emit pomodoroStateChanged();

                    setIsPaused(true);

                    if (m_toastEnabled) {
                        showTrayNotification("🍅 뽀모도로 집중 완료!",
                            QString("%1분 집중을 성공적으로 마쳤습니다! %2분간 달콤한 휴식을 취하세요 ☕")
                            .arg(m_pomodoroFocus).arg(m_pomodoroRest));
                    }
                } else {
                    emit pomodoroStateChanged();
                }
            } else {
                // [휴식 모드 (일시정지)]
                if (m_isPomodoroBreak) {
                    m_pomodoroElapsedSecs++;
                    int targetRestSecs = qMax(1, m_pomodoroRest) * 60;
                    if (m_pomodoroElapsedSecs >= targetRestSecs) {
                        // 휴식 시간 도달 -> 자동으로 집중 모드로 전환
                        m_pomodoroElapsedSecs = 0;
                        m_isPomodoroBreak = false;
                        emit pomodoroStateChanged();

                        setIsPaused(false);

                        if (m_toastEnabled) {
                            showTrayNotification("🔔 뽀모도로 휴식 완료!",
                                QString("%1분 휴식이 끝났습니다! 새로운 집중 세션을 시작합니다 💪")
                                .arg(m_pomodoroRest));
                        }
                    } else {
                        emit pomodoroStateChanged();
                    }
                }
            }
        }
    }

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
                if (m_startTime != st) {
                    m_startTime = st;
                    m_totalPausedSecs = 0;
                    if (m_isPaused) {
                        m_pauseStartTime = time(nullptr);
                    }
                }

                // 휴식 모드(isPaused)인 동안에는 경과 시간(elapsedSecs)의 카운트 증가를 일시정지
                if (!m_isPaused) {
                    int secs = qMax(0, (int)(time(nullptr) - st - m_totalPausedSecs));
                    if (m_elapsedSecs != secs) {
                        m_elapsedSecs = secs;
                        emit elapsedSecsChanged();

                        // 주간 요일별 집중 시간 실시간 갱신 (오늘 이전 누적치 + 현재 세션 누적치)
                        time_t now = time(nullptr);
                        struct tm *lt = localtime(&now);
                        if (lt) {
                            int dayIdx = (lt->tm_wday == 0) ? 6 : (lt->tm_wday - 1);
                            int curMins = (m_sessionStartFocusSecs + m_elapsedSecs) / 60;
                            if (dayIdx >= 0 && dayIdx < m_weeklyMinutes.size()) {
                                if (m_weeklyMinutes[dayIdx].toInt() != curMins) {
                                    m_weeklyMinutes[dayIdx] = curMins;
                                    emit weeklyMinutesChanged();
                                }
                            }
                        }

                        // 자정(일자 변경) 감지 시 이전 일자 커밋 및 새 일자 초기화
                        QString curDate = QDate::currentDate().toString("yyyy-MM-dd");
                        if (!m_todayDateStr.isEmpty() && curDate != m_todayDateStr) {
                            saveHistory();
                            ensureTodayRecord();
                        }

                        // 60초 주기 history_daily.json 자동 동기화
                        m_historyFlushCounter++;
                        if (m_historyFlushCounter >= 60) {
                            m_historyFlushCounter = 0;
                            saveHistory();
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
                }
            }
        }

        // 3. 차단 횟수
        if (j.contains("raw_stats")) {
            auto &rs = j["raw_stats"];
            int blocked = rs.value("blocked_connections", 0);
            int killed  = rs.value("killed_processes", 0);
            int total   = blocked + killed;
            if (m_blockedAttempts != total) {
                if (total > m_blockedAttempts) {
                    time_t now = time(nullptr);
                    // 직전 차단으로부터 45초 이상 경과 시 새로운 딴짓 세션으로 카운트
                    if (m_lastBlockTimestamp == 0 || (now - m_lastBlockTimestamp) >= 45) {
                        m_blockedSessions++;
                        emit blockedSessionsChanged();
                    }
                    m_lastBlockTimestamp = now;
                }
                m_blockedAttempts = total;
                emit blockedAttemptsChanged();
            }
            if (m_blockedSessions == 0 && m_blockedAttempts > 0) {
                m_blockedSessions = 1;
                emit blockedSessionsChanged();
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
        m_pauseStartTime = time(nullptr);
        // 휴식 모드로 전환 시 뽀모도로 휴식 상태 동기화
        m_isPomodoroBreak = true;
        m_pomodoroElapsedSecs = 0;
        emit pomodoroStateChanged();

        // 1. core_engine/pause.flag 생성
        QFile f(m_pauseFlagPath);
        if (f.open(QIODevice::WriteOnly)) { f.write("1"); f.close(); }

        // 2. 현재 디렉터리 pause.flag 생성 (안전 fallback)
        QFile f2("pause.flag");
        if (f2.open(QIODevice::WriteOnly)) { f2.write("1"); f2.close(); }
        
        qDebug() << "[BackendManager] pause.flag created at:" << m_pauseFlagPath;
    } else {
        if (m_pauseStartTime > 0) {
            int pauseDuration = qMax(0, (int)(time(nullptr) - m_pauseStartTime));
            m_totalPausedSecs += pauseDuration;
            m_pauseStartTime = 0;
            qDebug() << "[BackendManager] Resumed focus. Pause duration:" << pauseDuration << "sec, Total paused:" << m_totalPausedSecs << "sec";
        }
        // 집중 모드로 복귀 시 뽀모도로 집중 상태 동기화
        m_isPomodoroBreak = false;
        m_pomodoroElapsedSecs = 0;
        emit pomodoroStateChanged();

        QFile::remove(m_pauseFlagPath);
        QFile::remove("pause.flag");
        qDebug() << "[BackendManager] pause.flag removed!";
    }
    emit isPausedChanged();
    reloadStats();
    saveHistory();
}

void BackendManager::togglePause()
{
    setIsPaused(!m_isPaused);
}

void BackendManager::setBlockMode(int v)
{
    if (m_blockMode != v) {
        m_blockMode = v;
        emit blockModeChanged();
        saveSettings();
        saveWhitelistFile();
    }
}

void BackendManager::setRstRetry(int v)
{
    if (m_rstRetry != v) { m_rstRetry = v; emit rstRetryChanged(); saveSettings(); }
}

void BackendManager::setPomodoroFocus(int v)
{
    if (m_pomodoroFocus != v) { m_pomodoroFocus = v; emit pomodoroFocusChanged(); emit pomodoroStateChanged(); saveSettings(); }
}

void BackendManager::setPomodoroRest(int v)
{
    if (m_pomodoroRest != v) { m_pomodoroRest = v; emit pomodoroRestChanged(); emit pomodoroStateChanged(); saveSettings(); }
}

void BackendManager::setPomodoroAutoEnabled(bool v)
{
    if (m_pomodoroAutoEnabled != v) {
        m_pomodoroAutoEnabled = v;
        emit pomodoroAutoEnabledChanged();
        saveSettings();
    }
}

void BackendManager::resetPomodoro()
{
    m_pomodoroElapsedSecs = 0;
    m_isPomodoroBreak = false;
    emit pomodoroStateChanged();
}

int BackendManager::pomodoroRemainingSecs() const
{
    int targetSecs = m_isPomodoroBreak ? (qMax(1, m_pomodoroRest) * 60) : (qMax(1, m_pomodoroFocus) * 60);
    return qMax(0, targetSecs - m_pomodoroElapsedSecs);
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

void BackendManager::setGeminiApiKey(const QString &key)
{
    QString trimmed = key.trimmed();
    if (m_geminiApiKey != trimmed) {
        m_geminiApiKey = trimmed;
        emit geminiApiKeyChanged();
        saveSettings();
    }
}

QString BackendManager::maskedGeminiApiKey() const
{
    if (m_geminiApiKey.isEmpty()) return "미설정";
    if (m_geminiApiKey.length() <= 8) return "******";
    return m_geminiApiKey.left(4) + "..." + m_geminiApiKey.right(4);
}

void BackendManager::saveAppSettings(int blockMode, int rstRetry, int pomodoroFocus, int pomodoroRest,
                                     bool toastEnabled, bool autoStart, bool trayOnClose,
                                     const QString &userName, const QString &targetTime,
                                     const QString &toggleShortcut, bool toggleShortcutEnabled)
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

    bool hotkeyChanged = false;
    if (!toggleShortcut.isEmpty() && m_toggleShortcut != toggleShortcut.trimmed()) {
        m_toggleShortcut = toggleShortcut.trimmed();
        emit toggleShortcutChanged();
        hotkeyChanged = true;
    }
    if (m_toggleShortcutEnabled != toggleShortcutEnabled) {
        m_toggleShortcutEnabled = toggleShortcutEnabled;
        emit toggleShortcutEnabledChanged();
        hotkeyChanged = true;
    }
    if (hotkeyChanged) {
        registerGlobalHotkey();
    }

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
    saveWhitelistFile();
}

void BackendManager::setToggleShortcut(const QString &v)
{
    QString trimmed = v.trimmed();
    if (m_toggleShortcut != trimmed) {
        m_toggleShortcut = trimmed;
        emit toggleShortcutChanged();
        saveSettings();
        registerGlobalHotkey();
    }
}

void BackendManager::setToggleShortcutEnabled(bool v)
{
    if (m_toggleShortcutEnabled != v) {
        m_toggleShortcutEnabled = v;
        emit toggleShortcutEnabledChanged();
        saveSettings();
        if (v) registerGlobalHotkey();
        else unregisterGlobalHotkey();
    }
}

void BackendManager::toggleBlockMode()
{
    m_blockMode = (m_blockMode == 0) ? 1 : 0;
    emit blockModeChanged();
    saveSettings();
    saveWhitelistFile();

    QString modeTitle = (m_blockMode == 0) ? "🚨 엄격 모드 (Default-Deny)" : "🔓 유연 모드 (Blacklist)";
    QString modeMsg = (m_blockMode == 0)
        ? "화이트리스트 등록 사이트만 접속 허용됩니다 (미등록 차단 ON)."
        : "미등록 사이트 접속이 허용됩니다 (블랙리스트만 차단 OFF).";

    if (m_toastEnabled) {
        emit trayNotificationRequested(modeTitle, modeMsg);
    }
    qDebug() << "[BackendManager] Block mode toggled to:" << (m_blockMode == 0 ? "Strict" : "Flexible");
}

void BackendManager::registerGlobalHotkey()
{
    unregisterGlobalHotkey();
    if (!m_toggleShortcutEnabled || m_toggleShortcut.trimmed().isEmpty()) return;

#ifdef _WIN32
    QString sc = m_toggleShortcut.trimmed();
    UINT mods = 0;
    if (sc.contains("Ctrl", Qt::CaseInsensitive)) mods |= MOD_CONTROL;
    if (sc.contains("Alt", Qt::CaseInsensitive))  mods |= MOD_ALT;
    if (sc.contains("Shift", Qt::CaseInsensitive)) mods |= MOD_SHIFT;
    if (sc.contains("Win", Qt::CaseInsensitive))   mods |= MOD_WIN;
    mods |= 0x4000; // MOD_NOREPEAT

    QString keyPart = sc;
    int lastPlus = sc.lastIndexOf('+');
    if (lastPlus != -1) keyPart = sc.mid(lastPlus + 1).trimmed();

    UINT vk = 0;
    if (keyPart.startsWith("F", Qt::CaseInsensitive) && keyPart.length() >= 2) {
        bool ok = false;
        int fNum = keyPart.mid(1).toInt(&ok);
        if (ok && fNum >= 1 && fNum <= 24) vk = VK_F1 + (fNum - 1);
    } else if (keyPart.length() == 1) {
        QChar c = keyPart.at(0).toUpper();
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) vk = (UINT)c.unicode();
    } else if (keyPart.compare("Space", Qt::CaseInsensitive) == 0) {
        vk = VK_SPACE;
    } else if (keyPart.compare("Tab", Qt::CaseInsensitive) == 0) {
        vk = VK_TAB;
    } else if (keyPart.compare("Esc", Qt::CaseInsensitive) == 0 || keyPart.compare("Escape", Qt::CaseInsensitive) == 0) {
        vk = VK_ESCAPE;
    }

    if (vk != 0) {
        if (RegisterHotKey(NULL, HOTKEY_TOGGLE_MODE_ID, mods, vk)) {
            qDebug() << "[BackendManager] Windows Global Hotkey registered successfully:" << sc << "mods:" << mods << "vk:" << vk;
        } else {
            qWarning() << "[BackendManager] Failed to register Windows Global Hotkey (key combination may be in use):" << sc;
        }
    }
#endif
}

void BackendManager::unregisterGlobalHotkey()
{
#ifdef _WIN32
    UnregisterHotKey(NULL, HOTKEY_TOGGLE_MODE_ID);
#endif
}

bool BackendManager::testRegisterShortcut(const QString &keySequence)
{
#ifdef _WIN32
    QString sc = keySequence.trimmed();
    if (sc.isEmpty()) return false;
    UINT mods = 0;
    if (sc.contains("Ctrl", Qt::CaseInsensitive)) mods |= MOD_CONTROL;
    if (sc.contains("Alt", Qt::CaseInsensitive))  mods |= MOD_ALT;
    if (sc.contains("Shift", Qt::CaseInsensitive)) mods |= MOD_SHIFT;
    if (sc.contains("Win", Qt::CaseInsensitive))   mods |= MOD_WIN;
    mods |= 0x4000;

    QString keyPart = sc;
    int lastPlus = sc.lastIndexOf('+');
    if (lastPlus != -1) keyPart = sc.mid(lastPlus + 1).trimmed();

    UINT vk = 0;
    if (keyPart.startsWith("F", Qt::CaseInsensitive) && keyPart.length() >= 2) {
        bool ok = false;
        int fNum = keyPart.mid(1).toInt(&ok);
        if (ok && fNum >= 1 && fNum <= 24) vk = VK_F1 + (fNum - 1);
    } else if (keyPart.length() == 1) {
        QChar c = keyPart.at(0).toUpper();
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) vk = (UINT)c.unicode();
    } else if (keyPart.compare("Space", Qt::CaseInsensitive) == 0) {
        vk = VK_SPACE;
    } else if (keyPart.compare("Tab", Qt::CaseInsensitive) == 0) {
        vk = VK_TAB;
    } else if (keyPart.compare("Esc", Qt::CaseInsensitive) == 0 || keyPart.compare("Escape", Qt::CaseInsensitive) == 0) {
        vk = VK_ESCAPE;
    }
    if (vk == 0) return false;

    int testId = 0xBEEE;
    bool success = RegisterHotKey(NULL, testId, mods, vk);
    if (success) {
        UnregisterHotKey(NULL, testId);
        return true;
    }
    return false;
#else
    return true;
#endif
}

void BackendManager::requestAiUserAnalysis()
{
    if (m_isAiAnalyzing) return;

    // 1. 최소 가드레일 조건 검증
    if (!canRequestAiAnalysis()) {
        QString reason = getAiAnalysisBlockReason();
        m_aiUserType = "안내";
        m_aiTypeIcon = "info";
        m_aiTypeColor = "#818CF8";
        m_aiAnalysisReport = reason;
        m_aiActionPlan.clear();
        m_aiActionPlan << "타이머를 시작하고 최소 1분 이상 집중을 진행해보세요."
                       << "유혹 사이트 차단 활동 데이터가 기록되면 AI 정밀 분석이 활성화됩니다.";
        emit aiAnalysisFinished();
        return;
    }

    m_isAiAnalyzing = true;
    emit isAiAnalyzingChanged();

    // 2. 경량 요약본 및 Gemini REST 요청 본문 생성
    QJsonObject digest = generateAiPayloadDigest();
    QJsonObject reqBody = buildGeminiRequestBody(digest);
    QByteArray postData = QJsonDocument(reqBody).toJson(QJsonDocument::Compact);

    // 3. Gemini REST API 엔드포인트 URL (신규 지원 모델: gemini-3.8-flash)
    QString endpoint = QString("https://generativelanguage.googleapis.com/v1beta/models/gemini-3.8-flash:generateContent?key=%1")
                       .arg(m_geminiApiKey);
    QUrl url(endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    qDebug() << "[BackendManager] Requesting Gemini API (gemini-3.8-flash)...";

    // 4. 비동기 HTTP POST 전송 (UI 블로킹 완전 차단)
    QNetworkReply *reply = m_netManager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_isAiAnalyzing = false;
        emit isAiAnalyzingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            QString errStr = reply->errorString();
            QByteArray errBytes = reply->readAll();
            qWarning() << "[BackendManager] Gemini API Network Error:" << errStr << errBytes;

            // API 키가 화면 에러 텍스트에 노출되지 않도록 URL 쿼리 파라미터 마스킹
            QString safeErr = errStr;
            int keyIdx = safeErr.indexOf("key=");
            if (keyIdx != -1) {
                int endIdx = safeErr.indexOf('&', keyIdx);
                if (endIdx == -1) endIdx = safeErr.indexOf(' ', keyIdx);
                if (endIdx == -1) endIdx = safeErr.length();
                safeErr.replace(keyIdx, endIdx - keyIdx, "key=******");
            }

            m_aiUserType = "⚠️ AI 진단 실패";
            m_aiTypeIcon = "alert-circle";
            m_aiTypeColor = "#F43F5E";
            m_aiAnalysisReport = QString("Gemini API 호출 중 네트워크 오류가 발생했습니다: %1 (API 키 상태 및 네트워크 연결을 확인해주세요.)").arg(safeErr);
            m_aiActionPlan.clear();
            m_aiActionPlan << "인터넷 연결이 활성화되어 있는지 확인하세요."
                           << "Gemini API 키 권한이 올바른지 확인하세요."
                           << "잠시 후 [AI 진단 실행]을 다시 시도해주세요.";
            emit aiAnalysisFinished();
            return;
        }

        QByteArray respBytes = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(respBytes);
        QJsonObject root = doc.object();

        QJsonArray candidates = root.value("candidates").toArray();
        if (candidates.isEmpty()) {
            qWarning() << "[BackendManager] Gemini response has no candidates:" << respBytes;
            m_aiUserType = "⚠️ 응답 비어있음";
            m_aiTypeIcon = "alert-circle";
            m_aiTypeColor = "#F43F5E";
            m_aiAnalysisReport = "AI 모델로부터 적절한 응답을 수신하지 못했습니다.";
            emit aiAnalysisFinished();
            return;
        }

        QJsonObject cand0 = candidates[0].toObject();
        QJsonObject contentObj = cand0.value("content").toObject();
        QJsonArray parts = contentObj.value("parts").toArray();
        if (parts.isEmpty()) {
            qWarning() << "[BackendManager] Gemini candidates content parts is empty";
            emit aiAnalysisFinished();
            return;
        }

        QString aiText = parts[0].toObject().value("text").toString().trimmed();

        // 마크다운 코드블록 방어 (```json ... ```)
        if (aiText.startsWith("```json")) {
            aiText = aiText.mid(7);
        } else if (aiText.startsWith("```")) {
            aiText = aiText.mid(3);
        }
        if (aiText.endsWith("```")) {
            aiText.chop(3);
        }
        aiText = aiText.trimmed();

        QJsonDocument aiDoc = QJsonDocument::fromJson(aiText.toUtf8());
        if (!aiDoc.isObject()) {
            qWarning() << "[BackendManager] AI response text is not valid JSON:" << aiText;
            m_aiUserType = "⚠️ 응답 해석 실패";
            m_aiTypeIcon = "alert-circle";
            m_aiTypeColor = "#F43F5E";
            m_aiAnalysisReport = "AI 응답을 JSON 객체로 변환할 수 없습니다: " + aiText.left(120);
            emit aiAnalysisFinished();
            return;
        }

        QJsonObject aiRes = aiDoc.object();
        m_aiUserType = aiRes.value("persona_title").toString("🌱 잠재력 발휘형");
        m_aiTypeIcon = aiRes.value("persona_icon").toString("sparkles");
        m_aiTypeColor = aiRes.value("theme_color").toString("#818CF8");
        m_aiAnalysisReport = aiRes.value("core_insight").toString("오늘의 네트워크 몰입 패턴 심층 분석이 완료되었습니다.");

        m_aiActionPlan.clear();
        QJsonArray plans = aiRes.value("action_plans").toArray();
        for (const auto &p : plans) {
            m_aiActionPlan.append(p.toString());
        }

        // 4단계: AI 추천 차단 도메인 수집 및 정제
        m_aiRecommendedDomains.clear();
        QJsonArray recDomains = aiRes.value("recommended_block_domains").toArray();
        for (const auto &d : recDomains) {
            QString domStr = d.toString().trimmed().toLower();
            domStr.remove("https://").remove("http://").remove("*.");
            if (domStr.startsWith("www.")) domStr = domStr.mid(4);
            int slashIdx = domStr.indexOf('/');
            if (slashIdx != -1) domStr = domStr.left(slashIdx);

            if (!domStr.isEmpty() && !m_blacklistDomains.contains(domStr) && !m_aiRecommendedDomains.contains(domStr)) {
                m_aiRecommendedDomains.append(domStr);
            }
        }

        // 만약 AI가 중복된 도메인을 추천했거나 비어있는 경우, 상위 유혹/연관 도메인 기반 스마트 보조 추천 Fallback
        if (m_aiRecommendedDomains.isEmpty()) {
            QStringList fallbacks;
            QString top = m_topDomain.toLower();
            if (top.contains("youtube") || top.contains("googlevideo") || top.contains("gvt2")) {
                fallbacks << "googlevideo.com" << "ytimg.com" << "beacons.gvt2.com";
            } else if (top.contains("instagram") || top.contains("facebook")) {
                fallbacks << "cdninstagram.com" << "threads.net";
            } else if (top.contains("chzzk") || top.contains("naver")) {
                fallbacks << "live-api.chzzk.naver.com" << "apis.naver.com";
            }
            fallbacks << "tiktok.com" << "twitch.tv" << "netflix.com" << "x.com" << "reddit.com";

            for (const QString &fb : fallbacks) {
                if (!m_blacklistDomains.contains(fb) && !m_aiRecommendedDomains.contains(fb)) {
                    m_aiRecommendedDomains.append(fb);
                    if (m_aiRecommendedDomains.size() >= 2) break;
                }
            }
        }

        if (!m_aiRecommendedDomains.isEmpty()) {
            qDebug() << "[BackendManager] AI recommended block domains:" << m_aiRecommendedDomains.join(", ");
        }
        emit aiRecommendedDomainsChanged();

        // 졸업작품 시연 및 즉각적인 피드백을 위해 쿨다운 대기 시간 제거 (0초 즉시 재실행 가능)
        m_aiCooldownSecs = 0;
        emit aiCooldownSecsChanged();

        qDebug() << "[BackendManager] Gemini AI Analysis Success:" << m_aiUserType;
        emit aiAnalysisFinished();
    });
}

bool BackendManager::canRequestAiAnalysis() const
{
    if (m_geminiApiKey.isEmpty()) return false;
    // 최소 1분 이상 집중했거나 차단 시도 횟수가 1회 이상이어야 분석 가치 있음
    if (m_elapsedSecs < 60 && m_blockedAttempts == 0) return false;
    return true;
}

QString BackendManager::getAiAnalysisBlockReason() const
{
    if (m_geminiApiKey.isEmpty()) {
        return "Gemini API 키가 설정되지 않았습니다. 설정 페이지에서 API 키를 등록해주세요.";
    }
    if (m_elapsedSecs < 60 && m_blockedAttempts == 0) {
        return "최소 1분 이상 집중하거나 유혹 차단 활동 데이터가 수집된 후 정밀 진단이 가능합니다.";
    }
    return QString();
}

QJsonObject BackendManager::generateAiPayloadDigest() const
{
    QJsonObject digest;

    // 1. 세션 메트릭 요약
    QJsonObject sessionMetrics;
    sessionMetrics["elapsed_minutes"] = m_elapsedSecs / 60;

    // 목표 시간(HH:mm:ss) 파싱 -> 분 단위 환산
    int targetMins = 0;
    QStringList tParts = m_targetTime.split(":");
    if (tParts.size() == 3) {
        targetMins = tParts[0].toInt() * 60 + tParts[1].toInt();
    }
    sessionMetrics["target_minutes"] = targetMins;
    sessionMetrics["focus_score"] = m_focusScore;
    sessionMetrics["is_paused"] = m_isPaused;
    sessionMetrics["total_paused_minutes"] = m_totalPausedSecs / 60;
    sessionMetrics["goal_completion_rate"] = targetMins > 0 ? qMin(100.0, ((double)(m_elapsedSecs / 60) / targetMins) * 100.0) : 0.0;
    digest["session_metrics"] = sessionMetrics;

    // 2. 유혹 및 차단 패턴 요약 (의도 세션 및 백그라운드 재시도 버스트 맥락 정제)
    QJsonObject blockingSummary;
    blockingSummary["total_raw_blocked_packets"] = m_blockedAttempts;
    int estSessions = qMax(1, m_blockedSessions);
    blockingSummary["estimated_user_sessions"] = estSessions;

    // 패킷 수 대비 세션 수 차이가 크면(예: 세션당 3개 이상 또는 총 10개 이상) 백그라운드 탭 방치 버스트로 판단
    bool isBurst = (m_blockedAttempts >= 5 && (m_blockedAttempts >= estSessions * 2));
    blockingSummary["is_idle_retry_burst_detected"] = isBurst;
    if (isBurst) {
        blockingSummary["retry_burst_note"] = QString("사용자가 직접 %1회 접속 시도한 것이 아니라, 브라우저 탭/앱이 켜져 있어 백그라운드 재연결(Keep-Alive/Beacon)이 %1회 자동 발생한 패턴입니다.")
                                              .arg(m_blockedAttempts);
    }

    // 첫 이탈/차단 발생 시점 (분)
    int firstTemptationMin = -1;
    if (m_firstBlockTime > m_startTime && m_startTime > 0) {
        firstTemptationMin = (int)((m_firstBlockTime - m_startTime) / 60);
    }
    blockingSummary["first_temptation_elapsed_minute"] = firstTemptationMin;

    // 상위 차단 도메인 (최대 5개) 및 카테고리 집계
    QJsonArray topDomainsArray;
    int videoCount = 0, snsCount = 0, msgCount = 0, gameCount = 0, etcCount = 0;

    int domainLimit = qMin(5, m_topBlockedDomains.size());
    for (int i = 0; i < domainLimit; ++i) {
        QVariantMap m = m_topBlockedDomains[i].toMap();
        QString name = m.value("name").toString().toLower();
        int cnt = m.value("count").toInt();

        QJsonObject dObj;
        dObj["domain"] = name;
        dObj["count"] = cnt;

        QString cat = "other";
        if (name.contains("youtube") || name.contains("googlevideo") || name.contains("tiktok") || name.contains("twitch") || name.contains("netflix") || name.contains("chzzk") || name.contains("gvt2.com")) {
            cat = "video/streaming";
            videoCount += cnt;
        } else if (name.contains("instagram") || name.contains("facebook") || name.contains("twitter") || name.contains("x.com")) {
            cat = "sns";
            snsCount += cnt;
        } else if (name.contains("discord") || name.contains("kakao") || name.contains("telegram")) {
            cat = "messenger";
            msgCount += cnt;
        } else if (name.contains("steam") || name.contains("nexon") || name.contains("riot") || name.contains("blizzard") || name.contains("league")) {
            cat = "game";
            gameCount += cnt;
        } else {
            etcCount += cnt;
        }
        dObj["category"] = cat;
        topDomainsArray.append(dObj);
    }
    blockingSummary["top_blocked_domains"] = topDomainsArray;

    QJsonObject catDist;
    catDist["video"] = videoCount;
    catDist["sns"] = snsCount;
    catDist["messenger"] = msgCount;
    catDist["game"] = gameCount;
    catDist["other"] = etcCount;
    blockingSummary["category_distribution"] = catDist;
    digest["blocking_summary"] = blockingSummary;

    // 3. 시간대별 취약 구간 (타임라인 상위 2개 시간대)
    QJsonArray peakHours;
    QList<QPair<int, int>> hourCounts; // count, hour
    for (int h = 0; h < m_timelineData.size() && h < 24; ++h) {
        int cnt = m_timelineData[h].toInt();
        if (cnt > 0) hourCounts.append(qMakePair(cnt, h));
    }
    std::sort(hourCounts.begin(), hourCounts.end(), [](const QPair<int, int> &a, const QPair<int, int> &b) {
        return a.first > b.first;
    });
    for (int i = 0; i < qMin(2, hourCounts.size()); ++i) {
        peakHours.append(QString("%1시 (%2회 차단)").arg(hourCounts[i].second).arg(hourCounts[i].first));
    }
    digest["temporal_peak_hours"] = peakHours;

    // 4. 모드 및 뽀모도로 설정 맥락
    QJsonObject configContext;
    configContext["block_mode"] = m_blockMode == 0 ? "Strict (Default-Deny)" : "Flexible (Blacklist)";
    configContext["pomodoro_focus_min"] = m_pomodoroFocus;
    configContext["pomodoro_rest_min"] = m_pomodoroRest;
    digest["context"] = configContext;

    // 5. 현재 활성 블랙리스트 목록
    QJsonArray blArray;
    for (const auto &b : m_blacklistDomains) {
        blArray.append(b.toString());
    }
    digest["current_blacklist"] = blArray;

    return digest;
}

QString BackendManager::getAiPayloadDigestJson() const
{
    return QString::fromUtf8(QJsonDocument(generateAiPayloadDigest()).toJson(QJsonDocument::Indented));
}

QJsonObject BackendManager::buildGeminiRequestBody(const QJsonObject &digest) const
{
    QString digestStr = QString::fromUtf8(QJsonDocument(digest).toJson(QJsonDocument::Compact));

    QString prompt = QString(
        "당신은 FocusGuard(네트워크 패킷 기반 집중력 수호 데스크톱 앱)의 전문 '디지털 몰입 인지행동 코치 AI'입니다.\n\n"
        "아래는 사용자가 오늘 진행한 집중 세션의 로컬 요약 통계(JSON Digest)입니다:\n"
        "%1\n\n"
        "[중요: 네트워크 특성 및 딴짓 세션 해석 지침]\n"
        "1. 'total_raw_blocked_packets'는 사용자가 직접 누른 횟수가 아니라, 브라우저의 TCP RST 재연결 루프와 백그라운드 추적 비콘(beacons*.gvt2.com 등)으로 인해 급격히 부풀려질 수 있습니다.\n"
        "2. 실제 사용자가 딴짓을 시도한 횟수는 'estimated_user_sessions'를 기준으로 파악하세요.\n"
        "3. 만약 'is_idle_retry_burst_detected'가 true라면, 사용자가 반복해서 딴짓을 시도한 것이 아니라 탭이나 앱을 닫지 않고 화면 뒤에 켜둔 채 방치한 상태입니다.\n"
        "4. 따라서 사용자를 무리하게 반복적인 도파민 중독자로 비난하지 마시고, '백그라운드에 열려 있는 탭/앱을 작업표시줄이나 브라우저 탭 닫기로 완전히 종료할 것'을 실질적인 처방 중 하나로 안내하세요.\n\n"
        "[분석 다양성 및 심층 진단 지침]\n"
        "- 예시 명칭을 그대로 복사하지 마세요. 실제 차단된 최상위 도메인(유튜브, 치지직, 인스타, 네이버, 게임 등)과 취약 시간대, 집중 경과 시간을 바탕으로 매번 참신하고 개성 있는 페르소나 명칭을 자유롭게 창작하세요.\n"
        "- 진단 결과가 매번 똑같이 나오지 않도록, 사용자의 세션 데이터(집중 지속력, 이탈 사이트의 성격, 시간대별 패턴)에 집중하여 다양한 인지 심리적 관점(주의력 피로, 작업 도피성 검색, 알고리즘 도파민 보상, 루틴 불균형 등)에서 날카롭고 신선한 리포트를 작성하세요.\n"
        "- 실천 계획(action_plans)은 단순한 '탭 닫기' 외에도 카테고리별 맞춤 처방(환경 통제, 인지적 파킹랏 기법, 뽀모도로 인터벌 재설계, 휴식 루틴 등)을 3가지 서로 다른 관점으로 다채롭게 제시하세요.\n\n"
        "반드시 아래 JSON 스키마를 엄격히 준수하여 순수 JSON 객체로만 응답하세요:\n"
        "{\n"
        "  \"persona_title\": \"실제 차단 데이터와 세션 특성을 반영한 참신하고 위트 있는 페르소나 명칭 (예: '🎬 숏폼 알고리즘 포획형', '🔍 지식 탐색형 딴짓러', '⏳ 마감 직전 초집중 러너', '🎮 심야 보상 탐색형' 등 맥락에 맞게 자유 작명)\",\n"
        "  \"persona_icon\": \"zap, shield-check, target, cpu, refresh, sparkles 중 가장 어울리는 영문 아이콘명 1개\",\n"
        "  \"theme_color\": \"#F43F5E (주의/도파민), #10B981 (안정/우수), #818CF8 (보통/성장), #A855F7 (탈출/게임), #38BDF8 (소통/SNS) 중 1개 색상 헥스코드\",\n"
        "  \"core_insight\": \"왜 이 시간대에 이 사이트로 이탈하는지, 사용자의 세션 패턴과 심리적 원인을 2~3문장으로 날카롭게 짚어주는 심층 진단 리포트\",\n"
        "  \"action_plans\": [\n"
        "    \"오늘 당장 실천할 수 있는 구체적이고 현실적인 환경 제어 처방\",\n"
        "    \"집중 유지 및 이탈 충동을 다스리는 인지 심리 기법 처방\",\n"
        "    \"세션 지속성 및 회복력을 높여주는 루틴 처방\"\n"
        "  ],\n"
        "  \"recommended_block_domains\": [\n"
        "    \"현재 차단 목록(current_blacklist)에 아직 없고, 함께 차단하면 좋은 연관 서브도메인, CDN, 비콘 또는 유사 유혹 사이트 (반드시 2~3개 추천, 소문자 도메인만, 예: googlevideo.com, ytimg.com, gvt2.com, tiktok.com, x.com)\"\n"
        "  ]\n"
        "}"
    ).arg(digestStr);

    QJsonObject root;
    QJsonArray contents;
    QJsonObject contentObj;
    contentObj["role"] = "user";

    QJsonArray parts;
    QJsonObject partObj;
    partObj["text"] = prompt;
    parts.append(partObj);

    contentObj["parts"] = parts;
    contents.append(contentObj);
    root["contents"] = contents;

    QJsonObject genConfig;
    genConfig["responseMimeType"] = "application/json";
    genConfig["temperature"] = 0.85;
    genConfig["topP"] = 0.95;
    genConfig["topK"] = 40;
    root["generationConfig"] = genConfig;

    return root;
}

QString BackendManager::getGeminiRequestBodyJson() const
{
    return QString::fromUtf8(QJsonDocument(buildGeminiRequestBody(generateAiPayloadDigest())).toJson(QJsonDocument::Indented));
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
        m_engineLogs.append(QString("[%1] [System] 엔진 정상 종료 시그널 전달 중...").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        emit engineLogsChanged();
        m_engineProcess->write("q\n");
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

// ─────────────────────────────────────────────────────────────────────────────
//  장기 데이터 누적 보관 엔진 (history_daily.json)
// ─────────────────────────────────────────────────────────────────────────────

void BackendManager::ensureTodayRecord()
{
    QString curDate = QDate::currentDate().toString("yyyy-MM-dd");
    if (m_todayDateStr != curDate) {
        m_todayDateStr = curDate;
        if (m_dailyHistory.contains(curDate)) {
            const auto &rec = m_dailyHistory[curDate];
            m_sessionStartFocusSecs       = rec.focusSecs;
            m_sessionStartBlockedAttempts = rec.blockedAttempts;
            m_sessionStartBlockedSessions = rec.blockedSessions;
            m_sessionStartPomodoroRounds  = rec.pomodoroRounds;
        } else {
            DailyRecord newRec;
            m_dailyHistory[curDate] = newRec;
            m_sessionStartFocusSecs       = 0;
            m_sessionStartBlockedAttempts = 0;
            m_sessionStartBlockedSessions = 0;
            m_sessionStartPomodoroRounds  = 0;
        }
    }
}

void BackendManager::loadHistory()
{
    m_dailyHistory.clear();
    std::ifstream file(m_historyFilePath.toStdString());
    if (!file.is_open()) {
        qDebug() << "[BackendManager] No existing history_daily.json found. Initializing fresh record.";
        ensureTodayRecord();
        refreshWeeklyMinutesFromHistory();
        return;
    }

    try {
        json j;
        file >> j;
        if (j.contains("days") && j["days"].is_object()) {
            for (auto &item : j["days"].items()) {
                QString dateStr = QString::fromStdString(item.key());
                const auto &val = item.value();
                DailyRecord rec;
                rec.focusSecs       = val.value("focusSecs", 0);
                rec.blockedAttempts = val.value("blockedAttempts", 0);
                rec.blockedSessions = val.value("blockedSessions", 0);
                rec.pomodoroRounds  = val.value("pomodoroRounds", 0);

                if (val.contains("hourlyFocusSecs") && val["hourlyFocusSecs"].is_array()) {
                    int idx = 0;
                    for (auto &h : val["hourlyFocusSecs"]) {
                        if (idx < 24) rec.hourlyFocusSecs[idx] = h.get<int>();
                        idx++;
                    }
                }
                if (val.contains("hourlyBlocks") && val["hourlyBlocks"].is_array()) {
                    int idx = 0;
                    for (auto &h : val["hourlyBlocks"]) {
                        if (idx < 24) rec.hourlyBlocks[idx] = h.get<int>();
                        idx++;
                    }
                }
                if (val.contains("topBlockedDomains") && val["topBlockedDomains"].is_object()) {
                    for (auto &dItem : val["topBlockedDomains"].items()) {
                        rec.topBlockedDomains[QString::fromStdString(dItem.key())] = dItem.value().get<int>();
                    }
                }
                m_dailyHistory[dateStr] = rec;
            }
        }
        ensureTodayRecord();
        refreshWeeklyMinutesFromHistory();
        emit historyDataChanged();
        qDebug() << "[BackendManager] Loaded history with" << m_dailyHistory.size() << "days of records.";
    } catch (const std::exception &e) {
        qWarning() << "[BackendManager] Failed to parse history_daily.json:" << e.what();
        file.close();
        QFile::copy(m_historyFilePath, m_historyFilePath + ".corrupted");
        ensureTodayRecord();
        refreshWeeklyMinutesFromHistory();
    }
}

void BackendManager::updateTodayRecordFromSession()
{
    ensureTodayRecord();
    if (m_todayDateStr.isEmpty()) return;

    auto &rec = m_dailyHistory[m_todayDateStr];
    rec.focusSecs       = m_sessionStartFocusSecs + m_elapsedSecs;
    rec.blockedAttempts = m_sessionStartBlockedAttempts + m_blockedAttempts;
    rec.blockedSessions = m_sessionStartBlockedSessions + m_blockedSessions;
    rec.pomodoroRounds  = qMax(rec.pomodoroRounds, m_sessionStartPomodoroRounds + m_pomodoroRound);

    int curHour = QTime::currentTime().hour();
    if (curHour >= 0 && curHour < 24) {
        rec.hourlyFocusSecs[curHour] = qMax(rec.hourlyFocusSecs[curHour], m_elapsedSecs % 3600);
        rec.hourlyBlocks[curHour]    = qMax(rec.hourlyBlocks[curHour], m_blockedSessions);
    }

    if (m_topDomain != "없음" && !m_topDomain.isEmpty() && m_topDomainCount > 0) {
        rec.topBlockedDomains[m_topDomain] = qMax(rec.topBlockedDomains.value(m_topDomain, 0), m_topDomainCount);
    }

    for (const auto &item : m_topBlockedDomains) {
        QVariantMap map = item.toMap();
        QString domain = map.value("domain").toString();
        int count = map.value("count").toInt();
        if (!domain.isEmpty() && count > 0) {
            rec.topBlockedDomains[domain] = qMax(rec.topBlockedDomains.value(domain, 0), count);
        }
    }
}

void BackendManager::saveHistory()
{
    updateTodayRecordFromSession();

    json rootJ;
    rootJ["version"] = 1;
    rootJ["lastUpdated"] = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();

    long long totalSecs = 0;
    json daysJ = json::object();
    for (auto it = m_dailyHistory.begin(); it != m_dailyHistory.end(); ++it) {
        const QString &dStr = it.key();
        const DailyRecord &rec = it.value();
        totalSecs += rec.focusSecs;

        json dObj;
        dObj["focusSecs"]       = rec.focusSecs;
        dObj["blockedAttempts"] = rec.blockedAttempts;
        dObj["blockedSessions"] = rec.blockedSessions;
        dObj["pomodoroRounds"]  = rec.pomodoroRounds;

        json hFocus = json::array();
        for (int v : rec.hourlyFocusSecs) hFocus.push_back(v);
        dObj["hourlyFocusSecs"] = hFocus;

        json hBlocks = json::array();
        for (int v : rec.hourlyBlocks) hBlocks.push_back(v);
        dObj["hourlyBlocks"] = hBlocks;

        json topD = json::object();
        for (auto dit = rec.topBlockedDomains.begin(); dit != rec.topBlockedDomains.end(); ++dit) {
            topD[dit.key().toStdString()] = dit.value();
        }
        dObj["topBlockedDomains"] = topD;

        daysJ[dStr.toStdString()] = dObj;
    }

    rootJ["totalAccumulatedSecs"] = totalSecs;
    rootJ["days"] = daysJ;

    QSaveFile sFile(m_historyFilePath);
    if (sFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::string dumped = rootJ.dump(2);
        sFile.write(dumped.data(), (qint64)dumped.size());
        if (sFile.commit()) {
            refreshWeeklyMinutesFromHistory();
            emit historyDataChanged();
        } else {
            qWarning() << "[BackendManager] Failed to commit history_daily.json save file.";
        }
    } else {
        qWarning() << "[BackendManager] Cannot open QSaveFile for history_daily.json.";
    }
}

void BackendManager::refreshWeeklyMinutesFromHistory()
{
    QDate today = QDate::currentDate();
    int dayOfWeek = today.dayOfWeek(); // 1 = Monday, 7 = Sunday
    QDate monday = today.addDays(-(dayOfWeek - 1));

    while (m_weeklyMinutes.size() < 7) {
        m_weeklyMinutes.append(0);
    }

    bool changed = false;
    for (int i = 0; i < 7; ++i) {
        QDate d = monday.addDays(i);
        QString ds = d.toString("yyyy-MM-dd");
        int mins = 0;
        if (m_dailyHistory.contains(ds)) {
            mins = m_dailyHistory[ds].focusSecs / 60;
        }
        if (m_weeklyMinutes[i].toInt() != mins) {
            m_weeklyMinutes[i] = mins;
            changed = true;
        }
    }

    if (changed) {
        emit weeklyMinutesChanged();
    }
}

int BackendManager::totalAccumulatedMinutes() const
{
    long long totalSecs = 0;
    for (const auto &d : m_dailyHistory) {
        totalSecs += d.focusSecs;
    }
    return (int)(totalSecs / 60);
}

int BackendManager::totalAccumulatedSessions() const
{
    int total = 0;
    for (const auto &d : m_dailyHistory) {
        total += d.blockedSessions;
    }
    return total;
}

int BackendManager::totalAccumulatedRounds() const
{
    int total = 0;
    for (const auto &d : m_dailyHistory) {
        total += d.pomodoroRounds;
    }
    return total;
}

QVariantMap BackendManager::getDailyStats(const QString &dateStr)
{
    QVariantMap res;
    if (m_dailyHistory.contains(dateStr)) {
        const auto &rec = m_dailyHistory[dateStr];
        res["focusSecs"]       = rec.focusSecs;
        res["focusMinutes"]    = rec.focusSecs / 60;
        res["blockedAttempts"] = rec.blockedAttempts;
        res["blockedSessions"] = rec.blockedSessions;
        res["pomodoroRounds"]  = rec.pomodoroRounds;

        QVariantList hFocus;
        for (int v : rec.hourlyFocusSecs) hFocus.append(v);
        res["hourlyFocusSecs"] = hFocus;

        QVariantList hBlocks;
        for (int v : rec.hourlyBlocks) hBlocks.append(v);
        res["hourlyBlocks"] = hBlocks;

        QVariantMap topD;
        for (auto it = rec.topBlockedDomains.begin(); it != rec.topBlockedDomains.end(); ++it) {
            topD[it.key()] = it.value();
        }
        res["topBlockedDomains"] = topD;
    }
    return res;
}

QVariantList BackendManager::getWeeklyMinutesFromHistory()
{
    refreshWeeklyMinutesFromHistory();
    return m_weeklyMinutes;
}

QVariantList BackendManager::getTrendRange(int daysCount)
{
    updateTodayRecordFromSession();
    int count = qBound(1, daysCount, 90);
    QDate today = QDate::currentDate();
    QVariantList res;

    for (int i = count - 1; i >= 0; --i) {
        QDate d = today.addDays(-i);
        QString ds = d.toString("yyyy-MM-dd");
        QVariantMap item;
        item["fullDate"] = ds;
        item["date"] = d.toString("MM/dd");

        QString wName = "월";
        switch (d.dayOfWeek()) {
        case 1: wName = "월"; break;
        case 2: wName = "화"; break;
        case 3: wName = "수"; break;
        case 4: wName = "목"; break;
        case 5: wName = "금"; break;
        case 6: wName = "토"; break;
        case 7: wName = "일"; break;
        }
        item["weekday"] = wName;

        int mins = 0;
        int blocks = 0;
        int rounds = 0;
        if (m_dailyHistory.contains(ds)) {
            const auto &rec = m_dailyHistory[ds];
            mins = rec.focusSecs / 60;
            blocks = rec.blockedSessions;
            rounds = rec.pomodoroRounds;
        }
        item["minutes"] = mins;
        item["blocks"]  = blocks;
        item["rounds"]  = rounds;

        res.append(item);
    }
    return res;
}

QVariantList BackendManager::getTopBlockedDomainsHistorical(int daysCount, int limit)
{
    updateTodayRecordFromSession();
    int count = qBound(1, daysCount, 90);
    QDate today = QDate::currentDate();
    QMap<QString, int> agg;

    for (int i = 0; i < count; ++i) {
        QString ds = today.addDays(-i).toString("yyyy-MM-dd");
        if (m_dailyHistory.contains(ds)) {
            const auto &rec = m_dailyHistory[ds];
            for (auto it = rec.topBlockedDomains.begin(); it != rec.topBlockedDomains.end(); ++it) {
                agg[it.key()] += it.value();
            }
        }
    }

    // 카운트 기준 내림차순 정렬
    QList<QPair<int, QString>> sortedList;
    int grandTotal = 0;
    for (auto it = agg.begin(); it != agg.end(); ++it) {
        if (it.value() > 0) {
            sortedList.append(qMakePair(it.value(), it.key()));
            grandTotal += it.value();
        }
    }

    std::sort(sortedList.begin(), sortedList.end(), [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
        return a.first > b.first;
    });

    QStringList palette = { "#F43F5E", "#8B5CF6", "#3B82F6", "#10B981", "#F59E0B" };
    QVariantList res;
    int takeCount = qMin((int)sortedList.size(), qMax(1, limit));
    for (int i = 0; i < takeCount; ++i) {
        QVariantMap item;
        item["name"]    = sortedList[i].second;
        item["count"]   = sortedList[i].first;
        item["percent"] = (grandTotal > 0) ? (sortedList[i].first * 100 / grandTotal) : 0;
        item["color"]   = palette[i % palette.size()];
        res.append(item);
    }
    return res;
}

QVariantList BackendManager::getWeekdayAverages()
{
    updateTodayRecordFromSession();
    int totalMins[7] = {0};
    int totalBlks[7] = {0};
    int dayCounts[7] = {0};

    for (auto it = m_dailyHistory.begin(); it != m_dailyHistory.end(); ++it) {
        QDate d = QDate::fromString(it.key(), "yyyy-MM-dd");
        if (d.isValid()) {
            int dIdx = d.dayOfWeek() - 1; // 0..6
            if (dIdx >= 0 && dIdx < 7) {
                totalMins[dIdx] += it.value().focusSecs / 60;
                totalBlks[dIdx] += it.value().blockedSessions;
                dayCounts[dIdx]++;
            }
        }
    }

    QStringList wNames = { "월", "화", "수", "목", "금", "토", "일" };
    QVariantList res;
    for (int i = 0; i < 7; ++i) {
        QVariantMap item;
        item["dayName"]    = wNames[i];
        item["avgMinutes"] = (dayCounts[i] > 0) ? (totalMins[i] / dayCounts[i]) : 0;
        item["totalBlocks"]= totalBlks[i];
        item["sampleDays"] = dayCounts[i];
        res.append(item);
    }
    return res;
}

int BackendManager::getHeatValue(int dayIdx, int hourIdx)
{
    if (dayIdx < 0 || dayIdx >= 7 || hourIdx < 0 || hourIdx >= 24) return 0;
    QDate today = QDate::currentDate();
    int todayDayOfWeek = today.dayOfWeek(); // 1..7 (1=Mon)
    int todayIdx = todayDayOfWeek - 1;       // 0..6

    // 미래 요일은 0 반환
    if (dayIdx > todayIdx) return 0;

    QDate targetDate = today.addDays(dayIdx - todayIdx);
    QString ds = targetDate.toString("yyyy-MM-dd");

    if (m_dailyHistory.contains(ds)) {
        const auto &rec = m_dailyHistory[ds];
        int secs = rec.hourlyFocusSecs[hourIdx];

        // 오늘 현재 세션의 시간대별 집중 시간 실시간 합산
        if (dayIdx == todayIdx && hourIdx == QTime::currentTime().hour()) {
            secs = qMax(secs, m_elapsedSecs % 3600);
        }

        if (secs <= 0)   return 0;
        if (secs < 900)  return 1; // 15분 미만
        if (secs < 1800) return 2; // 30분 미만
        if (secs < 2700) return 3; // 45분 미만
        return 4;                  // 45분 이상
    }
    return 0;
}

QString BackendManager::getMostProductiveWeekday()
{
    QVariantList avgs = getWeekdayAverages();
    int bestIdx = -1;
    int bestVal = 0;
    for (int i = 0; i < avgs.size(); ++i) {
        int m = avgs[i].toMap().value("avgMinutes").toInt();
        if (m > bestVal) {
            bestVal = m;
            bestIdx = i;
        }
    }
    if (bestIdx >= 0 && bestVal > 0) {
        QStringList wNames = { "월요일", "화요일", "수요일", "목요일", "금요일", "토요일", "일요일" };
        int h = bestVal / 60;
        int min = bestVal % 60;
        return QString("%1 (평균 %2시간 %3분)").arg(wNames[bestIdx]).arg(h).arg(min);
    }
    return "데이터 수집 중";
}

QString BackendManager::getMostDistractedWeekday()
{
    QVariantList avgs = getWeekdayAverages();
    int worstIdx = -1;
    int worstVal = 0;
    for (int i = 0; i < avgs.size(); ++i) {
        int b = avgs[i].toMap().value("totalBlocks").toInt();
        if (b > worstVal) {
            worstVal = b;
            worstIdx = i;
        }
    }
    if (worstIdx >= 0 && worstVal > 0) {
        QStringList wNames = { "월요일", "화요일", "수요일", "목요일", "금요일", "토요일", "일요일" };
        return QString("%1 (총 %2회 차단)").arg(wNames[worstIdx]).arg(worstVal);
    }
    return "데이터 수집 중";
}

void BackendManager::flushHistory()
{
    saveHistory();
}

