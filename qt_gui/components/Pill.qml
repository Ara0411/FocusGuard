import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    property alias text: pillLabel.text
    property color fg: "#A5B4FC"
    property int fontSize: 11
    radius: height / 2
    implicitWidth: pillLabel.implicitWidth + 18
    implicitHeight: fontSize + 11
    color: "#1E2438"
    Txt {
        id: pillLabel
        anchors.centerIn: parent
        color: parent.fg
        font.pixelSize: parent.fontSize
        font.weight: Font.Medium
    }
}
