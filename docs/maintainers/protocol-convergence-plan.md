# RoboMaster 2027 自定义终端协议适配

当前自定义终端的协议目标为《RoboMaster 2027 University Series Communication Protocol V1.0.0》
（2026-11-02）。Topic、方向、频率、QoS、字段类型和字段号以协议的字段表、命令总表为准；
示例代码用于辅助阅读。当示例与表格冲突时，按表格实现，并在下文记录差异。

## 自定义终端接入

| 类型 | Topic | 自定义终端处理 |
| --- | --- | --- |
| 接收 | `GameStatus`、`GlobalUnitStatus`、`GlobalLogisticsStatus`、`GlobalSpecialMechanism` | 比赛状态、单位状态、后勤、特殊机制；控制度字段单独投影到指挥界面 |
| 接收 | `Event`、`RobotInjuryStat`、`RobotRespawnStatus`、`RobotStaticStatus`、`RobotDynamicStatus`、`RobotModuleStatus`、`RobotPosition`、`Buff`、`PenaltyInfo` | 事件、机器人和判罚状态；`RobotPosition` 按字段 presence 局部更新 |
| 接收 | `RobotPathPlanInfo`、`RadarInfoToClient`、`CustomByteBlock` | 哨兵路径、雷达位置、自定义字节流 |
| 接收 | `TechCoreMotionStateSync`、`RobotPerformanceSelectionSync`、`RuneStatusSync`、`SentryStatusSync`、`DartSelectTargetStatusSync`、`SentryCtrlResult`、`MapClickInfo` | 2027 专项状态及命令结果 |
| 发送 | `MapClickCmd`、`AssemblyCommand`、`RobotPerformanceSelectionCommand`、`CommonCommand`、`RuneActivateCommand`、`DartCommand`、`SentryCtrlCommand` | 地图标记、装配、性能体系、通用指令、符文、飞镖和哨兵控制；按命令总表限频 |
| 发送 | `KeyboardMouseControl`、`CustomControl` | 自定义终端键鼠控制与自定义数据；约束字段长度和发送频率 |

收发均使用 `src/network/proto/robomaster.proto` 作为 C++ canonical schema。数据经
`NetworkManager` 解码或编码，再由 `GameData` 提供给界面。2027 指挥状态通过
`GameData.command2027State` 暴露，不复用旧版护盾字段语义。

## 已按字段表适配的关键点

- `GlobalUnitStatus` 字段 15/16 是双方控制度；字段 3/8 是双方基地控制状态，不是基地护盾。
- `RobotPosition` 的字段 presence 用于增量更新；消息缺少 `robot_id` 时按当前终端机器人归属。
- `RadarInfoToClient` 字段 1 是重复 `uint32`，每台机器人按 X(cm)、Y(cm)、特殊识别状态排列；
  顺序为敌方 1/3/4/6/7，再己方 1/3/4/6/7。
- `MapClickCmd.robot_id` 是固定 7 字节的机器人 ID 列表；不是位图。
- `RobotDynamicStatus` 字段 4 为保留项；模块化 42mm 权限来自 `RobotModuleStatus` 字段 11。
- 附录 II 的 MQTT Client ID 使用 `0x0100 + robot_id`；机器人 2 为预留位，不建立该身份。
- 不在 2027 命令总表中的旧 `AirSupportCommand` 和 `HeroDeployModeEventCommand` 不会从自定义终端发送。

## PDF 示例差异

`RadarInfoToClient` 字段表规定字段 1 为重复 `uint32`，且说明每台机器人连续发送 X/Y/识别状态；
同节 protobuf 示例却声明重复 `RadarSingleRobotInfo`，与表格的类型和形状不一致。自定义终端按字段表
实现为重复标量，并保留 10 台机器人各三个值的布局。

`RobotPosition` 字段表没有列出 `robot_id`，文末示例却包含字段 5。自定义终端保留字段 5 以兼容该示例，
同时将其设为 optional；如果线上包不含该字段，则归属当前配置的机器人。

## 验证边界

本地 C++ schema、编译和 CTest 已通过。PDF 与样例的差异按字段表处理；本仓库没有 2027 裁判系统
Broker 的抓包或验收结果，因此“能连接现场服务器并完成每条命令交互”仍需现场联调确认。协议
版本、Topic 和表格核对进度见 `src/network/proto/protocol_manifest.json`。
