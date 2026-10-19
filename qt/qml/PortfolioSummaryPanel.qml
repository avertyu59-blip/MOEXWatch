import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: root
    required property var controller
    required property var dividends
    readonly property var income: { const snapshot=controller.totals; return controller.incomeWithDividends(Number(dividends.totals.receivedNet || 0),dividends.totals.valid===true) }
    Layout.fillWidth: true
    Connections { target: root.controller; function onPortfolioChanged() { if(!cashInput.activeFocus) cashInput.text=root.controller.totals.cashRaw } }
    spacing: 12
    GridLayout {
        Layout.fillWidth: true
        columns: root.width < 560 ? 1 : root.width < 1100 ? 2 : 3
        columnSpacing: 16; rowSpacing: 12
        MetricCard { Layout.fillWidth: true; title: "Активы с учётом средств"; amount: root.controller.totals.totalAssets; note: root.controller.totals.valuationNote }
        MetricCard { Layout.fillWidth: true; title: "Себестоимость позиций"; amount: root.controller.totals.cost; note: "Остаток покупок с комиссиями" }
        MetricCard { Layout.fillWidth: true; title: "Бумажная прибыль"; amount: root.controller.totals.unrealized; amountColor: root.controller.totals.unrealizedValue < 0 ? Theme.negative : Theme.positive; note: "Открытые позиции · до налога" }
        MetricCard { Layout.fillWidth: true; title: "Продажи + дивиденды"; amount: root.income.realized; note: "Продажи до налога + дивиденды после указанного удержания" }
        MetricCard { Layout.fillWidth: true; title: "Общий результат"; amount: root.income.total; note: "Продажи, бумажная прибыль и полученные дивиденды. Налог на сделки не рассчитан." }
        MetricCard { Layout.fillWidth: true; title: "Доля трёх крупнейших"; amount: root.controller.totals.top3; note: "От стоимости бумаг · только свежие котировки" }
    }
    Surface {
        Layout.fillWidth: true
        implicitHeight: cashColumn.implicitHeight + 32
        ColumnLayout {
            id: cashColumn
            anchors.fill: parent; anchors.margins: 16; spacing: 8
            Label { text: "Свободные средства: " + root.controller.totals.cash; color: Theme.text; font.family: Theme.family; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Flow {
                Layout.fillWidth: true; spacing: 8
                TextField {
                    id: cashInput
                    objectName: "portfolioFreeCash"
                    width: Math.min(240, parent.width)
                    text: ""
                    Component.onCompleted: text=root.controller.totals.cashRaw
                    onActiveFocusChanged: if(!activeFocus) text=root.controller.totals.cashRaw
                    placeholderText: "Сумма в рублях"
                    enabled: root.controller.cashWritable
                    selectByMouse: true
                    Accessible.name: "Свободные средства в рублях"
                    onAccepted: root.controller.setFreeCash(text)
                }
                ActionButton { text: "Сохранить средства"; enabled: root.controller.cashWritable; onClicked: root.controller.setFreeCash(cashInput.text); Accessible.name: "Сохранить свободные средства портфеля" }
            }
            Label { visible: root.controller.totals.cashError.length > 0; text: root.controller.totals.cashError; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true; Accessible.role: Accessible.AlertMessage }
            Label { text: "Ручной остаток. Покупки, продажи и дивиденды не изменяют его автоматически. Не добавляйте дивиденд повторно, если он уже включён в остаток."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { text: "Результат продаж до налога: " + root.controller.totals.realized; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { text: "Котировки: актуальных " + root.controller.totals.freshCount + " · устаревших " + root.controller.totals.staleCount + " · отсутствуют " + root.controller.totals.missingCount; color: root.controller.totals.fresh ? Theme.muted : Theme.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
