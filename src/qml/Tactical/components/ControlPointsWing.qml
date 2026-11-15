pragma ComponentBehavior: Bound
import QtQuick 2.15

Rectangle {
    id: root
    property bool isBlueTeam: false
    property string teamName: ""
    property var controlState: ({})
    readonly property var points: controlState[isBlueTeam ? "blueControlPoints" : "redControlPoints"]
    readonly property string pointsText: points === undefined ? "--" : String(points)
    readonly property color accent: isBlueTeam ? "#55c6ff" : "#ff6475"
    property double nowMs: Date.now()
    readonly property bool stale: !controlState.unitUpdatedMs
        || nowMs - Number(controlState.unitUpdatedMs) > 3500
    color: isBlueTeam ? "#0c2133" : "#2b1520"
    border.color: accent
    radius: 5

    Timer { interval: 1000; repeat: true; running: root.visible; onTriggered: root.nowMs = Date.now() }
    Column {
        anchors.left: parent.left; anchors.leftMargin: parent.width * 0.035
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.6
        spacing: 4
        Text {
            width: parent.width
            text: root.teamName
            color: "#f2f8ff"
            font.pixelSize: Math.max(10, root.height * 0.21)
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            text: "累计控制点 · " + (root.isBlueTeam ? "蓝方" : "红方")
                + (root.stale ? " · 待同步" : "")
            color: root.accent
            font.pixelSize: Math.max(9, root.height * 0.15)
        }
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: parent.width * 0.035
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.32
        text: root.pointsText
        color: root.accent
        font.pixelSize: root.height * 0.52
        minimumPixelSize: 12
        fontSizeMode: Text.HorizontalFit
        horizontalAlignment: Text.AlignRight
        font.bold: true
    }
}
