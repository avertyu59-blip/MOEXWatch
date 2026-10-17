import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: dialog
    objectName: "operationDialog"
    required property var controller
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true; title: "Записать операцию"
    width: Math.min(460, parent.width - 32)
    height: Math.min(610, parent.height - 32)
    onOpened: { ticker.forceActiveFocus(); controller.clearError() }
    contentItem: ScrollView {
        clip: true
        ColumnLayout {
            width: parent.width; spacing: 8
            Label { text: "Дата · ГГГГ-ММ-ДД" }
            TextField { id: date; objectName: "operationDate"; text: Qt.formatDate(new Date(),"yyyy-MM-dd"); Layout.fillWidth: true; Accessible.name: "Дата операции" }
            Label { text: "Тикер" }
            TextField { id: ticker; objectName: "operationTicker"; placeholderText: "SBER"; Layout.fillWidth: true; maximumLength: 32; Accessible.name: "Тикер" }
            Label { text: "Операция" }
            ComboBox { id: side; model: ["Покупка","Продажа"]; Layout.fillWidth: true; Accessible.name: "Сторона операции" }
            Label { text: "Количество акций" }
            TextField { id: quantity; objectName: "operationQuantity"; placeholderText: "10"; Layout.fillWidth: true; Accessible.name: "Количество акций" }
            Label { text: "Цена за акцию, ₽" }
            TextField { id: price; objectName: "operationPrice"; placeholderText: "100,00"; Layout.fillWidth: true; Accessible.name: "Цена покупки или продажи" }
            Label { text: "Комиссия за операцию, ₽" }
            TextField { id: fee; text: "0"; Layout.fillWidth: true; Accessible.name: "Комиссия" }
            Label { text: dialog.controller.error; visible: text.length > 0; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    footer: DialogButtonBox {
        ActionButton {
            objectName: "saveOperationButton"; text: "Записать"; primary: true; enabled: dialog.controller.writable
            onClicked: {
                if(dialog.controller.addOperation(date.text,ticker.text,side.currentIndex,quantity.text,price.text,fee.text)) {
                    quantity.clear(); price.clear(); fee.text = "0"; dialog.close()
                }
            }
        }
        ActionButton { text: "Отмена"; onClicked: dialog.close() }
    }
}
