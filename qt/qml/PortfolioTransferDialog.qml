import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: dialog
    objectName: "portfolioTransferDialog"
    required property var controller
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    title: "Импорт и экспорт операций · CSV UTF-8"
    width: Math.min(1000, parent.width - 32)
    height: Math.min(720, parent.height - 32)
    background: Surface {}
    onClosed: controller.cancelImport()
    contentItem: ColumnLayout {
        spacing: 8
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Формат: date;ticker;side;quantity;price;commission. Дата ГГГГ-ММ-ДД, сторона BUY/SELL. Экспорт также сохраняет id, position_id, sequence. Применение целиком блокируется при ошибке." }
        RowLayout {
            ActionButton { text: "Выбрать CSV"; enabled: !dialog.controller.busy; onClicked: dialog.controller.chooseImport() }
            ActionButton { text: "Экспорт журнала"; enabled: !dialog.controller.busy; onClicked: dialog.controller.chooseExport() }
            BusyIndicator { running: dialog.controller.busy; visible: running; implicitWidth: 32; implicitHeight: 32 }
        }
        CheckBox { text: "Включить возможные дубликаты без ID"; checked: dialog.controller.includeDuplicates; onToggled: dialog.controller.includeDuplicates = checked }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.negative; visible: dialog.controller.includeDuplicates; text: "Внимание: одинаковые операции могут быть импортированы повторно. Совпадающие ID всегда пропускаются. Проверка без ID сравнивает дату, тикер, сторону, количество и цену, без комиссии." }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Новые: " + dialog.controller.newCount + " · Совпадения: " + dialog.controller.duplicateCount + " · Ошибки: " + dialog.controller.errorCount + " · Новые позиции: " + dialog.controller.newPositionCount + "\nИзменятся: " + dialog.controller.affectedTickers.join(", ") }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
            model: dialog.controller.previewRows
            ScrollBar.vertical: ScrollBar {}
            delegate: Surface {
                required property var record
                width: ListView.view.width; height: content.implicitHeight + 16
                ColumnLayout {
                    id: content; anchors.fill: parent; anchors.margins: 8; spacing: 3
                    Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: record.status === "error" ? Theme.negative : Theme.text; text: "Строка " + record.line + " · " + (record.date || "") + " · " + (record.ticker || "") + " " + (record.side || "") + " · " + (record.quantity || "") + " × " + (record.price || "") + " · Комиссия: " + (record.commission === undefined ? "—" : record.commission) }
                    Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: record.message }
                    Label { Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font.pixelSize: 11; visible: !!record.id; text: "ID: " + (record.id || "") + " · Позиция: " + (record.positionId || "") }
                }
            }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: dialog.controller.status }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.negative; visible: text.length > 0; text: dialog.controller.error }
    }
    footer: DialogButtonBox {
        ActionButton { objectName: "applyCsvImport"; text: "Применить импорт"; primary: true; enabled: dialog.controller.summary.canApply; onClicked: dialog.controller.applyImport(dialog.controller.previewToken) }
        ActionButton { text: "Отменить предпросмотр"; onClicked: dialog.controller.cancelImport() }
        ActionButton { text: "Закрыть"; onClicked: dialog.close() }
    }
}
