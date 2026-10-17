import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: dialog
    required property var controller
    property string positionId
    property var selected: ({})
    parent: Overlay.overlay; anchors.centerIn: parent
    modal: true; title: "Уровни позиции · " + (selected.ticker || "")
    width: Math.min(440, parent.width - 32)
    onOpened: { selected=controller.position(positionId); stop.text=selected.stop || ""; take.text=selected.take || ""; controller.clearError() }
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "Пустое поле отключает уровень. Настройка относится к выбранному циклу позиции."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Стоп-лосс, ₽" }
        TextField { id: stop; Layout.fillWidth: true; Accessible.name: "Стоп-лосс" }
        Label { text: "Тейк-профит, ₽" }
        TextField { id: take; Layout.fillWidth: true; Accessible.name: "Тейк-профит" }
        Label { text: dialog.controller.error; visible: text.length>0; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    footer: DialogButtonBox {
        ActionButton { text: "Сохранить"; primary: true; enabled: dialog.controller.writable; onClicked: { if(dialog.controller.saveLevels(dialog.positionId,stop.text,take.text)) dialog.close() } }
        ActionButton { text: "Отмена"; onClicked: dialog.close() }
    }
}
