import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: pgStats
    property int trendPeriodDays: 7
    readonly property var heatColors: ["#161D2B", "#252C5C", "#3B3499", "#5B4BD6", "#8B74FF"]
    readonly property var currentTopWasted: {
        var list = backend.getTopBlockedDomainsHistorical(pgStats.trendPeriodDays, 5)
        if (list && list.length > 0) return list
        return root.topWasted
    }
    readonly property int currentPeriodBlockedCount: {
        var total = 0
        for (var i = 0; i < currentTopWasted.length; ++i) {
            total += (currentTopWasted[i].count || 0)
        }
        return total > 0 ? total : root.totalBlocked
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

                RowLayout {
                    spacing: 8
                    Pill {
                        text: "역대 누적 " + root.hm(backend.totalAccumulatedMinutes)
                        fg: "#A5B4FC"
                        color: "#1E1B4B"
                    }
                    Pill {
                        text: "총 방어 " + backend.totalAccumulatedSessions + "회"
                        fg: "#FDA4AF"
                        color: "#2A1420"
                    }
                    Pill {
                        text: "🍅 " + backend.totalAccumulatedRounds + "회 달성"
                        fg: "#6EE7B7"
                        color: "#0F2A22"
                    }
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
                    value: (backend.blockedSessions > 0 && backend.blockedAttempts > backend.blockedSessions)
                           ? (backend.blockedSessions + "회 (" + backend.blockedAttempts + "개)")
                           : (backend.blockedAttempts + "회")
                    sub: "약 " + (backend.blockedSecs > 0 ? root.hm(Math.floor(backend.blockedSecs / 60)) : ((backend.blockedSessions > 0 ? backend.blockedSessions : backend.blockedAttempts) * 3) + "분") + " 절약"
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

            // ---------- AI 사용자 인터넷 사용 유형 분석 카드 ----------
            Card {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.preferredHeight: aiCol.implicitHeight + 36

                ColumnLayout {
                    id: aiCol
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            radius: 8
                            color: "#1E1B4B"
                            border.width: 1
                            border.color: "#4338CA"
                            Txt {
                                anchors.centerIn: parent
                                text: "✨"
                                font.pixelSize: 16
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Txt {
                                text: "AI 사용자 인터넷 사용 유형 진단 리포트"
                                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#F2F5FB"
                            }
                            Txt {
                                text: "오늘의 방어 통계와 딴짓 도메인 패턴을 머신 분석하여 도출한 맞춤형 몰입 페르소나입니다."
                                font.pixelSize: 11; color: "#7C8699"
                            }
                        }

                        SolidBtn {
                            height: 34
                            text: backend.isAiAnalyzing ? "AI 분석 중..." : "AI 진단 실행"
                            glyph: backend.isAiAnalyzing ? "clock" : "refresh"
                            bg: backend.isAiAnalyzing ? "#475569" : "#6366F1"
                            enabled: !backend.isAiAnalyzing
                            onClicked: backend.requestAiUserAnalysis()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#1E2638"
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.minimumHeight: 140
                        spacing: 18

                        // 좌측: 유형 뱃지 & 아이콘
                        Rectangle {
                            Layout.preferredWidth: 240
                            Layout.fillHeight: true
                            Layout.minimumHeight: 140
                            radius: 12
                            color: "#121826"
                            border.width: 1
                            border.color: "#242F46"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                // 중앙 엠블럼 원 (이모지 / 이모티콘)
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 50; height: 50; radius: 25
                                    color: "#182033"
                                    border.width: 1.5
                                    border.color: backend.aiTypeColor

                                    Txt {
                                        anchors.centerIn: parent
                                        text: {
                                            var str = (backend.aiUserType || "").trim()
                                            if (str.indexOf("분석 대기") !== -1 || str.indexOf("안내") !== -1 || str.length === 0) {
                                                return "💡"
                                            }
                                            // 1. 유니코드 서러게이트 페어 4바이트 이모지 (예: 👻, 🎧, 🚀, 🌱 등)
                                            if (str.length >= 2) {
                                                var c1 = str.charCodeAt(0)
                                                var c2 = str.charCodeAt(1)
                                                if (c1 >= 0xD800 && c1 <= 0xDBFF && c2 >= 0xDC00 && c2 <= 0xDFFF) {
                                                    return str.substr(0, 2)
                                                }
                                            }
                                            // 2. 단일 16비트 특수 기호 이모지 (예: ⚡ 0x26A1, ☕ 0x2615, ✨ 0x2728, ⚠️ 0x26A0 등)
                                            var c = str.charCodeAt(0)
                                            if ((c >= 0x2600 && c <= 0x27BF) || (c >= 0x2300 && c <= 0x23FF) || (c >= 0x2B00 && c <= 0x2BFF)) {
                                                return str.substr(0, 1)
                                            }
                                            // 3. 한글이나 일반 텍스트인 경우 엠블럼 기본 이모지 반환
                                            return "🎯"
                                        }
                                        font.pixelSize: 24
                                    }
                                }

                                // 페르소나 명칭 (박스 크기 내 자동 줄바꿈으로 텍스트 이탈 방지)
                                Txt {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 4
                                    Layout.rightMargin: 4
                                    text: backend.aiUserType
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    color: backend.aiTypeColor
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    lineHeight: 1.2
                                }

                                Pill {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: backend.isAiAnalyzing ? "Gemini 분석 중..." : "Gemini AI 정밀 진단"
                                    fontSize: 10
                                    fg: backend.isAiAnalyzing ? "#38BDF8" : "#94A3B8"
                                    color: "#1E293B"
                                }
                            }
                        }

                        // 우측: 총평 및 개선 가이드 3단계
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: reportTxt.implicitHeight + 20
                                radius: 10
                                color: "#131A29"
                                border.width: 1
                                border.color: "#1F2B42"
                                Txt {
                                    id: reportTxt
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    text: backend.aiAnalysisReport
                                    font.pixelSize: 12
                                    lineHeight: 1.45
                                    color: "#CBD5E1"
                                    wrapMode: Text.WordWrap
                                }
                            }

                            // 실천 가이드 3가지
                            ColumnLayout {
                                visible: backend.aiActionPlan.length > 0
                                Layout.fillWidth: true
                                spacing: 6

                                Repeater {
                                    model: backend.aiActionPlan
                                    delegate: RowLayout {
                                        required property var modelData
                                        required property int index
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Rectangle {
                                            Layout.preferredWidth: 18
                                            Layout.preferredHeight: 18
                                            radius: 9
                                            color: "#1E293B"
                                            Txt {
                                                anchors.centerIn: parent
                                                text: (index + 1)
                                                font.pixelSize: 10
                                                font.weight: Font.Bold
                                                color: "#38BDF8"
                                            }
                                        }
                                        Txt {
                                            Layout.fillWidth: true
                                            text: modelData
                                            font.pixelSize: 11
                                            color: "#94A3B8"
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }

                            // AI 추천 추가 차단 도메인 (원클릭 블랙리스트 등록)
                            ColumnLayout {
                                visible: backend.aiActionPlan.length > 0 || (backend.aiRecommendedDomains && backend.aiRecommendedDomains.length > 0)
                                Layout.fillWidth: true
                                spacing: 8

                                RowLayout {
                                    spacing: 6
                                    Icon {
                                        name: "alert"
                                        stroke: "#FB7185"
                                        implicitWidth: 14; implicitHeight: 14
                                    }
                                    Txt {
                                        text: "AI 추천 추가 차단 도메인 (클릭 시 즉시 블랙리스트 등록)"
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                        color: "#FB7185"
                                    }
                                }

                                Flow {
                                    visible: backend.aiRecommendedDomains && backend.aiRecommendedDomains.length > 0
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Repeater {
                                        model: backend.aiRecommendedDomains
                                        delegate: Rectangle {
                                            required property string modelData
                                            height: 28
                                            width: chipRow.implicitWidth + 20
                                            radius: 7
                                            color: chipHover.hovered ? "#4C1D24" : "#2A1420"
                                            border.width: 1
                                            border.color: chipHover.hovered ? "#F43F5E" : "#9F1239"

                                            HoverHandler {
                                                id: chipHover
                                                cursorShape: Qt.PointingHandCursor
                                            }

                                            RowLayout {
                                                id: chipRow
                                                anchors.centerIn: parent
                                                spacing: 6
                                                Icon {
                                                    name: "plus"
                                                    stroke: "#FDA4AF"
                                                    implicitWidth: 12; implicitHeight: 12
                                                }
                                                Txt {
                                                    text: modelData
                                                    font.pixelSize: 12
                                                    font.weight: Font.Medium
                                                    color: "#FDA4AF"
                                                }
                                            }

                                            TapHandler {
                                                onTapped: {
                                                    backend.addToBlacklist(modelData)
                                                    backend.showTrayNotification("블랙리스트 추가 완료", modelData + " 도메인이 차단 목록에 등록되었습니다.")
                                                }
                                            }
                                        }
                                    }
                                }

                                Txt {
                                    visible: !backend.aiRecommendedDomains || backend.aiRecommendedDomains.length === 0
                                    text: "✓ 모든 주요 연관 도메인이 이미 차단 목록에 안전하게 등록되어 있습니다."
                                    font.pixelSize: 11
                                    color: "#10B981"
                                }
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
                            Txt { text: pgStats.trendPeriodDays === 30 ? "최근 30일 몰입 추이" : "주간 집중도 추이"; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Item { Layout.fillWidth: true }

                            // [7일] / [30일] 기간 선택 탭
                            RowLayout {
                                spacing: 4
                                Rectangle {
                                    implicitWidth: 44; implicitHeight: 24; radius: 6
                                    color: pgStats.trendPeriodDays === 7 ? "#2D3450" : "#1A202C"
                                    border.color: pgStats.trendPeriodDays === 7 ? "#6366F1" : "transparent"
                                    border.width: 1
                                    Txt { anchors.centerIn: parent; text: "7일"; font.pixelSize: 11; font.weight: pgStats.trendPeriodDays === 7 ? Font.DemiBold : Font.Normal; color: pgStats.trendPeriodDays === 7 ? "#FFFFFF" : "#7C8699" }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            pgStats.trendPeriodDays = 7
                                            weekCanvas.requestPaint()
                                            donut.requestPaint()
                                        }
                                    }
                                }
                                Rectangle {
                                    implicitWidth: 48; implicitHeight: 24; radius: 6
                                    color: pgStats.trendPeriodDays === 30 ? "#2D3450" : "#1A202C"
                                    border.color: pgStats.trendPeriodDays === 30 ? "#6366F1" : "transparent"
                                    border.width: 1
                                    Txt { anchors.centerIn: parent; text: "30일"; font.pixelSize: 11; font.weight: pgStats.trendPeriodDays === 30 ? Font.DemiBold : Font.Normal; color: pgStats.trendPeriodDays === 30 ? "#FFFFFF" : "#7C8699" }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            pgStats.trendPeriodDays = 30
                                            weekCanvas.requestPaint()
                                            donut.requestPaint()
                                        }
                                    }
                                }
                            }

                            Item { Layout.preferredWidth: 6 }

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

                                var trendData = backend.getTrendRange(pgStats.trendPeriodDays)
                                var n = (trendData && trendData.length > 0) ? trendData.length : 7

                                var padL = 44, padB = 28, padT = 12
                                var plotW = width - padL
                                var plotH = height - padB - padT

                                var maxValInMinutes = root.weekGoalMin * 1.3
                                for (var k = 0; k < n; k++) {
                                    var valK = (trendData && trendData[k]) ? trendData[k].minutes : 0
                                    if (valK > maxValInMinutes) maxValInMinutes = valK
                                }
                                var maxV = Math.max(180, Math.ceil(maxValInMinutes / 60) * 60)

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
                                var bw = (n <= 7) ? Math.min(34, slot * 0.46) : Math.max(4, slot * 0.65)
                                for (var i = 0; i < n; i++) {
                                    var item = (trendData && trendData[i]) ? trendData[i] : null
                                    var v = item ? item.minutes : (root.weekMinutes[i] || 0)
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
                                        roundRect(bx, by, bw, Math.max(bh, 4), Math.min(5, bw / 2))
                                        ctx.fill()

                                        // 값 라벨 (7일일 때만 상단 시간 표시)
                                        if (n <= 7) {
                                            ctx.fillStyle = hit ? "#C0C8FF" : "#8A94A8"
                                            ctx.textAlign = "center"
                                            ctx.fillText(Math.floor(v / 60) + "h " + (v % 60) + "m", bx + bw / 2, by - 11)
                                        }
                                    } else {
                                        ctx.fillStyle = "#1E2536"
                                        ctx.beginPath()
                                        roundRect(bx, padT + plotH - 3, bw, 3, 2)
                                        ctx.fill()

                                        if (n <= 7) {
                                            ctx.fillStyle = "#465064"
                                            ctx.textAlign = "center"
                                            ctx.fillText("-", bx + bw / 2, padT + plotH - 10)
                                        }
                                    }

                                    // X축 라벨
                                    if (n <= 7) {
                                        var wLabel = (item && item.weekday) ? item.weekday : root.weekLabels[i]
                                        ctx.fillStyle = "#7C8699"
                                        ctx.textAlign = "center"
                                        ctx.fillText(wLabel, bx + bw / 2, padT + plotH + 14)
                                    } else {
                                        if (i % 5 === 0 || i === n - 1) {
                                            var dLabel = item ? item.date : ""
                                            ctx.fillStyle = "#7C8699"
                                            ctx.textAlign = "center"
                                            ctx.fillText(dLabel, bx + bw / 2, padT + plotH + 14)
                                        }
                                    }
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

                        CardTitle { label: "최다 방어 도메인"; glyph: "shield-check"; glyphColor: "#F43F5E" }

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
                                        var wastedList = pgStats.currentTopWasted
                                        var total = 0
                                        for (var i = 0; i < wastedList.length; i++)
                                            total += (wastedList[i].count || 0)
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
                                        for (var j = 0; j < wastedList.length; j++) {
                                            var cnt = wastedList[j].count || 0
                                            if (cnt <= 0) continue
                                            var frac = cnt / total
                                            var end = start + Math.PI * 2 * frac
                                            ctx.strokeStyle = wastedList[j].color || "#7B61FF"
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
                                        text: "총 " + pgStats.currentPeriodBlockedCount + "회"
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
                                    visible: pgStats.currentTopWasted.length === 0
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "아직 차단된 도메인이 없습니다"
                                    font.pixelSize: 12; color: "#5C6579"
                                }

                                Repeater {
                                    model: pgStats.currentTopWasted
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
                            spacing: 8
                            Rectangle {
                                Layout.preferredWidth: 10; Layout.preferredHeight: 10
                                radius: 2; color: "#131824"; border.width: 1; border.color: "#283348"
                            }
                            Txt { text: "수집 중 / 0분"; font.pixelSize: 11; color: "#5F6879" }
                            Item { Layout.preferredWidth: 6 }
                            Txt { text: "15분"; font.pixelSize: 11; color: "#5F6879" }
                            Repeater {
                                model: [pgStats.heatColors[1], pgStats.heatColors[2], pgStats.heatColors[3], pgStats.heatColors[4]]
                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.preferredWidth: 11; Layout.preferredHeight: 11
                                    radius: 2; color: modelData
                                }
                            }
                            Txt { text: "60분"; font.pixelSize: 11; color: "#5F6879" }
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
                                property bool isToday: {
                                    var d = new Date().getDay()
                                    return dayIdx === (d === 0 ? 6 : d - 1)
                                }
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5

                                Txt {
                                    Layout.preferredWidth: 26
                                    text: heatRow.isToday ? (root.weekLabels[index] + "•") : root.weekLabels[index]
                                    font.pixelSize: 11
                                    font.weight: heatRow.isToday ? Font.DemiBold : Font.Normal
                                    color: heatRow.isToday ? "#A5B4FC" : (index >= 5 ? "#5E5A82" : "#6C7689")
                                }

                                Repeater {
                                    model: 24
                                    delegate: Rectangle {
                                        id: heatTile
                                        required property int index
                                        property int hourIdx: index
                                        property int lv: root.heatValue(heatRow.dayIdx, hourIdx)
                                        property bool isCurrentHour: heatRow.isToday && (new Date().getHours() === hourIdx)
                                        property bool isFutureHour: heatRow.isToday && (new Date().getHours() < hourIdx)
                                        property bool isPastDay: {
                                            var d = new Date().getDay()
                                            var today = (d === 0 ? 6 : d - 1)
                                            return heatRow.dayIdx < today
                                        }
                                        property bool isFutureDay: {
                                            var d = new Date().getDay()
                                            var today = (d === 0 ? 6 : d - 1)
                                            return heatRow.dayIdx > today
                                        }

                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: 4

                                        // 색상 톤 정리
                                        color: {
                                            if (lv > 0) return pgStats.heatColors[lv]
                                            if (isCurrentHour) return "#1F2842"
                                            if (heatRow.isToday) return "#151C2C"
                                            if (isPastDay) return "#131824"
                                            return "#0F141F" // 미래 요일
                                        }

                                        // 현재 시간대 및 호버 테두리
                                        border.width: tileHover.hovered ? 1 : (isCurrentHour ? 1.5 : (heatRow.isToday ? 0.5 : 0))
                                        border.color: tileHover.hovered ? "#A5B4FC" : (isCurrentHour ? "#818CF8" : (heatRow.isToday ? "#212B40" : "transparent"))

                                        HoverHandler { id: tileHover }
                                        ToolTip.visible: tileHover.hovered
                                        ToolTip.delay: 200
                                        ToolTip.text: {
                                            var dayName = root.weekLabels[heatRow.dayIdx] + "요일"
                                            var timeStr = (hourIdx < 10 ? "0" + hourIdx : hourIdx) + ":00"

                                            if (lv > 0) {
                                                return dayName + " " + timeStr + " · " + (lv * 15) + "분 집중 완료"
                                            }
                                            if (isCurrentHour) {
                                                return dayName + " " + timeStr + " · [현재 시간대] 실시간 측정 중"
                                            }
                                            if (heatRow.isToday) {
                                                if (isFutureHour) {
                                                    return dayName + " " + timeStr + " · 예정 (데이터 수집 대기)"
                                                } else {
                                                    return dayName + " " + timeStr + " · 집중 기록 없음"
                                                }
                                            }
                                            if (isFutureDay) {
                                                return dayName + " " + timeStr + " · 예정 (데이터 수집 대기)"
                                            }
                                            return dayName + " " + timeStr + " · 데이터 수집 중 (기록 없음)"
                                        }
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
        function onHistoryDataChanged() {
            if (weekCanvas) weekCanvas.requestPaint()
            if (donut) donut.requestPaint()
        }
    }
}
