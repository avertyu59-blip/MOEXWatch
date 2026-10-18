import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Surface {
    id: root
    required property var controller
    implicitHeight: body.implicitHeight + 32
    ColumnLayout {
        id: body
        anchors.fill: parent; anchors.margins: 16; spacing: 12
        Label { text: "Дивидендный доход"; font.pixelSize: 20; color: Theme.text }
        Label { text: "Получено после удержания налога: " + root.controller.totals.receivedNetText + " · Ожидается до налогов: " + root.controller.totals.expectedGrossText; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Прогноз состава до даты последней покупки: " + root.controller.totals.projectedGrossText; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Ожидания не входят в прибыль и свободные средства. Факт подтверждается по брокерскому отчёту. Кэш календаря может быть устаревшим; проверьте источник перед подтверждением."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: text.length>0; text: root.controller.error; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Repeater {
            model: root.controller.events
            delegate: ColumnLayout {
                required property var record
                Layout.fillWidth: true
                Label { text: record.ticker + " · " + record.recordDate + " · " + record.grossText + (record.projected ? " · прогноз состава" : " · ожидается"); color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ActionButton { text: "Подтвердить поступление"; enabled: root.controller.writable && record.canConfirm; onClicked: { const p=root.controller.preview(record.key); if(p.previewToken){ receipt.preview=p; receiptDate.text=Qt.formatDate(new Date(),"yyyy-MM-dd"); receiptGross.text=p.gross; receiptTax.text="0"; receiptReference.clear(); receipt.open() } } }
            }
        }
        Repeater {
            model: root.controller.receipts
            delegate: ColumnLayout {
                required property var record
                Layout.fillWidth: true
                Label { text: record.ticker + " · " + record.paymentDate + " · получено " + record.netText; color: Theme.positive; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { visible: record.journalMismatch || record.correction; text: record.journalMismatch ? "Аудит: журнал или источник изменён/недоступен. Факт поступления сохранён." : "Аудит: сумма отличается от расчётной; в бэктест не включается."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ActionButton { text: "Удалить подтверждение"; enabled: root.controller.writable; destructive: true; onClicked: { deletion.receiptId=record.id; deletion.open() } }
            }
        }
    }
    Dialog {
        id: receipt
        objectName: "dividendReceiptDialog"
        parent: Overlay.overlay
        property var preview: ({})
        modal: true
        title: "Подтверждение дивиденда · " + (preview.ticker || "")
        anchors.centerIn: parent
        width: Math.min(520,parent ? parent.width-24 : 520)
        height: Math.min(560,parent ? parent.height-24 : 560)
        ScrollView {
            anchors.fill: parent; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: parent.width; spacing: 12
                Label { text: "Право по журналу: " + (receipt.preview.quantity || 0) + " акций. Введите фактические суммы по отчёту. Налог автоматически не оценивается."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: receipt.preview.source || ""; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; color: Theme.muted }
                TextField { id: receiptDate; objectName: "dividendPaymentDate"; placeholderText: "Дата поступления YYYY-MM-DD"; Layout.fillWidth: true; Accessible.name: placeholderText }
                TextField { id: receiptGross; objectName: "dividendGross"; placeholderText: "Начислено до налога, ₽"; Layout.fillWidth: true; Accessible.name: placeholderText }
                TextField { id: receiptTax; objectName: "dividendTax"; placeholderText: "Фактически удержанный налог, ₽"; Layout.fillWidth: true; Accessible.name: placeholderText }
                TextField { id: receiptReference; objectName: "dividendReference"; placeholderText: "Ссылка на строку отчёта / примечание"; maximumLength: 1000; Layout.fillWidth: true; Accessible.name: placeholderText }
                Label { text: root.controller.error; visible: text.length>0; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ActionButton { text: "Сохранить поступление"; objectName: "dividendConfirm"; enabled: root.controller.writable; Layout.fillWidth: true; onClicked: { if(root.controller.confirm(receipt.preview.key,receiptDate.text,receiptGross.text,receiptTax.text,receiptReference.text,receipt.preview.previewToken))receipt.close() } }
                ActionButton { text: "Отмена"; Layout.fillWidth: true; onClicked: receipt.close() }
            }
        }
    }
    Dialog {
        id: deletion
        parent: Overlay.overlay
        property string receiptId: ""
        modal: true; anchors.centerIn: parent
        title: "Удалить факт поступления?"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: root.controller.removeReceipt(receiptId)
    }
}
