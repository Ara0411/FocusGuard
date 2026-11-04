import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "transparent"
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 40
        anchors.rightMargin: 40
        
        ColumnLayout {
            spacing: 4
            Text {
                text: "안녕하세요, " + backend.userName + " 님! 👋"
                color: "white"
                font.bold: true
                font.pixelSize: 24
            }
            Text {
                text: "오늘 " + parseInt(backend.focusTime.split(":")[0]) + "시간째 집중 중입니다! 🔥"
                color: "#9CA3AF"
                font.pixelSize: 14
            }
        }
        
        Item { Layout.fillWidth: true } // Spacer
        
        // Utility Icons
        RowLayout {
            spacing: 20
            
            // Bell Icon placeholder
            Rectangle {
                width: 40; height: 40; radius: 20; color: "#1F2937"
                Text { anchors.centerIn: parent; text: "🔔"; font.pixelSize: 16 }
            }
            // Moon Icon placeholder
            Rectangle {
                width: 40; height: 40; radius: 20; color: "#1F2937"
                Text { anchors.centerIn: parent; text: "🌙"; font.pixelSize: 16 }
            }
            // Profile Icon placeholder
            Rectangle {
                width: 40; height: 40; radius: 20; color: "#374151"
                Text { anchors.centerIn: parent; text: backend.userName.charAt(0); color: "white"; font.bold: true; font.pixelSize: 16 }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    // onClicked: openSettingsModal()
                }
            }
        }
    }
}
