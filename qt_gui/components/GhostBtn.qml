import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: gb
    property string text: ""
    property string glyph: ""
    property color fg: "#C3CBDB"
    property int fontSize: 12
    signal clicked()
    implicitHeight: 38
    implicitWidth: gbRow.implicitWidth + 30
    radius: 10
    color: gbHover.hovered ? "#1D2536" : "transparent"
    border.width: 1
    border.color: gbHover.hovered ? "#3E4A64" : "#2D3648"
    HoverHandler { id: gbHover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: gb.clicked() }
    RowLayout {
        id: gbRow
        anchors.centerIn: parent
        spacing: 8
        Icon {
            visible: gb.glyph !== ""
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: visible ? 15 : 0
            Layout.preferredHeight: 15
            name: gb.glyph; stroke: gb.fg
        }
        Txt {
            Layout.alignment: Qt.AlignVCenter
            text: gb.text; color: gb.fg; font.pixelSize: gb.fontSize
        }
    }
}
