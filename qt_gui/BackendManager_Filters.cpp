#include "BackendManager.h"
#include <QFile>
#include <QSaveFile>
#include <QDebug>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// =============================================================================
//  화이트리스트 / 블랙리스트 / 차단 앱 파일 I/O 및 CRUD (whitelist.json)
// =============================================================================

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

    // 1. 기존 파일 읽기 (기존 키 보존 및 안전 업데이트)
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

void BackendManager::setBlockMode(int v)
{
    if (m_blockMode != v) {
        m_blockMode = v;
        emit blockModeChanged();
        saveSettings();
        saveWhitelistFile();
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
