import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: cbx
    property bool checked: false
    signal toggled(bool value)
    implicitWidth: 18
    implicitHeight: 18
    radius: 5
    color: checked ? "#6366F1" : "transparent"
    border.width: 1.4
    border.color: checked ? "#6366F1" : "#3A4457"
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: { cbx.checked = !cbx.checked; cbx.toggled(cbx.checked) } }
    Icon {
        anchors.centerIn: parent
        visible: cbx.checked
        name: "check"; stroke: "#FFFFFF"; weight: 2.8
        implicitWidth: 12; implicitHeight: 12
    }
}
