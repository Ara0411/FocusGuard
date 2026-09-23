#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class BackendManager : public QObject
{
    Q_OBJECT

    // ── 스칼라 집중 지표 ──────────────────────────────────────
    Q_PROPERTY(double   focusScore        READ focusScore        NOTIFY focusScoreChanged)
    Q_PROPERTY(int      elapsedSecs       READ elapsedSecs       NOTIFY elapsedSecsChanged)
    Q_PROPERTY(QString  focusTime         READ focusTime         NOTIFY focusTimeChanged)
    Q_PROPERTY(int      blockedAttempts   READ blockedAttempts   NOTIFY blockedAttemptsChanged)
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
    Q_PROPERTY(bool    toastEnabled  READ toastEnabled  WRITE setToastEnabled  NOTIFY toastEnabledChanged)
    Q_PROPERTY(bool    autoStart     READ autoStart     WRITE setAutoStart     NOTIFY autoStartChanged)
    Q_PROPERTY(bool    trayOnClose   READ trayOnClose   WRITE setTrayOnClose   NOTIFY trayOnCloseChanged)
    Q_PROPERTY(QString settingsFilePath READ settingsFilePath CONSTANT)

public:
    explicit BackendManager(QObject *parent = nullptr);

    // Getters
    double      focusScore()       const { return m_focusScore; }
    int         elapsedSecs()      const { return m_elapsedSecs; }
    QString     focusTime()        const { return m_focusTime; }
    int         blockedAttempts()  const { return m_blockedAttempts; }
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
    bool        toastEnabled()     const { return m_toastEnabled; }
    bool        autoStart()        const { return m_autoStart; }
    bool        trayOnClose()      const { return m_trayOnClose; }

    // Setters
    void setIsPaused(bool v);
    void setUserName(const QString &v);
    void setTargetTime(const QString &v);
    void setBlockMode(int v);
    void setRstRetry(int v);
    void setPomodoroFocus(int v);
    void setPomodoroRest(int v);
    void setToastEnabled(bool v);
    void setAutoStart(bool v);
    void setTrayOnClose(bool v);

public slots:
    void reloadStats();
    void loadSettings();
    void saveSettings();
    void togglePause();

    // 일괄 설정 저장 (QML 호출용)
    Q_INVOKABLE void saveAppSettings(int blockMode, int rstRetry, int pomodoroFocus, int pomodoroRest, bool toastEnabled, bool autoStart, bool trayOnClose, const QString &userName, const QString &targetTime);

    // 화이트리스트 / 블랙리스트 CRUD (SitesPage 연동용)
    Q_INVOKABLE void addToWhitelist(const QString &domain);
    Q_INVOKABLE void removeFromWhitelist(const QString &domain);
    Q_INVOKABLE void addToBlacklist(const QString &domain);
    Q_INVOKABLE void removeFromBlacklist(const QString &domain);

    // 앱 / 프로세스 차단 CRUD (SitesPage 연동용)
    Q_INVOKABLE void addBlockedProcess(const QString &processName);
    Q_INVOKABLE void removeBlockedProcess(const QString &processName);

signals:
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
    void toastEnabledChanged();
    void autoStartChanged();
    void trayOnCloseChanged();

private:
    // 파일 경로 (생성자에서 자동 탐색)
    QString m_statsFilePath;
    QString m_whitelistFilePath;
    QString m_pauseFlagPath;
    QString m_settingsFilePath;

    // 폴링 타이머
    QTimer *m_timer = nullptr;

    // 데이터
    double       m_focusScore      = 0.0;
    int          m_elapsedSecs     = 0;
    QString      m_focusTime       = "00:00:00";
    int          m_blockedAttempts = 0;
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
    bool         m_toastEnabled    = true;
    bool         m_autoStart       = true;
    bool         m_trayOnClose     = true;
    time_t       m_startTime       = 0;
    time_t       m_firstBlockTime  = 0;

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
};

