pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: root
    required property var controller
    property var market
    clip:true; contentWidth:availableWidth
    ColumnLayout {
        width:root.availableWidth; spacing:16
        Surface {
            Layout.fillWidth:true; implicitHeight:rules.implicitHeight+32
            ColumnLayout {
                id: rules; anchors.left:parent.left; anchors.right:parent.right; anchors.top:parent.top; anchors.margins:16; spacing:8
                Label { text:"Правила технических сигналов"; color:Theme.text; font.pixelSize:20; font.bold:true; Layout.fillWidth:true; wrapMode:Text.WordWrap }
                Flow {
                    Layout.fillWidth:true; spacing:8
                    CheckBox { id:oversold; text:"RSI ниже"; checked:root.controller.settings.oversold }
                    TextField { id:below; width:72; text:String(root.controller.settings.oversoldBelow); Accessible.name:"Порог перепроданности RSI" }
                    CheckBox { id:overbought; text:"RSI выше"; checked:root.controller.settings.overbought }
                    TextField { id:above; width:72; text:String(root.controller.settings.overboughtAbove); Accessible.name:"Порог перекупленности RSI" }
                    CheckBox { id:sma; text:"Цена выше SMA200"; checked:root.controller.settings.aboveSma }
                    CheckBox { id:target; text:"Рядом с целью"; checked:root.controller.settings.nearTarget }
                    TextField { id:targetPercent; width:72; text:String(root.controller.settings.targetPercent); Accessible.name:"Допустимое расстояние до цели в процентах" }
                }
                Flow {
                    Layout.fillWidth:true; spacing:8
                    ComboBox { id:mode; model:["Любое условие","Все условия"]; currentIndex:root.controller.settings.all?1:0; Accessible.name:"Режим совпадения сигналов" }
                    CheckBox { id:notify; text:"Уведомления"; checked:root.controller.settings.notifications }
                    CheckBox { id:autoRefresh; text:"Автообновление"; checked:root.controller.settings.autoRefresh }
                    ComboBox { id:interval; model:["15 минут","30 минут","60 минут"]; currentIndex:[15,30,60].indexOf(root.controller.settings.minutes); Accessible.name:"Интервал обновления" }
                    ActionButton { text:"Сохранить правила"; primary:true; enabled:root.controller.writable; onClicked:root.controller.saveSettings({oversold:oversold.checked,oversoldBelow:Number(below.text.replace(",",".")),overbought:overbought.checked,overboughtAbove:Number(above.text.replace(",",".")),aboveSma:sma.checked,nearTarget:target.checked,targetPercent:Number(targetPercent.text.replace(",",".")),all:mode.currentIndex===1,notifications:notify.checked,autoRefresh:autoRefresh.checked,minutes:[15,30,60][interval.currentIndex]}) }
                }
                Label { text:root.controller.summary; color:Theme.muted; Layout.fillWidth:true; wrapMode:Text.WordWrap }
                Label { text:"Уведомление возникает при переходе от несовпадения к совпадению. Первый снимок и устаревшие котировки уведомлений не создают."; color:Theme.muted; Layout.fillWidth:true; wrapMode:Text.WordWrap; font.pixelSize:12 }
                Label { visible:root.controller.error.length>0; text:root.controller.error; color:Theme.warning; Layout.fillWidth:true; wrapMode:Text.WordWrap }
            }
        }
        Surface {
            Layout.fillWidth:true; implicitHeight:260
            ColumnLayout {
                anchors.fill:parent; anchors.margins:16
                Label { text:"Текущие совпадения"; color:Theme.text; font.pixelSize:18 }
                ListView {
                    id:matches; objectName:"signalMatches"; Layout.fillWidth:true; Layout.fillHeight:true; model:root.controller.matches; clip:true; ScrollBar.vertical:ScrollBar {}
                    delegate:ItemDelegate {
                        id:match; required property var record; width:matches.width; height:Math.max(60,reason.implicitHeight+32)
                        onClicked:root.controller.openTicker(record.ticker); Accessible.name:"График сигнала "+record.ticker
                        background:Rectangle { color:match.hovered||match.activeFocus?Theme.selection:Theme.surface }
                        contentItem:Label { id:reason; text:match.record.ticker+" · "+match.record.reasons; color:Theme.text; wrapMode:Text.WordWrap }
                    }
                    Label { anchors.centerIn:parent; visible:matches.count===0; text:"Свежих совпадений нет"; color:Theme.muted }
                }
            }
        }
        Surface {
            Layout.fillWidth:true; implicitHeight:alertContent.implicitHeight+32
            ColumnLayout {
                id:alertContent; anchors.left:parent.left; anchors.right:parent.right; anchors.top:parent.top; anchors.margins:16; spacing:12
                Label { text:"Ценовые уведомления"; color:Theme.text; font.pixelSize:18 }
                Flow {
                    Layout.fillWidth:true; spacing:8
                    ComboBox { id:alertTicker; model:root.market?root.market.tickers:[]; width:130; Accessible.name:"Бумага ценового условия" }
                    ComboBox { id:side; model:["Цена ≤","Цена ≥"]; width:120; Accessible.name:"Направление ценового условия" }
                    TextField { id:level; width:140; placeholderText:"Цена, ₽"; Accessible.name:"Порог цены" }
                    ActionButton { text:"Добавить условие"; enabled:root.controller.writable&&alertTicker.currentText.length>0; onClicked: { if(root.controller.addAlert(alertTicker.currentText,side.currentIndex===0,level.text))level.clear() } }
                }
                Repeater {
                    model:root.controller.alerts
                    Flow {
                        id:rule; required property var record; Layout.fillWidth:true; spacing:8
                        CheckBox { text:rule.record.ticker+(rule.record.below?" ≤ ":" ≥ ")+rule.record.level+" ₽"; checked:rule.record.enabled; enabled:root.controller.writable; onToggled:root.controller.setAlertEnabled(rule.record.id,checked) }
                        ActionButton { text:"Удалить"; destructive:true; enabled:root.controller.writable; Accessible.name:"Удалить условие "+rule.record.id; onClicked:root.controller.deleteAlert(rule.record.id) }
                    }
                }
            }
        }
        Surface {
            Layout.fillWidth:true; implicitHeight:380
            ColumnLayout {
                anchors.fill:parent; anchors.margins:16; spacing:8
                Flow {
                    Layout.fillWidth:true; spacing:12
                    Label { text:"История срабатываний"; color:Theme.text; font.pixelSize:18 }
                    ActionButton { text:"Очистить историю"; destructive:true; enabled:root.controller.writable; onClicked:confirmClear.open() }
                }
                ListView {
                    id:history; objectName:"signalHistory"; Layout.fillWidth:true; Layout.fillHeight:true; clip:true; model:root.controller.history; ScrollBar.vertical:ScrollBar {}
                    delegate:ItemDelegate {
                        id:event; required property var record; width:history.width; height:Math.max(80,eventText.implicitHeight+24)
                        onClicked:root.controller.openTicker(record.ticker); Accessible.name:"Событие "+record.ticker+" "+record.date
                        background:Rectangle { color:event.hovered||event.activeFocus?Theme.selection:Theme.surface }
                        contentItem:Label { id:eventText; text:event.record.date+" · "+event.record.ticker+" · "+event.record.kind+"\n"+event.record.condition+"\nКотировка: "+event.record.quoteTime; color:Theme.text; wrapMode:Text.WordWrap; font.pixelSize:12 }
                    }
                    Label { anchors.centerIn:parent; visible:history.count===0; text:"Новых переходов условий ещё не было"; color:Theme.muted }
                }
            }
        }
    }
    Dialog { id:confirmClear; title:"Очистить историю сигналов?"; modal:true; standardButtons:Dialog.Yes|Dialog.No; anchors.centerIn:Overlay.overlay; onAccepted:root.controller.clearHistory() }
}
