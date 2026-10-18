import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
 id: root
 required property var controller
 property string selectedId: ""
 property var selected: ({})
 function choose(row) { selectedId=row.id; selected=row; reason.text=row.reason; exitReason.text=row.exitReason; state.currentIndex=row.status === "reviewed" ? 1 : 0 }
 contentWidth: availableWidth; clip: true
 ColumnLayout {
  width: root.availableWidth; spacing: 16
  Text { text: "Дневник решений"; color: Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold }
  Text { text: "Идеи и разборы отделены от реальных сделок портфеля."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
  RowLayout {
   Layout.fillWidth: true
   TextField { id: filter; placeholderText: "Фильтр по тикеру"; Layout.fillWidth: true; maximumLength: 32 }
   ComboBox { id: statusFilter; model: ["Все", "Открытые идеи", "Разобранные"] }
  }
  Text { visible: root.controller.entries.length === 0; text: "Записей пока нет. Создайте идею во вкладке «План покупки»."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
  ListView {
   id: entriesList
   Layout.fillWidth: true; Layout.preferredHeight: 360; clip: true; spacing: 8
   ScrollBar.vertical: ScrollBar { }
   model: root.controller.entries.filter(function(row) { return row.ticker.indexOf(filter.text.trim().toUpperCase()) >= 0 && (statusFilter.currentIndex === 0 || row.status === (statusFilter.currentIndex === 1 ? "open" : "reviewed")) })
   delegate: Surface {
    required property var modelData
    width: entriesList.width; height: row.implicitHeight + 24
    RowLayout {
     id: row; anchors.fill: parent; anchors.margins: 12
     ColumnLayout {
      Layout.fillWidth: true
      Text { text: modelData.ticker + " · " + (modelData.status === "open" ? "Открытая идея" : "Разобрано"); color: Theme.text; font.weight: Font.DemiBold }
      Text { text: new Date(modelData.date).toLocaleString(Qt.locale()) + " · Вход: " + modelData.plan.entry + " · Стоп: " + modelData.plan.stop + " · Цель: " + modelData.plan.target; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
      Text { text: modelData.reason; color: Theme.text; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight; Layout.fillWidth: true }
     }
     ActionButton { text: "Открыть"; onClicked: root.choose(modelData) }
    }
   }
  }
  Text { visible: entriesList.count === 0 && root.controller.entries.length > 0; text: "По фильтру записей нет"; color: Theme.muted }
  Surface {
   visible: root.selectedId !== ""; Layout.fillWidth: true; implicitHeight: edit.implicitHeight + 32
   ColumnLayout {
    id: edit; anchors.fill: parent; anchors.margins: 16; spacing: 12
    Text { text: "Разбор: " + (root.selected.ticker || ""); color: Theme.text; font.pixelSize: 18 }
    TextArea { id: reason; Layout.fillWidth: true; Layout.preferredHeight: 110; wrapMode: TextEdit.Wrap; placeholderText: "Обоснование"; padding: 12; color: Theme.text; placeholderTextColor: Theme.muted; Accessible.name: "Обоснование решения"; background: Rectangle { color: Theme.raised; radius: 8; border.width: reason.activeFocus ? 2 : 1; border.color: reason.activeFocus ? Theme.accent : Theme.border } }
    TextArea { id: exitReason; Layout.fillWidth: true; Layout.preferredHeight: 110; wrapMode: TextEdit.Wrap; placeholderText: "Условия выхода / результат разбора"; padding: 12; color: Theme.text; placeholderTextColor: Theme.muted; Accessible.name: "Условия выхода и итоговый разбор"; background: Rectangle { color: Theme.raised; radius: 8; border.width: exitReason.activeFocus ? 2 : 1; border.color: exitReason.activeFocus ? Theme.accent : Theme.border } }
    ComboBox { id: state; model: ["Открытая идея", "Разобрано"] }
    RowLayout {
     ActionButton { text: "Сохранить изменения"; enabled: root.controller.writable; onClicked: if(root.controller.updateDecision(root.selectedId,reason.text,exitReason.text,state.currentIndex === 0 ? "open" : "reviewed")) note.text = "Изменения сохранены" }
     ActionButton { text: "Удалить"; enabled: root.controller.writable; onClicked: confirm.open() }
    }
   }
  }
  Text { id: note; color: Theme.positive }
  Text { text: root.controller.error; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
 }
 Dialog {
  id: confirm; title: "Удалить запись дневника?"; modal: true; anchors.centerIn: parent; standardButtons: Dialog.Yes | Dialog.No
  onAccepted: if(root.controller.deleteDecision(root.selectedId)) { root.selectedId=""; root.selected=({}); note.text="Запись удалена" }
 }
}
