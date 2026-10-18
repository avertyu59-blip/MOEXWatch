import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
ScrollView {
 id: root
 required property var controller
 required property var market
 property var current: controller.card(ticker.currentText)
 function fmt(v) { return v === undefined || v === null ? "—" : Number(v).toLocaleString(Qt.locale(), 'f', 2) }
 Connections { target: root.controller; function onChanged() { root.current=root.controller.card(ticker.currentText) } }
 FileDialog { id: importer; title: "Отчётность JSON: сначала предпросмотр"; nameFilters: ["JSON (*.json)"]; onAccepted: root.controller.previewImport(selectedFile) }
 FileDialog { id: exporter; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; nameFilters: ["JSON (*.json)"]; onAccepted: root.controller.exportFile(selectedFile) }
 ColumnLayout {
  width: Math.max(400,root.availableWidth); spacing: 16
  Label { text: "Карточка компании"; font.pixelSize: 24; color: Theme.text }
  Label { text: "Проверяемая отчётность · суммы в млн ₽, число акций — в штуках. Отсутствующие данные обозначены «—». Цифры введены пользователем; приложение проверяет формат, но не подтверждает достоверность отчёта."; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Theme.muted }
  RowLayout {
   ComboBox { id: ticker; model: root.market.tickers; currentIndex: Math.max(0,root.market.tickers.indexOf(root.market.selectedTicker)); onCurrentTextChanged: root.current=root.controller.card(currentText); onActivated: root.market.selectedTicker=currentText }
   ActionButton { objectName: "companyEditButton"; text: "Ввести / изменить"; enabled: root.controller.writable; onClicked: { edit.draft=Object.assign({ticker:ticker.currentText,company:"",sector:"",standard:"IFRS",period:"",durationMonths:12,currency:"RUB",unit:"million",publicationDate:"",checkedDate:new Date().toISOString().slice(0,10),sourceUrl:""},root.current); validation.text="";edit.open() } }
  }
  RowLayout {
   ActionButton { text: "Импорт JSON"; enabled: root.controller.writable; onClicked: importer.open() }
   ActionButton { text: "Экспорт JSON"; onClicked: exporter.open() }
   ActionButton { text: "Источник"; enabled: !!root.current.sourceUrl; onClicked: root.controller.openSource(ticker.currentText) }
  }
  Label { text: root.controller.error; visible: text.length>0; color: Theme.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
  Surface {
   Layout.fillWidth: true; implicitHeight: details.implicitHeight+32
   ColumnLayout { id: details; anchors.fill: parent; anchors.margins: 16
    Label { text: root.current.company || "Нет введённой отчётности для выбранной бумаги"; color: Theme.text; font.pixelSize: 20; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Label { text: root.current.ticker ? root.current.standard+" · "+root.current.period+" · "+root.current.durationMonths+" мес. · проверено "+root.current.checkedDate+(root.current.stale ? " · требуется обновление" : "") : "Добавьте данные из официального отчёта или импортируйте JSON."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Repeater { model: [{k:"revenue",n:"Выручка, млн ₽"},{k:"netProfit",n:"Чистая прибыль, млн ₽"},{k:"fcf",n:"Свободный денежный поток, млн ₽"},{k:"netDebt",n:"Чистый долг, млн ₽"},{k:"netDebtEbitda",n:"Чистый долг / EBITDA"},{k:"netMarginPercent",n:"Чистая рентабельность, %"},{k:"dividendYieldPercent",n:"Дивдоходность по введённой выплате, %"},{k:"pe",n:"P/E"},{k:"pb",n:"P/B"},{k:"evEbitda",n:"EV / EBITDA"}]
     RowLayout { required property var modelData; Layout.fillWidth: true; Label { text: modelData.n; color: Theme.muted; Layout.fillWidth: true } Label { text: root.fmt(root.current[modelData.k]); color: Theme.text } }
    }
    Label { text: root.current.metricNote || ""; color: Theme.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    Label { text: root.current.valuationNote || ""; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
   }
  }
  Surface { Layout.fillWidth: true; visible: root.controller.preview.length>0; implicitHeight: previewRows.implicitHeight+32
   ColumnLayout { id: previewRows; anchors.fill: parent; anchors.margins: 16
    Label { text: "Предпросмотр: совпадающие тикеры будут заменены"; color: Theme.warning }
    Repeater { model: root.controller.preview; Label { required property var modelData; text: modelData.ticker+" · "+modelData.company+" · "+modelData.period+" · "+modelData.standard+"\nВыручка: "+root.fmt(modelData.revenue)+" млн ₽; прибыль: "+root.fmt(modelData.netProfit)+" млн ₽\n"+modelData.sourceUrl+" · проверено "+modelData.checkedDate; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
    ActionButton { text: "Применить проверенный импорт"; enabled: root.controller.writable; onClicked: root.controller.applyImport() }
   }
  }
 }
 Dialog {
  id: edit; objectName: "companyEditDialog"; property var draft: ({})
  title: "Отчётность из источника"; modal: true; width: Math.min(720,root.width-24); height: Math.min(720,root.height-24); anchors.centerIn: Overlay.overlay; standardButtons: Dialog.Cancel
  ColumnLayout { anchors.fill: parent
   Label { text: "Пользовательские данные: приложение проверяет формат и формулы, но не подтверждает источник. Суммы — млн ₽; пустое число = нет данных."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
   ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true
    ColumnLayout { width: parent.width; spacing: 8
     Repeater { model: [{k:"ticker",n:"Тикер"},{k:"company",n:"Название компании"},{k:"sector",n:"Сектор (для банков: Банки)"},{k:"period",n:"Период: 2025, 2026-H1, 2026-9M"},{k:"publicationDate",n:"Дата публикации YYYY-MM-DD"},{k:"checkedDate",n:"Дата проверки YYYY-MM-DD"},{k:"sourceUrl",n:"Ссылка на отчёт HTTPS"}]
      ColumnLayout { required property var modelData; Layout.fillWidth: true
       Label { text: modelData.n; color: Theme.muted }
       TextField { Layout.fillWidth: true; text: edit.draft[parent.modelData.k] || ""; onTextEdited: edit.draft[parent.modelData.k]=text }
      }
     }
     RowLayout {
      Label { text: "Стандарт" }
      ComboBox { model: ["IFRS","RAS"]; currentIndex: edit.draft.standard==="RAS"?1:0; onActivated: edit.draft.standard=currentText }
      Label { text: "Месяцев" }
      ComboBox { model: [3,6,9,12]; currentIndex: Math.max(0,[3,6,9,12].indexOf(edit.draft.durationMonths)); onActivated: edit.draft.durationMonths=Number(currentText) }
     }
     Repeater { model: [{k:"revenue",n:"Выручка, млн ₽"},{k:"netProfit",n:"Чистая прибыль, млн ₽"},{k:"ebitda",n:"EBITDA, млн ₽"},{k:"cfo",n:"Операционный денежный поток, млн ₽"},{k:"capex",n:"Капзатраты, млн ₽ (положительное число)"},{k:"debt",n:"Долг, млн ₽"},{k:"cash",n:"Денежные средства, млн ₽"},{k:"equity",n:"Собственный капитал, млн ₽"},{k:"shares",n:"Количество акций, штук"},{k:"dividendPerShare",n:"Выплата, ₽/акцию (для доходности)"}]
      ColumnLayout { required property var modelData; Layout.fillWidth: true
       Label { text: modelData.n; color: Theme.muted }
       TextField { Layout.fillWidth: true; placeholderText: "Нет данных"; text: edit.draft[parent.modelData.k]===undefined || edit.draft[parent.modelData.k]===null ? "" : String(edit.draft[parent.modelData.k]); onTextEdited: { let n=Number(text.replace(",",".")); edit.draft[parent.modelData.k]=text.trim()===""?null:(isFinite(n)?n:text) } }
      }
     }
    }
   }
   Label { id: validation; color: Theme.negative; Layout.fillWidth: true; wrapMode: Text.WordWrap }
   ActionButton { text: "Проверить и сохранить"; onClicked: { if(root.controller.saveRecord(edit.draft))edit.close();else validation.text=root.controller.error } }
  }
 }
}
