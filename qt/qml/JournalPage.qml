import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Surface {
    id: root
    required property var controller
    signal addRequested()
    property string selectedId: ""
    property string selectedDescription: ""
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 16
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Журнал операций"; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.fillWidth: true }
            ActionButton { text: "+ Операция"; primary: true; enabled: root.controller.writable; onClicked: root.addRequested() }
        }
        Text { text: "От новых к старым. Выберите строку для удаления; связь сохраняется по ID операции."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Flickable {
            id: table
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: Math.max(width,800); contentHeight: height
            ScrollBar.horizontal: ScrollBar { }
            ColumnLayout {
                width: table.contentWidth; height: table.height; spacing: 0
                Row {
                    Repeater {
                        model: ["Дата","Тикер","Операция","Акций","Цена, ₽","Комиссия, ₽"]
                        Text { required property string modelData; text: modelData; width: 130; height: 36; color: Theme.muted; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter }
                    }
                }
                ListView {
                    id: list
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: root.controller.operations
                    ScrollBar.vertical: ScrollBar { }
                    delegate: ItemDelegate {
                        id: row
                        required property var record
                        width: list.width; height: 56
                        Accessible.name: record.date + " " + record.ticker + " " + record.side
                        background: Rectangle { color: root.selectedId === row.record.id ? Theme.selection : row.hovered ? Theme.raised : Theme.surface; border.color: row.activeFocus ? Theme.accent : Theme.border; border.width: row.activeFocus ? 2 : 0 }
                        onClicked: { root.selectedId = record.id; root.selectedDescription = record.date + " · " + record.ticker + " · " + record.side }
                        contentItem: Row {
                            Repeater {
                                model: [row.record.date,row.record.ticker,row.record.side,row.record.quantity,row.record.price,row.record.fee]
                                Text { required property string modelData; text: modelData; width: 130; height: 40; color: Theme.text; font.pixelSize: 14; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                    }
                    Text { visible: list.count === 0; anchors.centerIn: parent; text: "Операций пока нет"; color: Theme.muted }
                }
            }
        }
        ActionButton { text: "Удалить выбранную"; destructive: true; enabled: root.controller.writable && root.selectedId !== ""; onClicked: confirm.open() }
    }
    Dialog {
        id: confirm
        parent: Overlay.overlay; anchors.centerIn: parent; modal: true
        title: "Удаление операции"
        width: Math.min(480, parent.width - 32)
        height: Math.min(280, parent.height - 32)
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label { text: root.selectedDescription + "\n\nУдаление может изменить остаток и границы позиции. Старые уровни останутся у прежнего ID. Продолжить?"; wrapMode: Text.WordWrap }
        onAccepted: { if(root.controller.deleteOperation(root.selectedId)) { root.selectedId = ""; root.selectedDescription = "" } }
    }
}
