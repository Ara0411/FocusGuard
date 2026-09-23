import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: ct
    property string label: ""
    property string glyph: "bars"
    property color glyphColor: "#A5B4FC"
    spacing: 9
    Icon { name: ct.glyph; stroke: ct.glyphColor
           implicitWidth: 19; implicitHeight: 19 }
    Txt { text: ct.label; font.pixelSize: 14; font.weight: Font.DemiBold }
}
