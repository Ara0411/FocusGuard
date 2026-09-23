import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: pgSet

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
                title: "환경설정"
                subtitle: "엔진 동작 방식과 알림, 시스템 에이전트를 설정합니다"

                GhostBtn {
                    height: 42
                    text: "기본값 초기화"
                    glyph: "refresh"
                    onClicked: {
                        nameInput.text = "사용자"
                        targetInput.text = "03:00:00"
                        root.blockMode = 0
                        root.nicIndex = 0
                        root.rstRetry = 3
                        root.pomodoroFocus = 25
                        root.pomodoroRest = 5
                        root.toastEnabled = true
                        root.autoStart = true
                        root.trayOnClose = true
                    }
                }
                SolidBtn {
                    height: 42
                    text: "변경사항 저장"
                    glyph: "save"
                    onClicked: {
                        backend.saveAppSettings(
                            root.blockMode,
                            root.rstRetry,
                            root.pomodoroFocus,
                            root.pomodoroRest,
                            root.toastEnabled,
                            root.autoStart,
                            root.trayOnClose,
                            nameInput.text,
                            targetInput.text
                        )
                        saveToast.show("설정이 성공적으로 저장되었습니다")
                    }
                }
            }

            // ---------- 사용자 프로필 & 일일 집중 목표 카드 ----------
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.preferredHeight: 104

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 24

                    // 프로필 섹션
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Rectangle {
                            Layout.preferredWidth: 46
                            Layout.preferredHeight: 46
                            radius: 23
                            color: "#1E2235"
                            border.width: 1
                            border.color: "#3F4A75"
                            Icon {
                                anchors.centerIn: parent
                                name: "user"; stroke: "#818CF8"
                                implicitWidth: 22; implicitHeight: 22
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Txt { text: "사용자 프로필 (닉네임)"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#C3CBDB" }
                            Field {
                                id: nameInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                glyph: "user"
                                placeholderText: "이름 또는 닉네임을 입력하세요"
                                text: backend.userName
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 4
                        Layout.bottomMargin: 4
                        color: "#1F2836"
                    }

                    // 일일 목표 섹션
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Rectangle {
                            Layout.preferredWidth: 46
                            Layout.preferredHeight: 46
                            radius: 23
                            color: "#162822"
                            border.width: 1
                            border.color: "#1F523E"
                            Icon {
                                anchors.centerIn: parent
                                name: "target"; stroke: "#34D399"
                                implicitWidth: 22; implicitHeight: 22
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            RowLayout {
                                spacing: 8
                                Txt { text: "일일 목표 집중 시간"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#C3CBDB" }
                                Pill { text: "HH:MM:SS"; fg: "#34D399"; color: "#122A22"; fontSize: 10 }
                            }
                            Field {
                                id: targetInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                glyph: "clock"
                                placeholderText: "03:00:00"
                                text: backend.targetTime
                            }
                        }
                    }
                }
            }

            // ---------- 3단 카드 ----------
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.bottomMargin: 28
                spacing: 20

                // ===== 1. 네트워크 엔진 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 560
                    Layout.alignment: Qt.AlignTop

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 0

                        CardTitle { label: "네트워크 엔진 설정"; glyph: "cpu"; glyphColor: "#A5B4FC" }
                        Txt {
                            Layout.topMargin: 8
                            Layout.fillWidth: true
                            text: "Npcap 드라이버가 패킷을 가로채는 방식을 결정합니다"
                            font.pixelSize: 12; color: "#6A7488"
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 20 }

                        Txt { text: "차단 모드"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#C3CBDB" }
                        Item { Layout.preferredHeight: 10 }

                        Repeater {
                            model: [
                                { t: "엄격 모드",  s: "Default-Deny", d: "화이트리스트에 없는 모든 도메인을 차단합니다. 시험 기간에 권장." },
                                { t: "유연 모드",  s: "Blacklist",    d: "블랙리스트에 등록한 도메인만 차단합니다. 자료 조사가 많을 때." }
                            ]
                            delegate: Rectangle {
                                id: modeCard
                                required property var modelData
                                required property int index
                                property bool on: root.blockMode === index

                                Layout.fillWidth: true
                                Layout.preferredHeight: 88
                                Layout.bottomMargin: 10
                                radius: 12
                                color: on ? "#171F33" : "#121824"
                                border.width: 1
                                border.color: on ? "#4550A8" : "#212A38"
                                Behavior on color { ColorAnimation { duration: 140 } }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: root.blockMode = modeCard.index }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 13

                                    Rectangle {
                                        Layout.preferredWidth: 19
                                        Layout.preferredHeight: 19
                                        Layout.alignment: Qt.AlignTop
                                        Layout.topMargin: 2
                                        radius: 10
                                        color: "transparent"
                                        border.width: 1.6
                                        border.color: modeCard.on ? "#7B61FF" : "#3A4457"
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 9; height: 9; radius: 5
                                            color: "#7B61FF"
                                            visible: modeCard.on
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        RowLayout {
                                            spacing: 8
                                            Txt {
                                                text: modeCard.modelData.t
                                                font.pixelSize: 13; font.weight: Font.DemiBold
                                                color: modeCard.on ? "#EDF0FA" : "#9AA5BA"
                                            }
                                            Pill {
                                                text: modeCard.modelData.s
                                                fontSize: 10
                                                fg: modeCard.on ? "#A5B4FC" : "#6A7488"
                                                color: modeCard.on ? "#1D2438" : "#171D29"
                                            }
                                        }
                                        Txt {
                                            Layout.fillWidth: true
                                            text: modeCard.modelData.d
                                            font.pixelSize: 12
                                            lineHeight: 1.35
                                            color: "#6A7488"
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 10 }

                        Txt { text: "네트워크 인터페이스 (NIC)"; font.pixelSize: 12
                              font.weight: Font.DemiBold; color: "#C3CBDB" }
                        Item { Layout.preferredHeight: 10 }
                        Combo {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            options: root.nicList
                            currentIndex: root.nicIndex
                            onActivated: function(i) { root.nicIndex = i }
                        }

                        Item { Layout.preferredHeight: 18 }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 64
                            radius: 10
                            color: "#101622"
                            border.width: 1
                            border.color: "#212A38"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                Icon {
                                    name: "shield-check"; stroke: "#10B981"
                                    implicitWidth: 18; implicitHeight: 18
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Txt {
                                        text: "패킷 주입 최적화 (Hairpinning TCP RST)"
                                        font.pixelSize: 12; font.weight: Font.DemiBold; color: "#C3CBDB"
                                    }
                                    Txt {
                                        text: "양방향 세션 즉시 차단 알고리즘이 내장 고정되어 최상의 차단율을 유지합니다."
                                        font.pixelSize: 11; color: "#5F6879"
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // ===== 2. 뽀모도로 & 알림 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 560
                    Layout.alignment: Qt.AlignTop

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 0

                        CardTitle { label: "뽀모도로 타이머 & 알림"; glyph: "clock"; glyphColor: "#10B981" }
                        Txt {
                            Layout.topMargin: 8
                            Layout.fillWidth: true
                            text: "한 사이클의 집중·휴식 길이와 알림 방식을 정합니다"
                            font.pixelSize: 12; color: "#6A7488"
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 20 }

                        Txt { text: "집중 시간"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#C3CBDB" }
                        Item { Layout.preferredHeight: 10 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Repeater {
                                model: [25, 50]
                                delegate: Rectangle {
                                    id: focusOpt
                                    required property int modelData
                                    property bool on: root.pomodoroFocus === modelData
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 58
                                    radius: 12
                                    color: on ? "#171F33" : "#121824"
                                    border.width: 1
                                    border.color: on ? "#4550A8" : "#212A38"
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: root.pomodoroFocus = focusOpt.modelData }
                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 3
                                        Txt {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: focusOpt.modelData + "분"
                                            font.pixelSize: 17; font.weight: Font.DemiBold
                                            color: focusOpt.on ? "#EDF0FA" : "#8A94A8"
                                        }
                                        Txt {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: focusOpt.modelData === 25 ? "표준 뽀모도로" : "딥 워크"
                                            font.pixelSize: 10
                                            color: focusOpt.on ? "#8B7BFF" : "#5F6879"
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 20 }

                        Txt { text: "휴식 시간"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#C3CBDB" }
                        Item { Layout.preferredHeight: 10 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Repeater {
                                model: [5, 10]
                                delegate: Rectangle {
                                    id: restOpt
                                    required property int modelData
                                    property bool on: root.pomodoroRest === modelData
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 58
                                    radius: 12
                                    color: on ? "#122A24" : "#121824"
                                    border.width: 1
                                    border.color: on ? "#1D5745" : "#212A38"
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: root.pomodoroRest = restOpt.modelData }
                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 3
                                        Txt {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: restOpt.modelData + "분"
                                            font.pixelSize: 17; font.weight: Font.DemiBold
                                            color: restOpt.on ? "#EDF0FA" : "#8A94A8"
                                        }
                                        Txt {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: restOpt.modelData === 5 ? "짧은 휴식" : "긴 휴식"
                                            font.pixelSize: 10
                                            color: restOpt.on ? "#34D399" : "#5F6879"
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 22 }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 62
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Txt { text: "차단 발생 시 윈도우 토스트 알림"
                                      font.pixelSize: 12; color: "#D3DAE8" }
                                Txt { text: "차단된 도메인 이름을 알림 센터에 띄웁니다"
                                      font.pixelSize: 11; color: "#5F6879" }
                            }
                            Toggle {
                                Layout.alignment: Qt.AlignVCenter
                                checked: root.toastEnabled
                                onToggled: function(v) { root.toastEnabled = v }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 62
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Txt { text: "사이클 종료 알림음"
                                      font.pixelSize: 12; color: "#D3DAE8" }
                                Txt { text: "집중·휴식이 끝날 때 짧은 소리로 알립니다"
                                      font.pixelSize: 11; color: "#5F6879" }
                            }
                            Toggle { Layout.alignment: Qt.AlignVCenter; checked: true }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // ===== 3. 시스템 에이전트 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 560
                    Layout.alignment: Qt.AlignTop

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 0

                        CardTitle { label: "시스템 에이전트"; glyph: "sliders"; glyphColor: "#FBBF24" }
                        Txt {
                            Layout.topMargin: 8
                            Layout.fillWidth: true
                            text: "백그라운드 실행과 로컬 데이터 보관을 관리합니다"
                            font.pixelSize: 12; color: "#6A7488"
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 14 }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 62
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Txt { text: "윈도우 시작 시 자동 실행"; font.pixelSize: 12; color: "#D3DAE8" }
                                Txt { text: "부팅과 함께 감시 엔진을 올립니다"
                                      font.pixelSize: 11; color: "#5F6879" }
                            }
                            Toggle {
                                Layout.alignment: Qt.AlignVCenter
                                checked: root.autoStart
                                onToggled: function(v) { root.autoStart = v }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 62
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Txt { text: "창을 닫으면 트레이로 최소화"; font.pixelSize: 12; color: "#D3DAE8" }
                                Txt { text: "X 버튼을 눌러도 차단은 계속됩니다"
                                      font.pixelSize: 11; color: "#5F6879" }
                            }
                            Toggle {
                                Layout.alignment: Qt.AlignVCenter
                                checked: root.trayOnClose
                                onToggled: function(v) { root.trayOnClose = v }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }

                        Item { Layout.preferredHeight: 20 }

                        Txt { text: "로컬 데이터 (JSON)"; font.pixelSize: 12
                              font.weight: Font.DemiBold; color: "#C3CBDB" }
                        Txt {
                            Layout.topMargin: 6
                            Layout.fillWidth: true
                            text: "집중 기록과 정책은 서버로 전송되지 않고 이 경로에만 저장됩니다."
                            font.pixelSize: 11; color: "#5F6879"
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 12 }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Field {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                glyph: "folder"
                                readOnly: true
                                text: root.backupPath
                                color: "#8A94A8"
                            }
                            GhostBtn {
                                Layout.preferredWidth: 96
                                Layout.preferredHeight: 42
                                text: "찾아보기"
                            }
                        }

                        Item { Layout.preferredHeight: 12 }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            GhostBtn {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                text: "지금 백업"
                                glyph: "download"
                                onClicked: saveToast.show("백업 파일을 저장했습니다")
                            }
                            GhostBtn {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                text: "복원하기"
                                glyph: "refresh"
                            }
                        }

                        Item { Layout.preferredHeight: 20 }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 74
                            radius: 11
                            color: "#2A1420"
                            border.width: 1
                            border.color: "#5A2036"
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 15
                                spacing: 12
                                Icon {
                                    Layout.alignment: Qt.AlignTop
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    name: "alert"; stroke: "#F43F5E"
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Txt { text: "모든 기록 삭제"; font.pixelSize: 12
                                          font.weight: Font.DemiBold; color: "#FB7185" }
                                    Txt {
                                        Layout.fillWidth: true
                                        text: "집중 기록·차단 로그·정책이 모두 사라지며 되돌릴 수 없습니다."
                                        font.pixelSize: 11; color: "#A87088"
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }

    // 저장 토스트
    Rectangle {
        id: saveToast
        property string message: "설정을 저장했습니다"
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

    Connections {
        target: backend
        function onUserNameChanged() {
            if (!nameInput.activeFocus) nameInput.text = backend.userName
        }
        function onTargetTimeChanged() {
            if (!targetInput.activeFocus) targetInput.text = backend.targetTime
        }
    }
}
