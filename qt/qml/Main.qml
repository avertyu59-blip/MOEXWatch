import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic
ApplicationWindow {
    id: window
    objectName: "mainWindow"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
    visible: true
    width: Math.min(1440, Screen.desktopAvailableWidth)
    height: Math.min(920, Screen.desktopAvailableHeight)
    minimumWidth: 640; minimumHeight: 480
    title: "MOEXWatch 0.8.1 · Qt"
    font.family: Theme.family
    font.pixelSize: 14
    color: Theme.canvas
    property int pageIndex: 0
    property string notice: ""
    readonly property bool compact: width < 1000
    readonly property var pages: [
        {key:"overview",name:"Обзор",icon:"⌁"}, {key:"screener",name:"Скринер",icon:"⌕"},
        {key:"signals",name:"Сигналы",icon:"◇"}, {key:"plan",name:"План покупки",icon:"＋"},
        {key:"company",name:"Компания",icon:"▤"}, {key:"compare",name:"Сравнение",icon:"⇄"},
        {key:"events",name:"События",icon:"▦"}, {key:"strategy",name:"Стратегия",icon:"∿"},
        {key:"portfolio",name:"Портфель",icon:"◫"}, {key:"journal",name:"Операции",icon:"≡"},
        {key:"diary",name:"Дневник",icon:"✎"}, {key:"settings",name:"Настройки",icon:"⚙"}]
    palette.window: Theme.canvas
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.text: Theme.text
    palette.placeholderText: Theme.muted
    palette.button: Theme.raised
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.textOnAccent
    Component.onCompleted: Theme.dark=backend.dark
    Connections { target: backend; function onSettingsChanged() { Theme.dark=backend.dark } }
    Connections { target: signalBackend; function onOpenChart() { window.pageIndex=0 } }
    onClosing: { eventsBackend.cancel(); backend.cancel() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "Ctrl+R"; onActivated: backend.busy ? backend.cancel() : backend.refresh() }
    Shortcut { sequence: "Ctrl+1"; onActivated: window.pageIndex=0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: window.pageIndex=1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: window.pageIndex=2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: window.pageIndex=3 }
    Shortcut { sequence: "Ctrl+5"; onActivated: window.pageIndex=4 }
    Shortcut { sequence: "Ctrl+6"; onActivated: window.pageIndex=5 }
    Shortcut { sequence: "Ctrl+7"; onActivated: window.pageIndex=6 }
    Shortcut { sequence: "Ctrl+8"; onActivated: window.pageIndex=7 }
    Shortcut { sequence: "Ctrl+9"; onActivated: window.pageIndex=8 }
    Shortcut { sequence: "Ctrl+0"; onActivated: window.pageIndex=9 }
    Shortcut { sequence: "Ctrl+I"; enabled: backend.writable; onActivated: portfolioTransfer.open() }
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillHeight: true; Layout.preferredWidth: window.compact ? 72 : 208
            color: Theme.surface
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 8
                Item {
                    Layout.fillWidth: true; implicitHeight: 44; Layout.topMargin: 12; Layout.bottomMargin: 12
                    Text { anchors.fill: parent; verticalAlignment: Text.AlignVCenter; text: window.compact ? "MW" : "▥  MOEX Watch"; color: Theme.text; font.pixelSize: 21; font.weight: Font.DemiBold; }
                    WindowDragArea { objectName: "logoDragArea"; anchors.fill: parent; targetWindow: window }
                }
                ListView {
                    id: navigation
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                    model: window.pages
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Button {
                        id: nav
                        required property int index
                        required property var modelData
                        width: navigation.width; height: 40
                        Accessible.name: modelData.name
                        onClicked: window.pageIndex=index
                        ToolTip.visible: hovered && window.compact
                        ToolTip.text: modelData.name
                        background: Rectangle { radius: 8; color: window.pageIndex===nav.index ? Theme.selection : nav.hovered ? Theme.raised : "transparent"; border.color: nav.activeFocus ? Theme.accent : "transparent"; border.width: 2 }
                        contentItem: Text { text: nav.modelData.icon + (window.compact ? "" : "   " + nav.modelData.name); color: Theme.text; font.pixelSize: window.compact ? 22 : 14; verticalAlignment: Text.AlignVCenter; horizontalAlignment: window.compact ? Text.AlignHCenter : Text.AlignLeft }
                    }
                }
                ActionButton { text: window.compact ? "◐" : Theme.dark ? "Светлая тема" : "Тёмная тема"; Accessible.name: Theme.dark ? "Включить светлую тему" : "Включить тёмную тему"; Layout.fillWidth: true; implicitWidth: 40; onClicked: backend.dark=!backend.dark }
                Text { visible: !window.compact; text: "MOEXWatch 0.8.1\nC++ · Qt Quick"; color: Theme.muted; font.pixelSize: 12; lineHeight: 1.5; Layout.topMargin: 8 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: window.compact ? 12 : 24; spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Item {
                    Layout.fillWidth: true; implicitHeight: 40
                    Text { anchors.fill: parent; text: window.pages[window.pageIndex].name; color: Theme.text; font.pixelSize: window.compact ? 22 : 28; font.weight: Font.DemiBold; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                    WindowDragArea { objectName: "headerDragArea"; anchors.fill: parent; targetWindow: window }
                }
                BusyIndicator { running: backend.busy; visible: running; implicitWidth: 32; implicitHeight: 32 }
                ActionButton { text: backend.busy ? "Отменить" : "Обновить рынок"; primary: !backend.busy; onClicked: backend.busy ? backend.cancel() : backend.refresh() }
                ActionButton { objectName: "windowMinimize"; text: "−"; implicitWidth: 40; Accessible.name: "Свернуть окно"; onClicked: window.showMinimized() }
                ActionButton { objectName: "windowMaximize"; text: window.visibility===Window.Maximized ? "❐" : "□"; implicitWidth: 40; Accessible.name: window.visibility===Window.Maximized ? "Восстановить размер окна" : "Развернуть окно"; onClicked: window.visibility===Window.Maximized ? window.showNormal() : window.showMaximized() }
                ActionButton { objectName: "windowClose"; text: "×"; implicitWidth: 40; destructive: true; Accessible.name: "Закрыть приложение"; onClicked: window.close() }
            }
            Label { text: backend.status; color: Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Rectangle {
                visible: backend.error.length>0
                Layout.fillWidth: true; implicitHeight: errorText.implicitHeight+24
                color: Theme.raised; radius: 8; border.color: Theme.warning
                Label { id: errorText; anchors.fill: parent; anchors.margins: 12; text: backend.error; color: Theme.warning; wrapMode: Text.WordWrap }
            }
            Surface {
                visible: window.notice.length>0; Layout.fillWidth: true; implicitHeight: noticeRow.implicitHeight+16
                RowLayout { id: noticeRow; anchors.fill: parent; anchors.margins: 8
                    Label { text: window.notice; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    ActionButton { text: "Закрыть"; onClicked: window.notice="" }
                }
            }
            StackLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: window.pageIndex
                OverviewPage { controller: backend }
                ScreenerPage { controller: signalBackend; market: backend }
                SignalsPage { controller: signalBackend; market: backend }
                PurchasePlanPage { controller: decisionBackend; marketController: backend }
                CompanyPage { controller: researchBackend; market: backend }
                CompanyComparisonPage { controller: researchBackend; market: backend }
                EventsPage { controller: eventsBackend; market: backend }
                StrategyPage { controller: strategyBackend; market: backend }
                PortfolioPage { controller: backend; dividends: dividendPortfolioBackend; onImportRequested: portfolioTransfer.open(); onExportRequested: { portfolioTransfer.open(); Qt.callLater(transferBackend.chooseExport) } onAddRequested: operation.open(); onLevelsRequested: function(id) { levels.positionId=id; levels.open() } }
                JournalPage { controller: backend; onImportRequested: portfolioTransfer.open(); onExportRequested: { portfolioTransfer.open(); Qt.callLater(transferBackend.chooseExport) } onAddRequested: operation.open() }
                DecisionDiaryPage { controller: decisionBackend }
                SettingsPage { controller: backend }
            }
        }
    }
    WindowDragArea { objectName: "topDragArea"; x: 6; y: 6; width: window.width - 12; height: window.compact ? 6 : 18; targetWindow: window }
    // Native system resizing preserves platform drag behavior and minimum size.
    // Eight thin hit areas leave the page and header controls unobstructed.
    Repeater {
        model: [Qt.LeftEdge, Qt.RightEdge, Qt.TopEdge, Qt.BottomEdge,
                Qt.LeftEdge | Qt.TopEdge, Qt.RightEdge | Qt.TopEdge,
                Qt.LeftEdge | Qt.BottomEdge, Qt.RightEdge | Qt.BottomEdge]
        delegate: MouseArea {
            required property int index
            required property int modelData
            objectName: "windowResize" + index
            readonly property bool horizontalEdge: (modelData & (Qt.LeftEdge | Qt.RightEdge)) !== 0
            readonly property bool verticalEdge: (modelData & (Qt.TopEdge | Qt.BottomEdge)) !== 0
            x: (modelData & Qt.RightEdge) ? window.width - width : horizontalEdge ? 0 : 6
            y: (modelData & Qt.BottomEdge) ? window.height - height : verticalEdge ? 0 : 6
            width: horizontalEdge ? 6 : window.width - 12
            height: verticalEdge ? 6 : window.height - 12
            z: 100
            enabled: window.visibility === Window.Windowed
            visible: enabled
            cursorShape: horizontalEdge && verticalEdge
                ? (((modelData & Qt.LeftEdge) !== 0) === ((modelData & Qt.TopEdge) !== 0) ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor)
                : horizontalEdge ? Qt.SizeHorCursor : Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            onPressed: window.startSystemResize(modelData)
        }
    }
    PortfolioTransferDialog { id: portfolioTransfer; objectName: "portfolioTransferDialog"; parent: Overlay.overlay; controller: transferBackend }
    OperationDialog { id: operation; controller: backend }
    LevelsDialog { id: levels; controller: backend }
}
