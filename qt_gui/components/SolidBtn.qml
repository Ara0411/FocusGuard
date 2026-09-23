import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: sb
    property string text: ""
    property string glyph: ""
    property int fontSize: 13
    signal clicked()
    implicitHeight: 40
    implicitWidth: sbRow.implicitWidth + 40
    radius: 10
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: sbHover.hovered ? "#8A72FF" : "#7B61FF" }
        GradientStop { position: 1.0; color: sbHover.hovered ? "#6E71F5" : "#6366F1" }
    }
    HoverHandler { id: sbHover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: sb.clicked() }
    RowLayout {
        id: sbRow
        anchors.centerIn: parent
        spacing: 8
        Icon {
            visible: sb.glyph !== ""
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: visible ? 15 : 0
            Layout.preferredHeight: 15
            name: sb.glyph; stroke: "#FFFFFF"
        }
        Txt {
            Layout.alignment: Qt.AlignVCenter
            text: sb.text; color: "#FFFFFF"
            font.pixelSize: sb.fontSize; font.weight: Font.DemiBold
        }
    }
}
