#include "core/GameData.h"
#include "core/TacticalAnalyzer.h"

#include <QtTest>
#include <QSignalSpy>

#include <initializer_list>

class TestTacticalAnalyzer : public QObject {
    Q_OBJECT

private slots:
    void test2027CommandProjection() {
        GameData gameData;
        gameData.setCurrentRobotId(101);
        robomaster::GlobalUnitStatus status;
        status.set_total_cp_ally(120);
        status.set_total_cp_enemy(80);
        status.set_outpost_r_status(1);
        status.set_outpost_b_status(3);
        for (int hp : {110, 130, 140, 170, 210, 230, 240, 270}) status.add_robot_health(hp);
        gameData.updateGlobalUnitStatus(status);
        QCOMPARE(gameData.command2027State().value("blueControlPoints").toInt(), 120);
        QCOMPARE(gameData.command2027State().value("redControlPoints").toInt(), 80);
        QCOMPARE(gameData.command2027State().value("rOutpostStatus").toInt(), 4);
        QCOMPARE(gameData.command2027State().value("bOutpostStatus").toInt(), 2);
        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);
        analyzer.analyze();
        const auto robots = analyzer.allyRobotList();
        QCOMPARE(robots.size(), 4);
        QCOMPARE(robots[0].toMap().value("label").toString(), QStringLiteral("重装"));
        QCOMPARE(robots[1].toMap().value("hp").toInt(), 130);
        QCOMPARE(robots[3].toMap().value("hp").toInt(), 170);
        for (const auto &entry : robots) QVERIFY(entry.toMap().value("slot").toInt() != 2);
        robomaster::GlobalSpecialMechanism mechanisms;
        mechanisms.add_mechanism_id(3);
        mechanisms.add_mechanism_time_sec(-9);
        gameData.updateGlobalSpecialMechanism(mechanisms);
        QCOMPARE(gameData.command2027State().value("mechanisms").toMap().value("3").toInt(), -9);
        mechanisms.Clear();
        gameData.updateGlobalSpecialMechanism(mechanisms);
        QVERIFY(gameData.command2027State().value("mechanisms").toMap().isEmpty());
        status.Clear();
        status.set_total_cp_ally(0);
        gameData.updateGlobalUnitStatus(status);
        QCOMPARE(gameData.command2027State().value("blueControlPoints").toInt(), 0);
        QCOMPARE(gameData.command2027State().value("redControlPoints").toInt(), 80);
        gameData.setCurrentRobotId(1);
        QVERIFY(gameData.command2027State().isEmpty());
    }

    void testRespawnEconomyCalculation_data() {
        QTest::addColumn<int>("remainingTime");
        QTest::addColumn<int>("robotLevel");
        QTest::addColumn<int>("economy");
        QTest::addColumn<int>("expectedCost");
        QTest::addColumn<int>("expectedCount");

        QTest::newRow("match-start-insufficient") << 420 << 1 << 19 << 20 << 0;
        QTest::newRow("first-second-exact") << 419 << 1 << 100 << 100 << 1;
        QTest::newRow("minute-boundary") << 360 << 2 << 239 << 120 << 1;
        QTest::newRow("after-minute-boundary") << 359 << 3 << 659 << 220 << 2;
        QTest::newRow("match-end-high-level") << 0 << 10 << 1520 << 760 << 2;
    }

    void testRespawnEconomyCalculation() {
        QFETCH(int, remainingTime);
        QFETCH(int, robotLevel);
        QFETCH(int, economy);
        QFETCH(int, expectedCost);
        QFETCH(int, expectedCount);

        GameData gameData;
        gameData.setCurrentRobotId(1);
        gameData.getRobotDataRef(1).level = robotLevel;
        gameData.setRedEconomy(economy);
        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::BATTLE), remainingTime, 0, 0, 1, false);

        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);
        analyzer.analyze();

        const QVariantMap status = analyzer.topStatusData();
        QCOMPARE(status.value(QStringLiteral("respawnGoldCost")).toInt(), expectedCost);
        QCOMPARE(status.value(QStringLiteral("affordableRespawnCount")).toInt(), expectedCount);
        QVERIFY(status.value(QStringLiteral("respawnEconomyVisible")).toBool());
    }

    void testRespawnEconomyUsesBluePerspective() {
        GameData gameData;
        gameData.setCurrentRobotId(101);
        gameData.getRobotDataRef(101).level = 4;
        gameData.setRedEconomy(9999);
        gameData.setBlueEconomy(399);
        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::BATTLE), 300, 0, 0, 1, false);

        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);
        analyzer.analyze();

        const QVariantMap status = analyzer.topStatusData();
        QCOMPARE(status.value(QStringLiteral("respawnGoldCost")).toInt(), 240);
        QCOMPARE(status.value(QStringLiteral("affordableRespawnCount")).toInt(), 1);
    }

    void testRespawnEconomyCardBeforeBattleAndHiddenWhenPaused() {
        GameData gameData;
        gameData.setCurrentRobotId(1);
        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::PREPARATION), 89, 0, 0, 1, false);

        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);
        analyzer.analyze();

        const QVariantMap status = analyzer.topStatusData();
        QVERIFY(status.value(QStringLiteral("respawnEconomyVisible")).toBool());
        QCOMPARE(status.value(QStringLiteral("respawnGoldCost")).toInt(), 0);
        QCOMPARE(status.value(QStringLiteral("affordableRespawnCount")).toInt(), 0);

        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::BATTLE), 300, 0, 0, 1, true);
        analyzer.analyze();
        QVERIFY(!analyzer.topStatusData()
                     .value(QStringLiteral("respawnEconomyVisible"))
                     .toBool());
    }

    void testDefaultLayoutModeIsMapPrimary() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        QCOMPARE(analyzer.layoutMode(), QStringLiteral("map_primary"));
    }

    void testMockAnalysisMetricsExposeFiveConfiguredCards() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        analyzer.analyze();

        const QVariantList metrics = analyzer.analysisMetrics();
        QCOMPARE(metrics.size(), 5);

        const QStringList expectedKeys{
            QStringLiteral("ally_economy"),
            QStringLiteral("enemy_economy"),
            QStringLiteral("ally_damage"),
            QStringLiteral("enemy_damage"),
            QStringLiteral("hp_diff")
        };
        const QStringList expectedTitles{
            QStringLiteral("我方总经济"),
            QStringLiteral("敌方总经济"),
            QStringLiteral("我方总伤害"),
            QStringLiteral("敌方总伤害"),
            QStringLiteral("总血量差")
        };

        for (int i = 0; i < metrics.size(); ++i) {
            const QVariantMap metric = metrics.at(i).toMap();
            QCOMPARE(metric.value(QStringLiteral("key")).toString(), expectedKeys.at(i));
            QCOMPARE(metric.value(QStringLiteral("title")).toString(), expectedTitles.at(i));
            QVERIFY(metric.contains(QStringLiteral("compareText")));
        }
    }

    void testRobotListsSkipSlotFive() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        analyzer.setUseMockData(false);

        const QVariantList allyRobots = analyzer.allyRobotList();
        const QVariantList enemyRobots = analyzer.enemyRobotList();

        QCOMPARE(allyRobots.size(), 4);
        QCOMPARE(enemyRobots.size(), 4);

        auto containsSlotFive = [](const QVariantList &robots) {
            for (const QVariant &entry : robots) {
                if (entry.toMap().value("slot").toInt() == 5) {
                    return true;
                }
            }
            return false;
        };

        QVERIFY(!containsSlotFive(allyRobots));
        QVERIFY(!containsSlotFive(enemyRobots));

        // 2027 界面不再展示空中系统（6 号）。
        auto containsSlotSix = [](const QVariantList &robots) {
            for (const QVariant &entry : robots) {
                if (entry.toMap().value("slot").toInt() == 6) {
                    return true;
                }
            }
            return false;
        };
        QVERIFY(!containsSlotSix(allyRobots));
        QVERIFY(!containsSlotSix(enemyRobots));
    }

    void testRobotListsDefaultMaxHpToOneHundredFifty() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        analyzer.setUseMockData(false);

        const QVariantList allyRobots = analyzer.allyRobotList();
        const QVariantList enemyRobots = analyzer.enemyRobotList();

        QCOMPARE(allyRobots.size(), 4);
        QCOMPARE(enemyRobots.size(), 4);

        for (const QVariant &entry : allyRobots) {
            const QVariantMap robot = entry.toMap();
            QCOMPARE(robot.value(QStringLiteral("maxHp")).toInt(), 150);
        }

        for (const QVariant &entry : enemyRobots) {
            const QVariantMap robot = entry.toMap();
            QCOMPARE(robot.value(QStringLiteral("maxHp")).toInt(), 150);
        }
    }

    void testRealAnalysisMetricsUseAllyEnemyTotals() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        gameData.setRedEconomy(1200);
        gameData.setBlueEconomy(900);

        DamageEventData allyDamage{};
        allyDamage.attackerId = 1;
        allyDamage.victimId = 101;
        allyDamage.damage = 500;
        allyDamage.armorId = 1;
        allyDamage.hurtType = 0;
        gameData.recordDamageEvent(allyDamage);

        DamageEventData enemyDamage{};
        enemyDamage.attackerId = 101;
        enemyDamage.victimId = 1;
        enemyDamage.damage = 300;
        enemyDamage.armorId = 1;
        enemyDamage.hurtType = 0;
        gameData.recordDamageEvent(enemyDamage);

        analyzer.setUseMockData(false);
        analyzer.analyze();

        const QVariantList metrics = analyzer.analysisMetrics();
        QCOMPARE(metrics.size(), 5);

        const QVariantMap allyEconomy = metrics.at(0).toMap();
        QCOMPARE(allyEconomy.value(QStringLiteral("key")).toString(), QStringLiteral("ally_economy"));
        QCOMPARE(allyEconomy.value(QStringLiteral("value")).toInt(), 1200);
        QCOMPARE(allyEconomy.value(QStringLiteral("compareText")).toString(), QStringLiteral("敌方 900"));

        const QVariantMap enemyEconomy = metrics.at(1).toMap();
        QCOMPARE(enemyEconomy.value(QStringLiteral("key")).toString(), QStringLiteral("enemy_economy"));
        QCOMPARE(enemyEconomy.value(QStringLiteral("value")).toInt(), 900);
        QCOMPARE(enemyEconomy.value(QStringLiteral("compareText")).toString(), QStringLiteral("我方 1200"));

        const QVariantMap allyDamageMetric = metrics.at(2).toMap();
        QCOMPARE(allyDamageMetric.value(QStringLiteral("key")).toString(), QStringLiteral("ally_damage"));
        QCOMPARE(allyDamageMetric.value(QStringLiteral("value")).toInt(), 500);
        QCOMPARE(allyDamageMetric.value(QStringLiteral("compareText")).toString(), QStringLiteral("敌方 300"));

        const QVariantMap enemyDamageMetric = metrics.at(3).toMap();
        QCOMPARE(enemyDamageMetric.value(QStringLiteral("key")).toString(), QStringLiteral("enemy_damage"));
        QCOMPARE(enemyDamageMetric.value(QStringLiteral("value")).toInt(), 300);
        QCOMPARE(enemyDamageMetric.value(QStringLiteral("compareText")).toString(), QStringLiteral("我方 500"));

        const QVariantMap hpDiffMetric = metrics.at(4).toMap();
        QCOMPARE(hpDiffMetric.value(QStringLiteral("key")).toString(), QStringLiteral("hp_diff"));
        QCOMPARE(hpDiffMetric.value(QStringLiteral("value")).toString(), QStringLiteral("+0"));
    }

    void testEnemyPaidRespawnSignalMatchesFlashAndDoesNotRepeat() {
        GameData gameData;
        gameData.setCurrentRobotId(1);
        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);

        RobotData &enemyHero = gameData.getRobotDataRef(101);
        auto publishHealth = [&](int hp) {
            robomaster::GlobalUnitStatus packet;
            for (int value : {300, 300, 300, 300, hp, 300, 300, 300}) packet.add_robot_health(value);
            gameData.updateGlobalUnitStatus(packet);
        };
        publishHealth(300);
        QSignalSpy paidRespawnSpy(
            &analyzer, &RM::TacticalAnalyzer::enemyPaidRespawnDetected);
        QVERIFY(paidRespawnSpy.isValid());

        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);

        publishHealth(0);
        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);

        publishHealth(300);
        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 1);
        QCOMPARE(paidRespawnSpy.takeFirst().at(0).toInt(), 101);

        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);
    }

    void testEnemyPaidRespawnUsesGlobalZeroWhileStaticConnectionRemainsOnline() {
        GameData gameData;
        gameData.setCurrentRobotId(106);
        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);

        RobotData &enemyHero = gameData.getRobotDataRef(1);
        enemyHero.tabStaticConnected = true;
        enemyHero.currentHP = 600;
        auto publishHealth = [&](int hp) {
            robomaster::GlobalUnitStatus packet;
            for (int value : {300, 300, 300, 300, hp, 300, 300, 300}) packet.add_robot_health(value);
            gameData.updateGlobalUnitStatus(packet);
        };
        publishHealth(600);

        QSignalSpy paidRespawnSpy(
            &analyzer, &RM::TacticalAnalyzer::enemyPaidRespawnDetected);
        QVERIFY(paidRespawnSpy.isValid());

        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);

        publishHealth(0);
        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);

        publishHealth(600);
        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 1);
        QCOMPARE(paidRespawnSpy.takeFirst().at(0).toInt(), 1);

        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);
    }
};

QTEST_MAIN(TestTacticalAnalyzer)
#include "test_tactical_analyzer.moc"
