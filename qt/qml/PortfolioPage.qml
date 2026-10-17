import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: root
    required property var controller
    signal addRequested()
    signal levelsRequested(string id)
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: root.availableWidth; spacing: 20
        GridLayout {
            Layout.fillWidth: true; columns: root.width < 760 ? 1 : 3; columnSpacing: 16; rowSpacing: 12
            MetricCard { Layout.fillWidth: true; title: "Стоимость позиций"; amount: root.controller.totals.value; note: root.controller.totals.valuationNote }
            MetricCard { Layout.fillWidth: true; title: "Бумажная прибыль"; amount: root.controller.totals.unrealized; amountColor: root.controller.totals.unrealizedValue < 0 ? Theme.negative : Theme.positive; note: "Открытые позиции · до налога" }
            MetricCard { Layout.fillWidth: true; title: "Реализованная прибыль"; amount: root.controller.totals.realized; amountColor: root.controller.totals.realizedValue < 0 ? Theme.negative : Theme.text; note: "С учётом комиссий · до налога" }
        }
        GridLayout {
            Layout.fillWidth: true; columns: root.width < 1100 ? 1 : 2; columnSpacing: 16; rowSpacing: 16
            Surface {
                Layout.fillWidth: true; Layout.preferredHeight: 480; Layout.minimumWidth: 300
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Открытые позиции"; color: Theme.text; font.family: Theme.family; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
                        ComboBox {
                            objectName: "positionSort"
                            model: ["По тикеру", "По доле", "По прибыли"]
                            implicitWidth: 148
                            onActivated: root.controller.sortPositions(["ticker","weight","profit"][currentIndex])
                            Accessible.name: "Сортировка открытых позиций"
                        }
                    }
                    Text { visible: root.controller.totals.openCount === 0; text: "Портфель пока пуст. Запишите первую покупку."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Flickable {
                        id: table
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentWidth: Math.max(width, 832); contentHeight: height; clip: true
                        ScrollBar.horizontal: ScrollBar { }
                        ColumnLayout {
                            width: table.contentWidth; height: table.height; spacing: 0
                            Row {
                                height: 36; spacing: 0
                                Repeater {
                                    model: [{title:"Тикер",w:104},{title:"Акций",w:88},{title:"Средняя, ₽",w:112},{title:"Цена, ₽",w:112},{title:"Доля",w:80},{title:"Прибыль, ₽ / %",w:148},{title:"Уровни",w:88}]
                                    Text { required property var modelData; text: modelData.title; width: modelData.w; height: 36; verticalAlignment: Text.AlignVCenter; color: Theme.muted; font.pixelSize: 12 }
                                }
                            }
                            ListView {
                                id: list
                                objectName: "positionsView"
                                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                                model: root.controller.positions
                                ScrollBar.vertical: ScrollBar { }
                                delegate: Rectangle {
                                    required property var record
                                    width: list.width; height: 76
                                    color: mouse.containsMouse ? Theme.raised : Theme.surface
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                                    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onDoubleClicked: root.levelsRequested(record.id) }
                                    Row {
                                        anchors.verticalCenter: parent.verticalCenter
                                        Text { width: 104; height: 48; verticalAlignment: Text.AlignVCenter; text: record.ticker; color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
                                        Text { width: 88; height: 48; verticalAlignment: Text.AlignVCenter; text: record.quantity; color: Theme.text; font.pixelSize: 13 }
                                        Text { width: 112; height: 48; verticalAlignment: Text.AlignVCenter; text: record.average; color: Theme.text; font.pixelSize: 13 }
                                        Column {
                                            width: 112; anchors.verticalCenter: parent.verticalCenter; spacing: 4
                                            Text { text: record.price; color: Theme.text; font.pixelSize: 13 }
                                            Text { text: record.stale ? "Устарела / нет" : "Актуальная"; color: record.stale ? Theme.warning : Theme.muted; font.pixelSize: 10 }
                                        }
                                        Text { width: 80; height: 48; verticalAlignment: Text.AlignVCenter; text: record.weight; color: Theme.muted; font.pixelSize: 13 }
                                        Column {
                                            width: 148; anchors.verticalCenter: parent.verticalCenter; spacing: 4
                                            Text { text: record.profit; color: record.profitValue < 0 ? Theme.negative : Theme.positive; font.pixelSize: 14 }
                                            Text { text: record.percent; color: record.profitValue < 0 ? Theme.negative : Theme.positive; font.pixelSize: 12 }
                                        }
                                        ActionButton { text: "SL / TP"; implicitWidth: 80; onClicked: root.levelsRequested(record.id); Accessible.name: "Уровни позиции " + record.ticker }
                                    }
                                }
                            }
                        }
                    }
                    ActionButton { objectName: "addOperationButton"; text: "+ Записать операцию"; primary: true; enabled: root.controller.writable; onClicked: root.addRequested() }
                }
            }
            Allocation { controller: root.controller; Layout.preferredWidth: 300; Layout.fillWidth: root.width < 1100; Layout.preferredHeight: 480 }
        }
        Text { text: "Количество — в акциях. Уровни SL/TP привязаны к ID позиции и служат для наблюдения; заявки на биржу не отправляются."; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
}
