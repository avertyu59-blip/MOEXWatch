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
        Label { text: "Формат операций и постоянные ID сохранены. Qt-версия использует тот же профиль MOEXWatch. Профиль защищён от одновременного изменения разными копиями."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: root.controller.backupBusy ? "Создаю копию…" : "Резервная копия"; enabled: !root.controller.backupBusy; onClicked: root.controller.backup() }
            ActionButton { text: "Каталог данных"; onClicked: root.controller.openDataFolder() }
        }
        Label { text: "MOEXWatch · Qt Quick\nCSV-операции импортируются после предпросмотра. Свободные деньги вводятся вручную; поступления дивидендов подтверждаются отдельно. Тема сохраняется автоматически."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Item { Layout.fillHeight: true }
    }
}
