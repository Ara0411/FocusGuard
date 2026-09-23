import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: parent.width
            spacing: 20

            PageHead {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 26
                notify: root.notifyCount
                avatar: (root.userName && root.userName.length > 0) ? root.userName.charAt(0).toUpperCase() : "U"
                title: "안녕하세요, " + root.userName + " 님! 👋"
                subtitle: "오늘 <font color='#7B8BFF'><b>" + root.focusHours
                          + "시간째</b></font> 집중 중입니다! 🔥"
            }

        // ---------- 상단 3단 그리드 ----------
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            spacing: 20

            // (1) 집중 타이머
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 340
                Layout.preferredHeight: 352

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 0

                    RowLayout {
                        spacing: 9
                        Icon { name: "clock"; stroke: "#A5B4FC"; implicitWidth: 19; implicitHeight: 19 }
                        Txt { text: "집중 타이머"; font.pixelSize: 14; font.weight: Font.DemiBold }
                    }

                    Item { Layout.fillHeight: true }

                    // 원형 그라데이션 링
                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        width: 208; height: 208

                        Canvas {
                            id: ring
                            anchors.fill: parent
                            antialiasing: true
                            property real progress: root.ringProgress
                            onProgressChanged: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                var cx = width / 2, cy = height / 2
                                var r = width / 2 - 9
                                var lw = 11

                                ctx.lineWidth = lw
                                ctx.lineCap = "round"

                                // 트랙
                                ctx.strokeStyle = "#20283A"
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, 0, Math.PI * 2)
                                ctx.stroke()

                                if (progress <= 0.001) return

                                // 진행 링 (보라 → 블루 그라데이션)
                                var grd = ctx.createLinearGradient(0, 0, width, height)
                                grd.addColorStop(0.0, "#8B5CF6")
                                grd.addColorStop(0.5, "#6366F1")
                                grd.addColorStop(1.0, "#38BDF8")
                                ctx.strokeStyle = grd
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, -Math.PI / 2,
                                        -Math.PI / 2 + Math.PI * 2 * progress)
                                ctx.stroke()
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 5
                            Txt {
                                Layout.alignment: Qt.AlignHCenter
                                text: root.hms(root.timerCount)
                                font.pixelSize: 36
                                font.weight: Font.DemiBold
                                font.family: "Consolas"
                                font.letterSpacing: 0.5
                                color: "#F4F7FD"
                            }
                            Txt {
                                Layout.alignment: Qt.AlignHCenter
                                text: root.isFocusing ? "집중 중" : "일시 정지"
                                font.pixelSize: 13
                                color: "#8B93FF"
                            }
                            Item { Layout.preferredHeight: 16 }
                            Txt {
                                Layout.alignment: Qt.AlignHCenter
                                text: "목표 " + root.hms(root.targetCount)
                                font.pixelSize: 12
                                color: "#6A7488"
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        spacing: 12

                        // 집중 시작
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            radius: 11
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: startHover.hovered ? "#8A72FF" : "#7B61FF" }
                                GradientStop { position: 1.0; color: startHover.hovered ? "#6E71F5" : "#6366F1" }
                            }
                            HoverHandler { id: startHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: backend.togglePause() }
                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 8
                                Icon { name: root.isFocusing ? "stop" : "play"; stroke: "#FFFFFF"; implicitWidth: 14; implicitHeight: 14 }
                                Txt { text: root.isFocusing ? "집중 중지" : "집중 시작"
                                      font.pixelSize: 14; font.weight: Font.DemiBold; color: "#FFFFFF" }
                            }
                        }

                        // 휴식하기
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            radius: 11
                            color: !root.isFocusing ? "#4A2A0A" : (restHover.hovered ? "#212A3A" : "#1A2130")
                            border.width: 1
                            border.color: !root.isFocusing ? "#F59E0B" : "#2A3446"
                            HoverHandler { id: restHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: {
                                    if (root.isFocusing) backend.togglePause()
                                }
                            }
                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 8
                                Icon { name: "coffee"; stroke: !root.isFocusing ? "#FBBF24" : "#C6CEDF"; implicitWidth: 15; implicitHeight: 15 }
                                Txt { text: !root.isFocusing ? "휴식 중 ☕" : "휴식하기"
                                      font.pixelSize: 14
                                      color: !root.isFocusing ? "#FBBF24" : "#C6CEDF"
                                      font.weight: !root.isFocusing ? Font.DemiBold : Font.Normal }
                            }
                        }
                    }
                }
            }

            // (2) 오늘의 집중도
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 330
                Layout.preferredHeight: 352

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 0

                    RowLayout {
                        spacing: 9
                        Icon { name: "target"; stroke: "#A5B4FC"; implicitWidth: 19; implicitHeight: 19 }
                        Txt { text: "오늘의 집중도"; font.pixelSize: 14; font.weight: Font.DemiBold }
                    }

                    Item { Layout.preferredHeight: 30 }

                    Txt { text: "집중 목표 달성률"; font.pixelSize: 12; color: "#8B96AC" }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 14
                        spacing: 14

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 8
                            radius: 4
                            color: "#212A3A"
                            Rectangle {
                                width: parent.width * root.goalRatio
                                height: parent.height
                                radius: 4
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: "#7B61FF" }
                                    GradientStop { position: 1.0; color: "#6366F1" }
                                }
                                Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                            }
                        }
                        Txt {
                            text: Math.round(root.goalRatio * 100) + "%"
                            font.pixelSize: 19; font.weight: Font.DemiBold; color: "#8B7BFF"
                        }
                    }

                    Item { Layout.preferredHeight: 24 }

                    // 경고 박스
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        radius: 11
                        color: "#2A1420"
                        border.width: 1; border.color: "#5A2036"
                        RowLayout {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 15
                            spacing: 10
                            Icon { name: "check-circle"; stroke: "#F43F5E"
                                   implicitWidth: 17; implicitHeight: 17 }
                            Txt {
                                text: "오늘 차단된 딴짓 : 총 " + root.blockedCount + "회"
                                font.pixelSize: 13; font.weight: Font.Medium; color: "#FB7185"
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    // 하단 2분할
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 82
                        spacing: 0

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Txt { text: "집중 시간"; font.pixelSize: 12; color: "#8B96AC" }
                            Txt {
                                text: root.hms(root.timerCount)
                                font.pixelSize: 24; font.weight: Font.DemiBold
                                font.family: "Consolas"; color: "#F2F5FB"
                            }
                        }
                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 56
                            Layout.rightMargin: 18
                            color: "#232C3C"
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Txt { text: "최고 기록"; font.pixelSize: 12; color: "#8B96AC" }
                            Txt {
                                text: root.hms(root.bestRecord)
                                font.pixelSize: 24; font.weight: Font.DemiBold
                                font.family: "Consolas"; color: "#F2F5FB"
                            }
                        }
                    }
                }
            }

            // (3) 실시간 차단 로그
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 470
                Layout.preferredHeight: 352

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Icon { name: "pulse"; stroke: "#60A5FA"; implicitWidth: 19; implicitHeight: 19 }
                        Txt { text: "실시간 차단 로그"; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Item { Layout.fillWidth: true }
                        Pill {
                            text: "실시간"
                            fg: "#FB7185"
                            color: "#2A1420"
                            Rectangle {
                                anchors.fill: parent; radius: parent.radius
                                color: "transparent"; border.width: 1; border.color: "#57203A"
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 8 }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root.blockLogModel
                        interactive: false
                        spacing: 0
                        clip: true

                        delegate: Item {
                            required property string time
                            required property string domain
                            required property string action
                            width: ListView.view.width
                            height: 64

                            RowLayout {
                                anchors.fill: parent
                                anchors.rightMargin: 4
                                spacing: 13

                                Icon { name: "alert"; stroke: "#F43F5E"
                                       implicitWidth: 19; implicitHeight: 19 }

                                Txt {
                                    text: time
                                    font.pixelSize: 12
                                    font.family: "Consolas"
                                    color: "#C4CCDC"
                                }

                                Txt { text: "유해:"; font.pixelSize: 12; color: "#F43F5E"
                                      font.weight: Font.Medium }

                                Txt {
                                    Layout.fillWidth: true
                                    text: domain
                                    font.pixelSize: 12
                                    color: "#AEB8CB"
                                    elide: Text.ElideRight
                                }

                                Pill {
                                    text: action
                                    fg: "#FB7185"
                                    color: "#2A1420"
                                    fontSize: 10
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width; height: 1
                                color: "#1D2534"
                            }
                        }
                    }

                    // 전체 로그 보기
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        RowLayout {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            HoverHandler { id: logHover; cursorShape: Qt.PointingHandCursor }
                            Txt {
                                text: "전체 로그 보기"
                                font.pixelSize: 12
                                color: logHover.hovered ? "#C0C8FF" : "#96A0B6"
                            }
                            Icon {
                                name: "chevron"
                                stroke: logHover.hovered ? "#C0C8FF" : "#96A0B6"
                                implicitWidth: 14; implicitHeight: 14
                            }
                        }
                    }
                }
            }
        }

        // ---------- 중간 2단 그리드 ----------
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            spacing: 20

            // (4) 허용된 착한 사이트
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 500
                Layout.preferredHeight: 282

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Icon { name: "shield-check"; stroke: "#10B981"
                               implicitWidth: 19; implicitHeight: 19 }
                        Txt { text: "허용된 착한 사이트 (화이트)"
                              font.pixelSize: 14; font.weight: Font.DemiBold }
                        Item { Layout.fillWidth: true }
                        Pill {
                            text: root.whiteListModel.count + "개 활성화"
                            fg: "#34D399"
                            color: "#0F2A22"
                        }
                    }

                    Item { Layout.preferredHeight: 10 }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root.whiteListModel
                        interactive: false
                        clip: true

                        delegate: Item {
                            required property string site
                            required property string tag
                            required property int index
                            width: ListView.view.width
                            height: 36

                            RowLayout {
                                anchors.fill: parent
                                spacing: 12
                                Icon { name: "check-circle"; stroke: "#10B981"
                                       implicitWidth: 17; implicitHeight: 17 }
                                Txt {
                                    Layout.fillWidth: true
                                    text: site
                                    font.pixelSize: 13
                                    color: "#D3DAE8"
                                    elide: Text.ElideRight
                                }
                                Pill { text: tag; fg: "#A5B4FC"; color: "#1D2438"; fontSize: 10 }
                            }

                            Rectangle {
                                visible: index < root.whiteListModel.count - 1
                                anchors.bottom: parent.bottom
                                width: parent.width; height: 1
                                color: "#1B2331"
                            }
                        }
                    }
                }
            }

            // (5) 공부 필수 사이트 등록
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 640
                Layout.preferredHeight: 282

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 0

                    RowLayout {
                        spacing: 9
                        Icon { name: "globe"; stroke: "#60A5FA"; implicitWidth: 19; implicitHeight: 19 }
                        Txt { text: "공부 필수 사이트 등록"
                              font.pixelSize: 14; font.weight: Font.DemiBold }
                    }

                    Item { Layout.preferredHeight: 16 }

                    // 입력 + 등록
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        TextField {
                            id: siteInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            placeholderText: "인강/사전 주소 입력 (예: ebs.co.kr)"
                            placeholderTextColor: "#5C6579"
                            color: "#E2E8F4"
                            font.pixelSize: 12
                            font.family: "Pretendard, Malgun Gothic, Segoe UI"
                            leftPadding: 14
                            selectByMouse: true
                            background: Rectangle {
                                radius: 10
                                color: "#101622"
                                border.width: 1
                                border.color: siteInput.activeFocus ? "#4C4BE0" : "#242D3E"
                            }
                            onAccepted: { root.registerSite(siteInput.text); siteInput.text = "" }
                        }

                        Rectangle {
                            Layout.preferredWidth: 130
                            Layout.preferredHeight: 42
                            radius: 10
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: regHover.hovered ? "#8A72FF" : "#7B61FF" }
                                GradientStop { position: 1.0; color: regHover.hovered ? "#6E71F5" : "#6366F1" }
                            }
                            HoverHandler { id: regHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: { root.registerSite(siteInput.text); siteInput.text = "" } }
                            Txt {
                                anchors.centerIn: parent
                                text: "등록"
                                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#FFFFFF"
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 12 }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: root.studySiteModel
                        clip: true
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: Item {
                            required property int index
                            required property string site
                            required property string note
                            required property string tag
                            required property bool onDuty
                            width: ListView.view.width
                            height: 30

                            RowLayout {
                                anchors.fill: parent
                                anchors.rightMargin: 2
                                spacing: 10

                                Icon { name: "check-circle"; stroke: "#10B981"
                                       implicitWidth: 16; implicitHeight: 16 }

                                Txt {
                                    Layout.fillWidth: true
                                    text: site + "  " + note
                                    font.pixelSize: 12
                                    color: "#D3DAE8"
                                    elide: Text.ElideRight
                                }

                                Pill { text: tag; fg: "#A5B4FC"; color: "#1D2438"; fontSize: 10 }

                                Rectangle {
                                    Layout.preferredWidth: 62
                                    Layout.preferredHeight: 24
                                    radius: 7
                                    color: onDuty ? "#0F2A22" : "#232B3A"
                                    border.width: 1
                                    border.color: onDuty ? "#1D5745" : "#313B4E"
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    TapHandler {
                                        onTapped: root.studySiteModel.setProperty(index, "onDuty", !onDuty)
                                    }
                                    Txt {
                                        anchors.centerIn: parent
                                        text: onDuty ? "활성화" : "중지됨"
                                        font.pixelSize: 10
                                        color: onDuty ? "#34D399" : "#7C8699"
                                    }
                                }

                                Icon {
                                    name: "kebab"; stroke: "#6E7889"
                                    implicitWidth: 16; implicitHeight: 16
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---------- 하단 와이드 카드 ----------
        Card {
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            Layout.bottomMargin: 28
            Layout.preferredHeight: 300

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 9
                    Icon { name: "bars"; stroke: "#A5B4FC"; implicitWidth: 19; implicitHeight: 19 }
                    Txt { text: "오늘의 집중 통계"; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }

                    Rectangle {   // 기간 드롭다운
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 34
                        radius: 9
                        color: "#101622"
                        border.width: 1; border.color: "#242D3E"
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 10
                            Txt { text: "오늘"; font.pixelSize: 12; color: "#C3CBDB" }
                            Item { Layout.fillWidth: true }
                            Icon { name: "caret"; stroke: "#7C8699"
                                   implicitWidth: 15; implicitHeight: 15 }
                        }
                    }
                }

                Item { Layout.preferredHeight: 18 }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 24

                    // 좌측 요약
                    ColumnLayout {
                        Layout.preferredWidth: 210
                        Layout.fillHeight: true
                        spacing: 0

                        Txt { text: "총 집중 시간"; font.pixelSize: 12; color: "#8B96AC" }
                        Txt {
                            Layout.topMargin: 8
                            text: root.hms(root.timerCount)
                            font.pixelSize: 27; font.weight: Font.DemiBold
                            font.family: "Consolas"; color: "#F2F5FB"
                        }

                        Item { Layout.preferredHeight: 20 }

                        Txt { text: "차단된 시도"; font.pixelSize: 12; color: "#8B96AC" }
                        Txt {
                            Layout.topMargin: 8
                            text: root.blockedCount + "회"
                            font.pixelSize: 20; font.weight: Font.DemiBold; color: "#F2F5FB"
                        }

                        Item { Layout.preferredHeight: 20 }

                        Txt { text: "차단된 시간"; font.pixelSize: 12; color: "#8B96AC" }
                        Txt {
                            Layout.topMargin: 8
                            text: root.hms(root.blockedSecs)
                            font.pixelSize: 20; font.weight: Font.DemiBold
                            font.family: "Consolas"; color: "#F2F5FB"
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // 우측 막대 차트
                    Item {
                        id: chart
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        readonly property int maxMin: 60
                        readonly property real axisLeft: 42
                        readonly property real axisBottom: 30
                        readonly property real pinTop: 26
                        readonly property real plotH: height - axisBottom - pinTop
                        readonly property real plotW: width - axisLeft

                        // Y축 눈금 + 그리드
                        Repeater {
                            model: [60, 30, 0]
                            delegate: Item {
                                required property int modelData
                                required property int index
                                width: chart.width
                                height: 1
                                y: chart.pinTop + chart.plotH * (index / 2)

                                Txt {
                                    x: 0
                                    width: chart.axisLeft - 10
                                    horizontalAlignment: Text.AlignRight
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData + "분"
                                    font.pixelSize: 10
                                    color: "#5F6879"
                                }
                                Rectangle {
                                    x: chart.axisLeft
                                    width: chart.plotW
                                    height: 1
                                    color: "#1B2230"
                                }
                            }
                        }

                        // 막대
                        Row {
                            x: chart.axisLeft
                            y: chart.pinTop
                            width: chart.plotW
                            height: chart.plotH
                            spacing: 0

                            Repeater {
                                model: 24
                                delegate: Item {
                                    id: barCell
                                    required property int index
                                    width: chart.plotW / 24
                                    height: chart.plotH
                                    property bool isNow: index === root.currentHour
                                    property real val: root.hourlyFocus[index] !== undefined
                                                       ? root.hourlyFocus[index] : 0

                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.bottom: parent.bottom
                                        width: Math.max(8, barCell.width * 0.44)
                                        height: Math.max(barCell.val > 0 ? 4 : 0,
                                                         chart.plotH * barCell.val / chart.maxMin)
                                        radius: 4
                                        gradient: Gradient {
                                            GradientStop {
                                                position: 0.0
                                                color: barCell.isNow ? "#7DD3FC" : "#8B5CF6"
                                            }
                                            GradientStop {
                                                position: 1.0
                                                color: barCell.isNow ? "#38BDF8" : "#6D3EF0"
                                            }
                                        }
                                        Behavior on height { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
                                    }
                                }
                            }
                        }

                        // '현재' 핀 + 점선
                        Item {
                            id: nowPin
                            x: chart.axisLeft + chart.plotW * (root.currentHour + 0.5) / 24
                            y: 0
                            width: 1
                            height: chart.pinTop + chart.plotH

                            Canvas {   // 점선
                                anchors.fill: parent
                                anchors.topMargin: 22
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = "#3D4A63"
                                    ctx.lineWidth = 1
                                    ctx.setLineDash([3, 4])
                                    ctx.beginPath()
                                    ctx.moveTo(0.5, 0)
                                    ctx.lineTo(0.5, height)
                                    ctx.stroke()
                                }
                            }

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: 0
                                width: 48; height: 22; radius: 7
                                color: "#2A3348"
                                border.width: 1; border.color: "#3A4560"
                                Txt {
                                    anchors.centerIn: parent
                                    text: "현재"
                                    font.pixelSize: 10
                                    color: "#D3DAE8"
                                }
                            }
                        }

                        // X축 라벨 (짝수 시각)
                        Repeater {
                            model: 13
                            delegate: Txt {
                                required property int index
                                x: chart.axisLeft + chart.plotW * (index * 2) / 24 - width / 2
                                   + chart.plotW / 48
                                y: chart.pinTop + chart.plotH + 10
                                text: index * 2 < 10 ? "0" + (index * 2) : "" + (index * 2)
                                font.pixelSize: 10
                                font.family: "Consolas"
                                color: "#5F6879"
                            }
                        }
                    }
                }
            }
        }
        }
    }
}
