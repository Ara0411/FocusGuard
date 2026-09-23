import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    radius: 12
    color: "#131B2E"
    border.color: "#1F2937"
    border.width: 1
    
    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 20
        text: "⏰ 집중 타이머"
        color: "white"
        font.pixelSize: 16
        font.bold: true
    }
    
    // Circular Timer UI (Mockup with Rectangle/Border)
    Rectangle {
        width: 180; height: 180
        radius: 90
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -20
        color: "transparent"
        border.color: "#3B82F6"
        border.width: 8
        
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 5
            Text {
                text: backend.focusTime
                color: "white"
                font.bold: true
                font.pixelSize: 32
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: backend.isPaused ? "휴식 중 ☕" : "집중 중 🔥"
                color: backend.isPaused ? "#9CA3AF" : "#8B5CF6"
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
    
    RowLayout {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 20
        spacing: 15
        
        Button {
            text: "▶ 집중 시작"
            background: Rectangle { color: backend.isPaused ? "#374151" : "#8B5CF6"; radius: 6 }
            contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            Layout.preferredWidth: 120; Layout.preferredHeight: 40
            onClicked: {
                if (backend.isPaused) backend.togglePause()
            }
        }
        
        Button {
            text: "☕ 휴식하기"
            background: Rectangle { color: backend.isPaused ? "#F59E0B" : "transparent"; border.color: backend.isPaused ? "#F59E0B" : "#374151"; radius: 6 }
            contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            Layout.preferredWidth: 120; Layout.preferredHeight: 40
            onClicked: {
                if (!backend.isPaused) backend.togglePause()
            }
        }
    }
}
