cmake_minimum_required(VERSION 3.16)

project(FocusNetGuardian VERSION 1.0 LANGUAGES CXX)

set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2)

# nlohmann_json (FetchContent)
include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

set(PROJECT_SOURCES
    main.cpp
    BackendManager.h
    BackendManager.cpp
    FocusNetGuardian.rc
)

qt_add_executable(FocusNetGuardian
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

qt_add_qml_module(FocusNetGuardian
    URI FocusNet.Guardian
    VERSION 1.0
    RESOURCE_PREFIX "/"
    RESOURCES
        app_icon.png
        app_icon.ico
    QML_FILES
        # Root
        Main.qml

        # Pages
        pages/DashboardPage.qml
        pages/BlockLogsPage.qml
        pages/SitesPage.qml
        pages/AnalyticsPage.qml
        pages/SettingsPage.qml

        # Components
        components/Card.qml
        components/CardTitle.qml
        components/CheckBox2.qml
        components/Combo.qml
        components/Field.qml
        components/GhostBtn.qml
        components/Icon.qml
        components/Metric.qml
        components/PageHead.qml
        components/Pill.qml
        components/SegTabs.qml
        components/SolidBtn.qml
        components/Toggle.qml
        components/Txt.qml
)

target_link_libraries(FocusNetGuardian
    PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2 nlohmann_json::nlohmann_json
)

set_target_properties(FocusNetGuardian PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

qt_import_qml_plugins(FocusNetGuardian)
qt_finalize_executable(FocusNetGuardian)
