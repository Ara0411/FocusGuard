import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tg
    property bool checked: true
    signal toggled(bool value)
    implicitWidth: 44
    implicitHeight: 24
    radius: height / 2
    color: checked ? "#4B45D8" : "#222A39"
    border.width: 1
    border.color: checked ? "#6366F1" : "#333D4E"
    Behavior on color { ColorAnimation { duration: 160 } }
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: { tg.checked = !tg.checked; tg.toggled(tg.checked) } }
    Rectangle {
        width: 18; height: 18; radius: 9
        y: 3
        x: tg.checked ? tg.width - 21 : 3
        color: tg.checked ? "#FFFFFF" : "#8A94A8"
        Behavior on x { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
    }
}
