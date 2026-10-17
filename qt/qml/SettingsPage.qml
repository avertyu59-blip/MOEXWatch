import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Surface {
    id: root
    required property var controller
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 20
        Text { text: "Рабочее пространство"; color: Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold }
        Switch { text: "Тёмная тема"; checked: root.controller.dark; onToggled: root.controller.dark=checked }
        Label { text: "Формат операций и постоянные ID сохранены. Qt-версия использует тот же профиль MOEXWatch. Закрывайте классическую версию перед изменением портфеля здесь."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: root.controller.backupBusy ? "Создаю копию…" : "Резервная копия"; enabled: !root.controller.backupBusy; onClicked: root.controller.backup() }
            ActionButton { text: "Каталог данных"; onClicked: root.controller.openDataFolder() }
        }
        Label { text: "Qt Quick preview · этап 1\nПортфель и журнал перенесены. Обзор, графики, сигналы, бэктест и дивиденды пока доступны в классической сборке."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
