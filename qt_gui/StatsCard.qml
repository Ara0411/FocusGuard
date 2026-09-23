import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    radius: 12
    color: "#131B2E"
    border.color: "#1F2937"
    border.width: 1
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20
        
        Text {
            text: "🎯 오늘의 집중도"
            color: "white"
            font.pixelSize: 16
            font.bold: true
        }
        
        // Progress Bar Area
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            
            RowLayout {
                Layout.fillWidth: true
                Text { text: "집중 목표 달성률"; color: "#D1D5DB"; font.pixelSize: 14 }
                Item { Layout.fillWidth: true }
                Text { text: Math.round(backend.focusScore) + "%"; color: "#8B5CF6"; font.bold: true; font.pixelSize: 20 }
            }
            
            Rectangle {
                Layout.fillWidth: true
                height: 12
                radius: 6
                color: "#1F2937"
                
                Rectangle {
                    width: parent.width * (backend.focusScore / 100.0)
                    height: parent.height
                    radius: 6
                    color: "#8B5CF6"
                    
                    Behavior on width {
                        NumberAnimation { duration: 500; easing.type: Easing.OutQuad }
                    }
                }
            }
        }
        
        // Blocked Badge
        Rectangle {
            Layout.fillWidth: true
            height: 48
            radius: 8
            color: "#451a1a" // Dark Red tint
            border.color: "#EF4444"
            border.width: 1
            
            RowLayout {
                anchors.centerIn: parent
                spacing: 10
                Text { text: "🚫"; font.pixelSize: 16 }
                Text { text: "오늘 차단된 딴짓 : 총 " + backend.blockedAttempts + "회"; color: "#FCA5A5"; font.pixelSize: 14 }
            }
        }
        
        Item { Layout.fillHeight: true } // Spacer
        
        // Sub-metrics
        RowLayout {
            Layout.fillWidth: true
            
            ColumnLayout {
                Text { text: "집중 시간"; color: "#9CA3AF"; font.pixelSize: 12 }
                Text { text: backend.focusTime; color: "white"; font.pixelSize: 20; font.bold: true }
            }
            Item { Layout.fillWidth: true }
            ColumnLayout {
                Text { text: "최고 기록"; color: "#9CA3AF"; font.pixelSize: 12 }
                Text { text: "03:10:45"; color: "white"; font.pixelSize: 20; font.bold: true } // Mock
            }
        }
    }
}
