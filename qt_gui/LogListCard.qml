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
        spacing: 15
        
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "📈 실시간 차단 로그"
                color: "white"
                font.pixelSize: 16
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 60; height: 24; radius: 12
                color: "transparent"
                border.color: "#EF4444"
                border.width: 1
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 4
                    Rectangle {
                        width: 6; height: 6; radius: 3; color: "#EF4444"
                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.2; duration: 800 }
                            NumberAnimation { to: 1.0; duration: 800 }
                        }
                    }
                    Text { text: "실시간"; color: "#EF4444"; font.pixelSize: 11 }
                }
            }
        }
        
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backend.recentLogs
            
            delegate: Rectangle {
                width: parent.width
                height: 40
                color: "transparent"
                
                RowLayout {
                    anchors.fill: parent
                    spacing: 15
                    Text { text: "⚠️"; font.pixelSize: 14 }
                    Text { text: modelData.time; color: "#9CA3AF"; font.pixelSize: 12 }
                    Text { text: "유해:"; color: "#EF4444"; font.pixelSize: 12 }
                    Text { text: modelData.domain; color: "#D1D5DB"; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: "접근 차단!"; color: "#EF4444"; font.pixelSize: 12 }
                }
            }
            
            // Empty state
            Text {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: "차단된 기록이 없습니다."
                color: "#6B7280"
            }
        }
        
        Text {
            text: "전체 로그 보기 >"
            color: "#8B5CF6"
            font.pixelSize: 13
            Layout.alignment: Qt.AlignRight
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}
