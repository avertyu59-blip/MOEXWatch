#pragma once
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
class FinancialChart : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantList candles READ candles WRITE setCandles NOTIFY candlesChanged)
    Q_PROPERTY(QVariantMap palette READ palette WRITE setPalette NOTIFY settingsChanged)
    Q_PROPERTY(int sessions READ sessions WRITE setSessions NOTIFY settingsChanged)
    Q_PROPERTY(bool comparison READ comparison WRITE setComparison NOTIFY settingsChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY settingsChanged)
    Q_PROPERTY(bool showVolume READ showVolume WRITE setShowVolume NOTIFY settingsChanged)
    Q_PROPERTY(bool showRsi READ showRsi WRITE setShowRsi NOTIFY settingsChanged)
    Q_PROPERTY(double averagePrice READ averagePrice WRITE setAveragePrice NOTIFY settingsChanged)
    Q_PROPERTY(double stopLoss READ stopLoss WRITE setStopLoss NOTIFY settingsChanged)
    Q_PROPERTY(double takeProfit READ takeProfit WRITE setTakeProfit NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap hovered READ hovered NOTIFY hoveredChanged)
    Q_PROPERTY(int visibleStart READ visibleStart NOTIFY viewChanged)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY viewChanged)
  public:
    explicit FinancialChart(QQuickItem *parent = nullptr);
    QVariantList candles() const { return source_; }
    void setCandles(const QVariantList &);
    QVariantMap palette() const { return palette_; }
    void setPalette(const QVariantMap &);
    int sessions() const { return sessions_; }
    void setSessions(int);
    bool comparison() const { return comparison_; }
    void setComparison(bool);
    bool dark() const { return dark_; }
    void setDark(bool);
    bool showVolume() const { return volume_; }
    void setShowVolume(bool);
    bool showRsi() const { return rsi_; }
    void setShowRsi(bool);
    double averagePrice() const { return average_; }
    void setAveragePrice(double);
    double stopLoss() const { return stop_; }
    void setStopLoss(double);
    double takeProfit() const { return take_; }
    void setTakeProfit(double);
    QVariantMap hovered() const { return hovered_; }
    int visibleStart() const { return start_; }
    int visibleCount() const { return count_; }
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void zoomAt(double factor, double x);
    Q_INVOKABLE void panBars(int delta);
    Q_INVOKABLE void inspectAt(double x);
    Q_INVOKABLE void clearHover();
    void paint(QPainter *) override;
  signals:
    void candlesChanged();
    void settingsChanged();
    void hoveredChanged();
    void viewChanged();

  protected:
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void hoverMoveEvent(QHoverEvent *) override;
    void hoverLeaveEvent(QHoverEvent *) override;

  private:
    struct Bar {
        QVariantMap values;
        double o, h, l, c, v, benchmark, volumeMean;
    };
    QVariantList source_;
    QVector<Bar> bars_;
    QVariantMap hovered_, palette_;
    int sessions_ = 66, start_ = 0, count_ = 0, hoverIndex_ = -1;
    bool comparison_ = false, dark_ = true, volume_ = true, rsi_ = true, dragging_ = false;
    double average_ = 0, stop_ = 0, take_ = 0, dragX_ = 0;
    int dragStart_ = 0;
    QRectF plot() const;
    void setView(int, int);
};
