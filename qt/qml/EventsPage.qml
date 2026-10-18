import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: root
    required property var controller
    required property var market
    property bool showEventForm: false
    onVisibleChanged: { if(visible && controller && !controller.busy) controller.refresh() }
    Connections { target: root.market; function onSelectionChanged() { if(root.visible) Qt.callLater(root.controller.refresh) } }
    clip: true
    contentWidth: availableWidth
    Dialog {
        id: deletion
        property string eventId: ""
        title: "Удалить событие календаря?"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: root.controller.removeCorporate(eventId)
    }
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.gap
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Дивиденды и события"; color: Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold; Layout.fillWidth: true }
            ComboBox { model: root.market.tickers; currentIndex: Math.max(0,root.market.tickers.indexOf(root.market.selectedTicker)); onActivated: root.market.selectedTicker=currentText }
            ActionButton { text: root.controller.busy ? "Отменить" : "Обновить"; onClicked: root.controller.busy ? root.controller.cancel() : root.controller.refresh() }
        }
        Label { text: root.controller.status; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: text.length>0; text: root.controller.error; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "Сумма — на одну акцию. Доходность рассчитывается только по свежей котировке. Прогнозы и рекомендации не являются фактическими выплатами. Дата последней покупки рассчитывается только при известном торговом календаре."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Surface {
            Layout.fillWidth: true
            implicitHeight: calendarForm.implicitHeight+32
            ColumnLayout {
                id: calendarForm
                anchors.fill: parent; anchors.margins: 16
                Label { text: "Мой календарь: отчётность и собрания"; color: Theme.text; font.pixelSize: 20 }
                Label { text: "События добавляются пользователем по источнику. Автоматические даты не подставляются."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                CheckBox { text: "Добавить событие"; checked: root.showEventForm; onToggled: root.showEventForm=checked }
                TextField { visible: root.showEventForm; id: eventDate; placeholderText: "Дата YYYY-MM-DD"; Layout.fillWidth: true }
                ComboBox { visible: root.showEventForm; id: eventType; model: ["Отчётность","Собрание","Другое"]; Layout.fillWidth: true }
                TextField { visible: root.showEventForm; id: eventTitle; placeholderText: "Описание события"; maximumLength: 300; Layout.fillWidth: true }
                TextField { visible: root.showEventForm; id: eventSource; placeholderText: "https:// — официальный источник"; Layout.fillWidth: true }
                ComboBox { visible: root.showEventForm; id: eventStatus; model: ["Запланировано","Подтверждено","Отменено"]; Layout.fillWidth: true }
                ActionButton { visible: root.showEventForm; text: "Добавить событие для "+root.controller.ticker; enabled: root.controller.calendarWritable; onClicked: { if(root.controller.addCorporate(eventDate.text,eventType.currentText,eventTitle.text,eventSource.text,eventStatus.currentText)){eventTitle.clear();eventSource.clear()} } }
            }
        }
        Repeater {
            model: root.controller.corporate
            delegate: Surface {
                required property var record
                Layout.fillWidth: true
                implicitHeight: corporateRow.implicitHeight+32
                ColumnLayout {
                    id: corporateRow
                    anchors.fill: parent; anchors.margins: 16
                    Label { text: record.date+" · "+record.type+" · "+record.status; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: record.title; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout {
                        ActionButton { text: "Источник"; onClicked: root.controller.openSource(record.source) }
                        ActionButton { text: "Удалить"; enabled: root.controller.calendarWritable; onClicked: { deletion.eventId=record.id;deletion.open() } }
                    }
                }
            }
        }
        Repeater {
            model: root.controller.dividends
            delegate: Surface {
                required property var record
                Layout.fillWidth: true
                implicitHeight: card.implicitHeight+32
                ColumnLayout {
                    id: card
                    anchors.fill: parent; anchors.margins: 16; spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: Number(record.amount).toLocaleString(Qt.locale("ru_RU"),'f',2)+" "+record.currency; color: Theme.text; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true }
                        Label { text: record.status+(record.stale ? " · сохранено" : ""); color: record.stale ? Theme.warning : Theme.text }
                    }
                    Label { text: "Отсечка: "+record.recordDate+" · Последняя покупка: "+(record.lastBuy || "не рассчитана"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: "Доходность: "+(record.yield === undefined ? "нет свежей цены" : Number(record.yield).toFixed(2)+" %")+" · "+record.verification; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Label { text: record.lastBuyBasis; color: Theme.muted; visible: text.length>0; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout {
                        ActionButton { text: "Источник календаря"; enabled: record.source.length>0; onClicked: root.controller.openSource(record.source) }
                        ActionButton { text: "Источник эмитента"; enabled: record.issuerSource.length>0; onClicked: root.controller.openSource(record.issuerSource) }
                    }
                    Label { text: record.issuerChecked ? "Проверка эмитента: "+record.issuerChecked : ""; visible: text.length>0; color: Theme.muted }
                }
            }
        }
        Label { visible: root.controller.dividendCount===0; text: "Событий нет в загруженном календаре. Это не подтверждает отсутствие будущих выплат."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
}
