import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
 id: root
 required property var controller
 required property var market
 property var choices: []
 function toggle(t) { let a=choices.slice(); let i=a.indexOf(t); if(i>=0)a.splice(i,1);else if(a.length<4)a.push(t); choices=a;controller.setComparison(a) }
 function fmt(v) { return v===undefined || v===null ? "—" : Number(v).toLocaleString(Qt.locale(),'f',2) }
 ColumnLayout { width: Math.max(420,root.availableWidth); spacing: 16
  Label { text: "Сравнение компаний"; color: Theme.text; font.pixelSize: 24 }
  Label { text: "Выберите 2–4 компании с загруженной отчётностью. Сравнивайте прежде всего один сектор; различия периодов и стандартов отмечены отдельно."; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Theme.muted }
  Flow { Layout.fillWidth: true; spacing: 8
   Repeater { model: root.controller.records; CheckBox { required property var modelData; text: modelData.ticker; checked: root.choices.indexOf(text)>=0; enabled: checked || root.choices.length<4; onClicked: root.toggle(text) } }
  }
  Label { visible: root.controller.records.length<2; text: "Добавьте отчётность минимум двух компаний во вкладке «Компания»."; color: Theme.warning }
  Repeater { model: root.controller.comparison
   Surface { required property var modelData; id: entry; Layout.fillWidth: true; implicitHeight: body.implicitHeight+32
    ColumnLayout { id: body; anchors.fill: parent; anchors.margins: 16
     Label { text: entry.modelData.ticker+" · "+entry.modelData.company+" · "+entry.modelData.sector; color: Theme.text; font.pixelSize: 20; wrapMode: Text.WordWrap; Layout.fillWidth: true }
     Label { text: entry.modelData.period+" / "+entry.modelData.standard+" · "+entry.modelData.comparisonNote; color: entry.modelData.comparable?Theme.muted:Theme.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true }
     Label { text: "Выручка: "+root.fmt(entry.modelData.revenue)+" млн ₽ · Прибыль: "+root.fmt(entry.modelData.netProfit)+" млн ₽\nFCF: "+root.fmt(entry.modelData.fcf)+" млн ₽ · P/E: "+root.fmt(entry.modelData.pe)+" · P/B: "+root.fmt(entry.modelData.pb)+"\nЧистый долг / EBITDA: "+root.fmt(entry.modelData.netDebtEbitda)+" · EV/EBITDA: "+root.fmt(entry.modelData.evEbitda); color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
     ActionButton { text: "Открыть источник"; onClicked: root.controller.openSource(entry.modelData.ticker) }
    }
   }
  }
 }
}
