#include "window_geometry.hpp"
#include <QTest>
class WindowGeometryTests:public QObject {
 Q_OBJECT
private slots:
 void nativeTitleRemainsVisible(){QRect screen(0,0,1920,1080);QMargins frame(8,31,8,8);auto r=restoreWindowGeometry({0,0,1920,1080},screen,frame);QVERIFY(screen.contains(r.marginsAdded(frame)));QCOMPARE(r.top(),31);}
 void disconnectedMonitor(){QRect screen(-1920,0,1920,1080);QMargins frame(8,31,8,8);auto r=restoreWindowGeometry({4000,-2000,1440,920},screen,frame);QVERIFY(screen.contains(r.marginsAdded(frame)));}
 void corruptDimensions(){auto r=restoreWindowGeometry({-99999,-99999,-10,100000},{0,0,1366,768},{8,31,8,8});QCOMPARE(r.width(),640);QCOMPARE(r.height(),729);QCOMPARE(r.top(),31);}
};
QTEST_GUILESS_MAIN(WindowGeometryTests)
#include "window_geometry_tests.moc"
