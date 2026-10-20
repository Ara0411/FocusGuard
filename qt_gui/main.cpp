#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QQuickWindow>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <memory>
#include "BackendManager.h"

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QFile logFile(QCoreApplication::applicationDirPath() + "/debug_log.txt");
    if (!logFile.isOpen()) {
        logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    QTextStream out(&logFile);
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString typeStr;
    switch (type) {
    case QtDebugMsg: typeStr = "DEBUG"; break;
    case QtInfoMsg: typeStr = "INFO"; break;
    case QtWarningMsg: typeStr = "WARN"; break;
    case QtCriticalMsg: typeStr = "CRIT"; break;
    case QtFatalMsg: typeStr = "FATAL"; break;
    }
    out << "[" << timeStr << "] [" << typeStr << "] " << msg << "\n";
    out.flush();
}

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    HANDLE hSingleInstanceMutex = CreateMutexW(NULL, TRUE, L"FocusNetGuardian_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(NULL, L"FocusNet Guardian");
        if (hExisting) {
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
        }
        if (hSingleInstanceMutex) CloseHandle(hSingleInstanceMutex);
        return 0;
    }
#endif

    // QApplication (QuickControls2 네이티브 스타일용)
    QApplication app(argc, argv);

    qInstallMessageHandler(messageHandler);
    qDebug() << "=== FocusNet Guardian Starting ===";

    app.setOrganizationName("FocusGuardTeam");
    app.setApplicationName("FocusNet Guardian");

    QIcon appIcon(":/FocusNet/Guardian/app_icon.ico");
    if (appIcon.isNull()) {
        appIcon = QIcon(":/FocusNet/Guardian/app_icon.png");
    }
    app.setWindowIcon(appIcon);

    // QML 스타일: Basic(커스터마이징 자유) 강제 설정
    // 네이티브 Windows 스타일은 background/contentItem 커스터마이징 불가 경고 발생
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    // BackendManager: JSON 폴링 + pause.flag 제어
    qDebug() << "[Main] Constructing BackendManager...";
    auto backendManager = std::make_unique<BackendManager>();
    qDebug() << "[Main] BackendManager constructed successfully";

    QQmlApplicationEngine engine;
    // QML 전역에 'backend' 이름으로 노출
    engine.rootContext()->setContextProperty("backend", backendManager.get());

    // URI "FocusNet.Guardian" → Main.qml
    const QUrl url(u"qrc:/FocusNet/Guardian/Main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() {
            qCritical() << "[Main] objectCreationFailed triggered! Exiting app...";
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    qDebug() << "[Main] Loading QML from url:" << url;
    engine.load(url);
    qDebug() << "[Main] engine.load() call returned. rootObjects count:" << engine.rootObjects().size();

    // ── Windows 시스템 트레이 아이콘 설정 ──────────────────────────
    QSystemTrayIcon trayIcon(appIcon, &app);
    trayIcon.setToolTip("FocusNet Guardian — 네트워크 및 집중 보호 엔진 가동 중");

    QMenu trayMenu;
    QAction *actOpen = trayMenu.addAction("FocusNet Guardian 열기");
    QAction *actPause = trayMenu.addAction("집중 모드 일시정지 / 재개");
    trayMenu.addSeparator();
    QAction *actQuit = trayMenu.addAction("완전 종료");

    auto showWindow = [&engine]() {
        const auto rootObjects = engine.rootObjects();
        if (!rootObjects.isEmpty()) {
            if (auto win = qobject_cast<QQuickWindow*>(rootObjects.first())) {
                win->show();
                win->raise();
                win->requestActivate();
            }
        }
    };

    QObject::connect(actOpen, &QAction::triggered, showWindow);
    QObject::connect(actPause, &QAction::triggered, [&backendManager]() {
        backendManager->togglePause();
    });
    QObject::connect(actQuit, &QAction::triggered, &app, &QApplication::quit);

    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showWindow();
        }
    });

    QObject::connect(backendManager.get(), &BackendManager::showWindowRequested, showWindow);
    QObject::connect(backendManager.get(), &BackendManager::trayNotificationRequested,
        [&trayIcon](const QString &title, const QString &message) {
            trayIcon.showMessage(title, message, QSystemTrayIcon::Information, 3500);
        });

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    // 시작 시 메인 윈도우 명시적 표시 및 활성화
    showWindow();

    int ret = app.exec();

#ifdef _WIN32
    if (hSingleInstanceMutex) {
        ReleaseMutex(hSingleInstanceMutex);
        CloseHandle(hSingleInstanceMutex);
    }
#endif

    return ret;
}
