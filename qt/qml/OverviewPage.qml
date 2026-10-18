pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MOEXWatch.Charts 1.0

ScrollView {
    id: root
    required property var controller
    clip: true
    contentWidth: availableWidth
    readonly property var analysis: controller.analysis
    readonly property string ticker: controller.selectedTicker
    readonly property bool wide: availableWidth >= 1120
    onTickerChanged: {
        if (targetInput) targetInput.text = analysis.target > 0 ? String(analysis.target) : ""
    }
    onAnalysisChanged: {
        if (targetInput && !targetInput.activeFocus)
            targetInput.text = analysis.target > 0 ? String(analysis.target) : ""
    }
    function value(number, digits) {
        if (number === undefined || number === null || number === "") return "—"
        if (typeof number === "string") return number
        return isFinite(number) ? Number(number).toLocaleString(Qt.locale("ru_RU"), "f", digits === undefined ? 2 : digits) : "—"
    }
    function indicator(number, suffix) { return value(number) + (number === undefined || number === null ? "" : suffix) }
    Connections {
        target: root.controller
        function onSelectionChanged() { Qt.callLater(function() { chart.resetView() }) }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 16
        Surface {
            Layout.fillWidth: true
            implicitHeight: tools.implicitHeight + 32
            ColumnLayout {
                id: tools
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                anchors.margins: 16; spacing: 12
                Text { text: "Рынок и наблюдения"; color: Theme.text; font.family: Theme.family; font.pixelSize: 20; font.weight: Font.DemiBold }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: tickerInput
                        objectName: "overviewTickerInput"
                        width: Math.min(164, tools.width)
                        placeholderText: "Тикер, например SBER"
                        Accessible.name: "Тикер для добавления в наблюдения"
                        selectByMouse: true; maximumLength: 20
                        onAccepted: { if (addTickerButton.enabled) addTickerButton.clicked() }
                    }
                    ActionButton {
                        id: addTickerButton
                        objectName: "overviewAddTicker"
                        text: "+ В наблюдения"
                        enabled: root.controller.writable && tickerInput.text.trim().length > 0
                        onClicked: {
                            if (root.controller.addTicker(tickerInput.text.trim().toUpperCase())) tickerInput.clear()
                        }
                    }
                    ActionButton {
                        objectName: "overviewRefresh"
                        text: root.controller.busy ? "Отменить загрузку" : "Обновить данные"
                        primary: !root.controller.busy
                        onClicked: root.controller.busy ? root.controller.cancel() : root.controller.refresh()
                    }
                }
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: root.wide ? 2 : 1
            columnSpacing: 16; rowSpacing: 16
            Surface {
                Layout.fillWidth: true
                Layout.minimumWidth: 260
                Layout.preferredHeight: 624
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: root.controller.selectedTicker || "Выберите бумагу"; color: Theme.text; font.pixelSize: 23; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: root.value(root.analysis.price) + " ₽"; color: Theme.text; font.pixelSize: 20 }
                    }
                    Flow {
                        Layout.fillWidth: true; spacing: 6
                        Repeater {
                            model: [{label:"1 мес.",days:22},{label:"3 мес.",days:66},{label:"1 год",days:252},{label:"Всё",days:0}]
                            ActionButton {
                                required property var modelData
                                text: modelData.label; implicitWidth: 68
                                primary: root.controller.chartSessions === modelData.days
                                Accessible.name: "Период графика: " + modelData.label
                                onClicked: root.controller.chartSessions=modelData.days
                            }
                        }
                        ActionButton { text: "−"; implicitWidth: 40; Accessible.name: "Уменьшить масштаб графика"; onClicked: chart.zoomAt(0.8, chart.width / 2) }
                        ActionButton { text: "+"; implicitWidth: 40; Accessible.name: "Увеличить масштаб графика"; onClicked: chart.zoomAt(1.25, chart.width / 2) }
                        ActionButton { text: "Сброс"; implicitWidth: 72; onClicked: chart.resetView(); Accessible.name: "Сбросить масштаб и положение графика" }
                    }
                    Flow {
                        Layout.fillWidth: true; spacing: 8
                        CheckBox { text: "Сравнение с IMOEX, %"; checked: root.controller.chartComparison; onToggled: root.controller.chartComparison=checked; Accessible.name: text }
                        CheckBox { id: volumeToggle; text: "Объём"; checked: true; Accessible.name: text }
                        CheckBox { id: rsiToggle; text: "RSI(14)"; checked: true; Accessible.name: text }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: chart.hovered && chart.hovered.date ? chart.hovered.date + "   O " + root.value(chart.hovered.open) + "   H " + root.value(chart.hovered.high) + "   L " + root.value(chart.hovered.low) + "   C " + root.value(chart.hovered.close) : "Наведите указатель на свечу, чтобы увидеть OHLC"
                        color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap
                    }
                    Flow {
                        Layout.fillWidth: true; spacing: 12
                        Repeater {
                            model: root.controller.chartComparison
                                ? [{label:"Акция",tint:Theme.accent},{label:"IMOEX",tint:Theme.warning}]
                                : [{label:"SMA20",tint:Theme.accent},{label:"SMA50",tint:Theme.warning},{label:"SMA200",tint:Theme.chartColors[2]}]
                            Row {
                                required property var modelData
                                spacing: 4
                                Rectangle { width: 16; height: 2; anchors.verticalCenter: parent.verticalCenter; color: parent.modelData.tint }
                                Text { text: parent.modelData.label; color: Theme.muted; font.pixelSize: 11 }
                            }
                        }
                        Repeater {
                            model: root.controller.chartComparison ? [] : [
                                {label:"Средняя",tint:Theme.accent,level:root.controller.chartLevels.average},
                                {label:"Стоп",tint:Theme.negative,level:root.controller.chartLevels.stop},
                                {label:"Тейк",tint:Theme.positive,level:root.controller.chartLevels.take}]
                            Row {
                                id: levelLegend
                                required property var modelData
                                visible: modelData.level > 0
                                spacing: 4
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter; spacing: 2
                                    Repeater { model: 3; Rectangle { width: 4; height: 2; color: levelLegend.modelData.tint } }
                                }
                                Text { text: parent.modelData.label; color: Theme.muted; font.pixelSize: 11 }
                            }
                        }
                    }
                    FinancialChart {
                        id: chart
                        objectName: "overviewChart"
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Layout.minimumHeight: 260
                        candles: root.controller.chartCandles
                        sessions: root.controller.chartSessions
                        comparison: root.controller.chartComparison
                        dark: Theme.dark
                        palette: ({canvas: Theme.surface, text: Theme.text, muted: Theme.muted,
                            border: Theme.border, accent: Theme.accent, positive: Theme.positive,
                            negative: Theme.negative, warning: Theme.warning, purple: Theme.chartColors[2]})
                        averagePrice: root.controller.chartLevels.average || 0
                        stopLoss: root.controller.chartLevels.stop || 0
                        takeProfit: root.controller.chartLevels.take || 0
                        showVolume: volumeToggle.checked
                        showRsi: rsiToggle.checked
                        Accessible.name: "График " + root.controller.selectedTicker
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Left) chart.panBars(-5)
                            else if (event.key === Qt.Key_Right) chart.panBars(5)
                            else if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) chart.zoomAt(1.25, chart.width / 2)
                            else if (event.key === Qt.Key_Minus) chart.zoomAt(0.8, chart.width / 2)
                            else if (event.key === Qt.Key_Home) chart.resetView()
                            else return
                            event.accepted = true
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: (root.analysis.historyStatus || "История ещё не загружена") + (root.analysis.historyTime ? " · " + root.analysis.historyTime : "")
                        color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Колесо / + −: масштаб · Перетаскивание / ← →: сдвиг · Home: сброс"
                        color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap
                    }
                }
            }
            Surface {
                Layout.fillWidth: true
                Layout.preferredWidth: root.wide ? 300 : -1
                Layout.maximumWidth: root.wide ? 340 : 16777215
                implicitHeight: analytics.implicitHeight + 32
                Layout.alignment: Qt.AlignTop
                ColumnLayout {
                    id: analytics
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
                    spacing: 14
                    Text { text: "Аналитика бумаги"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Text { Layout.fillWidth: true; text: root.analysis.quoteStatus || "Котировка не загружена"; color: Theme.warning; wrapMode: Text.WordWrap; font.pixelSize: 12 }
                    Text {
                        visible: !!root.analysis.dataError
                        Layout.fillWidth: true; text: root.analysis.dataError || ""
                        color: Theme.warning; wrapMode: Text.WordWrap; font.pixelSize: 12
                    }
                    Text {
                        visible: root.controller.chartComparison
                        Layout.fillWidth: true
                        text: (root.analysis.indexStatus || "IMOEX: данные ещё не загружены") + (root.analysis.indexError ? "\n" + root.analysis.indexError : "")
                        color: root.analysis.indexError ? Theme.warning : Theme.muted
                        wrapMode: Text.WordWrap; font.pixelSize: 12
                    }
                    GridLayout {
                        columns: 2; Layout.fillWidth: true; columnSpacing: 12; rowSpacing: 12
                        Repeater {
                            model: [
                                {name:"Изменение за сессию",amount:root.indicator(root.analysis.change," %")},
                                {name:"RSI(14)",amount:root.value(root.analysis.rsi)},
                                {name:"SMA20 / 50",amount:root.value(root.analysis.sma20)+" / "+root.value(root.analysis.sma50)},
                                {name:"SMA200",amount:root.value(root.analysis.sma200)},
                                {name:"ATR(14)",amount:root.value(root.analysis.atr)+" ₽ · "+root.indicator(root.analysis.atrPercent," %")},
                                {name:"Объём / средний",amount:root.indicator(root.analysis.volumeRatio,"×")},
                                {name:"От максимума",amount:root.indicator(root.analysis.drawdown," %")}
                            ]
                            ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true; spacing: 4
                                Text { Layout.fillWidth: true; text: modelData.name; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap }
                                Text { Layout.fillWidth: true; text: modelData.amount; color: Theme.text; font.pixelSize: 14; wrapMode: Text.WrapAnywhere }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                    Text { text: "Моя целевая цена, ₽"; color: Theme.text; font.pixelSize: 13 }
                    TextField {
                        id: targetInput
                        objectName: "overviewTargetInput"
                        Layout.fillWidth: true
                        text: root.analysis.target > 0 ? String(root.analysis.target) : ""
                        placeholderText: "Пусто — убрать цель"
                        selectByMouse: true
                        Accessible.name: "Целевая цена выбранной бумаги в рублях"
                        enabled: root.controller.writable && root.controller.selectedTicker.length > 0
                        onAccepted: { if (enabled) root.controller.setTarget(root.controller.selectedTicker,text) }
                    }
                    ActionButton { text: "Сохранить цель"; Layout.fillWidth: true; enabled: root.controller.writable && root.controller.selectedTicker.length > 0; onClicked: root.controller.setTarget(root.controller.selectedTicker,targetInput.text) }
                    Text { Layout.fillWidth: true; text: (root.analysis.demo ? "Источник: синтетические демонстрационные данные" : "Источник: MOEX ISS") + (root.analysis.sourceTime ? "\nКотировка: " + root.analysis.sourceTime : "") + "\nИндикаторы рассчитаны по дневным свечам. Недостаточная история обозначается «—»."; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap }
                }
            }
        }
        Surface {
            Layout.fillWidth: true
            implicitHeight: 328
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Список наблюдения"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                    ActionButton { text: "Удалить"; destructive: true; enabled: root.controller.writable && root.controller.selectedTicker.length > 0; onClicked: root.controller.removeTicker(root.controller.selectedTicker); Accessible.name: "Удалить выбранную бумагу из наблюдений" }
                }
                Text { text: "Выберите строку для просмотра графика и показателей."; color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Flickable {
                    id: watchTable
                    Layout.fillWidth: true; Layout.fillHeight: true
                    contentWidth: Math.max(width, 680); contentHeight: height; clip: true
                    ScrollBar.horizontal: ScrollBar {}
                    ColumnLayout {
                        width: watchTable.contentWidth; height: watchTable.height; spacing: 0
                        Row {
                            height: 32
                            Repeater {
                                model: [{name:"Бумага",w:110},{name:"Цена, ₽",w:116},{name:"Изменение",w:108},{name:"RSI(14)",w:88},{name:"Состояние данных",w:240}]
                                Text { required property var modelData; text: modelData.name; width: modelData.w; height: 32; verticalAlignment: Text.AlignVCenter; color: Theme.muted; font.pixelSize: 12 }
                            }
                        }
                        ListView {
                            id: watchList
                            objectName: "overviewWatchlist"
                            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                            model: root.controller.watchlist
                            ScrollBar.vertical: ScrollBar {}
                            delegate: ItemDelegate {
                                id: row
                                required property var record
                                width: watchList.width; height: 48
                                padding: 0
                                Accessible.name: record.ticker + ", цена " + root.value(record.price)
                                onClicked: root.controller.selectedTicker=record.ticker
                                background: Rectangle { color: root.controller.selectedTicker === row.record.ticker ? Theme.selection : row.hovered ? Theme.raised : Theme.surface; border.width: row.activeFocus ? 2 : 0; border.color: Theme.accent }
                                contentItem: Row {
                                    Text { width: 110; height: 40; verticalAlignment: Text.AlignVCenter; text: row.record.ticker; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                                    Text { width: 116; height: 40; verticalAlignment: Text.AlignVCenter; text: root.value(row.record.price); color: Theme.text; font.pixelSize: 13 }
                                    Text { width: 108; height: 40; verticalAlignment: Text.AlignVCenter; text: root.indicator(row.record.change," %"); color: typeof row.record.change === "number" && row.record.change < 0 ? Theme.negative : Theme.text; font.pixelSize: 13 }
                                    Text { width: 88; height: 40; verticalAlignment: Text.AlignVCenter; text: root.value(row.record.rsi); color: Theme.text; font.pixelSize: 13 }
                                    Text { width: 240; height: 40; verticalAlignment: Text.AlignVCenter; text: row.record.status || (row.record.stale ? "Устарела / нет котировки" : "Котировка загружена"); color: row.record.stale ? Theme.warning : Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
                                }
                            }
                            Text { anchors.centerIn: parent; visible: watchList.count === 0; text: "Добавьте тикер в наблюдения"; color: Theme.muted; font.pixelSize: 14 }
                        }
                    }
                }
            }
        }
    }
}
