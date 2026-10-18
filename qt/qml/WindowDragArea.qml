import QtQuick
import QtQuick.Controls

MouseArea {
    id: dragArea
    required property var targetWindow
    property point pressGlobal
    property point origin
    property real anchorRatio: 0
    property real anchorY: 0
    property bool moving: false
    property bool manualMove: false
    acceptedButtons: Qt.LeftButton
    hoverEnabled: true
    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
    ToolTip.visible: containsMouse && !pressed
    ToolTip.delay: 800
    ToolTip.text: "Перетащите окно · Двойной щелчок — развернуть"

    onPressed: function(mouse) {
        pressGlobal = mapToGlobal(mouse.x, mouse.y)
        origin = Qt.point(targetWindow.x, targetWindow.y)
        const local = targetWindow.contentItem.mapFromGlobal(pressGlobal.x, pressGlobal.y)
        anchorRatio = Math.max(0, Math.min(1, local.x / targetWindow.width))
        anchorY = Math.max(0, Math.min(40, local.y))
        moving = false
        manualMove = false
    }
    onPositionChanged: function(mouse) {
        if (!pressed) return
        const global = mapToGlobal(mouse.x, mouse.y)
        if (!moving) {
            if (Math.abs(global.x - pressGlobal.x) + Math.abs(global.y - pressGlobal.y) < Qt.styleHints.startDragDistance) return
            moving = true
            if (targetWindow.visibility === Window.Maximized) {
                targetWindow.showNormal()
                targetWindow.x = Math.round(global.x - targetWindow.width * anchorRatio)
                targetWindow.y = Math.round(global.y - anchorY)
                origin = Qt.point(targetWindow.x, targetWindow.y)
                pressGlobal = global
            }
            // Native move retains Windows snapping and cross-monitor DPI handling.
            // Platforms that reject it still allow moving the frameless window.
            const started = targetWindow.startSystemMove()
            manualMove = !started && pressed
        }
        if (manualMove) {
            targetWindow.x = Math.round(origin.x + global.x - pressGlobal.x)
            targetWindow.y = Math.round(origin.y + global.y - pressGlobal.y)
        }
    }
    onReleased: { moving = false; manualMove = false }
    onCanceled: { moving = false; manualMove = false }
    onDoubleClicked: targetWindow.visibility === Window.Maximized ? targetWindow.showNormal() : targetWindow.showMaximized()
}
