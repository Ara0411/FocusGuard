import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Canvas {
    property string name: ""
    property color stroke: "#A5B4FC"
    property real weight: 1.9
    implicitWidth: 18
    implicitHeight: 18
    antialiasing: true
    onStrokeChanged: requestPaint()
    onNameChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var u = Math.min(width, height) / 24
        ctx.scale(u, u)
        ctx.strokeStyle = stroke
        ctx.fillStyle = stroke
        ctx.lineWidth = weight
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        function poly(pts, close, fill) {
            ctx.beginPath()
            ctx.moveTo(pts[0][0], pts[0][1])
            for (var i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1])
            if (close) ctx.closePath()
            if (fill) ctx.fill(); else ctx.stroke()
        }
        function circle(cx, cy, r, fill) {
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            if (fill) ctx.fill(); else ctx.stroke()
        }

        switch (name) {
        case "home":
            poly([[3, 11], [12, 3.2], [21, 11]])
            poly([[5.4, 9.6], [5.4, 20.5], [18.6, 20.5], [18.6, 9.6]])
            poly([[10, 20.5], [10, 14.5], [14, 14.5], [14, 20.5]])
            break
        case "shield":
            poly([[12, 2.8], [20, 6], [20, 11.5], [12, 21.2], [4, 11.5], [4, 6]], true)
            break
        case "shield-check":
            poly([[12, 2.8], [20, 6], [20, 11.5], [12, 21.2], [4, 11.5], [4, 6]], true)
            poly([[8.6, 11.6], [11.1, 14.1], [15.6, 8.8]])
            break
        case "shield-off":
            poly([[12, 2.8], [20, 6], [20, 11.5], [12, 21.2], [4, 11.5], [4, 6]], true)
            poly([[9, 12], [15, 12]])
            break
        case "globe":
            circle(12, 12, 9)
            ctx.save(); ctx.translate(12, 12); ctx.scale(0.5, 1)
            ctx.beginPath(); ctx.arc(0, 0, 9, 0, Math.PI * 2); ctx.stroke(); ctx.restore()
            poly([[3.2, 9], [20.8, 9]])
            poly([[3.2, 15], [20.8, 15]])
            break
        case "bars":
            poly([[4.5, 20], [4.5, 13]])
            poly([[9.5, 20], [9.5, 8.5]])
            poly([[14.5, 20], [14.5, 15]])
            poly([[19.5, 20], [19.5, 5]])
            break
        case "gear":
            circle(12, 12, 3.4)
            for (var g = 0; g < 8; g++) {
                var a = g * Math.PI / 4
                poly([[12 + Math.cos(a) * 6.2, 12 + Math.sin(a) * 6.2],
                      [12 + Math.cos(a) * 9.2, 12 + Math.sin(a) * 9.2]])
            }
            break
        case "clock":
            circle(12, 12, 8.6)
            poly([[12, 7], [12, 12], [16, 14]])
            break
        case "target":
            circle(12, 12, 8.6); circle(12, 12, 4.8); circle(12, 12, 1.3, true)
            break
        case "pulse":
            poly([[2.5, 12], [7, 12], [10, 5], [14, 19], [17, 12], [21.5, 12]])
            break
        case "alert":
            poly([[12, 3.4], [22, 20.4], [2, 20.4]], true)
            poly([[12, 9.5], [12, 14.5]])
            circle(12, 17.6, 1.05, true)
            break
        case "check-circle":
            circle(12, 12, 8.8)
            poly([[8.2, 12.2], [10.9, 14.9], [15.9, 9.1]])
            break
        case "check":
            poly([[5, 12.6], [9.6, 17.2], [19, 6.8]])
            break
        case "coffee":
            poly([[4.5, 9.5], [4.5, 16.5], [15.5, 16.5], [15.5, 9.5]], true)
            ctx.beginPath(); ctx.arc(15.5, 12.8, 3.2, -Math.PI / 2, Math.PI / 2); ctx.stroke()
            poly([[3.5, 20], [16.5, 20]])
            poly([[8, 6.5], [8, 4]]); poly([[12, 6.5], [12, 4]])
            break
        case "play":
            poly([[8, 5.2], [19, 12], [8, 18.8]], true, true)
            break
        case "stop":
            ctx.beginPath(); ctx.rect(6.5, 6.5, 11, 11); ctx.stroke()
            break
        case "bell":
            poly([[6, 17], [6, 10.6], [12, 4.4], [18, 10.6], [18, 17]], true)
            poly([[3.6, 17], [20.4, 17]])
            poly([[10, 19.4], [14, 19.4]])
            break
        case "moon":
            ctx.beginPath()
            ctx.arc(12, 12, 8.6, Math.PI * 0.32, Math.PI * 1.48)
            ctx.arc(9, 10.4, 8.6, Math.PI * 1.48, Math.PI * 0.32, true)
            ctx.closePath(); ctx.stroke()
            break
        case "chevron":
            poly([[9.5, 5.5], [16, 12], [9.5, 18.5]])
            break
        case "chevron-left":
            poly([[14.5, 5.5], [8, 12], [14.5, 18.5]])
            break
        case "caret":
            poly([[7, 10], [12, 15], [17, 10]])
            break
        case "kebab":
            circle(12, 5.4, 1.5, true); circle(12, 12, 1.5, true); circle(12, 18.6, 1.5, true)
            break
        case "plus":
            poly([[12, 5.5], [12, 18.5]]); poly([[5.5, 12], [18.5, 12]])
            break
        case "x":
            poly([[6.5, 6.5], [17.5, 17.5]]); poly([[17.5, 6.5], [6.5, 17.5]])
            break
        case "search":
            circle(10.5, 10.5, 6.6)
            poly([[15.4, 15.4], [20.2, 20.2]])
            break
        case "download":
            poly([[12, 3.6], [12, 15.2]])
            poly([[7.2, 10.6], [12, 15.4], [16.8, 10.6]])
            poly([[4.4, 19.6], [19.6, 19.6]])
            break
        case "trash":
            poly([[4.2, 6.6], [19.8, 6.6]])
            poly([[9.2, 6.6], [9.2, 4.2], [14.8, 4.2], [14.8, 6.6]])
            poly([[6.4, 6.6], [7.4, 20.2], [16.6, 20.2], [17.6, 6.6]])
            poly([[10.3, 10], [10.3, 17]]); poly([[13.7, 10], [13.7, 17]])
            break
        case "refresh":
            ctx.beginPath(); ctx.arc(12, 12, 7.8, Math.PI * 0.32, Math.PI * 1.75); ctx.stroke()
            poly([[17.4, 3.4], [18.4, 8.4], [13.4, 8.2]])
            break
        case "save":
            poly([[4.4, 5.6], [4.4, 19.6], [19.6, 19.6], [19.6, 8.4], [16.8, 5.6]], true)
            poly([[8, 5.6], [8, 10.4], [15, 10.4], [15, 5.6]])
            poly([[8, 19.6], [8, 14.4], [16, 14.4], [16, 19.6]])
            break
        case "folder":
            poly([[3.4, 19.4], [3.4, 5.6], [9.6, 5.6], [11.6, 8.2], [20.6, 8.2], [20.6, 19.4]], true)
            break
        case "cpu":
            ctx.beginPath(); ctx.rect(6.4, 6.4, 11.2, 11.2); ctx.stroke()
            ctx.beginPath(); ctx.rect(10, 10, 4, 4); ctx.stroke()
            poly([[9.4, 3], [9.4, 6.4]]); poly([[14.6, 3], [14.6, 6.4]])
            poly([[9.4, 17.6], [9.4, 21]]); poly([[14.6, 17.6], [14.6, 21]])
            poly([[3, 9.4], [6.4, 9.4]]); poly([[3, 14.6], [6.4, 14.6]])
            poly([[17.6, 9.4], [21, 9.4]]); poly([[17.6, 14.6], [21, 14.6]])
            break
        case "sliders":
            poly([[3.4, 7.5], [20.6, 7.5]]); poly([[3.4, 16.5], [20.6, 16.5]])
            circle(9, 7.5, 2.7); circle(15.6, 16.5, 2.7)
            break
        case "lock":
            ctx.beginPath(); ctx.rect(5, 10.6, 14, 9.6); ctx.stroke()
            ctx.beginPath(); ctx.arc(12, 10.4, 4.2, Math.PI, 0); ctx.stroke()
            break
        case "calendar":
            ctx.beginPath(); ctx.rect(3.8, 5.6, 16.4, 14.6); ctx.stroke()
            poly([[3.8, 10.2], [20.2, 10.2]])
            poly([[8.4, 3.2], [8.4, 7.2]]); poly([[15.6, 3.2], [15.6, 7.2]])
            break
        case "grid":
            ctx.beginPath(); ctx.rect(4, 4, 7, 7); ctx.stroke()
            ctx.beginPath(); ctx.rect(13, 4, 7, 7); ctx.stroke()
            ctx.beginPath(); ctx.rect(4, 13, 7, 7); ctx.stroke()
            ctx.beginPath(); ctx.rect(13, 13, 7, 7); ctx.stroke()
            break
        case "ban":
            circle(12, 12, 8.8)
            poly([[6, 18], [18, 6]])
            break
        case "flame":
            poly([[12, 2.8], [17.6, 9.4], [16.4, 12.4], [19, 14.6], [12, 21.2], [5.6, 15.2], [8.2, 8.6], [10.4, 11.6]], true)
            break
        case "user":
            circle(12, 8, 3.8)
            ctx.beginPath()
            ctx.arc(12, 20.5, 6.5, Math.PI, 0)
            ctx.stroke()
            break
        }
    }
}
