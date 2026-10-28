import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: pgSet

    property int currentTargetHours: {
        var t = backend.targetTime || "03:00:00"
        var p = t.split(":")
        return parseInt(p[0]) || 3
    }
    property int currentTargetMinutes: {
        var t = backend.targetTime || "03:00:00"
        var p = t.split(":")
        return (p.length > 1) ? (parseInt(p[1]) || 0) : 0
    }
    readonly property string targetTimeValue: {
        var h = (currentTargetHours < 10 ? "0" : "") + currentTargetHours
        var m = (currentTargetMinutes < 10 ? "0" : "") + currentTargetMinutes
        return h + ":" + m + ":00"
    }

    function setTargetHoursAndMinutes(h, m) {
        currentTargetHours = Math.max(1, Math.min(12, h))
        currentTargetMinutes = Math.max(0, Math.min(59, m))
    }

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
                        pgSet.setTargetHoursAndMinutes(3, 0)
                        root.blockMode = 0
                        root.nicIndex = 0
                        root.rstRetry = 3
                        root.pomodoroFocus = 25
                        root.pomodoroRest = 5
                        root.toastEnabled = true
                        root.autoStart = true
                        root.trayOnClose = true
                        root.toggleShortcut = "Ctrl+Shift+U"
                        root.toggleShortcutEnabled = true
                    }
                }
                SolidBtn {
                    height: 42
                    text: "변경사항 저장"
                    glyph: "save"
                    bg: "#3B82F6"
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
                            pgSet.targetTimeValue,
                            root.toggleShortcut,
                            root.toggleShortcutEnabled
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
                Layout.preferredHeight: 114

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
                            Txt { text: "사용자 프로필 (닉네임)"; font.pixelSize: 12; font.weight: Font.Bold; color: "#E2E8F0" }
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

                    // 일일 목표 섹션 (단순화된 2개 스피너 UI)
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
                            spacing: 8
                            RowLayout {
                                spacing: 8
                                Txt { text: "일일 목표 집중 시간"; font.pixelSize: 12; font.weight: Font.Bold; color: "#E2E8F0" }
                                Pill { text: pgSet.targetTimeValue; fg: "#34D399"; color: "#122A22"; fontSize: 10 }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 14

                                // 시간(Hour) 스피너
                                Rectangle {
                                    Layout.preferredWidth: 136
                                    Layout.preferredHeight: 40
                                    radius: 8
                                    color: "#121A26"
                                    border.width: 1
                                    border.color: "#212D42"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        spacing: 0
                                        Rectangle {
                                            width: 32; height: 32; radius: 6
                                            color: hMinusHover.hovered ? "#212F47" : "transparent"
                                            HoverHandler { id: hMinusHover; cursorShape: Qt.PointingHandCursor }
                                            TapHandler { onTapped: pgSet.setTargetHoursAndMinutes(pgSet.currentTargetHours - 1, pgSet.currentTargetMinutes) }
                                            Txt { anchors.centerIn: parent; text: "−"; font.pixelSize: 15; font.weight: Font.Bold; color: "#A5B4FC" }
                                        }
                                        Txt {
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignHCenter
                                            text: pgSet.currentTargetHours + " 시간"
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            color: "#E2E8F0"
                                        }
                                        Rectangle {
                                            width: 32; height: 32; radius: 6
                                            color: hPlusHover.hovered ? "#212F47" : "transparent"
                                            HoverHandler { id: hPlusHover; cursorShape: Qt.PointingHandCursor }
                                            TapHandler { onTapped: pgSet.setTargetHoursAndMinutes(pgSet.currentTargetHours + 1, pgSet.currentTargetMinutes) }
                                            Txt { anchors.centerIn: parent; text: "+"; font.pixelSize: 15; font.weight: Font.Bold; color: "#A5B4FC" }
                                        }
                                    }
                                }

                                // 분(Minute) 스피너 (15분 단위)
                                Rectangle {
                                    Layout.preferredWidth: 136
                                    Layout.preferredHeight: 40
                                    radius: 8
                                    color: "#121A26"
                                    border.width: 1
                                    border.color: "#212D42"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        spacing: 0
                                        Rectangle {
                                            width: 32; height: 32; radius: 6
                                            color: mMinusHover.hovered ? "#212F47" : "transparent"
                                            HoverHandler { id: mMinusHover; cursorShape: Qt.PointingHandCursor }
                                            TapHandler {
                                                onTapped: {
                                                    var newM = pgSet.currentTargetMinutes - 15
                                                    if (newM < 0) newM = 45
                                                    pgSet.setTargetHoursAndMinutes(pgSet.currentTargetHours, newM)
                                                }
                                            }
                                            Txt { anchors.centerIn: parent; text: "−"; font.pixelSize: 15; font.weight: Font.Bold; color: "#34D399" }
                                        }
                                        Txt {
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignHCenter
                                            text: (pgSet.currentTargetMinutes < 10 ? "0" : "") + pgSet.currentTargetMinutes + " 분"
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            color: "#E2E8F0"
                                        }
                                        Rectangle {
                                            width: 32; height: 32; radius: 6
                                            color: mPlusHover.hovered ? "#212F47" : "transparent"
                                            HoverHandler { id: mPlusHover; cursorShape: Qt.PointingHandCursor }
                                            TapHandler {
                                                onTapped: {
                                                    var newM = pgSet.currentTargetMinutes + 15
                                                    if (newM > 45) newM = 0
                                                    pgSet.setTargetHoursAndMinutes(pgSet.currentTargetHours, newM)
                                                }
                                            }
                                            Txt { anchors.centerIn: parent; text: "+"; font.pixelSize: 15; font.weight: Font.Bold; color: "#34D399" }
                                        }
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }
            }

            // ---------- 3단 카드 ----------
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 28
                spacing: 16

                // ===== 1. 네트워크 엔진 설정 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 340
                    Layout.preferredHeight: 650
                    Layout.alignment: Qt.AlignTop
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 22
                        anchors.topMargin: 20
                        anchors.bottomMargin: 20
                        spacing: 0

                        CardTitle { label: "네트워크 엔진 설정"; glyph: "cpu"; glyphColor: "#A5B4FC" }
                        Txt {
                            Layout.topMargin: 8
                            Layout.fillWidth: true
                            text: "Npcap 드라이버가 패킷을 가로채고 제어하는 방식을 결정합니다"
                            font.pixelSize: 12; color: "#6A7488"
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 18 }

                        // ── 1. 차단 정책 ──
                        RowLayout {
                            spacing: 6
                            Rectangle { width: 3; height: 12; radius: 1.5; color: "#7B61FF" }
                            Txt { text: "도메인 필터링 정책"; font.pixelSize: 12; font.weight: Font.Bold; color: "#E2E8F0" }
                        }
                        Item { Layout.preferredHeight: 8 }

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
                                Layout.preferredHeight: 78
                                Layout.bottomMargin: 8
                                radius: 11
                                color: on ? "#171F33" : "#121824"
                                border.width: 1
                                border.color: on ? "#4550A8" : "#212A38"
                                Behavior on color { ColorAnimation { duration: 140 } }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: root.blockMode = modeCard.index }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 12

                                    Rectangle {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        Layout.alignment: Qt.AlignTop
                                        Layout.topMargin: 2
                                        radius: 9
                                        color: "transparent"
                                        border.width: 1.6
                                        border.color: modeCard.on ? "#7B61FF" : "#3A4457"
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 8; height: 8; radius: 4
                                            color: "#7B61FF"
                                            visible: modeCard.on
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
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
                                            font.pixelSize: 11
                                            lineHeight: 1.3
                                            color: "#6A7488"
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }

                        // ── 1-1. 차단 모드 전환 단축키 (Quick Hotkey) ──
                        Item { Layout.preferredHeight: 8 }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: hotkeyInnerCol.implicitHeight + 20
                            radius: 9
                            color: "#0F1522"
                            border.width: 1
                            border.color: "#1E2738"

                            ColumnLayout {
                                id: hotkeyInnerCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 10
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Icon { name: "terminal"; stroke: "#A5B4FC"; implicitWidth: 15; implicitHeight: 15 }
                                    Txt {
                                        text: "단축키 빠른 전환 (Quick Hotkey)"
                                        font.pixelSize: 11; font.weight: Font.Bold; color: "#E2E8F0"
                                    }
                                    Item { Layout.fillWidth: true }
                                    Toggle {
                                        checked: root.toggleShortcutEnabled
                                        onToggled: function(v) { root.toggleShortcutEnabled = v }
                                    }
                                }

                                Txt {
                                    Layout.fillWidth: true
                                    text: "작업 중 어디서나 단축키로 엄격 ↔ 유연 모드를 즉시 토글합니다 (콘솔 'U'키와 동기화)."
                                    font.pixelSize: 10; color: "#6A7488"; wrapMode: Text.WordWrap; lineHeight: 1.25
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    visible: root.toggleShortcutEnabled

                                    // 단축키 캡처/표시 박스
                                    Rectangle {
                                        id: keyBox
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 30
                                        radius: 6
                                        color: isRecording ? "#2A1E3D" : "#161E2E"
                                        border.width: 1
                                        border.color: isRecording ? "#A78BFA" : "#2E3B50"

                                        property bool isRecording: false

                                        focus: isRecording
                                        Keys.onPressed: function(event) {
                                            if (!isRecording) return;
                                            event.accepted = true;

                                            if (event.key === Qt.Key_Escape) {
                                                isRecording = false;
                                                return;
                                            }

                                            var parts = [];
                                            if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl");
                                            if (event.modifiers & Qt.AltModifier) parts.push("Alt");
                                            if (event.modifiers & Qt.ShiftModifier) parts.push("Shift");
                                            if (event.modifiers & Qt.MetaModifier) parts.push("Win");

                                            var k = event.key;
                                            var keyName = "";
                                            if (k >= Qt.Key_F1 && k <= Qt.Key_F24) {
                                                keyName = "F" + (k - Qt.Key_F1 + 1);
                                            } else if (k >= Qt.Key_A && k <= Qt.Key_Z) {
                                                keyName = String.fromCharCode(k);
                                            } else if (k >= Qt.Key_0 && k <= Qt.Key_9) {
                                                keyName = String.fromCharCode(k);
                                            } else if (k === Qt.Key_Space) {
                                                keyName = "Space";
                                            } else if (k === Qt.Key_Tab) {
                                                keyName = "Tab";
                                            }

                                            if (keyName.length > 0) {
                                                parts.push(keyName);
                                                var fullSeq = parts.join("+");
                                                if (parts.length > 1 || keyName.startsWith("F")) {
                                                    root.toggleShortcut = fullSeq;
                                                    isRecording = false;
                                                }
                                            }
                                        }

                                        RowLayout {
                                            anchors.centerIn: parent
                                            spacing: 4
                                            Txt {
                                                text: keyBox.isRecording ? "조합 키를 누르세요..." : (root.toggleShortcut || "Ctrl+Shift+U")
                                                font.pixelSize: 11
                                                font.family: "Consolas"
                                                font.weight: Font.DemiBold
                                                color: keyBox.isRecording ? "#C4B5FD" : "#818CF8"
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                keyBox.isRecording = true;
                                                keyBox.forceActiveFocus();
                                            }
                                        }
                                    }

                                    // 변경 버튼
                                    Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 30
                                        radius: 6
                                        color: "#1E273A"
                                        border.width: 1
                                        border.color: "#303E58"
                                        Txt {
                                            anchors.centerIn: parent
                                            text: keyBox.isRecording ? "취소" : "변경"
                                            font.pixelSize: 11; color: "#A5B4FC"
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                keyBox.isRecording = !keyBox.isRecording;
                                                if (keyBox.isRecording) keyBox.forceActiveFocus();
                                            }
                                        }
                                    }

                                    // 기본값 복원 버튼
                                    Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 30
                                        radius: 6
                                        color: "transparent"
                                        border.width: 1
                                        border.color: "#283348"
                                        Txt {
                                            anchors.centerIn: parent
                                            text: "초기화"
                                            font.pixelSize: 10; color: "#6A7488"
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                keyBox.isRecording = false;
                                                root.toggleShortcut = "Ctrl+Shift+U";
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ── 구분선 ──
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836"; Layout.topMargin: 6; Layout.bottomMargin: 14 }

                        // ── 2. 네트워크 인터페이스 (NIC) ──
                        RowLayout {
                            spacing: 6
                            Rectangle { width: 3; height: 12; radius: 1.5; color: "#38BDF8" }
                            Txt { text: "네트워크 인터페이스 어댑터 (NIC)"; font.pixelSize: 12; font.weight: Font.Bold; color: "#E2E8F0" }
                        }
                        Item { Layout.preferredHeight: 4 }
                        Txt {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            text: "인터넷 트래픽이 활성화된 네트워크를 자동 감지하거나 수동 지정합니다."
                            font.pixelSize: 11
                            color: "#7E8B9F"
                            wrapMode: Text.WordWrap
                            lineHeight: 1.35
                        }
                        Item { Layout.preferredHeight: 8 }
                        Combo {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            options: root.nicList
                            currentIndex: root.nicIndex
                            onActivated: function(i) {
                                root.nicIndex = i
                                backend.selectNetworkAdapter(i)
                                if (i === 0) {
                                    saveToast.show("네트워크 인터페이스가 '자동 감지'(으)로 설정되었습니다.")
                                } else {
                                    saveToast.show("네트워크 인터페이스가 '" + root.nicList[i] + "'(으)로 변경되었습니다.")
                                }
                            }
                        }

                        // ── 구분선 ──
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836"; Layout.topMargin: 12; Layout.bottomMargin: 14 }

                        // ── 3. 패킷 최적화 ──
                        RowLayout {
                            spacing: 6
                            Rectangle { width: 3; height: 12; radius: 1.5; color: "#10B981" }
                            Txt { text: "커널 패킷 주입 최적화"; font.pixelSize: 12; font.weight: Font.Bold; color: "#E2E8F0" }
                        }
                        Item { Layout.preferredHeight: 8 }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70
                            radius: 10
                            color: "#101622"
                            border.width: 1
                            border.color: "#212A38"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 12

                                Icon {
                                    name: "shield-check"; stroke: "#10B981"
                                    implicitWidth: 18; implicitHeight: 18
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Txt {
                                        Layout.fillWidth: true
                                        text: "패킷 주입 최적화 (Hairpinning TCP RST)"
                                        font.pixelSize: 12; font.weight: Font.Bold; color: "#E2E8F0"
                                        elide: Text.ElideRight
                                    }
                                    Txt {
                                        Layout.fillWidth: true
                                        text: "양방향 세션 즉시 차단 알고리즘이 내장 고정되어 최상의 차단율을 유지합니다."
                                        font.pixelSize: 11; color: "#5F6879"
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // ===== 2. 뽀모도로 타이머 & 알림 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 380
                    Layout.preferredHeight: 650
                    Layout.alignment: Qt.AlignTop
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 22
                        anchors.topMargin: 20
                        anchors.bottomMargin: 20
                        spacing: 0

                        CardTitle { label: "뽀모도로 타이머 & 알림"; glyph: "clock"; glyphColor: "#10B981" }
                        Txt {
                            Layout.topMargin: 8
                            Layout.fillWidth: true
                            text: "한 사이클의 집중·휴식 길이와 알림 방식을 정합니다"
                            font.pixelSize: 12; color: "#6A7488"
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 18 }

                        Txt { text: "집중 시간"; font.pixelSize: 12; font.weight: Font.Bold; color: "#E2E8F0" }
                        Item { Layout.preferredHeight: 8 }
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
                                    Layout.preferredHeight: 64
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
                                            font.pixelSize: 18; font.weight: Font.DemiBold
                                            color: focusOpt.on ? "#EDF0FA" : "#8A94A8"
                                        }
                                        Txt {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: focusOpt.modelData === 25 ? "표준 뽀모도로" : "딥 워크"
                                            font.pixelSize: 11
                                            color: focusOpt.on ? "#8B7BFF" : "#5F6879"
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 18 }

                        Txt { text: "휴식 시간"; font.pixelSize: 12; font.weight: Font.Bold; color: "#E2E8F0" }
                        Item { Layout.preferredHeight: 8 }
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
                                    Layout.preferredHeight: 64
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
                                            font.pixelSize: 18; font.weight: Font.DemiBold
                                            color: restOpt.on ? "#EDF0FA" : "#8A94A8"
                                        }
                                        Txt {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: restOpt.modelData === 5 ? "짧은 휴식" : "긴 휴식"
                                            font.pixelSize: 11
                                            color: restOpt.on ? "#34D399" : "#5F6879"
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 16 }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3
                                Txt {
                                    Layout.fillWidth: true
                                    text: "뽀모도로 집중/휴식 자동 루프"
                                    font.pixelSize: 12; color: "#D3DAE8"
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    text: "시간 도달 시 자동으로 휴식(일시정지) 및 집중 모드로 전환"
                                    font.pixelSize: 11; color: "#5F6879"
                                    wrapMode: Text.WordWrap
                                }
                            }
                            Toggle {
                                Layout.alignment: Qt.AlignVCenter
                                checked: backend.pomodoroAutoEnabled
                                onToggled: function(v) { backend.pomodoroAutoEnabled = v }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3
                                Txt {
                                    Layout.fillWidth: true
                                    text: "차단 발생 시 윈도우 토스트 알림"
                                    font.pixelSize: 12; color: "#D3DAE8"
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    text: "차단된 도메인 이름을 알림 센터에 띄웁니다"
                                    font.pixelSize: 11; color: "#5F6879"
                                    wrapMode: Text.WordWrap
                                }
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
                            Layout.preferredHeight: 56
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3
                                Txt {
                                    Layout.fillWidth: true
                                    text: "사이클 종료 알림음"
                                    font.pixelSize: 12; color: "#D3DAE8"
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    text: "집중·휴식이 끝날 때 짧은 소리로 알립니다"
                                    font.pixelSize: 11; color: "#5F6879"
                                    wrapMode: Text.WordWrap
                                }
                            }
                            Toggle { Layout.alignment: Qt.AlignVCenter; checked: true }
                        }

                        // ── 하단 안내 팁 박스 (읽기 전용 Tip 디자인 분리) ──
                        Item { Layout.preferredHeight: 14 }
                        Rectangle {
                            id: tipBox
                            Layout.fillWidth: true
                            implicitHeight: tipContentRow.implicitHeight + 24
                            radius: 9
                            color: "#0F172A"
                            border.width: 1
                            border.color: "#1E293B"

                            RowLayout {
                                id: tipContentRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                anchors.topMargin: 12
                                spacing: 12

                                Rectangle {
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    Layout.alignment: Qt.AlignTop
                                    Layout.topMargin: 2
                                    radius: 13
                                    color: "#1A253A"
                                    Txt {
                                        anchors.centerIn: parent
                                        text: "ℹ"
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                        color: "#94A3B8"
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Txt {
                                            text: "뽀모도로 권장 루틴"
                                            font.pixelSize: 12
                                            font.weight: Font.Bold
                                            color: "#FDE68A"
                                        }
                                        Pill {
                                            text: "TIP"
                                            fontSize: 9
                                            fg: "#FDE68A"
                                            color: "#282518"
                                        }
                                    }

                                    Txt {
                                        Layout.fillWidth: true
                                        text: "표준 4사이클(약 2시간) 몰입 후에는 15~30분의 긴 휴식을 취할 때 도파민 균형과 뇌 피로도 회복에 가장 효과적입니다."
                                        font.pixelSize: 11
                                        lineHeight: 1.35
                                        color: "#94A3B8"
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // ===== 3. 시스템 에이전트 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 340
                    Layout.preferredHeight: 650
                    Layout.alignment: Qt.AlignTop
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 22
                        anchors.topMargin: 20
                        anchors.bottomMargin: 20
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
                            Layout.preferredHeight: 56
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3
                                Txt {
                                    Layout.fillWidth: true
                                    text: "윈도우 시작 시 자동 실행"
                                    font.pixelSize: 12; color: "#D3DAE8"
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    text: "부팅과 함께 감시 엔진을 올립니다"
                                    font.pixelSize: 11; color: "#5F6879"
                                    wrapMode: Text.WordWrap
                                }
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
                            Layout.preferredHeight: 56
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3
                                Txt {
                                    Layout.fillWidth: true
                                    text: "창을 닫으면 트레이로 최소화"
                                    font.pixelSize: 12; color: "#D3DAE8"
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    text: "X 버튼을 눌러도 차단은 계속됩니다"
                                    font.pixelSize: 11; color: "#5F6879"
                                    wrapMode: Text.WordWrap
                                }
                            }
                            Toggle {
                                Layout.alignment: Qt.AlignVCenter
                                checked: root.trayOnClose
                                onToggled: function(v) { root.trayOnClose = v }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }

                        Item { Layout.preferredHeight: 18 }

                        Txt { text: "로컬 데이터 (JSON)"; font.pixelSize: 12
                              font.weight: Font.Bold; color: "#E2E8F0" }
                        Txt {
                            Layout.topMargin: 4
                            Layout.fillWidth: true
                            text: "집중 기록과 정책은 서버로 전송되지 않고 이 경로에만 저장됩니다."
                            font.pixelSize: 11; color: "#5F6879"
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 10 }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Field {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                glyph: "folder"
                                readOnly: true
                                text: root.backupPath
                                color: "#8A94A8"
                            }
                            GhostBtn {
                                Layout.preferredWidth: 88
                                Layout.preferredHeight: 40
                                text: "찾아보기"
                            }
                        }

                        Item { Layout.preferredHeight: 10 }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            GhostBtn {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                text: "지금 백업"
                                glyph: "download"
                                onClicked: saveToast.show("백업 파일을 저장했습니다")
                            }
                            GhostBtn {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                text: "복원하기"
                                glyph: "refresh"
                            }
                        }

                        // ── 통계 및 차단 기록 초기화 ──
                        Item { Layout.preferredHeight: 16 }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }
                        Item { Layout.preferredHeight: 14 }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 4
                                Txt {
                                    text: "통계 및 차단 기록 초기화"
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                    color: "#E2E8F0"
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    text: "누적 집중 시간과 차단 로그만 초기화되며, 환경설정은 유지됩니다."
                                    font.pixelSize: 11
                                    lineHeight: 1.3
                                    color: "#64748B"
                                    wrapMode: Text.WordWrap
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 108
                                Layout.preferredHeight: 36
                                Layout.alignment: Qt.AlignVCenter
                                radius: 8
                                color: delHover.hovered ? "#4C1D24" : "#24121A"
                                border.width: 1
                                border.color: delHover.hovered ? "#F43F5E" : "#7F1D1D"
                                HoverHandler { id: delHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler {
                                    onTapped: {
                                        saveToast.show("통계 및 차단 기록이 초기화되었습니다.")
                                    }
                                }
                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Icon { name: "trash"; stroke: "#FDA4AF"; implicitWidth: 13; implicitHeight: 13 }
                                    Txt { text: "기록 초기화"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#FDA4AF" }
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
            var t = backend.targetTime || "03:00:00"
            var p = t.split(":")
            pgSet.currentTargetHours = parseInt(p[0]) || 3
            pgSet.currentTargetMinutes = (p.length > 1) ? (parseInt(p[1]) || 0) : 0
        }
    }
}
