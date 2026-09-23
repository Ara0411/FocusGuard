import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Card {
    id: mt
    property string label: ""
    property string value: ""
    property string sub: ""
    property string glyph: "bars"
    property color accent: "#A5B4FC"
    property color accentBg: "#1D2438"
    property string pillText: ""
    implicitHeight: 118

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            Layout.alignment: Qt.AlignVCenter
            radius: 12
            color: mt.accentBg
            Icon { anchors.centerIn: parent; name: mt.glyph; stroke: mt.accent
                   implicitWidth: 20; implicitHeight: 20 }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Txt { text: mt.label; font.pixelSize: 12; color: "#8B96AC" }
            Txt {
                Layout.fillWidth: true
                text: mt.value
                font.pixelSize: 21; font.weight: Font.DemiBold; color: "#F2F5FB"
                elide: Text.ElideRight
            }
            Txt { visible: mt.sub !== ""; text: mt.sub; font.pixelSize: 12; color: mt.accent }
        }

        Pill {
            visible: mt.pillText !== ""
            Layout.alignment: Qt.AlignVCenter
            text: mt.pillText
            fg: mt.accent
            color: mt.accentBg
            fontSize: 10
        }
    }
}
