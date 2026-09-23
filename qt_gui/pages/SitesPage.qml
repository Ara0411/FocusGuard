import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: pgSites
    property int tabIndex: 0
    readonly property var activeModel: tabIndex === 0 ? root.policyWhiteModel : (tabIndex === 1 ? root.policyBlackModel : root.policyProcessModel)
    property int catIndex: 0

    function addPolicy(customVal) {
        var v = (customVal !== undefined ? customVal : siteField.text).trim()
        if (v.length === 0) return
        var today = new Date().toISOString().slice(0, 10)

        if (tabIndex === 0) {
            v = v.toLowerCase()
            activeModel.append({
                site: v,
                cat: root.siteCategories[catIndex],
                date: today,
                memo: memoField.text,
                onDuty: true
            })
            root.whiteListModel.append({ site: v, tag: root.siteCategories[catIndex] })
            backend.addToWhitelist(v)
        } else if (tabIndex === 1) {
            v = v.toLowerCase()
            activeModel.append({
                site: v,
                cat: root.siteCategories[catIndex],
                date: today,
                memo: memoField.text,
                onDuty: true
            })
            backend.addToBlacklist(v)
        } else {
            if (!v.toLowerCase().endsWith(".exe")) {
                v += ".exe"
            }
            activeModel.append({
                site: v,
                cat: "프로그램",
                date: today,
                memo: memoField.text !== "" ? memoField.text : "차단 앱 등록됨",
                onDuty: true
            })
            backend.addBlockedProcess(v)
        }
        siteField.text = ""
        memoField.text = ""
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
                title: "접속 정책 및 차단 앱 관리"
                subtitle: "네트워크 패킷(SNI·DNS)과 Windows 방해 프로세스를 실시간 차단합니다"

                GhostBtn {
                    height: 42
                    text: "정책 가져오기"
                    glyph: "folder"
                }
            }

            // ---------- 탭 스위처 (3단) ----------
            Row {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 10

                Repeater {
                    model: [
                        { name: "허용 사이트 (화이트리스트)", icon: "shield-check", color: "#10B981" },
                        { name: "차단 사이트 (블랙리스트)",   icon: "ban",          color: "#F43F5E" },
                        { name: "차단 프로그램 (앱)",         icon: "cpu",          color: "#A855F7" }
                    ]
                    delegate: Rectangle {
                        id: tabBtn
                        required property int index
                        required property var modelData
                        property bool on: pgSites.tabIndex === index
                        width: tabRow.implicitWidth + 34
                        height: 42
                        radius: 11
                        color: on ? "#1D2439" : (tabHover.hovered ? "#161C2A" : "#141A26")
                        border.width: 1
                        border.color: on ? "#3A4470" : "#212A38"
                        Behavior on color { ColorAnimation { duration: 140 } }
                        HoverHandler { id: tabHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: pgSites.tabIndex = index }

                        RowLayout {
                            id: tabRow
                            anchors.centerIn: parent
                            spacing: 9
                            Icon {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 17
                                Layout.preferredHeight: 17
                                name: tabBtn.modelData.icon
                                stroke: tabBtn.on ? tabBtn.modelData.color : "#6E7A92"
                            }
                            Txt {
                                Layout.alignment: Qt.AlignVCenter
                                text: tabBtn.modelData.name
                                font.pixelSize: 13
                                font.weight: tabBtn.on ? Font.DemiBold : Font.Normal
                                color: tabBtn.on ? "#EDF0FA" : "#8A94A8"
                            }
                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: Math.max(22, cntTxt.implicitWidth + 12)
                                Layout.preferredHeight: 20
                                radius: 10
                                color: tabBtn.on ? "#2A3348" : "#1B2230"
                                Txt {
                                    id: cntTxt
                                    anchors.centerIn: parent
                                    text: tabBtn.index === 0 ? root.policyWhiteModel.count
                                          : (tabBtn.index === 1 ? root.policyBlackModel.count : root.policyProcessModel.count)
                                    font.pixelSize: 10
                                    color: tabBtn.on ? "#C0C8FF" : "#7C8699"
                                }
                            }
                        }
                    }
                }
            }

            // ---------- 입력 바 ----------
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.preferredHeight: pgSites.tabIndex === 2 ? 134 : 92

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Field {
                            id: siteField
                            Layout.preferredWidth: pgSites.tabIndex === 2 ? 340 : 280
                            Layout.preferredHeight: 44
                            glyph: pgSites.tabIndex === 2 ? "cpu" : "globe"
                            placeholderText: pgSites.tabIndex === 0
                                             ? "허용할 도메인 (예: ebs.co.kr)"
                                             : (pgSites.tabIndex === 1 ? "차단할 도메인 (예: youtube.com)" : "차단할 실행 파일명 (예: kakaotalk.exe)")
                            onAccepted: pgSites.addPolicy()
                        }

                        Combo {
                            visible: pgSites.tabIndex !== 2
                            Layout.preferredWidth: 170
                            Layout.preferredHeight: 44
                            options: root.siteCategories
                            currentIndex: pgSites.catIndex
                            onActivated: function(i) { pgSites.catIndex = i }
                        }

                        Field {
                            id: memoField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            placeholderText: pgSites.tabIndex === 2
                                             ? "메모 (선택) — 예: 업무 집중용 메신저 차단"
                                             : "메모 (선택) — 등록 이유를 남겨두면 나중에 판단이 쉽습니다"
                            onAccepted: pgSites.addPolicy()
                        }

                        SolidBtn {
                            Layout.preferredWidth: 140
                            Layout.preferredHeight: 44
                            text: pgSites.tabIndex === 2 ? "앱 차단 등록" : "정책 등록"
                            glyph: "plus"
                            onClicked: pgSites.addPolicy()
                        }
                    }

                    // 앱 차단 탭일 때 빠른 프리셋 칩 목록
                    RowLayout {
                        visible: pgSites.tabIndex === 2
                        Layout.fillWidth: true
                        spacing: 8

                        Txt {
                            text: "자주 차단하는 앱:"
                            font.pixelSize: 11
                            color: "#7C8699"
                        }

                        Repeater {
                            model: [
                                { label: "카카오톡", exe: "kakaotalk.exe" },
                                { label: "디스코드", exe: "Discord.exe" },
                                { label: "스팀(Steam)", exe: "steam.exe" },
                                { label: "롤(LoL)", exe: "LeagueClient.exe" },
                                { label: "메모장", exe: "notepad.exe" }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                height: 26
                                width: chipTxt.implicitWidth + 20
                                radius: 13
                                color: chipHover.hovered ? "#28203E" : "#1B172E"
                                border.width: 1
                                border.color: chipHover.hovered ? "#9333EA" : "#4A2B70"
                                HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler {
                                    onTapped: pgSites.addPolicy(modelData.exe)
                                }
                                Txt {
                                    id: chipTxt
                                    anchors.centerIn: parent
                                    text: "+ " + modelData.label
                                    font.pixelSize: 11
                                    color: chipHover.hovered ? "#E9D5FF" : "#C084FC"
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ---------- 2단 그리드 (6 : 4) ----------
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.bottomMargin: 28
                spacing: 20

                // ===== 좌측 : 정책 목록 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 580

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 9
                            Icon {
                                name: pgSites.tabIndex === 0 ? "shield-check" : (pgSites.tabIndex === 1 ? "ban" : "cpu")
                                stroke: pgSites.tabIndex === 0 ? "#10B981" : (pgSites.tabIndex === 1 ? "#F43F5E" : "#A855F7")
                                implicitWidth: 19; implicitHeight: 19
                            }
                            Txt {
                                text: pgSites.tabIndex === 0 ? "등록된 허용 정책" : (pgSites.tabIndex === 1 ? "등록된 차단 정책" : "등록된 차단 프로그램 (앱)")
                                font.pixelSize: 14; font.weight: Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            GhostBtn {
                                height: 30
                                text: "선택 삭제"
                                glyph: "trash"
                                fg: "#FB7185"
                                fontSize: 12
                            }
                        }

                        Item { Layout.preferredHeight: 16 }

                        // 테이블 헤더
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            spacing: 12
                            CheckBox2 { Layout.preferredWidth: 18 }
                            Txt { Layout.fillWidth: true; text: pgSites.tabIndex === 2 ? "실행 파일명 (exe)" : "도메인"; font.pixelSize: 12; color: "#7C8699" }
                            Txt { Layout.preferredWidth: 100; text: "카테고리"; font.pixelSize: 12; color: "#7C8699"; horizontalAlignment: Text.AlignHCenter }
                            Txt { Layout.preferredWidth: 90;  text: "등록일";   font.pixelSize: 12; color: "#7C8699"; horizontalAlignment: Text.AlignHCenter }
                            Txt { Layout.preferredWidth: 50;  text: "상태";     font.pixelSize: 12; color: "#7C8699"; horizontalAlignment: Text.AlignHCenter }
                            Item { Layout.preferredWidth: 36 } // 휴지통 버튼 폭
                            Item { Layout.preferredWidth: 14 } // 우측 스크롤바 예약 공간
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1F2836" }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.topMargin: 4
                            clip: true
                            model: pgSites.activeModel
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Item {
                                id: siteRow
                                required property int index
                                required property string site
                                required property string cat
                                required property string date
                                required property string memo
                                required property bool onDuty

                                width: ListView.view.width
                                height: 54

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    radius: 8
                                    color: srHover.hovered ? "#161D2B" : "transparent"
                                }
                                HoverHandler { id: srHover }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 0
                                    anchors.rightMargin: 14 // 스크롤바와 겹침 방지 여백
                                    spacing: 12

                                    CheckBox2 { Layout.preferredWidth: 18; Layout.alignment: Qt.AlignVCenter }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 9
                                        Icon {
                                            name: siteRow.onDuty ? (pgSites.tabIndex === 2 ? "cpu" : "check-circle") : "shield-off"
                                            stroke: siteRow.onDuty
                                                    ? (pgSites.tabIndex === 0 ? "#10B981" : (pgSites.tabIndex === 1 ? "#F43F5E" : "#A855F7"))
                                                    : "#4E5768"
                                            implicitWidth: 16; implicitHeight: 16
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Txt {
                                                Layout.fillWidth: true
                                                text: siteRow.site
                                                font.pixelSize: 12
                                                color: siteRow.onDuty ? "#D3DAE8" : "#6A7488"
                                                elide: Text.ElideRight
                                            }
                                            Txt {
                                                visible: siteRow.memo !== ""
                                                Layout.fillWidth: true
                                                text: siteRow.memo
                                                font.pixelSize: 11
                                                color: "#5F6879"
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }

                                    Pill {
                                        Layout.preferredWidth: 100
                                        text: siteRow.cat
                                        fontSize: 10
                                        fg: root.catColor(siteRow.cat)
                                        color: root.catBg(siteRow.cat)
                                    }

                                    Txt {
                                        Layout.preferredWidth: 90
                                        text: siteRow.date
                                        font.pixelSize: 12
                                        font.family: "Consolas"
                                        color: "#7C8699"
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    Toggle {
                                        Layout.preferredWidth: 50
                                        Layout.alignment: Qt.AlignVCenter
                                        checked: siteRow.onDuty
                                        onToggled: function(v) {
                                            pgSites.activeModel.setProperty(siteRow.index, "onDuty", v)
                                        }
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: 36
                                        Layout.preferredHeight: 30
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 8
                                        color: delHover.hovered ? "#2A1420" : "transparent"
                                        HoverHandler { id: delHover; cursorShape: Qt.PointingHandCursor }
                                        TapHandler {
                                            onTapped: {
                                                var targetSite = siteRow.site
                                                if (pgSites.tabIndex === 0) {
                                                    backend.removeFromWhitelist(targetSite)
                                                } else if (pgSites.tabIndex === 1) {
                                                    backend.removeFromBlacklist(targetSite)
                                                } else {
                                                    backend.removeBlockedProcess(targetSite)
                                                }
                                                pgSites.activeModel.remove(siteRow.index)
                                            }
                                        }
                                        Icon {
                                            anchors.centerIn: parent
                                            name: "trash"
                                            stroke: delHover.hovered ? "#FB7185" : "#5F6879"
                                            implicitWidth: 16; implicitHeight: 16
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width; height: 1
                                    color: "#1B2331"
                                }
                            }
                        }
                    }
                }

                // ===== 우측 : 정책 안내 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 4
                    Layout.preferredHeight: 580

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 0

                        CardTitle { label: "정책 동작 원리"; glyph: "cpu"; glyphColor: "#A5B4FC" }

                        Item { Layout.preferredHeight: 14 }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 112
                            radius: 11
                            color: "#131B2C"
                            border.width: 1
                            border.color: "#25314C"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 8
                                RowLayout {
                                    spacing: 8
                                    Pill {
                                        text: pgSites.tabIndex === 2 ? "Process-Terminator" : "Default-Deny"
                                        fg: "#A5B4FC"; color: "#1D2438"; fontSize: 10
                                    }
                                    Pill {
                                        text: pgSites.tabIndex === 2 ? "스마트 감시" : "엄격 모드"
                                        fg: pgSites.tabIndex === 2 ? "#C084FC" : "#34D399"
                                        color: pgSites.tabIndex === 2 ? "#261536" : "#0F2A22"
                                        fontSize: 10
                                    }
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 12
                                    lineHeight: 1.45
                                    color: "#9AA5BA"
                                    text: pgSites.tabIndex === 2
                                          ? "Windows 프로세스 스냅샷을 0.2초마다 검사하여 등록된 방해 프로그램(exe)이 실행되면 즉시 프로세스를 강제 종료(TerminateProcess)합니다."
                                          : "엔진이 TLS Client Hello의 SNI 필드를 읽어 도메인을 판별합니다. 화이트리스트에 없으면 양방향으로 TCP RST 패킷을 주입해 세션을 즉시 끊습니다."
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 16 }

                        Txt { text: "차단 판정 순서"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#C3CBDB" }

                        Item { Layout.preferredHeight: 8 }

                        Repeater {
                            model: [
                                "OS 필수 예외 도메인인가?",
                                "화이트리스트에 등록되어 있는가?",
                                "블랙리스트에 명시되어 있는가?",
                                "위 어디에도 없으면 → 차단"
                            ]
                            delegate: RowLayout {
                                required property string modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                spacing: 10
                                Rectangle {
                                    width: 20; height: 20; radius: 10
                                    color: index === 3 ? "#2A1420" : "#1D2438"
                                    Txt {
                                        anchors.centerIn: parent
                                        text: index + 1
                                        font.pixelSize: 10
                                        color: index === 3 ? "#FB7185" : "#A5B4FC"
                                    }
                                }
                                Txt {
                                    Layout.fillWidth: true
                                    text: modelData
                                    font.pixelSize: 12
                                    color: index === 3 ? "#FB7185" : "#9AA5BA"
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 16 }

                        RowLayout {
                            spacing: 8
                            Icon { name: "lock"; stroke: "#FBBF24"; implicitWidth: 16; implicitHeight: 16 }
                            Txt { text: "OS 필수 예외 (해제 불가)"
                                  font.pixelSize: 12; font.weight: Font.DemiBold; color: "#C3CBDB" }
                        }

                        Item { Layout.preferredHeight: 8 }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 144
                            radius: 11
                            color: "#101622"
                            border.width: 1
                            border.color: "#242D3E"

                            Column {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 0
                                Repeater {
                                    model: [
                                        { n: "windowsupdate.microsoft.com", d: "Windows Update" },
                                        { n: "time.windows.com",           d: "NTP 시각 동기화" },
                                        { n: "ocsp.digicert.com",          d: "인증서 검증(OCSP)" },
                                        { n: "127.0.0.1 / ::1",            d: "로컬 루프백" }
                                    ]
                                    delegate: RowLayout {
                                        required property var modelData
                                        width: parent.width
                                        height: 30
                                        spacing: 10
                                        Icon {
                                            Layout.alignment: Qt.AlignVCenter
                                            Layout.preferredWidth: 14
                                            Layout.preferredHeight: 14
                                            name: "lock"; stroke: "#5F6879"
                                        }
                                        Txt {
                                            Layout.fillWidth: true
                                            text: modelData.n
                                            font.pixelSize: 12
                                            font.family: "Consolas"
                                            color: "#8A94A8"
                                            elide: Text.ElideRight
                                        }
                                        Txt {
                                            text: modelData.d
                                            font.pixelSize: 11
                                            color: "#5F6879"
                                        }
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
}
