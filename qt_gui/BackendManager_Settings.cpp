#include "BackendManager.h"
#include <QFile>
#include <QSaveFile>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QAbstractNativeEventFilter>
#include <QDebug>
#include <nlohmann/json.hpp>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
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

// =============================================================================
//  Windows 전역 단축키 (Global Hotkey) 제어
// =============================================================================

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

void BackendManager::registerGlobalHotkey()
{
    unregisterGlobalHotkey();
    if (!m_toggleShortcutEnabled || m_toggleShortcut.trimmed().isEmpty()) return;

#ifdef _WIN32
    if (!m_hotkeyFilter) {
        m_hotkeyFilter = new HotkeyEventFilter(this);
        QCoreApplication::instance()->installNativeEventFilter(m_hotkeyFilter);
    }

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

void BackendManager::cleanupGlobalHotkey()
{
#ifdef _WIN32
    unregisterGlobalHotkey();
    if (m_hotkeyFilter) {
        QCoreApplication::instance()->removeNativeEventFilter(m_hotkeyFilter);
        delete m_hotkeyFilter;
        m_hotkeyFilter = nullptr;
    }
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
