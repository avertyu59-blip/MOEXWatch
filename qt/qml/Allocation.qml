import QtQuick
import QtQuick.Layouts
Surface {
    id: root
    required property var controller
    implicitHeight: 420
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 16
        Text { text: "Структура позиций"; color: Theme.text; font.family: Theme.family; font.pixelSize: 18; font.weight: Font.DemiBold }
        Item {
            Layout.fillWidth: true; Layout.preferredHeight: 208
            Canvas {
                id: ring
                anchors.centerIn: parent; width: 208; height: 208
                onPaint: {
                    const ctx = getContext("2d"); ctx.reset()
                    const rows = root.controller.allocation
                    let angle = -Math.PI / 2
                    ctx.lineWidth = 30
                    if (!rows.length) { ctx.strokeStyle = Theme.border; ctx.beginPath(); ctx.arc(104,104,80,0,2*Math.PI); ctx.stroke() }
                    for (let i = 0; i < rows.length; ++i) {
                        const next = angle + rows[i].weight / 100 * 2 * Math.PI
                        ctx.strokeStyle = Theme.chartColors[i % Theme.chartColors.length]
                        ctx.beginPath(); ctx.arc(104,104,80,angle,next); ctx.stroke(); angle=next
                    }
                }
                Connections { target: root.controller; function onPortfolioChanged() { ring.requestPaint() } }
                Connections { target: Theme; function onDarkChanged() { ring.requestPaint() } }
            }
            Text { anchors.centerIn: parent; width: 128; height: 40; text: root.controller.allocation.length ? root.controller.totals.value : "Нет оценки"; fontSizeMode: Text.Fit; minimumPixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: Theme.text; font.family: Theme.family; font.pixelSize: 18; font.weight: Font.DemiBold }
        }
        Text { visible: root.controller.allocation.length === 0; text: "Для долей нужны свежие котировки всех открытых позиций."; color: Theme.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 13 }
        Repeater {
            model: root.controller.allocation
            RowLayout {
                required property var modelData
                required property int index
                Layout.fillWidth: true; spacing: 8
                Rectangle { width: 10; height: 10; radius: 5; color: Theme.chartColors[index % Theme.chartColors.length] }
                Text { text: modelData.ticker; color: Theme.text; font.pixelSize: 13; Layout.fillWidth: true }
                Text { text: Number(modelData.weight).toLocaleString(Qt.locale("ru_RU"),"f",1)+"%"; color: Theme.muted; font.pixelSize: 13 }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
