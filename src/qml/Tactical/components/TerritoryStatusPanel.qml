pragma ComponentBehavior: Bound
import QtQuick 2.15

Rectangle {
    id: root
    property var controlData: ({})
    property bool allyIsBlue: false
    property double nowMs: Date.now()
    readonly property bool expanded: height > 350
    readonly property bool stale: !controlData.unitUpdatedMs || nowMs - Number(controlData.unitUpdatedMs) > 3500
    readonly property bool mechanismStale: !controlData.mechanismUpdatedMs || nowMs - Number(controlData.mechanismUpdatedMs) > 3500
    readonly property var mechanisms: controlData.mechanisms || ({})
    readonly property var regions: [
        { name: "R 前哨", key: "rOutpostStatus", hp: "rOutpostHp", mechanism: "3", base: false, blue: false },
        { name: "B 前哨", key: "bOutpostStatus", hp: "bOutpostHp", mechanism: "4", base: false, blue: true },
        { name: "红方基地领地", key: "redBaseControl", mechanism: "5", base: true, blue: false },
        { name: "蓝方基地领地", key: "blueBaseControl", mechanism: "6", base: true, blue: true }
    ]
    color: "#07131e"
    border.color: "#28506a"
    radius: 5

    Timer { interval: 1000; repeat: true; running: root.visible; onTriggered: root.nowMs = Date.now() }
    function points(blue) {
        var value = root.controlData[blue ? "blueControlPoints" : "redControlPoints"]
        return value === undefined ? "--" : String(value)
    }
    function controlText(region) {
        var value = root.controlData[region.key]
        if (value === undefined) return "等待状态"
        value = Number(value)
        if (region.base) {
            if (value !== 1 && value !== 2) return "未知状态 " + value
            var blue = value === 1 ? region.blue : !region.blue
            return (blue ? "蓝方" : "红方") + (value === 1 ? "完全控制" : "临时控制")
        }
        // 前哨状态已在接收端归一化为红方视角，不随观察阵营变化。
        return ["未知状态", "红方完全控制", "红方临时控制", "蓝方临时控制", "蓝方完全控制"][value] || "未知状态 " + value
    }
    function timerText(id) {
        if (root.mechanismStale) return "争夺计时待同步"
        var value = root.mechanisms[id]
        if (value === undefined) return "当前无激活计时"
        value = Number(value)
        if (value === 0) return "激活状态未变化 · 0s"
        return (value > 0 ? "己方推进" : "敌方推进") + " · " + Math.abs(value) + "s"
    }
    function fortressText(id) {
        if (root.mechanismStale) return "--"
        return root.mechanisms[id] === undefined ? "未激活" : String(root.mechanisms[id]) + "s"
    }

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: root.expanded ? 16 : 8
        Row {
            width: parent.width
            Text { width: parent.width - 105; text: "领地控制 / 控制点积分"; color: "#e8f5ff"; font.pixelSize: root.expanded ? 20 : 15; font.bold: true }
            Text { text: root.stale ? "等待 / 数据过期" : "状态已同步"; color: root.stale ? "#ffca6a" : "#6be3ba"; font.pixelSize: 11 }
        }
        Row {
            width: parent.width
            spacing: 12
            Repeater {
                model: [false, true]
                Rectangle {
                    id: score
                    required property bool modelData
                    width: (parent.width - 12) / 2
                    height: root.expanded ? 92 : 52
                    radius: 4
                    color: score.modelData ? "#102b42" : "#351b27"
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
                        text: (score.modelData ? "蓝方" : "红方") + (root.allyIsBlue === score.modelData ? " · 己方" : " · 敌方")
                        color: score.modelData ? "#65c9ff" : "#ff8c95"
                        font.pixelSize: root.expanded ? 16 : 12
                    }
                    Text {
                        objectName: score.modelData ? "blueControlPoints" : "redControlPoints"
                        anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
                        text: root.points(score.modelData)
                        color: "#ffffff"; font.pixelSize: root.expanded ? 36 : 25; font.bold: true
                    }
                }
            }
        }
        Grid {
            width: parent.width
            columns: 2
            spacing: 8
            Repeater {
                model: root.regions
                Rectangle {
                    id: region
                    required property var modelData
                    width: (parent.width - 8) / 2
                    height: root.expanded ? 103 : 56
                    radius: 3
                    color: "#0d202e"
                    border.color: "#203e50"
                    Column {
                        anchors.fill: parent; anchors.margins: root.expanded ? 10 : 5
                        spacing: root.expanded ? 8 : 2
                        Text {
                            width: parent.width
                            text: region.modelData.name + (region.modelData.hp && root.controlData[region.modelData.hp] !== undefined ? " · HP " + root.controlData[region.modelData.hp] : "")
                            color: "#a6c3d4"; font.pixelSize: root.expanded ? 14 : 11
                            elide: Text.ElideRight
                        }
                        Text {
                            text: root.controlText(region.modelData)
                            color: "#f3d887"; font.pixelSize: root.expanded ? 19 : 13; font.bold: true
                        }
                        Text { text: root.timerText(region.modelData.mechanism); color: "#a8ccdd"; font.pixelSize: root.expanded ? 13 : 10 }
                    }
                }
            }
        }
        Text {
            width: parent.width
            text: "己方堡垒被占 " + root.fortressText("1") + "     敌方堡垒被占 " + root.fortressText("2")
            color: "#a6c3d4"; font.pixelSize: root.expanded ? 14 : 11
            elide: Text.ElideRight
        }
    }
}
