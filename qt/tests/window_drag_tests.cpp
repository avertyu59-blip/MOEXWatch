#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <memory>

class MoveWindow : public QQuickWindow {
  Q_OBJECT
public:
  int requests = 0;
  bool acceptNative = false;
  Q_INVOKABLE bool startSystemMove() {
    ++requests;
    return acceptNative;
  }
};

class WindowDragTests : public QObject {
  Q_OBJECT
  std::unique_ptr<QQmlEngine> engine;
  std::unique_ptr<MoveWindow> window;
  QQuickItem *area = nullptr;
  void drag(Qt::MouseButton button = Qt::LeftButton, int distance = 60) {
    const auto releaseGlobal = window->mapToGlobal(QPoint(80 + distance, 40));
    QTest::mousePress(window.get(), button, Qt::NoModifier, {80, 40});
    QTest::mouseMove(window.get(), {80 + distance, 40}, 20);
    QTest::mouseRelease(window.get(), button, Qt::NoModifier,
                        window->mapFromGlobal(releaseGlobal));
  }
private slots:
  void init() {
    engine = std::make_unique<QQmlEngine>();
    window = std::make_unique<MoveWindow>();
    window->setGeometry(100, 100, 800, 600);
    QQmlComponent component(engine.get(),
                            QUrl::fromLocalFile(QStringLiteral(DRAG_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    area = qobject_cast<QQuickItem *>(component.createWithInitialProperties(
        {{"targetWindow", QVariant::fromValue(window.get())}}));
    QVERIFY2(area, qPrintable(component.errorString()));
    area->setParent(window.get());
    area->setParentItem(window->contentItem());
    area->setSize({400, 80});
    window->show();
    QTest::qWait(20);
  }
  void cleanup() {
    window.reset();
    engine.reset();
    area = nullptr;
  }
  void clickDoesNotMove() {
    const auto before = window->position();
    QTest::mouseClick(window.get(), Qt::LeftButton, Qt::NoModifier, {80, 40});
    QCOMPARE(window->requests, 0);
    QCOMPARE(window->position(), before);
  }
  void jitterDoesNotMove() {
    const auto before = window->position();
    drag(Qt::LeftButton, 1);
    QCOMPARE(window->requests, 0);
    QCOMPARE(window->position(), before);
  }
  void fallbackMovesWindow() {
    const auto before = window->position();
    drag();
    QCOMPARE(window->requests, 1);
    QCOMPARE(window->position(), before + QPoint(60, 0));
  }
  void rightDragIgnored() {
    const auto before = window->position();
    drag(Qt::RightButton);
    QCOMPARE(window->requests, 0);
    QCOMPARE(window->position(), before);
  }
  void nativeMoveHasNoManualDelta() {
    window->acceptNative = true;
    const auto before = window->position();
    drag();
    QCOMPARE(window->requests, 1);
    QCOMPARE(window->position(), before);
  }
  void releaseEndsDrag() {
    drag();
    const auto before = window->position();
    QTest::mouseMove(window.get(), {200, 60}, 20);
    QCOMPARE(window->position(), before);
    QVERIFY(!area->property("moving").toBool());
    QVERIFY(!area->property("manualMove").toBool());
  }
  void cancellationStopsFallback() {
    QTest::mousePress(window.get(), Qt::LeftButton, Qt::NoModifier, {80, 40});
    QTest::mouseMove(window.get(), {140, 40}, 20);
    area->setVisible(false);
    QCoreApplication::processEvents();
    const auto before = window->position();
    QTest::mouseMove(window.get(), {200, 40}, 20);
    QTest::mouseRelease(window.get(), Qt::LeftButton, Qt::NoModifier,
                        {200, 40});
    QCOMPARE(window->position(), before);
    QVERIFY(!area->property("manualMove").toBool());
  }
  void doubleClickTogglesMaximize() {
    QTest::mouseDClick(window.get(), Qt::LeftButton, Qt::NoModifier, {80, 40});
    QCOMPARE(window->visibility(), QWindow::Maximized);
    QCOMPARE(window->requests, 0);
    QTest::mouseDClick(window.get(), Qt::LeftButton, Qt::NoModifier, {80, 40});
    QCOMPARE(window->visibility(), QWindow::Windowed);
  }
  void dragRestoresMaximizedWindow() {
    window->showMaximized();
    QTest::qWait(20);
    drag();
    QCOMPARE(window->visibility(), QWindow::Windowed);
    QCOMPARE(window->requests, 1);
    QVERIFY(window->width() >= 640);
    QVERIFY(window->height() >= 480);
  }
};
QTEST_MAIN(WindowDragTests)
#include "window_drag_tests.moc"
