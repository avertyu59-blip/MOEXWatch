import QtQuick
import QtQuick.Layouts
Surface {
    id: card
    property string title
    property string amount
    property string note
    property color amountColor: Theme.text
    implicitHeight: 136
    implicitWidth: 240
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 8
        Text { text: card.title; color: Theme.muted; font.family: Theme.family; font.pixelSize: 14 }
        Text { text: card.amount; color: card.amountColor; font.family: Theme.family; font.pixelSize: card.width < 280 ? 24 : 30; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { text: card.note; color: Theme.muted; font.family: Theme.family; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
}
