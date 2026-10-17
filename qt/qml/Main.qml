import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic
ApplicationWindow {
    id: window
    objectName: "mainWindow"
    visible: true
    width: Math.min(1440, Screen.desktopAvailableWidth)
    height: Math.min(920, Screen.desktopAvailableHeight)
    minimumWidth: 640; minimumHeight: 480
    title: "MOEXWatch · Qt Quick preview"
    font.family: Theme.family
    font.pixelSize: 14
    color: Theme.canvas
    property int pageIndex: 0
    readonly property bool compact: width < 1000
    palette.window: Theme.canvas
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.text: Theme.text
    palette.button: Theme.raised
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.textOnAccent
    Component.onCompleted: Theme.dark=backend.dark
    Connections { target: backend; function onChanged() { Theme.dark=backend.dark } }
    onClosing: backend.shutdown()
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillHeight: true; Layout.preferredWidth: window.compact ? 72 : 208
            color: Theme.surface
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 8
                Text { text: window.compact ? "MW" : "▥  MOEX Watch"; color: Theme.text; font.pixelSize: 21; font.weight: Font.DemiBold; Layout.topMargin: 12; Layout.bottomMargin: 28 }
                Repeater {
                    model: [{name:"Портфель",icon:"◫"},{name:"Операции",icon:"≡"},{name:"Настройки",icon:"⚙"}]
                    Button {
                        id: nav
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true; implicitHeight: 48
                        Accessible.name: modelData.name
                        onClicked: window.pageIndex=index
                        ToolTip.visible: hovered && window.compact
                        ToolTip.text: modelData.name
                        background: Rectangle { radius: 8; color: window.pageIndex===nav.index ? Theme.selection : nav.hovered ? Theme.raised : "transparent"; border.color: nav.activeFocus ? Theme.accent : "transparent"; border.width: 2 }
                        contentItem: Text { text: nav.modelData.icon + (window.compact ? "" : "   " + nav.modelData.name); color: Theme.text; font.pixelSize: window.compact ? 24 : 15; verticalAlignment: Text.AlignVCenter; horizontalAlignment: window.compact ? Text.AlignHCenter : Text.AlignLeft }
                    }
                }
                Item { Layout.fillHeight: true }
                ActionButton { text: window.compact ? "◐" : "Сменить тему"; Accessible.name: "Сменить тему"; Layout.fillWidth: true; implicitWidth: 40; onClicked: backend.dark=!backend.dark }
                Text { visible: !window.compact; text: "Qt Quick preview\nПортфель · этап 1"; color: Theme.muted; font.pixelSize: 12; lineHeight: 1.5; Layout.topMargin: 8 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: window.compact ? 12 : 24; spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Text { text: ["Портфель","Журнал операций","Настройки"][window.pageIndex]; color: Theme.text; font.pixelSize: window.compact ? 22 : 28; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                BusyIndicator { running: backend.busy; visible: running; implicitWidth: 32; implicitHeight: 32 }
                ActionButton { text: backend.busy ? "Отменить" : "Обновить"; primary: !backend.busy; onClicked: backend.busy ? backend.cancel() : backend.refresh() }
            }
            Label { text: backend.status; color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Rectangle {
                visible: backend.error.length>0
                Layout.fillWidth: true; implicitHeight: errorText.implicitHeight+24
                color: Theme.raised; radius: 8; border.color: Theme.warning
                Label { id: errorText; anchors.fill: parent; anchors.margins: 12; text: backend.error; color: Theme.warning; wrapMode: Text.WordWrap }
            }
            StackLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: window.pageIndex
                PortfolioPage { controller: backend; onAddRequested: operation.open(); onLevelsRequested: function(id) { levels.positionId=id; levels.open() } }
                JournalPage { controller: backend; onAddRequested: operation.open() }
                SettingsPage { controller: backend }
            }
        }
    }
    OperationDialog { id: operation; controller: backend }
    LevelsDialog { id: levels; controller: backend }
}
