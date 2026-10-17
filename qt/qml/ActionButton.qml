import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool primary: false
    property bool destructive: false
    implicitHeight: 40
    implicitWidth: Math.max(104, contentItem.implicitWidth + 32)
    font.family: Theme.family
    font.pixelSize: 14
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    background: Rectangle {
        radius: 8
        color: control.down ? Theme.selection : control.primary ? Theme.accent : control.hovered ? Theme.selection : Theme.raised
        opacity: control.enabled ? 1 : 0.5
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.muted : control.primary ? Theme.textOnAccent : control.destructive ? Theme.negative : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
