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

        Rectangle {
            visible: phActions.children.length > 0
            Layout.preferredWidth: 1
            Layout.preferredHeight: 26
            Layout.leftMargin: 6
            Layout.rightMargin: 2
            color: "#232C3C"
        },

        // 알림
        Rectangle {
            Layout.preferredWidth: 42; Layout.preferredHeight: 42
            radius: 21
            color: bellHover.hovered ? "#1A2130" : "#151B26"
            border.width: 1; border.color: "#212A38"
            HoverHandler { id: bellHover; cursorShape: Qt.PointingHandCursor }
            Icon { anchors.centerIn: parent; name: "bell"; stroke: "#9BA6BC"
                   implicitWidth: 19; implicitHeight: 19 }
            Rectangle {
                visible: ph.notify > 0
                x: 26; y: 3
                width: 17; height: 17; radius: 8.5
                color: "#F43F5E"
                border.width: 2; border.color: "#0D111A"
                Txt { anchors.centerIn: parent; text: ph.notify
                      font.pixelSize: 10; font.weight: Font.Bold; color: "#FFFFFF" }
            }
        },

        // 다크모드
        Rectangle {
            Layout.preferredWidth: 42; Layout.preferredHeight: 42
            radius: 21
            color: moonHover.hovered ? "#1A2130" : "#151B26"
            border.width: 1; border.color: "#212A38"
            HoverHandler { id: moonHover; cursorShape: Qt.PointingHandCursor }
            Icon { anchors.centerIn: parent; name: "moon"; stroke: "#9BA6BC"
                   implicitWidth: 19; implicitHeight: 19 }
        },

        // 프로필
        Rectangle {
            Layout.preferredWidth: 42; Layout.preferredHeight: 42
            radius: 21
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#2A3348" }
                GradientStop { position: 1.0; color: "#222A3C" }
            }
            border.width: 1; border.color: "#333D55"
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            Txt { anchors.centerIn: parent; text: ph.avatar
                  font.pixelSize: 15; font.weight: Font.DemiBold; color: "#D4DBEA" }
        }
    ]
}
