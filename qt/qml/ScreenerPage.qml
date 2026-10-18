pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: root
    required property var controller
    property var market
    clip: true; contentWidth: availableWidth
    function number(n) { return n === undefined || n === null ? "—" : Number(n).toLocaleString(Qt.locale("ru_RU"),"f",2) }
    ColumnLayout {
        width: root.availableWidth; spacing: 16
        Surface {
            Layout.fillWidth: true; implicitHeight: controls.implicitHeight+32
            ColumnLayout {
                id: controls; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 12
                Label { text: "Скринер наблюдаемых бумаг"; color: Theme.text; font.pixelSize: 20; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: "Фильтры применяются вместе. Для отбора используются только свежие котировки и доступная история."; color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    ComboBox { id: kind; width: 210; model: ["Все бумаги","RSI ниже 30","RSI выше 70","Цена выше SMA200","До цели не более 5% "]; Accessible.name: "Условие скринера" }
                    TextField { id: volume; width: 150; placeholderText: "Объём от, ×"; Accessible.name: "Минимальный объём относительно среднего"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    TextField { id: atr; width: 150; placeholderText: "ATR от, %"; Accessible.name: "Минимальный ATR в процентах"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    ActionButton { text: "Применить"; primary: true; onClicked: root.controller.setFilters({kind:kind.currentIndex,volumeMin:volume.text.trim()===""?0:Number(volume.text.replace(",",".")),atrMin:atr.text.trim()===""?0:Number(atr.text.replace(",","."))}) }
                }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    ComboBox { id: sorting; model: ["Тикер","Цена","Изменение","RSI","ATR, %","Объём, ×"]; Accessible.name: "Сортировка скринера"; onActivated: root.controller.sort(["ticker","price","change","rsi","atr","volume"][currentIndex],!descending.checked) }
                    CheckBox { id: descending; text: "По убыванию"; onToggled: root.controller.sort(["ticker","price","change","rsi","atr","volume"][sorting.currentIndex],!checked) }
                }
                Label { text: root.controller.summary; color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { visible: root.controller.error.length>0; text: root.controller.error; color: Theme.warning; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
        }
        Surface {
            Layout.fillWidth: true; implicitHeight: 520
            Flickable {
                id: table; anchors.fill: parent; anchors.margins: 16; clip: true
                contentWidth: Math.max(width,950); contentHeight: height
                ScrollBar.horizontal: ScrollBar {}
                ColumnLayout {
                    width: table.contentWidth; height: table.height; spacing: 4
                    Row {
                        Repeater { model: [{t:"Бумага",w:100},{t:"Цена, ₽",w:100},{t:"Изменение, %",w:115},{t:"RSI14",w:85},{t:"ATR, %",w:85},{t:"Объём, ×",w:95},{t:"Почему в списке",w:350}]; Label { required property var modelData; width:modelData.w; text:modelData.t; color:Theme.muted; font.pixelSize:12 } }
                    }
                    ListView {
                        id: list; objectName: "screenerList"; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: root.controller.screener
                        ScrollBar.vertical: ScrollBar {}
                        delegate: ItemDelegate {
                            id: item; required property var record; width:list.width; height:68; padding:0
                            Accessible.name: "Открыть график " + record.ticker
                            onClicked: root.controller.openTicker(record.ticker)
                            background: Rectangle { color: item.hovered||item.activeFocus?Theme.selection:Theme.surface; border.color:item.activeFocus?Theme.accent:"transparent" }
                            contentItem: Row {
                                Label { width:100; text:item.record.ticker; color:Theme.text; font.bold:true }
                                Label { width:100; text:root.number(item.record.price); color:Theme.text }
                                Label { width:115; text:root.number(item.record.change); color:Theme.text }
                                Label { width:85; text:root.number(item.record.rsi); color:Theme.text }
                                Label { width:85; text:root.number(item.record.atr); color:Theme.text }
                                Label { width:95; text:root.number(item.record.volume); color:Theme.text }
                                Label { width:350; text:(item.record.stale?"Устаревшие данные · ":"")+item.record.reasons; color:item.record.stale?Theme.warning:Theme.muted; wrapMode:Text.WordWrap; font.pixelSize:12 }
                            }
                        }
                        Label { anchors.centerIn:parent; visible:list.count===0; text:"Нет бумаг, отвечающих условиям"; color:Theme.muted }
                    }
                }
            }
        }
    }
}
