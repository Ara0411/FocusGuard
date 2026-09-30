import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: pgEngineLog

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
                title: "엔진 콘솔 및 디버그 모니터"
                subtitle: "Npcap 패킷 캡처 및 차단 엔진의 실시간 터미널 표준 입출력을 감시합니다"

                GhostBtn {
                    height: 42
                    text: "로그 비우기"
                    glyph: "trash"
                    onClicked: backend.clearEngineLogs()
                }

                SolidBtn {
                    height: 42
                    text: backend.isEngineRunning ? "엔진 정지" : "엔진 기동"
                    glyph: backend.isEngineRunning ? "stop" : "play"
                    bg: backend.isEngineRunning ? "#EF4444" : "#10B981"
                    onClicked: {
                        if (backend.isEngineRunning) {
                            backend.stopEngine()
                        } else {
                            backend.startEngine()
                        }
                    }
                }

                GhostBtn {
                    height: 42
                    text: "엔진 재시작"
                    glyph: "refresh"
                    onClicked: backend.restartEngine()
                }
            }

            // ---------- 상태 대시보드 바 ----------
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.preferredHeight: 74

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 16

                    Rectangle {
                        width: 12; height: 12; radius: 6
                        color: backend.isEngineRunning ? "#10B981" : "#EF4444"
                        SequentialAnimation on opacity {
                            running: backend.isEngineRunning; loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutQuad }
                            NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                        }
                    }

                    Txt {
                        text: backend.isEngineRunning ? "엔진 상태: ONLINE (Npcap 감시 동작 중)" : "엔진 상태: STOPPED (감시 중지됨)"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: backend.isEngineRunning ? "#34D399" : "#F87171"
                    }

                    Item { Layout.fillWidth: true }

                    Txt {
                        text: "선택 어댑터: " + ((backend.targetAdapter === "" || backend.targetAdapter === "AUTO") ? "자동 감지 (권장)" : backend.targetAdapter)
                        font.pixelSize: 12
                        color: "#94A3B8"
                    }

                    Pill {
                        text: "관리자 모드 (UAC)"
                        fontSize: 10
                        fg: "#38BDF8"
                        color: "#0C4A6E"
                    }
                }
            }

            // ---------- 실시간 터미널 로그 뷰어 ----------
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.bottomMargin: 28
                Layout.preferredHeight: 620

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Icon { name: "cpu"; stroke: "#818CF8"; implicitWidth: 16; implicitHeight: 16 }
                        Txt { text: "실시간 콘솔 출력 (stdout / stderr)"; font.pixelSize: 13; font.weight: Font.DemiBold }
                        Item { Layout.fillWidth: true }
                        Txt {
                            text: "총 " + backend.engineLogs.length + " 라인"
                            font.pixelSize: 11
                            color: "#64748B"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        color: "#070A10"
                        border.width: 1
                        border.color: "#1E293B"

                        ListView {
                            id: logView
                            anchors.fill: parent
                            anchors.margins: 12
                            clip: true
                            model: backend.engineLogs
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            onCountChanged: {
                                if (root.autoScroll) {
                                    logView.positionViewAtEnd()
                                }
                            }

                            delegate: Item {
                                required property string modelData
                                width: logView.width
                                height: logLineTxt.implicitHeight + 4

                                Txt {
                                    id: logLineTxt
                                    width: parent.width
                                    text: modelData
                                    font.family: "Consolas"
                                    font.pixelSize: 12
                                    wrapMode: Text.WrapAnywhere
                                    color: {
                                        if (modelData.indexOf("[Error]") !== -1 || modelData.indexOf("STDERR") !== -1 || modelData.indexOf("⛔") !== -1) return "#F87171"
                                        if (modelData.indexOf("[BLACKLIST HIT]") !== -1 || modelData.indexOf("❌") !== -1) return "#FB7185"
                                        if (modelData.indexOf("[Whitelist]") !== -1 || modelData.indexOf("✅") !== -1 || modelData.indexOf("ONLINE") !== -1) return "#34D399"
                                        if (modelData.indexOf("[DNS Sniff]") !== -1 || modelData.indexOf("🔍") !== -1) return "#38BDF8"
                                        if (modelData.indexOf("[System]") !== -1 || modelData.indexOf("🚀") !== -1) return "#A78BFA"
                                        return "#94A3B8"
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

