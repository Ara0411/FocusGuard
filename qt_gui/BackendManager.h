#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDate>
#include <QMap>
#include <QVector>

struct DailyRecord {
    int focusSecs = 0;              // 순수 누적 집중 시간 (초)
    int blockedAttempts = 0;        // 차단 패킷 수
    int blockedSessions = 0;        // 딴짓 시도 세션 수
    int pomodoroRounds = 0;         // 달성 뽀모도로 회차 수
    QVector<int> hourlyFocusSecs;   // 24시간 각 시간대 집중 초 (24개)
    QVector<int> hourlyBlocks;      // 24시간 각 시간대 차단 수 (24개)
    QMap<QString, int> topBlockedDomains; // 차단 도메인별 횟수

    DailyRecord() {
        hourlyFocusSecs.resize(24, 0);
        hourlyBlocks.resize(24, 0);
    }
};

class BackendManager : public QObject
{
    Q_OBJECT

    // ── 스칼라 집중 지표 ──────────────────────────────────────
    Q_PROPERTY(double   focusScore        READ focusScore        NOTIFY focusScoreChanged)
    Q_PROPERTY(int      elapsedSecs       READ elapsedSecs       NOTIFY elapsedSecsChanged)
    Q_PROPERTY(QString  focusTime         READ focusTime         NOTIFY focusTimeChanged)
    Q_PROPERTY(int      blockedAttempts   READ blockedAttempts   NOTIFY blockedAttemptsChanged)
    Q_PROPERTY(int      blockedSessions   READ blockedSessions   NOTIFY blockedSessionsChanged)
    Q_PROPERTY(int      blockedSecs       READ blockedSecs       NOTIFY blockedSecsChanged)
    Q_PROPERTY(QString  blockedTime       READ blockedTime       NOTIFY blockedTimeChanged)
    Q_PROPERTY(int      bestRecord        READ bestRecord        NOTIFY bestRecordChanged)

    // ── 리스트 ───────────────────────────────────────────────
    Q_PROPERTY(QVariantList timelineData       READ timelineData       NOTIFY timelineDataChanged)
    Q_PROPERTY(QVariantList recentLogs         READ recentLogs         NOTIFY recentLogsChanged)
    Q_PROPERTY(QVariantList topBlockedDomains  READ topBlockedDomains  NOTIFY topBlockedDomainsChanged)
    Q_PROPERTY(QVariantList whitelistDomains   READ whitelistDomains   NOTIFY whitelistDomainsChanged)
    Q_PROPERTY(QVariantList blacklistDomains   READ blacklistDomains   NOTIFY blacklistDomainsChanged)
    Q_PROPERTY(QString      topDomain          READ topDomain          NOTIFY topDomainChanged)
    Q_PROPERTY(int          topDomainCount     READ topDomainCount     NOTIFY topDomainCountChanged)
    Q_PROPERTY(QVariantList weeklyMinutes      READ weeklyMinutes      NOTIFY weeklyMinutesChanged)
    Q_PROPERTY(QVariantList blockedProcesses   READ blockedProcesses   NOTIFY blockedProcessesChanged)

    // ── 제어 및 설정 ─────────────────────────────────────────
    Q_PROPERTY(bool    isPaused   READ isPaused   WRITE setIsPaused  NOTIFY isPausedChanged)
    Q_PROPERTY(QString userName   READ userName   WRITE setUserName  NOTIFY userNameChanged)
    Q_PROPERTY(QString targetTime READ targetTime WRITE setTargetTime NOTIFY targetTimeChanged)
    Q_PROPERTY(int     blockMode     READ blockMode     WRITE setBlockMode     NOTIFY blockModeChanged)
    Q_PROPERTY(int     rstRetry      READ rstRetry      WRITE setRstRetry      NOTIFY rstRetryChanged)
    Q_PROPERTY(int     pomodoroFocus READ pomodoroFocus WRITE setPomodoroFocus NOTIFY pomodoroFocusChanged)
    Q_PROPERTY(int     pomodoroRest  READ pomodoroRest  WRITE setPomodoroRest  NOTIFY pomodoroRestChanged)
    Q_PROPERTY(bool    isPomodoroBreak READ isPomodoroBreak NOTIFY pomodoroStateChanged)
    Q_PROPERTY(int     pomodoroRemainingSecs READ pomodoroRemainingSecs NOTIFY pomodoroStateChanged)
    Q_PROPERTY(int     pomodoroRound READ pomodoroRound NOTIFY pomodoroRoundChanged)
    Q_PROPERTY(bool    pomodoroAutoEnabled READ pomodoroAutoEnabled WRITE setPomodoroAutoEnabled NOTIFY pomodoroAutoEnabledChanged)
    Q_PROPERTY(bool    toastEnabled  READ toastEnabled  WRITE setToastEnabled  NOTIFY toastEnabledChanged)
    Q_PROPERTY(bool    autoStart     READ autoStart     WRITE setAutoStart     NOTIFY autoStartChanged)
    Q_PROPERTY(bool    trayOnClose   READ trayOnClose   WRITE setTrayOnClose   NOTIFY trayOnCloseChanged)
    Q_PROPERTY(QString settingsFilePath READ settingsFilePath CONSTANT)

    // ── AI 통계 분석 (사용자 인터넷 사용 유형 및 Gemini 연동) ─────
    Q_PROPERTY(bool        isAiAnalyzing       READ isAiAnalyzing       NOTIFY isAiAnalyzingChanged)
    Q_PROPERTY(QString     aiUserType          READ aiUserType          NOTIFY aiAnalysisFinished)
    Q_PROPERTY(QString     aiTypeIcon          READ aiTypeIcon          NOTIFY aiAnalysisFinished)
    Q_PROPERTY(QString     aiTypeColor         READ aiTypeColor         NOTIFY aiAnalysisFinished)
    Q_PROPERTY(QString     aiAnalysisReport    READ aiAnalysisReport    NOTIFY aiAnalysisFinished)
    Q_PROPERTY(QStringList aiActionPlan        READ aiActionPlan        NOTIFY aiAnalysisFinished)
    Q_PROPERTY(QStringList aiRecommendedDomains READ aiRecommendedDomains NOTIFY aiRecommendedDomainsChanged)
    Q_PROPERTY(int         aiCooldownSecs      READ aiCooldownSecs      NOTIFY aiCooldownSecsChanged)
    Q_PROPERTY(bool        isAiInCooldown      READ isAiInCooldown      NOTIFY aiCooldownSecsChanged)
    Q_PROPERTY(QString     geminiApiKey        READ geminiApiKey        WRITE setGeminiApiKey NOTIFY geminiApiKeyChanged)
    Q_PROPERTY(QString     maskedGeminiApiKey  READ maskedGeminiApiKey  NOTIFY geminiApiKeyChanged)

    // ── Npcap 엔진 프로세스 및 콘솔 로그 제어 ─────────────────
    Q_PROPERTY(bool        isEngineRunning  READ isEngineRunning  NOTIFY isEngineRunningChanged)
    Q_PROPERTY(QStringList engineLogs       READ engineLogs       NOTIFY engineLogsChanged)

    // ── 네트워크 인터페이스 (NIC) 어댑터 제어 ─────────────────
    Q_PROPERTY(QStringList nicList          READ nicList          NOTIFY nicListChanged)
    Q_PROPERTY(int         nicIndex         READ nicIndex         WRITE setNicIndex NOTIFY nicIndexChanged)
    Q_PROPERTY(QString     targetAdapter    READ targetAdapter    WRITE setTargetAdapter NOTIFY targetAdapterChanged)

    // ── 장기 데이터 누적 보관 (history_daily.json) ───────────
    Q_PROPERTY(int totalAccumulatedMinutes READ totalAccumulatedMinutes NOTIFY historyDataChanged)
    Q_PROPERTY(int totalAccumulatedSessions READ totalAccumulatedSessions NOTIFY historyDataChanged)
    Q_PROPERTY(int totalAccumulatedRounds READ totalAccumulatedRounds NOTIFY historyDataChanged)

    // ── 차단 모드 토글 단축키 (전역 핫키) ─────────────────
    Q_PROPERTY(QString toggleShortcut READ toggleShortcut WRITE setToggleShortcut NOTIFY toggleShortcutChanged)
    Q_PROPERTY(bool toggleShortcutEnabled READ toggleShortcutEnabled WRITE setToggleShortcutEnabled NOTIFY toggleShortcutEnabledChanged)

public:
    explicit BackendManager(QObject *parent = nullptr);
    virtual ~BackendManager();

    // Getters
    double      focusScore()       const { return m_focusScore; }
    int         elapsedSecs()      const { return m_elapsedSecs; }
    QString     focusTime()        const { return m_focusTime; }
    int         blockedAttempts()  const { return m_blockedAttempts; }
    int         blockedSessions()  const { return m_blockedSessions; }
    int         blockedSecs()      const { return m_blockedSecs; }
    QString     blockedTime()      const { return m_blockedTime; }
    QVariantList timelineData()     const { return m_timelineData; }
    QVariantList recentLogs()       const { return m_recentLogs; }
    QVariantList topBlockedDomains() const { return m_topBlockedDomains; }
    QVariantList whitelistDomains()  const { return m_whitelistDomains; }
    QVariantList blacklistDomains()  const { return m_blacklistDomains; }
    QVariantList blockedProcesses()  const { return m_blockedProcesses; }
    QString     topDomain()        const { return m_topDomain; }
    int         topDomainCount()   const { return m_topDomainCount; }
    QVariantList weeklyMinutes()   const { return m_weeklyMinutes; }
    int         bestRecord()       const { return m_bestRecord; }
    bool        isPaused()         const { return m_isPaused; }
    QString     userName()         const { return m_userName; }
    QString     targetTime()       const { return m_targetTime; }
    QString     settingsFilePath() const { return m_settingsFilePath; }
    int         blockMode()        const { return m_blockMode; }
    int         rstRetry()         const { return m_rstRetry; }
    int         pomodoroFocus()    const { return m_pomodoroFocus; }
    int         pomodoroRest()     const { return m_pomodoroRest; }
    bool        isPomodoroBreak()  const { return m_isPomodoroBreak; }
    int         pomodoroRemainingSecs() const;
    int         pomodoroRound()    const { return m_pomodoroRound; }
    bool        pomodoroAutoEnabled() const { return m_pomodoroAutoEnabled; }
    bool        toastEnabled()     const { return m_toastEnabled; }
    bool        autoStart()        const { return m_autoStart; }
    bool        trayOnClose()      const { return m_trayOnClose; }

    // AI Getters
    bool        isAiAnalyzing()        const { return m_isAiAnalyzing; }
    QString     aiUserType()           const { return m_aiUserType; }
    QString     aiTypeIcon()           const { return m_aiTypeIcon; }
    QString     aiTypeColor()          const { return m_aiTypeColor; }
    QString     aiAnalysisReport()     const { return m_aiAnalysisReport; }
    QStringList aiActionPlan()         const { return m_aiActionPlan; }
    QStringList aiRecommendedDomains() const { return m_aiRecommendedDomains; }
    int         aiCooldownSecs()       const { return m_aiCooldownSecs; }
    bool        isAiInCooldown()       const { return m_aiCooldownSecs > 0; }
    QString     geminiApiKey()         const { return m_geminiApiKey; }
    QString     maskedGeminiApiKey()   const;

    // Engine & NIC Getters
    bool        isEngineRunning()  const { return m_isEngineRunning; }
    QStringList engineLogs()       const { return m_engineLogs; }
    QStringList nicList()          const { return m_nicList; }
    int         nicIndex()         const { return m_nicIndex; }
    QString     targetAdapter()    const { return m_targetAdapter; }

    // History Getters (history_daily.json)
    int         totalAccumulatedMinutes() const;
    int         totalAccumulatedSessions() const;
    int         totalAccumulatedRounds()   const;

    // Hotkey Getters & Setters
    QString     toggleShortcut()        const { return m_toggleShortcut; }
    void        setToggleShortcut(const QString &v);
    bool        toggleShortcutEnabled() const { return m_toggleShortcutEnabled; }
    void        setToggleShortcutEnabled(bool v);

    // Setters
    void setIsPaused(bool v);
    void setUserName(const QString &v);
    void setTargetTime(const QString &v);
    void setBlockMode(int v);
    void setRstRetry(int v);
    void setPomodoroFocus(int v);
    void setPomodoroRest(int v);
    void setPomodoroAutoEnabled(bool v);
    void setToastEnabled(bool v);
    void setAutoStart(bool v);
    void setTrayOnClose(bool v);
    void setNicIndex(int v);
    void setTargetAdapter(const QString &v);
    void setGeminiApiKey(const QString &v);

public slots:
    void reloadStats();
    void loadSettings();
    void saveSettings();
    void togglePause();
    void resetPomodoro();

    // 엔진 라이프사이클 및 로그 제어 (QML 호출용)
    Q_INVOKABLE void startEngine();
    Q_INVOKABLE void stopEngine();
    Q_INVOKABLE void restartEngine();
    Q_INVOKABLE void clearEngineLogs();

    // NIC 어댑터 갱신 및 선택
    Q_INVOKABLE void refreshNetworkAdapters();
    Q_INVOKABLE void selectNetworkAdapter(int index);

    // AI 인터넷 사용 유형 분석 요청 및 1~2단계 요약기/프롬프트 빌더 (QML 연동)
    Q_INVOKABLE void requestAiUserAnalysis();
    Q_INVOKABLE bool canRequestAiAnalysis() const;
    Q_INVOKABLE QString getAiAnalysisBlockReason() const;
    Q_INVOKABLE QString getAiPayloadDigestJson() const;
    Q_INVOKABLE QString getGeminiRequestBodyJson() const;
    QJsonObject generateAiPayloadDigest() const;
    QJsonObject buildGeminiRequestBody(const QJsonObject &digest) const;

    // 일괄 설정 저장 (QML 호출용)
    Q_INVOKABLE void saveAppSettings(int blockMode, int rstRetry, int pomodoroFocus, int pomodoroRest, bool toastEnabled, bool autoStart, bool trayOnClose, const QString &userName, const QString &targetTime, const QString &toggleShortcut = "", bool toggleShortcutEnabled = true);
    Q_INVOKABLE void toggleBlockMode();
    Q_INVOKABLE bool testRegisterShortcut(const QString &keySequence);

    // 화이트리스트 / 블랙리스트 CRUD (SitesPage 연동용)
    Q_INVOKABLE void addToWhitelist(const QString &domain);
    Q_INVOKABLE void removeFromWhitelist(const QString &domain);
    Q_INVOKABLE void addToBlacklist(const QString &domain);
    Q_INVOKABLE void removeFromBlacklist(const QString &domain);

    // 앱 / 프로세스 차단 CRUD (SitesPage 연동용)
    Q_INVOKABLE void addBlockedProcess(const QString &processName);
    Q_INVOKABLE void removeBlockedProcess(const QString &processName);

    // 선택 일괄 삭제 (다중 체크박스 선택 삭제)
    Q_INVOKABLE void removeSelectedSites(int tabIndex, const QStringList &targets);

    // 시스템 트레이 알림 및 윈도우 표시 제어
    Q_INVOKABLE void showTrayNotification(const QString &title, const QString &message);
    Q_INVOKABLE void requestShowWindow();
    Q_INVOKABLE void quitApp();

    // 장기 누적 통계 쿼리 (history_daily.json)
    Q_INVOKABLE QVariantMap getDailyStats(const QString &dateStr);
    Q_INVOKABLE QVariantList getWeeklyMinutesFromHistory();
    Q_INVOKABLE QVariantList getTrendRange(int daysCount);
    Q_INVOKABLE QVariantList getTopBlockedDomainsHistorical(int daysCount = 7, int limit = 5);
    Q_INVOKABLE QVariantList getWeekdayAverages();
    Q_INVOKABLE int getHeatValue(int dayIdx, int hourIdx);
    Q_INVOKABLE QString getMostProductiveWeekday();
    Q_INVOKABLE QString getMostDistractedWeekday();
    Q_INVOKABLE void flushHistory();

signals:
    void showWindowRequested();
    void historyDataChanged();
    void focusScoreChanged();
    void elapsedSecsChanged();
    void focusTimeChanged();
    void blockedAttemptsChanged();
    void blockedSecsChanged();
    void blockedTimeChanged();
    void bestRecordChanged();
    void timelineDataChanged();
    void recentLogsChanged();
    void topBlockedDomainsChanged();
    void whitelistDomainsChanged();
    void blacklistDomainsChanged();
    void blockedProcessesChanged();
    void topDomainChanged();
    void topDomainCountChanged();
    void weeklyMinutesChanged();
    void isPausedChanged();
    void userNameChanged();
    void targetTimeChanged();
    void blockModeChanged();
    void rstRetryChanged();
    void pomodoroFocusChanged();
    void pomodoroRestChanged();
    void pomodoroStateChanged();
    void pomodoroRoundChanged();
    void pomodoroAutoEnabledChanged();
    void toastEnabledChanged();
    void autoStartChanged();
    void trayOnCloseChanged();
    void isAiAnalyzingChanged();
    void aiAnalysisFinished();
    void isEngineRunningChanged();
    void engineLogsChanged();
    void nicListChanged();
    void nicIndexChanged();
    void targetAdapterChanged();
    void trayNotificationRequested(const QString &title, const QString &message);
    void geminiApiKeyChanged();
    void blockedSessionsChanged();
    void aiCooldownSecsChanged();
    void aiRecommendedDomainsChanged();
    void toggleShortcutChanged();
    void toggleShortcutEnabledChanged();

private:
    // 파일 경로 (생성자에서 자동 탐색)
    QString m_statsFilePath;
    QString m_whitelistFilePath;
    QString m_pauseFlagPath;
    QString m_settingsFilePath;
    QString m_historyFilePath;
    QString m_engineExePath;

    // 엔진 프로세스 및 로그
    class QProcess *m_engineProcess = nullptr;
    bool        m_isEngineRunning   = false;
    QStringList m_engineLogs;

    // NIC 장치 목록 (표시용 목록 vs 실제 하드웨어 명칭 목록)
    QStringList m_nicList;
    QStringList m_nicRawList;
    int         m_nicIndex          = 0;
    QString     m_targetAdapter     = "AUTO";

    // AI 인터넷 사용 유형 결과 캐시 및 Gemini API
    QString     m_geminiApiKey;
    bool        m_isAiAnalyzing    = false;
    QString     m_aiUserType       = "분석 대기 중";
    QString     m_aiTypeIcon       = "sparkles";
    QString     m_aiTypeColor      = "#818CF8";
    QString     m_aiAnalysisReport = "집중 세션 동안 축적된 네트워크 패킷 및 차단 로그를 AI가 분석하여 사용자의 인터넷 몰입 유형과 개선 가이드를 도출합니다.";
    QStringList m_aiActionPlan;
    QStringList m_aiRecommendedDomains;
    int         m_aiCooldownSecs   = 0;
    QTimer     *m_cooldownTimer    = nullptr;

    // 폴링 타이머 및 네트워크 매니저
    QTimer *m_timer = nullptr;
    QNetworkAccessManager *m_netManager = nullptr;

    // 데이터
    double       m_focusScore      = 0.0;
    int          m_elapsedSecs     = 0;
    QString      m_focusTime       = "00:00:00";
    int          m_blockedAttempts = 0;
    int          m_blockedSessions = 0;
    time_t       m_lastBlockTimestamp = 0;
    int          m_blockedSecs     = 0;
    QString      m_blockedTime     = "00:00:00";
    int          m_bestRecord      = 0;
    QString      m_topDomain       = "없음";
    int          m_topDomainCount  = 0;
    bool         m_isPaused        = false;
    QString      m_userName        = "사용자";
    QString      m_targetTime      = "03:00:00";
    int          m_blockMode       = 0;
    int          m_rstRetry        = 3;
    int          m_pomodoroFocus   = 25;
    int          m_pomodoroRest    = 5;
    bool         m_isPomodoroBreak = false;
    int          m_pomodoroElapsedSecs = 0;
    int          m_pomodoroRound   = 0;
    bool         m_pomodoroAutoEnabled = true;
    bool         m_toastEnabled    = true;
    bool         m_autoStart       = true;
    bool         m_trayOnClose     = true;
    QString      m_toggleShortcut  = "Ctrl+Shift+U";
    bool         m_toggleShortcutEnabled = true;
    void         registerGlobalHotkey();
    void         unregisterGlobalHotkey();
#ifdef _WIN32
    class HotkeyEventFilter *m_hotkeyFilter = nullptr;
#endif
    time_t       m_startTime       = 0;
    time_t       m_firstBlockTime  = 0;
    time_t       m_pauseStartTime  = 0;
    int          m_totalPausedSecs = 0;

    QVariantList m_timelineData;
    QVariantList m_recentLogs;
    QVariantList m_topBlockedDomains;
    QVariantList m_whitelistDomains;
    QVariantList m_blacklistDomains;
    QVariantList m_blockedProcesses;
    QVariantList m_weeklyMinutes;

    // 경로 탐색 헬퍼
    QString resolveWorkspacePath() const;
    void    loadWhitelistFile();
    void    saveWhitelistFile();

    // 장기 데이터 누적 엔진 헬퍼 (history_daily.json)
    QMap<QString, DailyRecord> m_dailyHistory;
    QString m_todayDateStr;
    int     m_sessionStartFocusSecs       = 0;
    int     m_sessionStartBlockedAttempts = 0;
    int     m_sessionStartBlockedSessions = 0;
    int     m_sessionStartPomodoroRounds  = 0;
    int     m_historyFlushCounter         = 0;

    void    loadHistory();
    void    saveHistory();
    void    ensureTodayRecord();
    void    updateTodayRecordFromSession();
    void    refreshWeeklyMinutesFromHistory();
};

