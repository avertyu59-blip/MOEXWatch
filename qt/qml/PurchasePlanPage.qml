import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
 id: root
 required property var controller
 required property var marketController
 property var result: ({})
 contentWidth: availableWidth; clip: true
 ColumnLayout {
  width: root.availableWidth; spacing: 16
  Text { text: "План покупки"; color: Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold }
  Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; text: "Размер позиции ограничивается бюджетом с комиссией и допустимым убытком. Размер лота укажите по спецификации бумаги. Расчёт для покупки без плеча; гэп может увеличить убыток." }
  Surface {
   Layout.fillWidth: true; implicitHeight: form.implicitHeight + 32
   GridLayout {
    id: form; anchors.fill: parent; anchors.margins: 16; columns: root.width < 650 ? 2 : 4; columnSpacing: 16; rowSpacing: 12
    Label { text: "Тикер"; color: Theme.text }
    TextField { id: ticker; objectName: "purchaseTicker"; Layout.fillWidth: true; text: root.marketController.selectedTicker; maximumLength: 32; onTextChanged: root.result = ({}) }
    Label { text: "Цена входа, ₽"; color: Theme.text }
    TextField { id: entry; objectName: "purchaseEntry"; Layout.fillWidth: true; placeholderText: "Например, 300"; onTextChanged: root.result = ({}) }
    Label { text: "Бюджет, ₽"; color: Theme.text }
    TextField { id: budget; objectName: "purchaseBudget"; Layout.fillWidth: true; text: "10000"; onTextChanged: root.result = ({}) }
    Label { text: "Акций в лоте"; color: Theme.text }
    TextField { id: lot; objectName: "purchaseLot"; Layout.fillWidth: true; placeholderText: "Обязательное поле"; onTextChanged: root.result = ({}) }
    Label { text: "Комиссия за сторону, %"; color: Theme.text; wrapMode: Text.WordWrap; Layout.maximumWidth: 150 }
    TextField { id: fee; objectName: "purchaseFee"; Layout.fillWidth: true; text: "0.05"; onTextChanged: root.result = ({}) }
    Label { text: "Стоп, ₽"; color: Theme.text }
    TextField { id: stop; objectName: "purchaseStop"; Layout.fillWidth: true; onTextChanged: root.result = ({}) }
    Label { text: "Цель, ₽"; color: Theme.text }
    TextField { id: target; objectName: "purchaseTarget"; Layout.fillWidth: true; onTextChanged: root.result = ({}) }
    Label { text: "Лимит убытка, ₽"; color: Theme.text }
    TextField { id: risk; objectName: "purchaseRisk"; Layout.fillWidth: true; text: "500"; onTextChanged: root.result = ({}) }
   }
  }
  ActionButton { objectName: "purchaseCalculate"; text: "Рассчитать"; primary: true; onClicked: root.result = root.controller.calculate(ticker.text,entry.text,budget.text,lot.text,fee.text,stop.text,target.text,risk.text) }
  Text { Layout.fillWidth: true; visible: root.result.valid === false; text: root.result.error || ""; color: Theme.negative; wrapMode: Text.WordWrap }
  Surface {
   visible: root.result.valid === true; Layout.fillWidth: true; implicitHeight: summary.implicitHeight + 32
   ColumnLayout {
    id: summary; anchors.fill: parent; anchors.margins: 16; spacing: 10
    Text { text: "Лотов: " + root.result.lots + " · Акций: " + root.result.quantity; color: Theme.text; font.pixelSize: 20 }
    Text { text: "Покупка с комиссией: " + Number(root.result.total).toFixed(2) + " ₽ · Комиссия входа: " + Number(root.result.commission).toFixed(2) + " ₽"; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Text { text: "Убыток при исполнении по стопу: " + Number(root.result.loss).toFixed(2) + " ₽ (" + Number(root.result.riskPercent).toFixed(2) + "% бюджета)"; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Text { text: "Прибыль у цели до налога: " + Number(root.result.profit).toFixed(2) + " ₽ · Прибыль / риск: " + Number(root.result.ratio).toFixed(2); color: root.result.profit > 0 ? Theme.positive : Theme.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Text { text: "Обе стороны комиссии учтены. Стоп — сценарий, исполнение по этой цене не гарантировано."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
   }
  }
  Label { text: "Почему рассматриваю покупку"; color: Theme.text }
  TextArea { id: reason; objectName: "purchaseReason"; Layout.fillWidth: true; Layout.preferredHeight: 100; wrapMode: TextEdit.Wrap; placeholderText: "Тезис, факты, условия отмены идеи"; padding: 12; color: Theme.text; placeholderTextColor: Theme.muted; Accessible.name: "Обоснование покупки"; background: Rectangle { color: Theme.surface; radius: 8; border.width: reason.activeFocus ? 2 : 1; border.color: reason.activeFocus ? Theme.accent : Theme.border } }
  Label { text: "Условия выхода и последующий разбор"; color: Theme.text }
  TextArea { id: exitReason; objectName: "purchaseExitReason"; Layout.fillWidth: true; Layout.preferredHeight: 100; wrapMode: TextEdit.Wrap; placeholderText: "Условия выхода и итоговый разбор"; padding: 12; color: Theme.text; placeholderTextColor: Theme.muted; Accessible.name: "Условия выхода и разбор решения"; background: Rectangle { color: Theme.surface; radius: 8; border.width: exitReason.activeFocus ? 2 : 1; border.color: exitReason.activeFocus ? Theme.accent : Theme.border } }
  ActionButton { text: "Сохранить идею в дневник"; enabled: root.result.valid === true && root.controller.writable; onClicked: if(root.controller.saveDecision(root.result,reason.text,exitReason.text)) { saved.text = "Идея сохранена в дневник"; reason.clear(); exitReason.clear() } }
  Text { id: saved; color: Theme.positive }
  Text { text: root.controller.error; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
  Text { text: "Сохранение идеи не создаёт операцию в портфеле и не отправляет заявку."; color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
 }
}
