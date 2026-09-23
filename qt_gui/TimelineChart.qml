import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    radius: 12
    color: "#131B2E"
    border.color: "#1F2937"
    border.width: 1
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 30
        
        // Left Meta Panel
        ColumnLayout {
            Layout.preferredWidth: 150
            Layout.alignment: Qt.AlignTop
            spacing: 20
            
            Text {
                text: "📊 오늘의 집중 통계"
                color: "white"
                font.pixelSize: 16
                font.bold: true
            }
            
            ColumnLayout {
                spacing: 4
                Text { text: "총 집중 시간"; color: "#9CA3AF"; font.pixelSize: 13 }
                Text { text: backend.focusTime; color: "white"; font.pixelSize: 22; font.bold: true }
            }
            ColumnLayout {
                spacing: 4
                Text { text: "차단된 시도"; color: "#9CA3AF"; font.pixelSize: 13 }
                Text { text: backend.blockedAttempts + "회"; color: "white"; font.pixelSize: 22; font.bold: true }
            }
            ColumnLayout {
                spacing: 4
                Text { text: "차단된 시간"; color: "#9CA3AF"; font.pixelSize: 13 }
                Text { text: "00:32:15"; color: "white"; font.pixelSize: 22; font.bold: true } // Mock
            }
            Item { Layout.fillHeight: true }
        }
        
        // Vertical Divider
        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: "#1F2937"
        }
        
        // Right Chart Panel
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // X/Y Axis Labels (Mock)
            ColumnLayout {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 30
                
                Text { text: "60회"; color: "#6B7280"; font.pixelSize: 11; Layout.alignment: Qt.AlignTop }
                Item { Layout.fillHeight: true }
                Text { text: "30회"; color: "#6B7280"; font.pixelSize: 11 }
                Item { Layout.fillHeight: true }
                Text { text: "0회"; color: "#6B7280"; font.pixelSize: 11; Layout.alignment: Qt.AlignBottom }
            }
            
            // Horizontal grid lines
            Repeater {
                model: 3
                Rectangle {
                    x: 40; width: parent.width - 40; height: 1
                    y: index * ((parent.height - 30) / 2)
                    color: "#1F2937"
                }
            }
            
            // Bars
            RowLayout {
                anchors.left: parent.left
                anchors.leftMargin: 40
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 30
                spacing: 8
                
                Repeater {
                    model: backend.timelineData
                    
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        
                        // Max assumed value = 60
                        property real ratio: Math.min(modelData / 60.0, 1.0)
                        
                        Rectangle {
                            width: 12
                            height: Math.max(parent.ratio * parent.height, 4) // minimum height
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#8B5CF6"
                            radius: 4
                            opacity: modelData > 0 ? 1.0 : 0.2
                            
                            // Hover effect
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: parent.color = "#A78BFA"
                                onExited: parent.color = "#8B5CF6"
                            }
                        }
                    }
                }
            }
            
            // X-Axis Labels (Hours)
            RowLayout {
                anchors.left: parent.left
                anchors.leftMargin: 40
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                spacing: 8
                
                Repeater {
                    model: 24
                    Item {
                        Layout.fillWidth: true
                        height: 20
                        Text {
                            anchors.centerIn: parent
                            text: (index % 2 === 0) ? (index < 10 ? "0"+index : index) : ""
                            color: "#6B7280"
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}
