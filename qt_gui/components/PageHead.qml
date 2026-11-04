import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: ph
    property string title: ""
    property string subtitle: ""
    property int    notify: 0
    property string avatar: (typeof root !== "undefined" && root.userName && root.userName.length > 0) ? root.userName.charAt(0).toUpperCase() : "U"
    default property alias actions: phActions.data
    spacing: 16

    children: [
        ColumnLayout {
            spacing: 7
            Txt {
                text: ph.title
                font.pixelSize: 21; font.weight: Font.DemiBold; color: "#F3F6FC"
                textFormat: Text.StyledText
            }
            Txt {
                visible: ph.subtitle !== ""
                text: ph.subtitle
                font.pixelSize: 14; color: "#8B96AC"
                textFormat: Text.StyledText
            }
        },

        Item { Layout.fillWidth: true },

        Row {
            id: phActions
            Layout.alignment: Qt.AlignVCenter
            spacing: 10
        },

        // 알림 벨 아이콘 & 차단 알림 뱃지
        Rectangle {
            visible: ph.notify > 0
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 36
            implicitHeight: 36
            radius: 18
            color: "#1E2235"
            border.width: 1
            border.color: "#3F4A75"

            Icon {
                anchors.centerIn: parent
                name: "bell"
                stroke: "#A5B4FC"
                implicitWidth: 16
                implicitHeight: 16
            }

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: -2
                anchors.rightMargin: -2
                implicitWidth: Math.max(16, badgeTxt.implicitWidth + 8)
                implicitHeight: 16
                radius: 8
                color: "#F43F5E"

                Txt {
                    id: badgeTxt
                    anchors.centerIn: parent
                    text: ph.notify > 99 ? "99+" : ph.notify
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    color: "#FFFFFF"
                }
            }

            HoverHandler { id: bellHover; cursorShape: Qt.PointingHandCursor }
            ToolTip.visible: bellHover.hovered
            ToolTip.delay: 200
            ToolTip.text: "오늘 감지 및 차단된 유해 시도: " + ph.notify + "건 (클릭 시 차단 로그 이동)"
            TapHandler {
                onTapped: {
                    if (typeof root !== "undefined" && root.goPage) {
                        root.goPage(1)
                    }
                }
            }
        }
    ]
}
