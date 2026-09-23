import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: cb
    property var options: []
    property int currentIndex: 0
    property string placeholder: ""
    property int fontSize: 12
    signal activated(int index)
    implicitHeight: 40
    implicitWidth: 160
    radius: 10
    color: "#101622"
    border.width: 1
    border.color: cbPop.opened ? "#4C4BE0" : "#242D3E"
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: cbPop.opened ? cbPop.close() : cbPop.open() }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 6
        Txt {
            Layout.fillWidth: true
            text: cb.options.length > 0 ? cb.options[cb.currentIndex] : cb.placeholder
            font.pixelSize: cb.fontSize
            color: cb.options.length > 0 ? "#D3DAE8" : "#5C6579"
            elide: Text.ElideRight
        }
        Icon {
            name: "caret"; stroke: "#7C8699"
            implicitWidth: 15; implicitHeight: 15
            rotation: cbPop.opened ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: 140 } }
        }
    }

    Popup {
        id: cbPop
        y: cb.height + 6
        width: cb.width
        padding: 6
        modal: false
        background: Rectangle {
            color: "#141A26"; radius: 11
            border.width: 1; border.color: "#2A3446"
        }
        contentItem: Column {
            spacing: 2
            Repeater {
                model: cb.options
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    width: cbPop.width - 12
                    height: 34
                    radius: 8
                    color: index === cb.currentIndex ? "#232C42"
                                                     : (optHover.hovered ? "#1A2130" : "transparent")
                    HoverHandler { id: optHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: { cb.currentIndex = index; cb.activated(index); cbPop.close() }
                    }
                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        text: modelData
                        font.pixelSize: cb.fontSize
                        color: index === cb.currentIndex ? "#C0C8FF" : "#AEB8CB"
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
