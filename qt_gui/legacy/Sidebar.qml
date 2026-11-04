import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "#111827"
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15
        
        // Logo and Title
        RowLayout {
            spacing: 10
            Layout.alignment: Qt.AlignTop
            Layout.bottomMargin: 20
            
            Rectangle {
                width: 32; height: 32
                radius: 8
                color: "#3B82F6"
                // Placeholder for Shield Icon
                Text {
                    anchors.centerIn: parent
                    text: "S"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 18
                }
            }
            ColumnLayout {
                spacing: 2
                Text {
                    text: "FocusNet"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 16
                }
                Text {
                    text: "Guardian"
                    color: "#9CA3AF"
                    font.pixelSize: 12
                }
            }
        }
        
        // Navigation Menu
        ListModel {
            id: navModel
            ListElement { name: "대시보드"; active: true }
            ListElement { name: "차단 로그"; active: false }
            ListElement { name: "사이트 관리"; active: false }
            ListElement { name: "집중 통계"; active: false }
            ListElement { name: "설정"; active: false }
        }
        
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: navModel
            interactive: false
            delegate: Rectangle {
                width: parent.width
                height: 48
                radius: 8
                color: model.active ? "#1E293B" : "transparent"
                
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    text: model.name
                    color: model.active ? "#3B82F6" : "#D1D5DB"
                    font.bold: model.active
                    font.pixelSize: 14
                }
                
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    // Handle navigation logic here later
                }
            }
        }
        
        // Bottom Focus Timer Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            radius: 12
            color: "#131B2E"
            border.color: "#1F2937"
            border.width: 1
            opacity: 0.85
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 8
                
                RowLayout {
                    Rectangle {
                        width: 8; height: 8; radius: 4; color: "#10B981"
                    }
                    Text {
                        text: "집중 모드 진행 중"
                        color: "#10B981"
                        font.pixelSize: 12
                    }
                }
                
                Text {
                    text: backend.targetTime
                    color: "#8B5CF6"
                    font.bold: true
                    font.pixelSize: 24
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Button {
                    text: "중지하기"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    background: Rectangle {
                        color: "transparent"
                        border.color: "#374151"
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}
