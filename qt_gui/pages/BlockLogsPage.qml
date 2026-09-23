import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: pgLogs
    property string query: ""
    property string catFilter: "전체"

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: parent.width
            spacing: 20

            // ---------- 헤더 ----------
            PageHead {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 26
                notify: root.notifyCount
                title: "차단 로그 분석"
                subtitle: "Npcap 엔진이 SNI·DNS 패킷을 실시간 감지 중입니다 · " + (root.blockLogModel.count > 0 ? "최근 차단: " + root.blockLogModel.get(0).time : "실시간 대기 중")

                Combo {
                    height: 42
                    width: 118
                    options: ["오늘", "최근 7일", "전체 기간"]
                    currentIndex: 0
                }
                GhostBtn {
                    height: 42
                    text: "로그 내보내기 (CSV)"
                    glyph: "download"
                }
            }

            // ---------- 요약 3열 ----------
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 20

                Metric {
                    Layout.fillWidth: true
                    label: "총 차단 건수"
                    value: root.totalBlocked + "회"
                    sub: "실시간 패킷 차단 가동 중"
                    glyph: "ban"
                    accent: "#F43F5E"
                    accentBg: "#2A1420"
                }
                Metric {
                    Layout.fillWidth: true
                    label: "최다 차단 도메인"
                    value: root.topDomain
                    sub: root.topDomainCount + "회 차단됨"
                    glyph: "flame"
                    accent: "#FB923C"
                    accentBg: "#2A1D12"
                }
                Metric {
                    Layout.fillWidth: true
                    label: "적용 차단 기술"
                    value: root.blockTech
                    glyph: "cpu"
                    accent: "#10B981"
                    accentBg: "#0F2A22"
                    pillText: "ACTIVE"
                }
            }

            // ---------- 데이터 테이블 ----------
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.bottomMargin: 28
                Layout.preferredHeight: 620

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 0

                    // 검색 + 카테고리 필터
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14

                        Field {
                            Layout.preferredWidth: 300
                            glyph: "search"
                            placeholderText: "도메인 검색 (예: youtube)"
                            onTextChanged: pgLogs.query = text.toLowerCase()
                        }

                        Row {
                            Layout.fillWidth: true
                            spacing: 7
                            Repeater {
                                model: ["전체", "동영상", "SNS", "메신저", "게임", "프로그램", "기타"]
                                delegate: Rectangle {
                                    required property string modelData
                                    property bool on: pgLogs.catFilter === modelData
                                    width: fTxt.implicitWidth + 24
                                    height: 30
                                    radius: 15
                                    color: on ? "#232C42" : (fHover.hovered ? "#1A2130" : "#121824")
                                    border.width: 1
                                    border.color: on ? "#4550A8" : "#242D3E"
                                    HoverHandler { id: fHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: pgLogs.catFilter = modelData }
                                    Txt {
                                        id: fTxt
                                        anchors.centerIn: parent
                                        text: modelData
                                        font.pixelSize: 12
                                        color: parent.on ? "#C0C8FF" : "#8A94A8"
                                    }
                                }
                            }
                        }

                        Txt {
                            text: root.logTableModel.count + "건 표시"
                            font.pixelSize: 12; color: "#6A7488"
                        }
                    }

                    Item { Layout.preferredHeight: 18 }

                    // 테이블 헤더
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        radius: 9
                        color: "#121824"
                        border.width: 1
                        border.color: "#1F2836"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12
                            Txt { Layout.preferredWidth: 86;  text: "발생 시간";      font.pixelSize: 12; color: "#7C8699" }
                            Txt { Layout.preferredWidth: 68;  text: "프로토콜";       font.pixelSize: 12; color: "#7C8699" }
                            Txt { Layout.fillWidth: true;     text: "감지된 SNI 도메인"; font.pixelSize: 12; color: "#7C8699" }
                            Txt { Layout.preferredWidth: 150; text: "사유";           font.pixelSize: 12; color: "#7C8699" }
                            Txt { Layout.preferredWidth: 90;  text: "조치";           font.pixelSize: 12; color: "#7C8699" }
                            Txt { Layout.preferredWidth: 110; text: "관리";           font.pixelSize: 12; color: "#7C8699"
                                  horizontalAlignment: Text.AlignRight }
                        }
                    }

                    // 테이블 본문
                    ListView {
                        id: logView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.topMargin: 4
                        clip: true
                        model: root.logTableModel
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: Item {
                            id: logRow
                            required property int index
                            required property string time
                            required property string proto
                            required property string domain
                            required property string reason
                            required property string cat
                            property bool match: (pgLogs.catFilter === "전체" || pgLogs.catFilter === cat)
                                                 && (pgLogs.query === "" || domain.toLowerCase().indexOf(pgLogs.query) >= 0)

                            width: ListView.view.width
                            height: match ? 50 : 0
                            visible: match
                            Behavior on height { NumberAnimation { duration: 140 } }

                            Rectangle {
                                anchors.fill: parent
                                anchors.topMargin: 1
                                radius: 8
                                color: rowHover.hovered ? "#161D2B" : "transparent"
                            }
                            HoverHandler { id: rowHover }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 12

                                Txt {
                                    Layout.preferredWidth: 86
                                    text: logRow.time
                                    font.pixelSize: 12; font.family: "Consolas"; color: "#C4CCDC"
                                }

                                Pill {
                                    Layout.preferredWidth: 68
                                    text: logRow.proto
                                    fontSize: 10
                                    fg: logRow.proto === "QUIC" ? "#38BDF8"
                                        : (logRow.proto === "DNS" ? "#FBBF24"
                                        : (logRow.proto === "APP" ? "#C084FC" : "#A5B4FC"))
                                    color: logRow.proto === "APP" ? "#2A1836" : "#1B2230"
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 9
                                    Icon { name: logRow.proto === "APP" ? "cpu" : "alert"
                                           stroke: logRow.proto === "APP" ? "#C084FC" : "#F43F5E"
                                           implicitWidth: 15; implicitHeight: 15 }
                                    Txt {
                                        Layout.fillWidth: true
                                        text: logRow.domain
                                        font.pixelSize: 12
                                        color: "#D3DAE8"
                                        elide: Text.ElideRight
                                        ToolTip.visible: rowHover.hovered && truncated
                                        ToolTip.text: logRow.domain
                                        ToolTip.delay: 400
                                    }
                                }

                                Txt {
                                    Layout.preferredWidth: 150
                                    text: logRow.reason
                                    font.pixelSize: 12
                                    color: "#8B96AC"
                                    elide: Text.ElideRight
                                }

                                Pill {
                                    Layout.preferredWidth: 90
                                    text: logRow.proto === "APP" ? "프로세스 종료" : "TCP RST"
                                    fontSize: 10
                                    fg: logRow.proto === "APP" ? "#C084FC" : "#FB7185"
                                    color: logRow.proto === "APP" ? "#2A1836" : "#2A1420"
                                }

                                GhostBtn {
                                    Layout.preferredWidth: 110
                                    Layout.preferredHeight: 30
                                    text: "예외 허용"
                                    glyph: "plus"
                                    fg: "#A5B4FC"
                                    fontSize: 12
                                    onClicked: {
                                        var targetDomain = logRow.domain
                                        var proto = logRow.proto
                                        console.log("[BlockLogsPage] exception allow clicked for:", targetDomain, "proto:", proto)
                                        if (proto === "APP") {
                                            backend.removeBlockedProcess(targetDomain)
                                            saveToast.show("'" + targetDomain + "' 앱 차단 해제 완료")
                                        } else {
                                            backend.addToWhitelist(targetDomain)
                                            saveToast.show("'" + targetDomain + "' 예외 허용 등록 완료")
                                        }
                                        root.logTableModel.remove(logRow.index)
                                    }
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width; height: 1
                                color: "#1B2331"
                                visible: logRow.match
                            }
                        }
                    }

                    // 푸터 : 페이지네이션 + 자동 스크롤
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 14
                        spacing: 12

                        RowLayout {
                            spacing: 9
                            Toggle {
                                checked: root.autoScroll
                                onToggled: function(v) {
                                    root.autoScroll = v
                                    if (v) logView.positionViewAtBeginning()
                                }
                            }
                            Txt {
                                text: "실시간 자동 스크롤"
                                font.pixelSize: 12
                                color: root.autoScroll ? "#C3CBDB" : "#6A7488"
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 32; height: 32; radius: 8
                            color: "transparent"
                            border.width: 1; border.color: "#2D3648"
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: root.logPage = Math.max(1, root.logPage - 1) }
                            Icon { anchors.centerIn: parent; name: "chevron-left"; stroke: "#8A94A8"
                                   implicitWidth: 14; implicitHeight: 14 }
                        }

                        Repeater {
                            model: 5
                            delegate: Rectangle {
                                required property int index
                                property bool on: (index + 1) === root.logPage
                                width: 32; height: 32; radius: 8
                                color: on ? "#2A3348" : (pgHover.hovered ? "#1A2130" : "transparent")
                                border.width: 1
                                border.color: on ? "#3A4560" : "#2D3648"
                                HoverHandler { id: pgHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: root.logPage = index + 1 }
                                Txt {
                                    anchors.centerIn: parent
                                    text: index + 1
                                    font.pixelSize: 12
                                    color: parent.on ? "#EDF0FA" : "#8A94A8"
                                }
                            }
                        }

                        Txt { text: "…"; font.pixelSize: 13; color: "#6A7488" }

                        Rectangle {
                            width: 32; height: 32; radius: 8
                            color: "transparent"
                            border.width: 1; border.color: "#2D3648"
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: root.logPage = Math.min(root.logPageCount, root.logPage + 1) }
                            Icon { anchors.centerIn: parent; name: "chevron"; stroke: "#8A94A8"
                                   implicitWidth: 14; implicitHeight: 14 }
                        }

                        Txt {
                            Layout.leftMargin: 6
                            text: root.logPage + " / " + root.logPageCount + " 페이지"
                            font.pixelSize: 12; color: "#6A7488"
                        }
                    }
                }
            }
        }
    }

    // 토스트 알림
    Rectangle {
        id: saveToast
        property string message: ""
        function show(msg) {
            if (msg !== undefined) message = msg
            opacity = 1
            hideTimer.restart()
        }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        width: toastRow.implicitWidth + 40
        height: 48
        radius: 12
        color: "#171F33"
        border.width: 1
        border.color: "#4550A8"
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 220 } }
        Timer { id: hideTimer; interval: 2200; onTriggered: saveToast.opacity = 0 }

        RowLayout {
            id: toastRow
            anchors.centerIn: parent
            spacing: 10
            Icon {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                name: "check-circle"; stroke: "#34D399"
            }
            Txt { text: saveToast.message; font.pixelSize: 13; color: "#E7ECF5" }
        }
    }
}
