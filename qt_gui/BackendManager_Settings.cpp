#include "BackendManager.h"
#include <QFile>
#include <QSaveFile>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// =============================================================================
//  환경설정(user_settings.json) 로드 및 저장
// =============================================================================

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
        if (j.contains("geminiApiKey")) {
            m_geminiApiKey = QString::fromStdString(j["geminiApiKey"].get<std::string>()).trimmed();
        } else {
            QByteArray envKey = qgetenv("GEMINI_API_KEY");
            if (!envKey.isEmpty()) {
                m_geminiApiKey = QString::fromUtf8(envKey).trimmed();
            }
        }
        if (j.contains("disabled_study_sites") && j["disabled_study_sites"].is_array()) {
            m_disabledStudySites.clear();
            for (auto &el : j["disabled_study_sites"])
                m_disabledStudySites.append(QString::fromStdString(el.get<std::string>()));
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
        emit weeklyMinutesChanged();
        emit disabledStudySitesChanged();
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
    if (!m_geminiApiKey.isEmpty()) {
        j["geminiApiKey"] = m_geminiApiKey.toStdString();
    }

    json arrDisabled = json::array();
    for (const auto &site : m_disabledStudySites)
        arrDisabled.push_back(site.toStdString());
    j["disabled_study_sites"] = arrDisabled;

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
    saveWhitelistFile();
}

// =============================================================================
//  시스템 트레이 및 윈도우 제어
// =============================================================================

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

void BackendManager::openSettingsFolder()
{
    QFileInfo fi(m_settingsFilePath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

bool BackendManager::createBackup()
{
    QFileInfo fi(m_settingsFilePath);
    QString dir = fi.absolutePath();
    QString backupSettings = dir + "/user_settings.backup.json";
    QString backupWhitelist = dir + "/whitelist.backup.json";

    QFile::remove(backupSettings);
    bool ok1 = QFile::copy(m_settingsFilePath, backupSettings);

    QFile::remove(backupWhitelist);
    bool ok2 = QFile::copy(m_whitelistFilePath, backupWhitelist);

    qDebug() << "[BackendManager] createBackup result:" << ok1 << ok2;
    return ok1 || ok2;
}

bool BackendManager::restoreBackup()
{
    QFileInfo fi(m_settingsFilePath);
    QString dir = fi.absolutePath();
    QString backupSettings = dir + "/user_settings.backup.json";
    QString backupWhitelist = dir + "/whitelist.backup.json";

    if (QFile::exists(backupSettings)) {
        QFile::remove(m_settingsFilePath);
        QFile::copy(backupSettings, m_settingsFilePath);
        loadSettings();
    }
    if (QFile::exists(backupWhitelist)) {
        QFile::remove(m_whitelistFilePath);
        QFile::copy(backupWhitelist, m_whitelistFilePath);
        loadWhitelistFile();
    }
    return true;
}

void BackendManager::resetAllStats()
{
    m_elapsedSecs = 0;
    m_focusTime = "00:00:00";
    m_blockedAttempts = 0;
    m_blockedSessions = 0;
    m_blockedSecs = 0;
    m_blockedTime = "00:00:00";
    m_bestRecord = 0;
    m_focusScore = 0;
    m_recentLogs.clear();
    m_topBlockedDomains.clear();
    m_topDomain = "-";
    m_topDomainCount = 0;
    m_pomodoroRound = 0;

    m_timelineData = QVariantList();
    for (int i = 0; i < 24; ++i) m_timelineData.append(0);

    m_weeklyMinutes = QVariantList();
    for (int i = 0; i < 7; ++i) m_weeklyMinutes.append(0);

    emit elapsedSecsChanged();
    emit focusTimeChanged();
    emit blockedAttemptsChanged();
    emit blockedSecsChanged();
    emit blockedTimeChanged();
    emit bestRecordChanged();
    emit focusScoreChanged();
    emit recentLogsChanged();
    emit topBlockedDomainsChanged();
    emit topDomainChanged();
    emit topDomainCountChanged();
    emit pomodoroRoundChanged();
    emit timelineDataChanged();
    emit weeklyMinutesChanged();

    saveSettings();

    // FocusGuard_stats.json 초기화
    QFile::remove("FocusGuard_stats.json");
    QFile::remove("core_engine/FocusGuard_stats.json");

    // history_daily.json 초기화
    QFile::remove("history_daily.json");
    QFile::remove("core_engine/history_daily.json");
    emit historyDataChanged();

    qDebug() << "[BackendManager] All statistics and block records have been reset.";
}
