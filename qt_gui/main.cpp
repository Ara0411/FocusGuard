#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "BackendManager.h"

int main(int argc, char *argv[])
{
    // QApplication (QuickControls2 네이티브 스타일용)
    QApplication app(argc, argv);

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
    BackendManager backendManager;

    QQmlApplicationEngine engine;
    // QML 전역에 'backend' 이름으로 노출
    engine.rootContext()->setContextProperty("backend", &backendManager);

    // URI "FocusNet.Guardian" → Main.qml
    const QUrl url(u"qrc:/FocusNet/Guardian/Main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
