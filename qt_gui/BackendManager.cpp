#include "BackendManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

// =============================================================================
//  경로 탐색: 실행 파일 위치에서 상위 폴더를 타고 올라가 FocusGuard 루트 반환
// =============================================================================
QString BackendManager::resolveWorkspacePath() const
{
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {   // 최대 8단계 상위 탐색
        if (d.exists("core_engine")) {
            return d.absolutePath();
        }
        if (!d.cdUp()) break;
    }
    // 못 찾으면 fallback (개발 머신 기본 경로)
    return "C:/Users/SHIN/Desktop/FG-Work-Space/FocusGuard";
}

// =============================================================================
//  생성자 및 소멸자
// =============================================================================
BackendManager::BackendManager(QObject *parent)
    : QObject(parent)
{
    // 워크스페이스 루트 자동 탐색
    QString ws = resolveWorkspacePath();
    m_statsFilePath     = ws + "/core_engine/FocusGuard_stats.json";
    m_whitelistFilePath = ws + "/core_engine/whitelist.json";
    m_pauseFlagPath     = ws + "/core_engine/pause.flag";
    m_settingsFilePath  = ws + "/qt_gui/user_settings.json";
    m_historyFilePath   = ws + "/qt_gui/history_daily.json";

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

    // 코어 감시 엔진(FocusGuard.exe) 자동 기동 (UAC 관리자 권한 상속)
    QTimer::singleShot(500, this, [this]() {
        startEngine();
    });
}

BackendManager::~BackendManager()
{
    blockSignals(true);
    saveHistory();
    stopEngine();
}

// =============================================================================
//  통계 JSON 폴링 (1초마다 호출) 및 뽀모도로 루프
// =============================================================================
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
                        emit trayNotificationRequested(
                            "🍅 뽀모도로 집중 완료!",
                            QString("%1회차 집중을 완수했습니다! %2분간 달콤한 휴식을 취하세요.")
                                .arg(m_pomodoroRound).arg(m_pomodoroRest)
                        );
                    }
                } else {
                    emit pomodoroStateChanged();
                }
            } else {
                // [휴식 진행 모드]
                m_isPomodoroBreak = true;
                m_pomodoroElapsedSecs++;
                int targetRestSecs = qMax(1, m_pomodoroRest) * 60;
                if (m_pomodoroElapsedSecs >= targetRestSecs) {
                    // 휴식 시간 만료 -> 자동으로 집중 모드로 복귀
                    m_pomodoroElapsedSecs = 0;
                    m_isPomodoroBreak = false;
                    emit pomodoroStateChanged();

                    setIsPaused(false);

                    if (m_toastEnabled) {
                        emit trayNotificationRequested(
                            "⚡ 휴식 종료, 다시 집중!",
                            QString("휴식이 종료되었습니다. %1회차 집중을 다시 시작합니다.")
                                .arg(m_pomodoroRound + 1)
                        );
                    }
                } else {
                    emit pomodoroStateChanged();
                }
            }
        }
    }

    // ── 외부(코어 엔진 콘솔 등)에서 whitelist.json 변경 시 GUI 동기화 ──
    static QDateTime lastWlModified;
    QFileInfo wlInfo(m_whitelistFilePath);
    if (wlInfo.exists()) {
        QDateTime modTime = wlInfo.lastModified();
        if (lastWlModified.isValid() && modTime > lastWlModified) {
            loadWhitelistFile();
        }
        lastWlModified = modTime;
    }

    std::ifstream file(m_statsFilePath.toStdString());
    if (!file.is_open()) return;

    // 빈 파일 가드
    file.seekg(0, std::ios::end);
    if (file.tellg() == 0) return;
    file.seekg(0, std::ios::beg);

    try {
        json j;
        file >> j;

        // 1. start_time
        if (j.contains("start_time")) {
            m_startTime = j["start_time"].get<time_t>();
            if (m_startTime > 0) {
                time_t now = time(nullptr);
                m_elapsedSecs = (int)difftime(now, m_startTime);
                if (m_elapsedSecs < 0) m_elapsedSecs = 0;

                int h = m_elapsedSecs / 3600;
                int m = (m_elapsedSecs % 3600) / 60;
                int s = m_elapsedSecs % 60;
                m_focusTime = QString("%1:%2:%3")
                    .arg(h, 2, 10, QChar('0'))
                    .arg(m, 2, 10, QChar('0'))
                    .arg(s, 2, 10, QChar('0'));
                emit elapsedSecsChanged();
                emit focusTimeChanged();
            }
        }

        // 2. focus_score_percent
        if (j.contains("focus_score_percent")) {
            m_focusScore = j["focus_score_percent"].get<double>();
            emit focusScoreChanged();
        }

        // 3. blocked_attempts (패킷 단위 차단 누적 카운트)
        if (j.contains("blocked_attempts")) {
            m_blockedAttempts = j["blocked_attempts"].get<int>();
            emit blockedAttemptsChanged();
        }

        // 4. blocked_sessions (의도된 딴짓 시도 세션 수)
        if (j.contains("blocked_sessions")) {
            m_blockedSessions = j["blocked_sessions"].get<int>();
            emit blockedSessionsChanged();
        } else {
            // 엔진 하위호환: 필드가 없으면 blocked_attempts를 기본값으로 사용
            m_blockedSessions = m_blockedAttempts;
            emit blockedSessionsChanged();
        }

        // 5. first_block_time & blockedSecs
        if (j.contains("first_block_time")) {
            m_firstBlockTime = j["first_block_time"].get<time_t>();
            if (m_firstBlockTime > 0) {
                time_t now = time(nullptr);
                m_blockedSecs = (int)difftime(now, m_firstBlockTime);
                if (m_blockedSecs < 0) m_blockedSecs = 0;

                int h = m_blockedSecs / 3600;
                int m = (m_blockedSecs % 3600) / 60;
                int s = m_blockedSecs % 60;
                m_blockedTime = QString("%1:%2:%3")
                    .arg(h, 2, 10, QChar('0'))
                    .arg(m, 2, 10, QChar('0'))
                    .arg(s, 2, 10, QChar('0'));
                emit blockedSecsChanged();
                emit blockedTimeChanged();
            }
        }

        // 6. hourly_blocks
        if (j.contains("hourly_blocks") && j["hourly_blocks"].is_array()) {
            QVariantList timeline;
            for (auto &el : j["hourly_blocks"])
                timeline.append(el.get<int>());
            while (timeline.size() < 24)
                timeline.append(0);
            if (m_timelineData != timeline) {
                m_timelineData = timeline;
                emit timelineDataChanged();
            }
        }

        // 7. recent_blocks
        if (j.contains("recent_blocks") && j["recent_blocks"].is_array()) {
            QVariantList logs;
            for (auto &el : j["recent_blocks"]) {
                QVariantMap item;
                std::string timeStr = el.value("time", "");
                if (timeStr.size() >= 19)
                    timeStr = timeStr.substr(11, 8); // "HH:MM:SS"만 추출
                item["time"] = QString::fromStdString(timeStr);
                item["desc"] = QString::fromStdString(el.value("desc", ""));
                item["type"] = QString::fromStdString(el.value("type", ""));
                logs.append(item);
            }
            if (m_recentLogs != logs) {
                m_recentLogs = logs;
                emit recentLogsChanged();
            }
        }

        // 8. top_blocked_domains
        if (j.contains("top_blocked_domains") && j["top_blocked_domains"].is_array()) {
            QVariantList topList;
            QString topDom = "없음";
            int topCnt = 0;
            int totalTopCnt = 0;

            for (auto &el : j["top_blocked_domains"]) {
                int c = el.value("count", 0);
                totalTopCnt += c;
            }

            QStringList palette = { "#F43F5E", "#8B5CF6", "#3B82F6", "#10B981", "#F59E0B" };
            int pIdx = 0;

            for (auto &el : j["top_blocked_domains"]) {
                std::string domain = el.value("domain", "");
                int count = el.value("count", 0);
                if (pIdx == 0 && count > 0) {
                    topDom = QString::fromStdString(domain);
                    topCnt = count;
                }
                QVariantMap item;
                item["name"] = QString::fromStdString(domain);
                item["count"] = count;
                item["percent"] = (totalTopCnt > 0) ? (count * 100 / totalTopCnt) : 0;
                item["color"] = palette[pIdx % palette.size()];
                pIdx++;
                topList.append(item);
            }

            if (m_topBlockedDomains != topList) {
                m_topBlockedDomains = topList;
                emit topBlockedDomainsChanged();
            }
            if (m_topDomain != topDom) {
                m_topDomain = topDom;
                emit topDomainChanged();
            }
            if (m_topDomainCount != topCnt) {
                m_topDomainCount = topCnt;
                emit topDomainCountChanged();
            }
        }

        // 9. 오늘 주간 누적(Weekly) 실시간 반영
        QDate today = QDate::currentDate();
        int dayOfWeek = today.dayOfWeek(); // 1 = Mon, 7 = Sun
        int dayIdx = dayOfWeek - 1;        // 0..6
        if (dayIdx >= 0 && dayIdx < m_weeklyMinutes.size()) {
            int currentMins = m_elapsedSecs / 60;
            if (m_weeklyMinutes[dayIdx].toInt() != currentMins) {
                m_weeklyMinutes[dayIdx] = currentMins;
                emit weeklyMinutesChanged();
            }
        }

        // 10. 장기 누적 엔진(history_daily.json)에 세션 데이터 실시간 동기화
        updateTodayRecordFromSession();
        if (++m_historyFlushCounter % 15 == 0) { // 15초마다 디스크 안전 커밋
            saveHistory();
        }

    } catch (const std::exception &e) {
        qDebug() << "[BackendManager] reloadStats JSON exception:" << e.what();
    }
}

// =============================================================================
//  휴식 / 집중 모드 제어 (pause.flag)
// =============================================================================
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

// =============================================================================
//  스칼라 설정 Setter
// =============================================================================
void BackendManager::setUserName(const QString &name)
{
    if (m_userName != name) { m_userName = name; emit userNameChanged(); saveSettings(); }
}

void BackendManager::setTargetTime(const QString &time)
{
    if (m_targetTime != time) { m_targetTime = time; emit targetTimeChanged(); saveSettings(); }
}

void BackendManager::setRstRetry(int v)
{
    if (m_rstRetry != v) { m_rstRetry = v; emit rstRetryChanged(); saveSettings(); }
}

// =============================================================================
//  뽀모도로(Pomodoro) Getter & Setter
// =============================================================================
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
