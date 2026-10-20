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
        }
    ]
}
