import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: st
    property var options: []
    property int currentIndex: 0
    property int fontSize: 12
    signal selected(int index)
    implicitHeight: 38
    implicitWidth: stRow.implicitWidth + 8
    radius: 10
    color: "#101622"
    border.width: 1
    border.color: "#242D3E"
    Row {
        id: stRow
        anchors.centerIn: parent
        spacing: 3
        Repeater {
            model: st.options
            delegate: Rectangle {
                required property string modelData
                required property int index
                width: segTxt.implicitWidth + 28
                height: st.height - 8
                radius: 8
                color: index === st.currentIndex ? "#2A3348"
                                                 : (segHover.hovered ? "#1A2130" : "transparent")
                HoverHandler { id: segHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { st.currentIndex = index; st.selected(index) } }
                Txt {
                    id: segTxt
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: st.fontSize
                    font.weight: index === st.currentIndex ? Font.DemiBold : Font.Normal
                    color: index === st.currentIndex ? "#EDF0FA" : "#8A94A8"
                }
            }
        }
    }
}
