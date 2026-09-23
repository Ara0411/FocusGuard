import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    ScrollView {
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        
        ColumnLayout {
            width: root.width - 80
            x: 40 // anchors.horizontalCenter inside ScrollView can cause issues
            spacing: 20
            
            // Top padding
            Item { Layout.preferredHeight: 10; Layout.fillWidth: true }
            
            // --- Row 1: Core Widgets (3 Columns) ---
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 320
            spacing: 20
            
            // 1. Focus Timer Card
            FocusTimerCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            
            // 2. Focus Score Card
            StatsCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            
            // 3. Real-time Block Logs Card
            LogListCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
        
        // --- Row 2: Policy Management (2 Columns) ---
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            spacing: 20
            
            // Allowlist Card Placeholder
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#131B2E"
                border.color: "#1F2937"
                border.width: 1
                
                Text {
                    anchors.margins: 20
                    anchors.top: parent.top
                    anchors.left: parent.left
                    text: "허용된 착한 사이트 (화이트리스트)"
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }
                
                // Add placeholder items
                Column {
                    anchors.top: parent.top
                    anchors.topMargin: 60
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 20
                    spacing: 10
                    
                    Repeater {
                        model: ["github.com", "naver.com", "microsoft.com", "ebs.co.kr"]
                        Rectangle {
                            width: parent.width
                            height: 40
                            color: "transparent"
                            RowLayout {
                                anchors.fill: parent
                                Text { text: "✅"; color: "#10B981" }
                                Text { text: modelData; color: "#D1D5DB"; Layout.fillWidth: true }
                                Text { text: "허용됨"; color: "#3B82F6"; font.pixelSize: 12 }
                            }
                        }
                    }
                }
            }
            
            // Register Domain Card Placeholder
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#131B2E"
                border.color: "#1F2937"
                border.width: 1
                
                Text {
                    anchors.margins: 20
                    anchors.top: parent.top
                    anchors.left: parent.left
                    text: "공부 필수 사이트 등록"
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }
                
                RowLayout {
                    anchors.top: parent.top
                    anchors.topMargin: 60
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 20
                    
                    TextField {
                        Layout.fillWidth: true
                        placeholderText: "인강/사전 주소 입력 (예: ebs.co.kr)"
                        color: "white"
                        background: Rectangle { color: "#1F2937"; radius: 6 }
                    }
                    Button {
                        text: "등록"
                        background: Rectangle { color: "#8B5CF6"; radius: 6 }
                        contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }
        }
        
        // --- Row 3: Focus Analytics Timeline (Full Width) ---
        TimelineChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            Layout.bottomMargin: 40
        }
    }
}
}
