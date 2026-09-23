import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: pgStats
    property int period: 1     // 0 일간 / 1 주간 / 2 월간
    readonly property var heatColors: ["#161D2B", "#252C5C", "#3B3499", "#5B4BD6", "#8B74FF"]

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
                title: "학습 몰입도 분석 리포트"

                function getWeekRange() {
                    var now = new Date()
                    var day = now.getDay()
                    var diffToMon = (day === 0 ? -6 : 1 - day)
                    var mon = new Date(now.getTime() + diffToMon * 86400000)
                    var sun = new Date(mon.getTime() + 6 * 86400000)
                    return mon.getFullYear() + "년 " + (mon.getMonth() + 1) + "월 " + mon.getDate() + "일 ~ "
                         + (sun.getMonth() + 1) + "월 " + sun.getDate() + "일 · 이번 주 기준"
                }
                subtitle: getWeekRange()

                SegTabs {
                    height: 42
                    options: ["일간", "주간", "월간"]
                    currentIndex: pgStats.period
                    onSelected: function(i) { pgStats.period = i }
                }
            }

            // ---------- 핵심 메트릭 4열 ----------
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 20

                Metric {
                    Layout.fillWidth: true
                    label: "오늘 집중 시간"
                    value: root.hm(Math.floor(backend.elapsedSecs / 60))
                    sub: "목표 " + root.hm(Math.floor(root.targetCount / 60)) + " 기준"
                    glyph: "clock"
                    accent: "#A5B4FC"
                    accentBg: "#1D2438"
                }
                Metric {
                    Layout.fillWidth: true
                    label: "집중 목표 달성률"
                    value: Math.round(root.goalRatio * 100) + "%"
                    sub: root.goalRatio >= 1.0 ? "오늘의 목표 달성! 🎉" : "목표까지 " + root.hm(Math.max(0, Math.floor((root.targetCount - backend.elapsedSecs) / 60))) + " 남음"
                    glyph: "target"
                    accent: "#10B981"
                    accentBg: "#0F2A22"
                }
                Metric {
                    Layout.fillWidth: true
                    label: "방어한 딴짓"
                    value: backend.blockedAttempts + "회"
                    sub: "약 " + (backend.blockedSecs > 0 ? root.hm(Math.floor(backend.blockedSecs / 60)) : (backend.blockedAttempts * 3) + "분") + " 절약"
                    glyph: "shield-check"
                    accent: "#F43F5E"
                    accentBg: "#2A1420"
                }

                // Focus Score (원형 미니 게이지)
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 118

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 14

                        Item {
                            Layout.preferredWidth: 66
                            Layout.preferredHeight: 66
                            Layout.alignment: Qt.AlignVCenter

                            Canvas {
                                id: focusCanvas
                                anchors.fill: parent
                                antialiasing: true
                                Component.onCompleted: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    var cx = width / 2, cy = height / 2, r = width / 2 - 5
                                    ctx.lineWidth = 7
                                    ctx.lineCap = "round"
                                    ctx.strokeStyle = "#20283A"
                                    ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke()
                                    var g = ctx.createLinearGradient(0, 0, width, height)
                                    g.addColorStop(0.0, "#8B5CF6")
                                    g.addColorStop(1.0, "#38BDF8")
                                    ctx.strokeStyle = g
                                    var ratio = Math.max(0, Math.min(100, root.focusScore)) / 100
                                    if (ratio > 0) {
                                        ctx.beginPath()
                                        ctx.arc(cx, cy, r, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * ratio)
                                        ctx.stroke()
                                    }
                                }
                            }
                            Txt {
                                anchors.centerIn: parent
                                text: root.focusScore
                                font.pixelSize: 20; font.weight: Font.DemiBold; color: "#F2F5FB"
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Txt { text: "Focus Score"; font.pixelSize: 12; color: "#8B96AC" }
                            Txt { text: root.focusScore + "점"
                                  font.pixelSize: 21; font.weight: Font.DemiBold; color: "#F2F5FB" }
                            Txt {
                                text: root.focusScore >= 80 ? "최상의 몰입도 🔥" : (root.focusScore >= 60 ? "양호한 집중도 👍" : (root.focusScore > 0 ? "집중 향상 권장 ⚡" : "측정 대기 중 ⏳"))
                                font.pixelSize: 12; color: "#8B7BFF"
                            }
                        }
                    }
                }
            }

            // ---------- 2단 시각화 (6 : 4) ----------
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 20

                // ===== 좌 : 주간 집중도 추이 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 340

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 9
                            Icon { name: "bars"; stroke: "#A5B4FC"; implicitWidth: 19; implicitHeight: 19 }
                            Txt { text: "주간 집중도 추이"; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Item { Layout.fillWidth: true }
                            RowLayout {
                                spacing: 7
                                Rectangle {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 2
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: 1; color: "#10B981"
                                }
                                Txt {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: "목표 " + root.hm(root.weekGoalMin)
                                    font.pixelSize: 12; color: "#7C8699"
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 14 }

                        Canvas {
                            id: weekCanvas
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            antialiasing: true
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()

                                function roundRect(x, y, w, h, r) {
                                    ctx.beginPath()
                                    ctx.moveTo(x + r, y)
                                    ctx.lineTo(x + w - r, y)
                                    ctx.arcTo(x + w, y, x + w, y + r, r)
                                    ctx.lineTo(x + w, y + h - r)
                                    ctx.arcTo(x + w, y + h, x + w - r, y + h, r)
                                    ctx.lineTo(x + r, y + h)
                                    ctx.arcTo(x, y + h, x, y + h - r, r)
                                    ctx.lineTo(x, y + r)
                                    ctx.arcTo(x, y, x + r, y, r)
                                    ctx.closePath()
                                }

                                var padL = 44, padB = 28, padT = 12
                                var plotW = width - padL
                                var plotH = height - padB - padT
                                var maxV = Math.max(180, Math.ceil((root.weekGoalMin * 1.3) / 60) * 60)
                                var n = root.weekMinutes.length

                                ctx.font = "10px 'Malgun Gothic'"
                                ctx.textBaseline = "middle"

                                // 가로 그리드 + Y 라벨
                                for (var g = 0; g <= 3; g++) {
                                    var yy = padT + plotH * (g / 3)
                                    ctx.strokeStyle = "#1B2230"
                                    ctx.lineWidth = 1
                                    ctx.beginPath()
                                    ctx.moveTo(padL, yy + 0.5); ctx.lineTo(width, yy + 0.5)
                                    ctx.stroke()
                                    ctx.fillStyle = "#5F6879"
                                    ctx.textAlign = "right"
                                    ctx.fillText(Math.round(maxV - maxV * g / 3) + "분", padL - 10, yy)
                                }

                                // 막대
                                var slot = plotW / n
                                var bw = Math.min(34, slot * 0.46)
                                for (var i = 0; i < n; i++) {
                                    var v = (root.weekMinutes && root.weekMinutes[i] !== undefined) ? root.weekMinutes[i] : 0
                                    var bh = plotH * Math.min(1, v / maxV)
                                    var bx = padL + slot * i + (slot - bw) / 2
                                    var by = padT + plotH - bh
                                    var hit = v >= root.weekGoalMin && v > 0

                                    if (v > 0) {
                                        var grd = ctx.createLinearGradient(0, by, 0, by + bh)
                                        if (hit) {
                                            grd.addColorStop(0.0, "#8B5CF6")
                                            grd.addColorStop(1.0, "#6D3EF0")
                                        } else {
                                            grd.addColorStop(0.0, "#3D4576")
                                            grd.addColorStop(1.0, "#2E3559")
                                        }
                                        ctx.fillStyle = grd
                                        ctx.beginPath()
                                        roundRect(bx, by, bw, Math.max(bh, 4), 5)
                                        ctx.fill()

                                        // 값 라벨
                                        ctx.fillStyle = hit ? "#C0C8FF" : "#8A94A8"
                                        ctx.textAlign = "center"
                                        ctx.fillText(Math.floor(v / 60) + "h " + (v % 60) + "m", bx + bw / 2, by - 11)
                                    } else {
                                        ctx.fillStyle = "#1E2536"
                                        ctx.beginPath()
                                        roundRect(bx, padT + plotH - 3, bw, 3, 2)
                                        ctx.fill()

                                        ctx.fillStyle = "#465064"
                                        ctx.textAlign = "center"
                                        ctx.fillText("-", bx + bw / 2, padT + plotH - 10)
                                    }

                                    // 요일 라벨
                                    ctx.fillStyle = "#7C8699"
                                    ctx.fillText(root.weekLabels[i], bx + bw / 2, padT + plotH + 14)
                                }

                                // 목표 점선
                                var gy = padT + plotH - plotH * (root.weekGoalMin / maxV)
                                ctx.strokeStyle = "#10B981"
                                ctx.lineWidth = 1.4
                                ctx.setLineDash([5, 5])
                                ctx.beginPath()
                                ctx.moveTo(padL, gy); ctx.lineTo(width, gy)
                                ctx.stroke()
                                ctx.setLineDash([])
                            }
                        }
                    }
                }

                // ===== 우 : 도넛 Top 5 =====
                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 4
                    Layout.preferredHeight: 340

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 0

                        CardTitle { label: "가장 많이 낭비할 뻔한 도메인"; glyph: "ban"; glyphColor: "#F43F5E" }

                        Item { Layout.preferredHeight: 10 }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 16

                            Item {
                                Layout.preferredWidth: 150
                                Layout.fillHeight: true

                                Canvas {
                                    id: donut
                                    anchors.centerIn: parent
                                    width: Math.min(parent.width, parent.height)
                                    height: width
                                    antialiasing: true
                                    onWidthChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        var cx = width / 2, cy = height / 2
                                        var r = width / 2 - 6
                                        var lw = 20
                                        var total = 0
                                        for (var i = 0; i < root.topWasted.length; i++)
                                            total += (root.topWasted[i].count || 0)
                                        if (total === 0) {
                                            ctx.lineWidth = lw
                                            ctx.strokeStyle = "#1E2535"
                                            ctx.beginPath()
                                            ctx.arc(cx, cy, r - lw / 2, 0, Math.PI * 2)
                                            ctx.stroke()
                                            return
                                        }

                                        ctx.lineWidth = lw
                                        ctx.lineCap = "butt"
                                        var start = -Math.PI / 2
                                        var gap = 0.045
                                        for (var j = 0; j < root.topWasted.length; j++) {
                                            var cnt = root.topWasted[j].count || 0
                                            if (cnt <= 0) continue
                                            var frac = cnt / total
                                            var end = start + Math.PI * 2 * frac
                                            ctx.strokeStyle = root.topWasted[j].color || "#7B61FF"
                                            ctx.beginPath()
                                            ctx.arc(cx, cy, r - lw / 2, start + gap / 2, end - gap / 2)
                                            ctx.stroke()
                                            start = end
                                        }
                                    }
                                }

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Txt {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "총 " + root.totalBlocked + "회"
                                        font.pixelSize: 19; font.weight: Font.DemiBold; color: "#F2F5FB"
                                    }
                                    Txt {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "차단"
                                        font.pixelSize: 12; color: "#7C8699"
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 0

                                Item { Layout.fillHeight: true }

                                Txt {
                                    visible: root.topWasted.length === 0
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "아직 차단된 도메인이 없습니다"
                                    font.pixelSize: 12; color: "#5C6579"
                                }

                                Repeater {
                                    model: root.topWasted
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        spacing: 10
                                        Rectangle {
                                            Layout.preferredWidth: 9
                                            Layout.preferredHeight: 9
                                            Layout.alignment: Qt.AlignVCenter
                                            radius: 3
                                            color: modelData.color || "#7B61FF"
                                        }
                                        Txt {
                                            Layout.fillWidth: true
                                            text: modelData.name || ""
                                            font.pixelSize: 12
                                            color: "#AEB8CB"
                                            elide: Text.ElideRight
                                        }
                                        Txt {
                                            text: (modelData.count || 0) + "회"
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            color: "#D3DAE8"
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }
                    }
                }
            }

            // ---------- 히트맵 ----------
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
                        Icon { name: "grid"; stroke: "#A5B4FC"; implicitWidth: 19; implicitHeight: 19 }
                        Txt { text: "요일 × 시간대 학습 몰입 히트맵"
                              font.pixelSize: 14; font.weight: Font.DemiBold }
                        Item { Layout.fillWidth: true }
                        RowLayout {
                            spacing: 7
                            Txt { text: "적음"; font.pixelSize: 11; color: "#5F6879" }
                            Repeater {
                                model: 5
                                delegate: Rectangle {
                                    required property int index
                                    Layout.preferredWidth: 13
                                    Layout.preferredHeight: 13
                                    radius: 3
                                    color: pgStats.heatColors[index]
                                }
                            }
                            Txt { text: "많음"; font.pixelSize: 11; color: "#5F6879" }
                        }
                    }

                    Item { Layout.preferredHeight: 18 }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 5

                        Repeater {
                            model: 7
                            delegate: RowLayout {
                                id: heatRow
                                required property int index
                                property int dayIdx: index
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                Txt {
                                    Layout.preferredWidth: 22
                                    text: root.weekLabels[index]
                                    font.pixelSize: 11
                                    color: index >= 5 ? "#6E6A92" : "#7C8699"
                                }

                                Repeater {
                                    model: 24
                                    delegate: Rectangle {
                                        required property int index
                                        property int lv: root.heatValue(heatRow.dayIdx, index)
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: 4
                                        color: pgStats.heatColors[lv]
                                        border.width: tileHover.hovered ? 1 : 0
                                        border.color: "#A5B4FC"
                                        HoverHandler { id: tileHover }
                                        ToolTip.visible: tileHover.hovered
                                        ToolTip.delay: 300
                                        ToolTip.text: root.weekLabels[heatRow.dayIdx] + "요일 "
                                                      + (index < 10 ? "0" + index : index) + "시 · "
                                                      + (lv * 15) + "분 집중"
                                    }
                                }
                            }
                        }

                        // 시간 라벨
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 18
                            spacing: 5
                            Item { Layout.preferredWidth: 22 }
                            Repeater {
                                model: 24
                                delegate: Txt {
                                    required property int index
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    text: index % 3 === 0 ? (index < 10 ? "0" + index : "" + index) : ""
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

    Connections {
        target: backend
        function onFocusScoreChanged() {
            if (focusCanvas) focusCanvas.requestPaint()
        }
        function onTopBlockedDomainsChanged() {
            if (donut) donut.requestPaint()
        }
        function onElapsedSecsChanged() {
            if (weekCanvas) weekCanvas.requestPaint()
        }
        function onWeeklyMinutesChanged() {
            if (weekCanvas) weekCanvas.requestPaint()
        }
    }
}
