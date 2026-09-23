import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TextField {
    id: fld
    property string glyph: ""
    implicitHeight: 40
    placeholderTextColor: "#5C6579"
    color: "#E2E8F4"
    font.pixelSize: 12
    font.family: "Pretendard, Malgun Gothic, Segoe UI"
    leftPadding: glyph === "" ? 14 : 38
    rightPadding: 12
    selectByMouse: true
    selectionColor: "#4C4BE0"
    background: Rectangle {
        radius: 10
        color: "#101622"
        border.width: 1
        border.color: fld.activeFocus ? "#4C4BE0" : "#242D3E"
        Icon {
            visible: fld.glyph !== ""
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 13
            name: fld.glyph; stroke: "#5C6579"
            implicitWidth: 16; implicitHeight: 16
        }
    }
}
