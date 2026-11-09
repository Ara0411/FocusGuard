#include "BackendManager.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// =============================================================================
//  Npcap 엔진 제어 및 터미널 로그 스트리밍 (QProcess / UAC ShellExecuteEx)
// =============================================================================

void BackendManager::startEngine()
{
    if (m_isEngineRunning) return;

    if (!QFile::exists(m_engineExePath)) {
        QString appDir = QCoreApplication::applicationDirPath();
        if (QFile::exists(appDir + "/FocusGuard.exe")) {
            m_engineExePath = appDir + "/FocusGuard.exe";
        } else {
            QString ws = resolveWorkspacePath();
            QString candidate = ws + "/core_engine/x64/Release/FocusGuard.exe";
            if (QFile::exists(candidate)) m_engineExePath = candidate;
            else m_engineExePath = ws + "/core_engine/x64/Debug/FocusGuard.exe";
        }
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

// =============================================================================
//  네트워크 인터페이스(NIC) 어댑터 수집 및 선택
// =============================================================================

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
