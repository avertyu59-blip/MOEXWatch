#include "chart_item.hpp"
#include <QDate>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QTest>
#include <QWheelEvent>
#include <limits>
class ChartTests final : public QObject {
    Q_OBJECT
    static QVariantList history(int n) {
        QVariantList out;
        for (int i = 0; i < n; ++i)
            out.push_back(QVariantMap{{"date", QDate(2025, 1, 1).addDays(i).toString(Qt::ISODate)},
                                      {"open", 100.0 + i},
                                      {"high", 103.0 + i},
                                      {"low", 98.0 + i},
                                      {"close", 102.0 + i},
                                      {"volume", 100.0},
                                      {"benchmark", 2000.0 + i * 4},
                                      {"rsi", 50.0}});
        return out;
    }
  private slots:
    void boundsZoomAndPan() {
        FinancialChart c;
        c.setWidth(800);
        c.setHeight(500);
        c.setCandles(history(200));
        QCOMPARE(c.visibleCount(), 66);
        QCOMPARE(c.visibleStart(), 134);
        c.panBars(-50);
        QCOMPARE(c.visibleStart(), 84);
        c.zoomAt(2, 12);
        QCOMPARE(c.visibleStart(), 84);
        QCOMPARE(c.visibleCount(), 33);
        c.zoomAt(.5, 12);
        QCOMPARE(c.visibleStart(), 84);
        QCOMPARE(c.visibleCount(), 66);
        c.panBars(-10000);
        QCOMPARE(c.visibleStart(), 0);
        c.panBars(10000);
        QCOMPARE(c.visibleStart(), 134);
        c.zoomAt(1e100, 400);
        QCOMPARE(c.visibleCount(), 10);
        c.zoomAt(1e-100, 400);
        QCOMPARE(c.visibleCount(), 200);
        QCOMPARE(c.visibleStart(), 0);
        c.setSessions(0);
        QCOMPARE(c.visibleCount(), 200);
        c.setSessions(20);
        QCOMPARE(c.visibleStart(), 180);
        c.resetView();
        QCOMPARE(c.visibleCount(), 20);
    }
    void anchorAtMiddle() {
        FinancialChart c;
        c.setWidth(800);
        c.setCandles(history(200));
        c.panBars(-50);
        double anchor = c.visibleStart() + c.visibleCount() * .5;
        c.zoomAt(2, 12 + (800 - 90) * .5);
        QVERIFY(std::abs(c.visibleStart() + c.visibleCount() * .5 - anchor) <= .5);
    }
    void filtersInvalidAndHover() {
        auto rows = history(22);
        auto high = rows[20].toMap();
        high["volume"] = 201;
        rows[20] = high;
        rows.push_back(QVariantMap{{"open", 1}, {"high", 0}, {"low", 1}, {"close", 1}});
        FinancialChart c;
        c.setWidth(800);
        c.setCandles(rows);
        QCOMPARE(c.visibleCount(), 22);
        c.inspectAt(12 + (800 - 90) * (20.5 / 22));
        QCOMPARE(c.hovered()["date"].toString(), QString("2025-01-21"));
        QVERIFY(c.hovered()["volumeAnomaly"].toBool());
        c.inspectAt(-1);
        QVERIFY(c.hovered().isEmpty());
        c.zoomAt(std::numeric_limits<double>::quiet_NaN(), 200);
        QCOMPARE(c.visibleCount(), 22);
        c.setCandles({});
        QCOMPARE(c.visibleCount(), 0);
        c.inspectAt(100);
        QVERIFY(c.hovered().isEmpty());
    }
    void refreshPreservesView() {
        FinancialChart c;
        c.setCandles(history(200));
        c.panBars(-60);
        c.zoomAt(2, 12);
        int start = c.visibleStart(), count = c.visibleCount();
        auto rows = history(201);
        c.setCandles(rows);
        QCOMPARE(c.visibleStart(), start);
        QCOMPARE(c.visibleCount(), count);
        auto prefixed = history(202);
        QVariantMap first = prefixed.first().toMap();
        first["date"] = "2024-12-31";
        prefixed.prepend(first);
        c.setCandles(prefixed);
        QCOMPARE(c.visibleStart(), start + 1);
        QCOMPARE(c.visibleCount(), count);
    }
    void wheelDragAndHoverEvents() {
        FinancialChart c;
        c.setWidth(800);
        c.setHeight(500);
        c.setCandles(history(200));
        QWheelEvent wheel(QPointF(400, 200), QPointF(400, 200), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&c, &wheel);
        QVERIFY(wheel.isAccepted());
        QVERIFY(c.visibleCount() < 66);
        c.panBars(-50);
        int before = c.visibleStart();
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(400, 200), QPointF(400, 200), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&c, &press);
        QMouseEvent move(QEvent::MouseMove, QPointF(500, 200), QPointF(500, 200), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&c, &move);
        QVERIFY(c.visibleStart() < before);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(500, 200), QPointF(500, 200), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&c, &release);
        QVERIFY(!c.hovered().isEmpty());
    }
    void comparisonRequiresCommonDate() {
        FinancialChart c;
        c.setWidth(800);
        c.setHeight(500);
        auto rows = history(20);
        for (int i = 0; i < 5; ++i) {
            auto m = rows[i].toMap();
            m.remove("benchmark");
            rows[i] = m;
        }
        c.setCandles(rows);
        c.setComparison(true);
        c.inspectAt(12 + 710 * (2.5 / 20));
        QVERIFY(!c.hovered().contains("changePercent"));
        c.inspectAt(12 + 710 * (5.5 / 20));
        QCOMPARE(c.hovered()["changePercent"].toDouble(), 0.0);
        QCOMPARE(c.hovered()["benchmarkPercent"].toDouble(), 0.0);
        c.inspectAt(12 + 710 * (10.5 / 20));
        QVERIFY(c.hovered()["changePercent"].toDouble() > 0);
    }
    void paletteIsApplied() {
        FinancialChart c;
        c.setWidth(800);
        c.setHeight(500);
        c.setPalette({{"canvas", QColor("#123456")}});
        QImage image(800, 500, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&image);
        c.paint(&p);
        QCOMPARE(image.pixelColor(0, 0), QColor("#123456"));
    }
    void paintsDifferentModes() {
        FinancialChart c;
        c.setWidth(800);
        c.setHeight(500);
        c.setCandles(history(120));
        auto paint = [&] {
            QImage img(800, 500, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QPainter p(&img);
            c.paint(&p);
            return img;
        };
        const auto normal = paint();
        QVERIFY(normal.pixelColor(20, 20).alpha() > 0);
        int candlePixels = 0;
        for (int yy = 0; yy < normal.height(); ++yy)
            for (int xx = 0; xx < normal.width(); ++xx)
                if (normal.pixelColor(xx, yy) == QColor("#56c979"))
                    ++candlePixels;
        QVERIFY(candlePixels > 100);
        c.setComparison(true);
        const auto comparison = paint();
        QVERIFY(comparison != normal);
        c.setDark(false);
        QVERIFY(paint() != comparison);
        c.setComparison(false);
        c.setAveragePrice(80);
        c.setStopLoss(70);
        c.setTakeProfit(250);
        c.inspectAt(400);
        QVERIFY(paint() != normal);
        auto rows = history(10);
        for (auto &row : rows) {
            auto m = row.toMap();
            m.remove("benchmark");
            row = m;
        }
        c.setCandles(rows);
        c.setComparison(true);
        QVERIFY(!paint().isNull());
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    ChartTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "chart_item_tests.moc"
