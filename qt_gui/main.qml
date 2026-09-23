// =============================================================================
//  FocusNet Guardian - Main.qml   (Full Application / 5 Pages)
//  Qt 6 / QtQuick 단일 파일 · 외부 리소스 및 서드파티 없음
//  실행:  qml Main.qml     또는  Qt Creator 에서 열기
// -----------------------------------------------------------------------------
//  Pages : 대시보드 / 차단 로그 / 사이트 관리 / 집중 통계 / 설정
//  전환   : StackView.replace + fade & slide 트랜지션
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "pages"


ApplicationWindow {
    id: root
    width: 1440
    height: 960
    minimumWidth: 1280
    minimumHeight: 760
    visible: true
    title: "FocusNet Guardian"
    color: "#0D111A"

    // =================================================================
    //  C++ 백엔드(Npcap 엔진) 연동용 루트 프로퍼티
    // =================================================================
    function parseTargetSeconds(t) {
        if (!t) return 3 * 3600;
        var p = t.split(":");
        if (p.length === 3) {
            var h = parseInt(p[0]) || 0;
            var m = parseInt(p[1]) || 0;
            var s = parseInt(p[2]) || 0;
            return Math.max(60, h * 3600 + m * 60 + s);
        }
        return 3 * 3600;
    }

    property int    currentPage:  0                    // 0~4 사이드바 인덱스
    property bool   isFocusing:   !backend.isPaused
    property int    timerCount:   backend.elapsedSecs
    property int    targetCount:  parseTargetSeconds(backend.targetTime)
    property int    blockedCount: backend.blockedAttempts
    property int    blockedSecs:  backend.blockedSecs
    property int    bestRecord:   backend.bestRecord
    property string userName:     backend.userName
    property int    focusHours:   Math.max(1, Math.floor(timerCount / 3600))
    property int    notifyCount:  3
    property int    currentHour:  new Date().getHours()
    property real   goalRatio:    Math.min(1.0, timerCount / Math.max(1, targetCount))

    readonly property real ringProgress: Math.min(1.0, timerCount / Math.max(1, targetCount))

    property var hourlyFocus: (backend.timelineData && backend.timelineData.length === 24)
                              ? backend.timelineData
                              : [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

    // ---- 차단 로그 --------------------------------------------------
    property int    totalBlocked:   backend.blockedAttempts
    property string topDomain:      backend.topDomain
    property int    topDomainCount: backend.topDomainCount
    property string blockTech:      "TCP RST 패킷 주입"
    property bool   autoScroll:     true
    property int    logPage:        1
    property int    logPageCount:   Math.max(1, Math.ceil(logTableModel.count / 10))

    property ListModel blockLogModel: ListModel {}
    property ListModel logTableModel: ListModel {}

    function updateLogs() {
        var logs = backend.recentLogs
        if (!logs) return
        blockLogModel.clear()
        logTableModel.clear()

        for (var i = 0; i < logs.length; ++i) {
            var item = logs[i]
            var rawDomain = item.domain || ""
            var isApp = rawDomain.indexOf("[APP] ") === 0
            var dispDomain = isApp ? rawDomain.replace("[APP] ", "") : rawDomain
            var proto = isApp ? "APP" : (item.proto || "TLS")
            var reason = isApp ? "차단 프로그램 실행 감지" : (item.reason || "화이트리스트 미등록")
            var cat = isApp ? "프로그램" : (item.cat || "기타")
            var action = isApp ? "강제 종료!" : (item.action || "접근 차단!")

            if (i < 10) {
                blockLogModel.append({
                    time: item.time || "",
                    domain: dispDomain,
                    action: action
                })
            }
            logTableModel.append({
                time: item.time || "",
                proto: proto,
                domain: dispDomain,
                reason: reason,
                cat: cat
            })
        }
    }

    function updateBlockedProcesses() {
        var list = backend.blockedProcesses
        if (!list) return
        policyProcessModel.clear()
        var today = new Date().toISOString().slice(0, 10)
        for (var i = 0; i < list.length; ++i) {
            var proc = list[i]
            policyProcessModel.append({
                site: proc,
                cat: "프로그램",
                date: today,
                memo: "차단 프로세스 등록됨",
                onDuty: true
            })
        }
    }

    function updateWhitelist() {
        var list = backend.whitelistDomains
        if (!list || list.length === 0) return
        whiteListModel.clear()
        policyWhiteModel.clear()
        studySiteModel.clear()

        var today = new Date().toISOString().slice(0, 10)
        for (var i = 0; i < list.length; ++i) {
            var domain = list[i]
            var cat = "기타"
            if (domain.indexOf("github") !== -1 || domain.indexOf("vscode") !== -1) cat = "개발"
            else if (domain.indexOf("ebs") !== -1 || domain.indexOf("eclass") !== -1 || domain.indexOf("yuhan") !== -1) cat = "인강/사전"
            else if (domain.indexOf("naver") !== -1 || domain.indexOf("google") !== -1) cat = "사전/포털"

            whiteListModel.append({ site: domain, tag: cat })
            studySiteModel.append({ site: domain, note: "(" + cat + ")", tag: cat, onDuty: true })
            policyWhiteModel.append({ site: domain, cat: cat, date: today, memo: "등록됨", onDuty: true })
        }
    }

    function updateBlacklist() {
        var list = backend.blacklistDomains
        if (!list || list.length === 0) return
        policyBlackModel.clear()

        var today = new Date().toISOString().slice(0, 10)
        for (var i = 0; i < list.length; ++i) {
            var domain = list[i]
            var cat = "기타"
            var dLower = domain.toLowerCase()
            if (dLower.indexOf("youtube") !== -1 || dLower.indexOf("googlevideo") !== -1 || dLower.indexOf("tiktok") !== -1 || dLower.indexOf("twitch") !== -1 || dLower.indexOf("youtu.be") !== -1 || dLower.indexOf("ytimg") !== -1) {
                cat = "동영상"
            } else if (dLower.indexOf("instagram") !== -1 || dLower.indexOf("facebook") !== -1 || dLower.indexOf("twitter") !== -1 || dLower.indexOf("x.com") !== -1 || dLower.indexOf("fbcdn") !== -1) {
                cat = "SNS"
            } else if (dLower.indexOf("discord") !== -1 || dLower.indexOf("kakao") !== -1) {
                cat = "메신저"
            } else if (dLower.indexOf("steam") !== -1) {
                cat = "게임"
            }

            policyBlackModel.append({ site: domain, cat: cat, date: today, memo: "차단 등록됨", onDuty: true })
        }
    }

    Connections {
        target: backend
        function onRecentLogsChanged() {
            root.updateLogs()
        }
        function onWhitelistDomainsChanged() {
            root.updateWhitelist()
        }
        function onBlacklistDomainsChanged() {
            root.updateBlacklist()
        }
        function onBlockModeChanged() {
            root.blockMode = backend.blockMode
        }
        function onRstRetryChanged() {
            root.rstRetry = backend.rstRetry
        }
        function onPomodoroFocusChanged() {
            root.pomodoroFocus = backend.pomodoroFocus
        }
        function onPomodoroRestChanged() {
            root.pomodoroRest = backend.pomodoroRest
        }
        function onToastEnabledChanged() {
            root.toastEnabled = backend.toastEnabled
        }
        function onAutoStartChanged() {
            root.autoStart = backend.autoStart
        }
        function onTrayOnCloseChanged() {
            root.trayOnClose = backend.trayOnClose
        }
        function onWeeklyMinutesChanged() {
            root.weekMinutes = backend.weeklyMinutes
        }
        function onTopBlockedDomainsChanged() {
            root.topWasted = (backend.topBlockedDomains && backend.topBlockedDomains.length > 0) ? backend.topBlockedDomains : []
        }
        function onBlockedProcessesChanged() {
            root.updateBlockedProcesses()
        }
    }

    Component.onCompleted: {
        root.updateLogs()
        root.updateWhitelist()
        root.updateBlacklist()
        root.updateBlockedProcesses()
        root.blockMode = backend.blockMode
        root.rstRetry = backend.rstRetry
        root.pomodoroFocus = backend.pomodoroFocus
        root.pomodoroRest = backend.pomodoroRest
        root.toastEnabled = backend.toastEnabled
        root.autoStart = backend.autoStart
        root.trayOnClose = backend.trayOnClose
        root.weekMinutes = backend.weeklyMinutes
    }

    // ---- 사이트 및 앱 정책 -------------------------------------------
    property ListModel whiteListModel: ListModel {}
    property ListModel studySiteModel: ListModel {}
    property ListModel policyWhiteModel: ListModel {}
    property ListModel policyBlackModel: ListModel {}
    property ListModel policyProcessModel: ListModel {}

    property var siteCategories: ["인강/사전", "개발", "사전/포털", "EBS 인터넷강의", "동영상", "SNS", "메신저", "게임", "기타"]

    // ---- 집중 통계 --------------------------------------------------
    property var weekLabels:  ["월", "화", "수", "목", "금", "토", "일"]
    property var weekMinutes: (backend.weeklyMinutes && backend.weeklyMinutes.length === 7)
                              ? backend.weeklyMinutes
                              : [0, 0, 0, 0, 0, 0, 0]
    property int weekGoalMin: Math.max(30, Math.floor(targetCount / 60))
    property var topWasted: (backend.topBlockedDomains && backend.topBlockedDomains.length > 0)
                            ? backend.topBlockedDomains
                            : []
    property int focusScore: Math.round(backend.focusScore)

    // ---- 설정 -------------------------------------------------------
    property int    blockMode:     backend.blockMode        // 0 = 엄격(Default-Deny), 1 = 유연(Blacklist)
    property var    nicList:       ["Intel(R) Wi-Fi 6E AX211 160MHz", "Realtek PCIe GbE Family Controller", "TAP-Windows Adapter V9"]
    property int    nicIndex:      0
    property int    rstRetry:      backend.rstRetry
    property int    pomodoroFocus: backend.pomodoroFocus
    property int    pomodoroRest:  backend.pomodoroRest
    property bool   toastEnabled:  backend.toastEnabled
    property bool   autoStart:     backend.autoStart
    property bool   trayOnClose:   backend.trayOnClose
    property string backupPath:    backend.settingsFilePath

    // =================================================================
    //  유틸
    // =================================================================
    function hms(sec) {
        var h = Math.floor(sec / 3600)
        var m = Math.floor((sec % 3600) / 60)
        var s = Math.floor(sec % 60)
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }
    function hm(min) {
        return Math.floor(min / 60) + "시간 " + (min % 60) + "분"
    }
    function catColor(c) {
        switch (c) {
        case "개발":            return "#38BDF8"
        case "사전/포털":       return "#10B981"
        case "인강/사전":       return "#A5B4FC"
        case "EBS 인터넷강의":  return "#818CF8"
        case "동영상":          return "#F43F5E"
        case "SNS":             return "#A855F7"
        case "메신저":          return "#FB923C"
        case "게임":            return "#FBBF24"
        default:                return "#94A3B8"
        }
    }
    function catBg(c) {
        switch (c) {
        case "동영상":  return "#2A1420"
        case "SNS":     return "#241733"
        case "메신저":  return "#2A1D12"
        case "게임":    return "#2A2412"
        case "사전/포털": return "#0F2A22"
        default:        return "#1D2438"
        }
    }
    // 요일 7 × 시간 24 히트맵 (오늘 요일은 실시간 타임라인 데이터 연동)
    function heatValue(day, hour) {
        var d = new Date().getDay()
        var todayIdx = (d === 0 ? 6 : d - 1)
        if (day === todayIdx) {
            var val = (root.hourlyFocus && root.hourlyFocus[hour] !== undefined) ? root.hourlyFocus[hour] : 0
            if (val <= 0) return 0
            if (val < 15) return 1
            if (val < 30) return 2
            if (val < 45) return 3
            return 4
        }
        return 0
    }

    // C++ backend가 1초마다 elapsedSecs를 갱신하므로 QML Timer는 UI 갱신 틱용으로만 유지
    Timer {
        interval: 1000; running: root.isFocusing; repeat: true
        onTriggered: { /* backend.elapsedSecs 바인딩 유지 */ }
    }

    // =================================================================
    //  재사용 인라인 컴포넌트
    // =================================================================

    // =================================================================
    //  루트 레이아웃 : 사이드바 + StackView
    // =================================================================
    function pageComponent(i) {
        switch (i) {
        case 1:  return cmpBlockLogs
        case 2:  return cmpSites
        case 3:  return cmpAnalytics
        case 4:  return cmpSettings
        default: return cmpDashboard
        }
    }
    function goPage(i) {
        if (i === root.currentPage) return
        root.currentPage = i
        stack.replace(pageComponent(i))
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // -------------------------------------------------------------
        //  좌측 사이드바
        // -------------------------------------------------------------
        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            color: "#111622"

            Rectangle {
                width: 1; color: "#1C2432"
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 0

                RowLayout {
                    Layout.topMargin: 6
                    Layout.leftMargin: 4
                    spacing: 12
                    Item {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        implicitWidth: 36
                        implicitHeight: 36

                        Image {
                            anchors.fill: parent
                            source: "qrc:/FocusNet/Guardian/app_icon.png"
                            sourceSize.width: 72
                            sourceSize.height: 72
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }
                    }
                    ColumnLayout {
                        spacing: 1
                        Txt { text: "FocusNet"; font.pixelSize: 17; font.weight: Font.DemiBold; color: "#F2F5FB" }
                        Txt { text: "Guardian"; font.pixelSize: 12; color: "#7B8BFF" }
                    }
                }

                Item { Layout.preferredHeight: 26 }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: ListModel {
                            ListElement { label: "대시보드";    glyph: "home" }
                            ListElement { label: "차단 로그";   glyph: "shield" }
                            ListElement { label: "사이트 관리"; glyph: "globe" }
                            ListElement { label: "집중 통계";   glyph: "bars" }
                            ListElement { label: "설정";       glyph: "gear" }
                        }
                        delegate: Rectangle {
                            id: navItem
                            required property int index
                            required property string label
                            required property string glyph
                            property bool active: index === root.currentPage

                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            radius: 12
                            color: active ? "#1D2439" : (navHover.hovered ? "#161C2A" : "transparent")
                            border.width: active ? 1 : 0
                            border.color: "#2B3350"
                            Behavior on color { ColorAnimation { duration: 130 } }

                            HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: root.goPage(navItem.index) }

                            Rectangle {   // 활성 인디케이터
                                width: 3; height: 20; radius: 2
                                anchors.verticalCenter: parent.verticalCenter
                                x: -20
                                color: "#7B61FF"
                                opacity: navItem.active ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 160 } }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                spacing: 13
                                Icon {
                                    name: navItem.glyph
                                    stroke: navItem.active ? "#A9B6FF" : "#6E7A92"
                                    implicitWidth: 19; implicitHeight: 19
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    text: navItem.label
                                    font.pixelSize: 14
                                    font.weight: navItem.active ? Font.DemiBold : Font.Normal
                                    color: navItem.active ? "#EDF0FA" : "#8A94A8"
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 168
                    color: "#161C2B"
                    border.color: "#252E42"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 0

                        RowLayout {
                            spacing: 10
                            Icon { name: "shield-check"; stroke: "#10B981"
                                   implicitWidth: 18; implicitHeight: 18 }
                            ColumnLayout {
                                spacing: 2
                                Txt { text: "집중 모드"; font.pixelSize: 13; font.weight: Font.DemiBold }
                                Txt { text: root.isFocusing ? "진행 중" : "정지됨"
                                      font.pixelSize: 11; color: "#7F8AA0" }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8
                            Rectangle {
                                width: 7; height: 7; radius: 3.5; color: "#7B61FF"
                                SequentialAnimation on opacity {
                                    running: root.isFocusing; loops: Animation.Infinite
                                    NumberAnimation { to: 0.25; duration: 900; easing.type: Easing.InOutQuad }
                                    NumberAnimation { to: 1.0;  duration: 900; easing.type: Easing.InOutQuad }
                                }
                            }
                            Txt {
                                text: root.hms(root.targetCount)
                                font.pixelSize: 16; font.weight: Font.DemiBold
                                font.family: "Consolas"; color: "#C9D2E6"
                            }
                        }

                        Item { Layout.preferredHeight: 10 }

                        GhostBtn {
                            Layout.fillWidth: true
                            height: 38
                            text: root.isFocusing ? "중지하기" : "다시 시작"
                            glyph: root.isFocusing ? "stop" : "play"
                            onClicked: backend.togglePause()
                        }
                    }
                }
            }
        }

        // -------------------------------------------------------------
        //  우측 메인 : StackView
        // -------------------------------------------------------------
        StackView {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            initialItem: cmpDashboard

            replaceEnter: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 230; easing.type: Easing.OutCubic }
                NumberAnimation { property: "y";       from: 18; to: 0; duration: 280; easing.type: Easing.OutCubic }
            }
            replaceExit: Transition {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 130 }
            }
        }
    }

    // =================================================================
    //  PAGE 0 : 대시보드
    // =================================================================

    // =================================================================
    //  페이지 컴포넌트 연결 (pages/ 폴더 분리)
    // =================================================================
    Component { id: cmpDashboard; DashboardPage {} }
    Component { id: cmpBlockLogs; BlockLogsPage {} }
    Component { id: cmpSites;     SitesPage {} }
    Component { id: cmpAnalytics; AnalyticsPage {} }
    Component { id: cmpSettings;  SettingsPage {} }

    // 대시보드 사이트 등록 핸들러 (C++ 로 시그널 전달 지점)
    function registerSite(v) {
        var t = (v || "").trim()
        if (t.length === 0) return
        studySiteModel.append({ site: t, note: "(인강/사전)", tag: "인강/사전", onDuty: true })
        whiteListModel.append({ site: t, tag: "인강/사전" })
        policyWhiteModel.append({ site: t, cat: "인강/사전", date: "2026-09-16",
                                  memo: "대시보드에서 등록", onDuty: true })
        backend.addToWhitelist(t)
    }
}
