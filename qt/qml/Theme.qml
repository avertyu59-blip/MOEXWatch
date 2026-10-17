pragma Singleton
import QtQuick
QtObject {
    property bool dark: true
    readonly property color canvas: dark ? "#0b1219" : "#f2f5f9"
    readonly property color surface: dark ? "#141d26" : "#ffffff"
    readonly property color raised: dark ? "#1c2834" : "#e9eef5"
    readonly property color text: dark ? "#ebf0f5" : "#182433"
    readonly property color muted: dark ? "#acb9c7" : "#4f6074"
    readonly property color border: dark ? "#314151" : "#c5cfdb"
    readonly property color accent: "#1762c0"
    readonly property color onAccent: "#ffffff"
    readonly property color selection: dark ? "#16395c" : "#d6e7fc"
    readonly property color positive: dark ? "#56c979" : "#147741"
    readonly property color negative: dark ? "#ff6975" : "#bc2a3b"
    readonly property color warning: dark ? "#f2af49" : "#9b5c00"
    readonly property var chartColors: [positive, "#3e91f5", "#ad80ec", warning, muted]
    readonly property int radius: 12
    readonly property int gap: 16
    readonly property string family: Qt.platform.os === "windows" ? "Segoe UI" : "DejaVu Sans"
}
