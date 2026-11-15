#include <QJSValue>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

class TacticalMainWindowStub : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool tacticalLargeMapMode READ tacticalLargeMapMode WRITE
                 setTacticalLargeMapMode NOTIFY tacticalLargeMapModeChanged)

public:
  bool tacticalLargeMapMode() const { return m_tacticalLargeMapMode; }

  void setTacticalLargeMapMode(bool enabled) {
    if (m_tacticalLargeMapMode == enabled) {
      return;
    }
    m_tacticalLargeMapMode = enabled;
    emit tacticalLargeMapModeChanged();
  }

signals:
  void tacticalLargeMapModeChanged();

private:
  bool m_tacticalLargeMapMode = false;
};

class TacticalGameDataStub : public QObject {
  Q_OBJECT
  Q_PROPERTY(int currentRobotId READ currentRobotId WRITE setCurrentRobotId NOTIFY
                 currentRobotIdChanged)
  Q_PROPERTY(QVariantMap command2027State READ command2027State NOTIFY command2027StateChanged)
  Q_PROPERTY(bool hasHeroFrame READ hasHeroFrame NOTIFY heroFrameUpdated)
  Q_PROPERTY(quint64 heroFrameRevision READ heroFrameRevision NOTIFY
                 heroFrameUpdated)
  Q_PROPERTY(QString heroFrameSource READ heroFrameSource NOTIFY
                 heroFrameUpdated)

public:
  QVariantMap command2027State() const { return m_commandState; }
  void publishControlPoints(int red, int blue) {
    m_commandState = {{"redControlPoints", red}, {"blueControlPoints", blue},
                     {"unitUpdatedMs", QDateTime::currentMSecsSinceEpoch()}};
    emit command2027StateChanged();
  }
  int currentRobotId() const { return m_currentRobotId; }
  bool hasHeroFrame() const { return m_hasHeroFrame; }
  quint64 heroFrameRevision() const { return m_heroFrameRevision; }
  QString heroFrameSource() const {
    return m_hasHeroFrame
               ? QStringLiteral("image://herovideo/frame?rev=%1")
                     .arg(m_heroFrameRevision)
               : QString();
  }

  void setCurrentRobotId(int robotId) {
    if (m_currentRobotId == robotId) {
      return;
    }
    m_currentRobotId = robotId;
    emit currentRobotIdChanged();
  }

  void publishHeroFrame() {
    m_hasHeroFrame = true;
    ++m_heroFrameRevision;
    emit heroFrameUpdated();
  }

signals:
  void currentRobotIdChanged();
  void command2027StateChanged();
  void heroFrameUpdated();

private:
  QVariantMap m_commandState;
  int m_currentRobotId = 1;
  bool m_hasHeroFrame = false;
  quint64 m_heroFrameRevision = 0;
};

class TestTacticalCommandPage : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    Q_INIT_RESOURCE(qml);
    Q_INIT_RESOURCE(resources);
  }

  void cleanupTestCase() {
    Q_CLEANUP_RESOURCE(resources);
    Q_CLEANUP_RESOURCE(qml);
  }

  void testLargeMapOverlayLifecycleAndGeometry_data() {
    QTest::addColumn<int>("pageWidth");
    QTest::addColumn<int>("pageHeight");

    // 下列数据仅验证跨平台一致的 Qt 逻辑坐标；Ubuntu 上的
    // X11/Wayland 场景图仍需在目标机器单独验证。
    QTest::newRow("low-resolution-4-by-3") << 800 << 600;
    QTest::newRow("hd-16-by-9") << 1280 << 720;
    QTest::newRow("laptop-near-16-by-9") << 1366 << 768;
    QTest::newRow("full-hd") << 1920 << 1080;
    QTest::newRow("16-by-10") << 1920 << 1200;
    QTest::newRow("qhd") << 2560 << 1440;
    QTest::newRow("ultrawide") << 3440 << 1440;
    QTest::newRow("4k") << 3840 << 2160;
  }

  void testLargeMapOverlayLifecycleAndGeometry() {
    QFETCH(int, pageWidth);
    QFETCH(int, pageHeight);

    QQmlEngine engine;
    TacticalMainWindowStub mainWindow;
    engine.rootContext()->setContextProperty(QStringLiteral("mainWindow"),
                                             &mainWindow);

    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> pageObject(component.create());
    QVERIFY2(pageObject, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(pageObject.data());
    QVERIFY(page);
    page->setWidth(pageWidth);
    page->setHeight(pageHeight);

    QCOMPARE(page->property("largeMapMode").toBool(), false);
    QCOMPARE(page->property("largeMapOverlayReady").toBool(), false);
    const QString originalLayoutMode = page->property("layoutMode").toString();
    page->setProperty("selectedRobotId", QStringLiteral("A3"));

    QObject *loader =
        page->findChild<QObject *>(QStringLiteral("tacticalLargeMapLoader"));
    QVERIFY(loader);
    auto *centerMap = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("tacticalCenterMap")));
    auto *auxMap = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("tacticalAuxMap")));
    QVERIFY(centerMap);
    QVERIFY(auxMap);
    QCOMPARE(centerMap->property("visualScale").toReal(), 1.0);
    QCOMPARE(auxMap->property("visualScale").toReal(), 1.0);
    QVERIFY(!centerMap->property("threadedCanvasRendering").toBool());
    QVERIFY(!auxMap->property("threadedCanvasRendering").toBool());
    QVERIFY(centerMap->isVisible());
    QVERIFY(!auxMap->isVisible());
    QCOMPARE(loader->property("active").toBool(), false);
    QVERIFY(
        !page->findChild<QObject *>(QStringLiteral("tacticalLargeMapOverlay")));

    mainWindow.setTacticalLargeMapMode(true);
    QTRY_VERIFY(page->property("largeMapMode").toBool());
    QTRY_VERIFY(page->property("largeMapOverlayReady").toBool());

    QObject *overlayObject = nullptr;
    QTRY_VERIFY((overlayObject = page->findChild<QObject *>(
                     QStringLiteral("tacticalLargeMapOverlay"))) != nullptr);
    auto *overlay = qobject_cast<QQuickItem *>(overlayObject);
    QVERIFY(overlay);
    QTRY_COMPARE(overlay->width(), static_cast<qreal>(pageWidth));
    QTRY_COMPARE(overlay->height(), static_cast<qreal>(pageHeight));
    QVERIFY(!centerMap->isVisible());
    QVERIFY(!auxMap->isVisible());

    auto *inputBlocker = qobject_cast<QQuickItem *>(page->findChild<QObject *>(
        QStringLiteral("tacticalLargeMapInputBlocker")));
    QVERIFY(inputBlocker);
    QCOMPARE(inputBlocker->width(), overlay->width());
    QCOMPARE(inputBlocker->height(), overlay->height());

    QObject *largeMapObject =
        page->findChild<QObject *>(QStringLiteral("tacticalLargeMap"));
    auto *largeMap = qobject_cast<QQuickItem *>(largeMapObject);
    QVERIFY(largeMap);
    QCOMPARE(largeMap->property("pageRoot").value<QObject *>(),
             pageObject.data());
    QCOMPARE(largeMap->scale(), 1.0);
    QCOMPARE(largeMap->z(), 1.0);
    QVERIFY(largeMap->property("threadedCanvasRendering").toBool());
    QVERIFY(largeMap->z() > inputBlocker->z());

    const QRectF mappedBounds = largeMap->mapRectToItem(
        page, QRectF(0.0, 0.0, largeMap->width(), largeMap->height()));
    QVERIFY(mappedBounds.left() >= -1.0);
    QVERIFY(mappedBounds.top() >= -1.0);
    QVERIFY(mappedBounds.right() <= page->width() + 1.0);
    QVERIFY(mappedBounds.bottom() <= page->height() + 1.0);
    const bool fillsWidth = qAbs(mappedBounds.width() - page->width()) <= 10.0;
    const bool fillsHeight =
        qAbs(mappedBounds.height() - page->height()) <= 1.0;
    QVERIFY2(fillsWidth || fillsHeight,
             "The aspect-preserving map must fill at least one screen axis");
    QCOMPARE(largeMap->width(), mappedBounds.width());
    QCOMPARE(largeMap->height(), mappedBounds.height());
    const qreal expectedWidth =
        qMin(static_cast<qreal>(pageWidth),
             static_cast<qreal>(pageHeight) * 840.0 / 474.0);
    const qreal expectedHeight = expectedWidth * 474.0 / 840.0;
    QVERIFY(qAbs(largeMap->width() - expectedWidth) <= 0.001);
    QVERIFY(qAbs(largeMap->height() - expectedHeight) <= 0.001);
    QVERIFY(qAbs(largeMap->property("visualScale").toReal() -
                 largeMap->width() / 840.0) <= 0.001);

    QVariantMap robot;
    robot.insert(QStringLiteral("x"), 0.25);
    robot.insert(QStringLiteral("y"), 0.20);
    QVariant hitAtRenderedPosition;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "isRobotHit", Q_RETURN_ARG(QVariant, hitAtRenderedPosition),
        Q_ARG(QVariant, QVariant(robot)), Q_ARG(QVariant, QVariant(0.25)),
        Q_ARG(QVariant, QVariant(0.80))));
    QVERIFY(hitAtRenderedPosition.toBool());
    QVariant missAtUnflippedPosition;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "isRobotHit", Q_RETURN_ARG(QVariant, missAtUnflippedPosition),
        Q_ARG(QVariant, QVariant(robot)), Q_ARG(QVariant, QVariant(0.25)),
        Q_ARG(QVariant, QVariant(0.20))));
    QVERIFY(!missAtUnflippedPosition.toBool());

    const QJSValue model = largeMap->property("model").value<QJSValue>();
    QVERIFY(model.isObject());
    QCOMPARE(model.property(QStringLiteral("radarAgeMs")).toInt(), 118);

    QPointer<QObject> destroyedWithLoader = overlayObject;
    mainWindow.setTacticalLargeMapMode(false);
    QTRY_VERIFY(!page->property("largeMapMode").toBool());
    QTRY_VERIFY(!page->property("largeMapOverlayReady").toBool());
    QTRY_VERIFY(destroyedWithLoader.isNull());
    QVERIFY(centerMap->isVisible());
    QVERIFY(!auxMap->isVisible());
    QCOMPARE(page->property("layoutMode").toString(), originalLayoutMode);
    QCOMPARE(page->property("selectedRobotId").toString(),
             QStringLiteral("A3"));
  }

  void testLargeMapUsesSameCampViewMappingAsTacticalMaps() {
    QQmlEngine engine;
    TacticalMainWindowStub mainWindow;
    TacticalGameDataStub gameData;
    gameData.setCurrentRobotId(101);
    engine.rootContext()->setContextProperty(QStringLiteral("mainWindow"),
                                             &mainWindow);
    engine.rootContext()->setContextProperty(QStringLiteral("gameData"),
                                             &gameData);

    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> pageObject(component.create());
    QVERIFY2(pageObject, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(pageObject.data());
    QVERIFY(page);
    page->setWidth(1920);
    page->setHeight(1080);

    QCOMPARE(page->property("allyIsBlue").toBool(), true);
    QCOMPARE(page->property("tacticalMapViewMirrored").toBool(), true);
    QCOMPARE(page->property("tacticalMapBackgroundSource").toString(),
             QStringLiteral("qrc:/images/minimap_bg_blue_left.png"));

    auto *centerMap = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("tacticalCenterMap")));
    QVERIFY(centerMap);
    QCOMPARE(centerMap->property("backgroundSource").toString(),
             QStringLiteral("qrc:/images/minimap_bg_blue_left.png"));
    QCOMPARE(centerMap->property("viewMirrored").toBool(), true);

    mainWindow.setTacticalLargeMapMode(true);
    QTRY_VERIFY(page->property("largeMapOverlayReady").toBool());

    auto *largeMap = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("tacticalLargeMap")));
    QVERIFY(largeMap);
    QCOMPARE(largeMap->property("backgroundSource").toString(),
             QStringLiteral("qrc:/images/minimap_bg_blue_left.png"));
    QCOMPARE(largeMap->property("viewMirrored").toBool(), true);

    QVariantMap robot;
    robot.insert(QStringLiteral("x"), 0.25);
    robot.insert(QStringLiteral("y"), 0.20);
    QVariant transformedRobot;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "transformRobot", Q_RETURN_ARG(QVariant, transformedRobot),
        Q_ARG(QVariant, QVariant(robot))));
    QVariant hitAtRotatedPosition;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "isRobotHit", Q_RETURN_ARG(QVariant, hitAtRotatedPosition),
        Q_ARG(QVariant, transformedRobot), Q_ARG(QVariant, QVariant(0.75)),
        Q_ARG(QVariant, QVariant(0.20))));
    QVERIFY(hitAtRotatedPosition.toBool());

    QVariant missAtRedPerspectivePosition;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "isRobotHit",
        Q_RETURN_ARG(QVariant, missAtRedPerspectivePosition),
        Q_ARG(QVariant, transformedRobot), Q_ARG(QVariant, QVariant(0.25)),
        Q_ARG(QVariant, QVariant(0.80))));
    QVERIFY(!missAtRedPerspectivePosition.toBool());
  }

  void testCommandHeaderUsesLiveControlPoints() {
    QQmlEngine engine;
    TacticalGameDataStub data;
    engine.rootContext()->setContextProperty(QStringLiteral("gameData"), &data);
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    auto *ally = page->findChild<QObject *>(QStringLiteral("allyControlPointsWing"));
    auto *enemy = page->findChild<QObject *>(QStringLiteral("enemyControlPointsWing"));
    QVERIFY(ally); QVERIFY(enemy);
    QCOMPARE(ally->property("pointsText").toString(), QStringLiteral("--"));
    data.publishControlPoints(120, 80);
    QTRY_COMPARE(ally->property("pointsText").toString(), QStringLiteral("120"));
    QCOMPARE(enemy->property("pointsText").toString(), QStringLiteral("80"));
    data.publishControlPoints(0, 95);
    QTRY_COMPARE(ally->property("pointsText").toString(), QStringLiteral("0"));
    data.setCurrentRobotId(101);
    QTRY_COMPARE(ally->property("pointsText").toString(), QStringLiteral("95"));
    QCOMPARE(enemy->property("pointsText").toString(), QStringLiteral("0"));
    auto *source = page->property("dataSource").value<QObject *>();
    QVERIFY(source);
    source->setProperty("layoutMode", QStringLiteral("video_primary"));
    QTRY_VERIFY(page->property("videoPrimary").toBool());
    data.publishControlPoints(7, 11);
    QTRY_COMPARE(ally->property("pointsText").toString(), QStringLiteral("11"));
    QCOMPARE(enemy->property("pointsText").toString(), QStringLiteral("7"));
  }

  void testControlLayoutRendering() {
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(page);
    QQuickWindow window;
    window.resize(1920, 1080);
    page->setParentItem(window.contentItem());
    page->setSize(window.size());
    QVariantMap controls{{"redControlPoints", 120}, {"blueControlPoints", 80},
       {"rOutpostHp", 1000}, {"bOutpostHp", 800}, {"rOutpostStatus", 1}, {"bOutpostStatus", 3},
       {"redBaseControl", 1}, {"blueBaseControl", 2}, {"unitUpdatedMs", QDateTime::currentMSecsSinceEpoch()},
       {"mechanismUpdatedMs", QDateTime::currentMSecsSinceEpoch()}, {"mechanisms", QVariantMap{{"3", -9}, {"6", 6}}}};
    // controlState is readonly; feed the rendering fixture through each panel instead.
    for (const QString &name : {QStringLiteral("tacticalAuxControl"), QStringLiteral("tacticalCenterControl")}) {
      auto *panel = page->findChild<QQuickItem *>(name);
      QVERIFY(panel);
      panel->setProperty("controlData", controls);
      QVERIFY(panel->height() > 240);
    }
    window.show();
    QTest::qWait(200);
    const QString capture = qEnvironmentVariable("RM_COMMAND_CAPTURE");
    if (!capture.isEmpty()) QVERIFY(window.grabWindow().save(capture));
  }

  void testControlPanelsReplaceIndustrialCamera() {
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    QVERIFY(!page->findChild<QObject *>(QStringLiteral("tacticalAuxCamera")));
    QVERIFY(!page->findChild<QObject *>(QStringLiteral("tacticalCenterCamera")));
    QObject *panel = page->findChild<QObject *>(QStringLiteral("tacticalAuxControl"));
    QVERIFY(panel);
    QVERIFY(panel->property("visible").toBool());
    QVERIFY(panel->setProperty("controlData", QVariantMap{{"redControlPoints", 120}, {"blueControlPoints", 80}}));
    QVariant redPoints, bluePoints;
    QVERIFY(QMetaObject::invokeMethod(panel, "points", Q_RETURN_ARG(QVariant, redPoints), Q_ARG(QVariant, false)));
    QVERIFY(QMetaObject::invokeMethod(panel, "points", Q_RETURN_ARG(QVariant, bluePoints), Q_ARG(QVariant, true)));
    QCOMPARE(redPoints.toString(), QStringLiteral("120"));
    QCOMPARE(bluePoints.toString(), QStringLiteral("80"));
    QObject *source = page->property("dataSource").value<QObject *>();
    QVERIFY(source);
    QVERIFY(source->setProperty("layoutMode", QStringLiteral("video_primary")));
    QTRY_VERIFY(page->property("videoPrimary").toBool());
    QVERIFY(!panel->property("visible").toBool());
    QObject *center = page->findChild<QObject *>(QStringLiteral("tacticalCenterControl"));
    QVERIFY(center);
    QTRY_VERIFY(center->property("visible").toBool());
    QVERIFY(page->findChild<QObject *>(QStringLiteral("tacticalAuxMap"))->property("visible").toBool());

    for (const QVariant &entry : page->property("allyRobotsForMap").value<QJSValue>().toVariant().toList())
      QVERIFY(entry.toMap().value("slot").toInt() != 2);
  }
};

QTEST_MAIN(TestTacticalCommandPage)
#include "test_tactical_command_page.moc"
