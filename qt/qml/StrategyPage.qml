pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    required property var controller
    required property var market
    clip: true
    contentWidth: availableWidth
    readonly property var outcome: controller.result
    function format(value, suffix) {
        return Number(value).toLocaleString(Qt.locale("ru_RU"), "f", 2) + (suffix || "")
    }
    ColumnLayout {
        width: root.availableWidth
        spacing: 16
        Surface {
            Layout.fillWidth: true
            implicitHeight: form.implicitHeight + 32
            ColumnLayout {
                id: form
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                anchors.margins: 16
                spacing: 12
                Label { text: "Проверка стратегии"; color: Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; text: "Бумага: " + root.market.selectedTicker + ". Выбор бумаги — в разделе «Обзор». Тест использует уже загруженную историю; обновление и запуск расчёта — отдельные действия." }
                Flow {
                    Layout.fillWidth: true
                    spacing: 12
                    Column {
                        spacing: 4
                        Label { text: "Стартовый капитал, ₽"; color: Theme.muted }
                        TextField { id: capital; objectName: "strategyCapital"; width: 190; text: "100000"; selectByMouse: true; Accessible.name: "Стартовый капитал в рублях" }
                    }
                    Column {
                        spacing: 4
                        Label { text: "Комиссия за сторону, %"; color: Theme.muted }
                        TextField { id: fee; objectName: "strategyFee"; width: 190; text: "0,05"; selectByMouse: true; Accessible.name: "Комиссия за сторону в процентах" }
                    }
                    Column {
                        spacing: 4
                        Label { text: "Начало, ГГГГ-ММ-ДД"; color: Theme.muted }
                        TextField { id: from; width: 190; placeholderText: "Вся история"; maximumLength: 10; selectByMouse: true; Accessible.name: "Начальная дата теста" }
                    }
                    Column {
                        spacing: 4
                        Label { text: "Окончание, ГГГГ-ММ-ДД"; color: Theme.muted }
                        TextField { id: to; width: 190; placeholderText: "Последняя сессия"; maximumLength: 10; selectByMouse: true; Accessible.name: "Конечная дата теста" }
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 12
                    Row {
                        spacing: 4
                        CheckBox { id: oversold; text: "RSI(14) ниже"; checked: true }
                        TextField { id: low; width: 76; text: "30"; Accessible.name: "Порог перепроданности RSI" }
                    }
                    Row {
                        spacing: 4
                        CheckBox { id: overbought; text: "RSI(14) выше" }
                        TextField { id: high; width: 76; text: "70"; Accessible.name: "Порог перекупленности RSI" }
                    }
                    CheckBox { id: sma; text: "Цена выше SMA200" }
                    Row {
                        spacing: 4
                        CheckBox { id: near; text: "До цели не более, %" }
                        TextField { id: distance; width: 76; text: "5"; Accessible.name: "Расстояние до целевой цены в процентах" }
                    }
                }
                CheckBox { id: all; text: "Требовать совпадения всех включённых условий" }
                Flow {
                    Layout.fillWidth: true
                    spacing: 12
                    ActionButton {
                        objectName: "strategyRun"
                        primary: true
                        text: root.controller.busy ? "Расчёт…" : "Запустить тест"
                        enabled: !root.controller.busy && !root.market.busy
                        onClicked: root.controller.run(capital.text, fee.text, from.text, to.text, {
                            oversoldEnabled: oversold.checked, oversoldBelow: low.text,
                            overboughtEnabled: overbought.checked, overboughtAbove: high.text,
                            aboveSma200: sma.checked, nearTarget: near.checked,
                            nearPercent: distance.text, requireAll: all.checked
                        })
                    }
                    ActionButton {
                        text: root.market.busy ? "Отменить обновление" : "Обновить историю"
                        onClicked: root.market.busy ? root.market.cancel() : root.market.refresh()
                    }
                }
                Label { Layout.fillWidth: true; visible: root.controller.error.length > 0; text: root.controller.error; color: Theme.negative; wrapMode: Text.WordWrap }
                Label { Layout.fillWidth: true; text: root.controller.status; color: Theme.muted; wrapMode: Text.WordWrap }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: Object.keys(root.outcome).length > 0
            color: root.outcome.outdated || root.outcome.historyStale || root.outcome.demo ? Theme.warning : Theme.text
            wrapMode: Text.WordWrap
            text: Object.keys(root.outcome).length ? (root.outcome.demo ? "ДЕМОНСТРАЦИОННЫЕ ДАННЫЕ · " : "")
                + root.outcome.ticker + " · " + root.outcome.from + " — " + root.outcome.to
                + " · общих сессий: " + root.outcome.commonSessions
                + " · условия: " + root.outcome.rules
                + " · капитал: " + root.format(root.outcome.capital, " ₽") + " · комиссия: " + root.format(root.outcome.commission, "% за сторону")
                + (root.outcome.outdated ? " · Данные изменились — повторите тест." : "")
                + (root.outcome.historyStale ? " · История сохранена локально либо заканчивается более 7 дней назад." : "") : ""
        }
        GridLayout {
            Layout.fillWidth: true
            visible: Object.keys(root.outcome).length > 0
            columns: root.availableWidth >= 1000 ? 4 : 2
            columnSpacing: 12; rowSpacing: 12
            MetricCard { Layout.fillWidth: true; title: "Стратегия"; amount: root.format(root.outcome.strategyReturn || 0, "%"); amountColor: root.outcome.strategyReturn >= 0 ? Theme.positive : Theme.negative; note: "Итог: " + root.format(root.outcome.strategyFinal || 0, " ₽") }
            MetricCard { Layout.fillWidth: true; title: "Удержание IMOEX"; amount: root.format(root.outcome.benchmarkReturn || 0, "%"); amountColor: root.outcome.benchmarkReturn >= 0 ? Theme.positive : Theme.negative; note: "Итог: " + root.format(root.outcome.benchmarkFinal || 0, " ₽") }
            MetricCard { Layout.fillWidth: true; title: "Максимальная просадка"; amount: root.format(root.outcome.drawdown || 0, "%"); note: "Стратегия, по закрытиям сессий" }
            MetricCard { Layout.fillWidth: true; title: "Закрытых сделок"; amount: String(root.outcome.trades || 0); note: "Включая закрытие в конце теста" }
        }
        Surface {
            Layout.fillWidth: true
            implicitHeight: assumptions.implicitHeight + 32
            Label {
                id: assumptions
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
                color: Theme.muted; wrapMode: Text.WordWrap
                text: "Как устроен тест\n\nСигнал определяется по закрытию сессии, исполнение — на открытии следующей общей торговой даты акции и IMOEX. Когда условие перестаёт выполняться, позиция закрывается. В конце периода оставшаяся позиция закрывается по последней цене закрытия.\n\nКапитал вкладывается целиком, допускаются дробные акции. Комиссия учитывается на покупке и продаже, в том числе при сравнении с IMOEX. Сравнение использует ценовой индекс, а не покупаемый инструмент.\n\nRSI и SMA рассчитываются заново от начала выбранного периода; для SMA200 нужно 200 сессий акции. Условие «Близко к цели» применяет текущую пользовательскую цель ко всему прошлому периоду: это сценарий, а не историческая рекомендация. Налог, дивиденды, лотность, проскальзывание и внутридневные стопы не учитываются. Прошлая доходность не предсказывает будущую."
                    + (root.outcome.smaWarmup ? "\n\nВнимание: выбранного периода недостаточно для полноценного сигнала SMA200." : "")
            }
        }
    }
}
