#include "chart_item.hpp"
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
namespace {
double number(const QVariantMap &m, const char *key) {
    bool ok = false;
    double v = m.value(key).toDouble(&ok);
    return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}
} // namespace
FinancialChart::FinancialChart(QQuickItem *p) : QQuickPaintedItem(p) {
    setAcceptedMouseButtons(Qt::LeftButton);
    setActiveFocusOnTab(true);
    setAcceptHoverEvents(true);
    setAntialiasing(true);
}
QRectF FinancialChart::plot() const {
    return QRectF(12, 16, std::max(1.0, width() - 90), std::max(1.0, height() - 46));
}
void FinancialChart::setCandles(const QVariantList &v) {
    if (source_ == v)
        return;
    const bool hadData = !bars_.isEmpty();
    const int oldCount = count_;
    const QString anchor = hadData && count_ ? bars_[start_].values.value("date").toString() : QString{};
    source_ = v;
    bars_.clear();
    double sum = 0;
    for (const auto &entry : v) {
        auto m = entry.toMap();
        const double o = number(m, "open"), h = number(m, "high"), l = number(m, "low"),
                     c = number(m, "close");
        if (!(o > 0 && h > 0 && l > 0 && c > 0 && l <= std::min(o, c) && h >= std::max(o, c)))
            continue;
        double vol = number(m, "volume");
        if (!std::isfinite(vol) || vol < 0)
            vol = 0;
        const int n = bars_.size();
        double mean = n >= 20 ? sum / 20 : std::numeric_limits<double>::quiet_NaN();
        bars_.push_back({m, o, h, l, c, vol, number(m, "benchmark"), mean});
        sum += vol;
        if (n >= 20)
            sum -= bars_[n - 20].v;
    }
    clearHover();
    if (hadData) {
        int first = -1;
        for (int i = 0; i < bars_.size(); ++i)
            if (bars_[i].values.value("date").toString() == anchor) {
                first = i;
                break;
            }
        setView(first >= 0 ? first : start_, std::min(oldCount, int(bars_.size())));
        update();
    } else
        resetView();
    emit candlesChanged();
}
void FinancialChart::setPalette(const QVariantMap &v) {
    if (palette_ == v)
        return;
    palette_ = v;
    update();
    emit settingsChanged();
}
void FinancialChart::setSessions(int v) {
    v = std::max(0, v);
    if (sessions_ == v)
        return;
    sessions_ = v;
    resetView();
    emit settingsChanged();
}
#define FLAG_SETTER(Method, field)                                                                           \
    void FinancialChart::Method(bool v) {                                                                    \
        if (field == v)                                                                                      \
            return;                                                                                          \
        field = v;                                                                                           \
        clearHover();                                                                                        \
        update();                                                                                            \
        emit settingsChanged();                                                                              \
    }
FLAG_SETTER(setComparison, comparison_)
FLAG_SETTER(setDark, dark_) FLAG_SETTER(setShowVolume, volume_) FLAG_SETTER(setShowRsi, rsi_)
#define LEVEL_SETTER(Method, field)                                                                          \
    void FinancialChart::Method(double v) {                                                                  \
        if (!std::isfinite(v) || v < 0)                                                                      \
            v = 0;                                                                                           \
        if (field == v)                                                                                      \
            return;                                                                                          \
        field = v;                                                                                           \
        update();                                                                                            \
        emit settingsChanged();                                                                              \
    }
    LEVEL_SETTER(setAveragePrice, average_) LEVEL_SETTER(setStopLoss, stop_)
        LEVEL_SETTER(setTakeProfit, take_) void FinancialChart::setView(int first, int count) {
    int n = bars_.size();
    count = std::clamp(count, 0, n);
    first = std::clamp(first, 0, n - count);
    if (start_ == first && count_ == count)
        return;
    start_ = first;
    count_ = count;
    clearHover();
    update();
    emit viewChanged();
}
void FinancialChart::resetView() {
    int n = bars_.size();
    int count = sessions_ ? std::min(sessions_, n) : n;
    setView(n - count, count);
    update();
}
void FinancialChart::zoomAt(double factor, double x) {
    if (count_ == 0 || !std::isfinite(factor) || factor <= 0 || !std::isfinite(x))
        return;
    const auto p = plot();
    double t = std::clamp((x - p.left()) / p.width(), 0.0, 1.0);
    int minimum = std::min(10, int(bars_.size()));
    double requested = count_ / factor;
    int count = int(std::round(std::clamp(requested, double(minimum), double(bars_.size()))));
    int first = int(std::round(start_ + t * count_ - t * count));
    setView(first, count);
}
void FinancialChart::panBars(int delta) {
    const qint64 first = qint64(start_) + delta;
    setView(int(std::clamp(first, qint64(0), qint64(bars_.size() - count_))), count_);
}
void FinancialChart::inspectAt(double x) {
    if (!count_ || !std::isfinite(x) || x < plot().left() || x > plot().right()) {
        clearHover();
        return;
    }
    int i = start_ + std::clamp(int((x - plot().left()) / plot().width() * count_), 0, count_ - 1);
    if (i == hoverIndex_)
        return;
    hoverIndex_ = i;
    hovered_ = bars_[i].values;
    if (comparison_) {
        int base = -1;
        for (int j = start_; j < start_ + count_; ++j)
            if (bars_[j].benchmark > 0) {
                base = j;
                break;
            }
        if (base >= 0 && i >= base && bars_[i].benchmark > 0) {
            hovered_["changePercent"] = 100 * (bars_[i].c / bars_[base].c - 1);
            hovered_["benchmarkPercent"] = 100 * (bars_[i].benchmark / bars_[base].benchmark - 1);
        }
    }
    hovered_["volumeAnomaly"] = std::isfinite(bars_[i].volumeMean) && bars_[i].v > 2 * bars_[i].volumeMean;
    emit hoveredChanged();
    update();
}
void FinancialChart::clearHover() {
    if (hoverIndex_ < 0 && hovered_.isEmpty())
        return;
    hoverIndex_ = -1;
    hovered_.clear();
    emit hoveredChanged();
    update();
}
void FinancialChart::wheelEvent(QWheelEvent *e) {
    zoomAt(std::pow(1.2, e->angleDelta().y() / 120.0), e->position().x());
    e->accept();
}
void FinancialChart::mousePressEvent(QMouseEvent *e) {
    forceActiveFocus(Qt::MouseFocusReason);
    dragging_ = true;
    dragX_ = e->position().x();
    dragStart_ = start_;
    e->accept();
}
void FinancialChart::mouseMoveEvent(QMouseEvent *e) {
    if (dragging_ && count_) {
        setView(dragStart_ - int(std::round((e->position().x() - dragX_) / plot().width() * count_)), count_);
    } else
        inspectAt(e->position().x());
    e->accept();
}
void FinancialChart::mouseReleaseEvent(QMouseEvent *e) {
    dragging_ = false;
    inspectAt(e->position().x());
    e->accept();
}
void FinancialChart::hoverMoveEvent(QHoverEvent *e) {
    if (!dragging_)
        inspectAt(e->position().x());
}
void FinancialChart::hoverLeaveEvent(QHoverEvent *) {
    clearHover();
}
void FinancialChart::paint(QPainter *p) {
    auto color = [&](const char *key, const char *darkValue, const char *lightValue) {
        QColor value = palette_.value(key).value<QColor>();
        return value.isValid() ? value : QColor(dark_ ? darkValue : lightValue);
    };
    const QColor bg = color("canvas", "#0b1219", "#f2f5f9"), fg = color("muted", "#acb9c7", "#4f6074"),
                 grid = color("border", "#314151", "#c5cfdb");
    const QColor up = color("positive", "#56c979", "#147741"), down = color("negative", "#ff6975", "#bc2a3b"),
                 blue = color("accent", "#3e91f5", "#1762c0"), amber = color("warning", "#f2af49", "#9b5c00"),
                 purple = color("purple", "#ad80ec", "#7844b8");
    p->fillRect(boundingRect(), bg);
    p->setPen(fg);
    QFont font = p->font();
    font.setPixelSize(11);
    p->setFont(font);
    if (!count_) {
        p->drawText(boundingRect(), Qt::AlignCenter, QStringLiteral("Нет данных для графика"));
        return;
    }
    QRectF whole = plot();
    double priceFraction = 1 - (volume_ ? 0.18 : 0) - (rsi_ ? 0.21 : 0);
    QRectF price(whole.left(), whole.top(), whole.width(), whole.height() * priceFraction);
    QRectF vol(whole.left(), price.bottom() + 12, whole.width(), whole.height() * 0.18 - 12);
    QRectF rs(whole.left(), volume_ ? vol.bottom() + 12 : price.bottom() + 12, whole.width(),
              whole.height() * 0.21 - 12);
    double low = std::numeric_limits<double>::max(), high = 0, maximumVolume = 1;
    int base = -1;
    if (comparison_)
        for (int i = start_; i < start_ + count_; ++i)
            if (bars_[i].benchmark > 0) {
                base = i;
                break;
            }
    for (int i = start_; i < start_ + count_; ++i) {
        const auto &b = bars_[i];
        maximumVolume = std::max(maximumVolume, b.v);
        if (!comparison_) {
            low = std::min(low, b.l);
            high = std::max(high, b.h);
        } else if (base >= 0 && b.benchmark > 0) {
            double a = 100 * (b.c / bars_[base].c - 1), z = 100 * (b.benchmark / bars_[base].benchmark - 1);
            low = std::min({low, a, z});
            high = std::max({high, a, z});
        }
    }
    if (comparison_ && base < 0) {
        p->drawText(price, Qt::AlignCenter, QStringLiteral("Нет совместных данных акции и IMOEX"));
        return;
    }
    if (!comparison_) {
        for (double level : {average_, stop_, take_})
            if (level > 0) {
                low = std::min(low, level);
                high = std::max(high, level);
            }
        for (int i = start_; i < start_ + count_; ++i)
            for (const char *key : {"sma20", "sma50", "sma200"}) {
                double v = number(bars_[i].values, key);
                if (v > 0) {
                    low = std::min(low, v);
                    high = std::max(high, v);
                }
            }
    }
    double margin = std::max((high - low) * 0.07, std::max(std::abs(high) * 0.001, 0.01));
    low -= margin;
    high += margin;
    auto y = [&](double value) { return price.bottom() - (value - low) / (high - low) * price.height(); };
    double step = whole.width() / count_;
    auto x = [&](int i) { return whole.left() + (i - start_ + 0.5) * step; };
    p->setPen(grid);
    for (int k = 0; k <= 4; ++k) {
        double yy = price.top() + k * price.height() / 4;
        p->drawLine(QPointF(price.left(), yy), QPointF(price.right(), yy));
        p->setPen(fg);
        p->drawText(QRectF(price.right() + 6, yy - 8, 70, 18),
                    QString::number(high - k * (high - low) / 4, 'f', 2) + (comparison_ ? "%" : ""));
        p->setPen(grid);
    }
    p->save();
    p->setClipRect(whole);
    auto line = [&](const char *key, QColor color, bool index) {
        QPainterPath path;
        bool begun = false;
        for (int i = start_; i < start_ + count_; ++i) {
            const auto &b = bars_[i];
            double value = index ? b.benchmark : number(b.values, key);
            if (comparison_) {
                if (base < 0 || !(b.benchmark > 0) || i < base) {
                    begun = false;
                    continue;
                }
                value = 100 * (value / (index ? bars_[base].benchmark : bars_[base].c) - 1);
            }
            if (!std::isfinite(value) || (!comparison_ && value <= 0)) {
                begun = false;
                continue;
            }
            QPointF point(x(i), y(value));
            if (!begun)
                path.moveTo(point);
            else
                path.lineTo(point);
            begun = true;
        }
        p->setPen(QPen(color, 1.5));
        p->drawPath(path);
    };
    if (comparison_) {
        line("close", blue, false);
        line("benchmark", amber, true);
    } else {
        for (int i = start_; i < start_ + count_; ++i) {
            const auto &b = bars_[i];
            QColor c = b.c >= b.o ? up : down;
            p->setPen(QPen(c, 1));
            p->drawLine(QPointF(x(i), y(b.h)), QPointF(x(i), y(b.l)));
            p->fillRect(QRectF(x(i) - std::max(1.0, step * 0.65) / 2, std::min(y(b.o), y(b.c)),
                               std::max(1.0, step * 0.65), std::max(1.0, std::abs(y(b.o) - y(b.c)))),
                        c);
        }
        line("sma20", blue, false);
        line("sma50", amber, false);
        line("sma200", purple, false);
        const double levels[] = {average_, stop_, take_};
        const QColor colors[] = {blue, down, up};
        const QString names[] = {QStringLiteral("Средняя"), QStringLiteral("Стоп"), QStringLiteral("Цель")};
        for (int j = 0; j < 3; ++j)
            if (levels[j] > 0) {
                p->setPen(QPen(colors[j], 1, Qt::DashLine));
                p->drawLine(QPointF(price.left(), y(levels[j])), QPointF(price.right(), y(levels[j])));
                p->drawText(QPointF(price.left() + 4, y(levels[j]) - 4),
                            names[j] + " " + QString::number(levels[j], 'f', 2));
            }
    }
    if (volume_) {
        for (int i = start_; i < start_ + count_; ++i) {
            const auto &b = bars_[i];
            bool unusual = std::isfinite(b.volumeMean) && b.v > 2 * b.volumeMean;
            double h = b.v / maximumVolume * vol.height();
            p->fillRect(QRectF(x(i) - step * .35, vol.bottom() - h, std::max(1.0, step * .7), h),
                        unusual ? amber : (b.c >= b.o ? up : down));
        }
        p->setPen(fg);
        p->drawText(QPointF(vol.left() + 4, vol.top() + 11),
                    QStringLiteral("Объём · жёлтый > 2× предыдущих 20 сессий"));
    }
    if (rsi_) {
        auto ry = [&](double v) { return rs.bottom() - v / 100 * rs.height(); };
        p->setPen(QPen(grid, 1, Qt::DashLine));
        for (int level : {30, 70}) {
            p->drawLine(QPointF(rs.left(), ry(level)), QPointF(rs.right(), ry(level)));
        }
        QPainterPath path;
        bool active = false;
        for (int i = start_; i < start_ + count_; ++i) {
            double value = number(bars_[i].values, "rsi");
            if (!std::isfinite(value) || value < 0 || value > 100) {
                active = false;
                continue;
            }
            if (!active)
                path.moveTo(x(i), ry(value));
            else
                path.lineTo(x(i), ry(value));
            active = true;
        }
        p->setPen(QPen(purple, 1.5));
        p->drawPath(path);
        p->setPen(fg);
        p->drawText(QPointF(rs.left() + 4, rs.top() + 11), QStringLiteral("RSI(14) · 30 / 70"));
    }
    if (hoverIndex_ >= start_ && hoverIndex_ < start_ + count_ &&
        (!comparison_ || (base >= 0 && hoverIndex_ >= base && bars_[hoverIndex_].benchmark > 0))) {
        p->setPen(QPen(fg, 1, Qt::DashLine));
        p->drawLine(QPointF(x(hoverIndex_), whole.top()), QPointF(x(hoverIndex_), whole.bottom()));
        p->drawLine(QPointF(price.left(), y(comparison_ ? 100 * (bars_[hoverIndex_].c / bars_[base].c - 1)
                                                        : bars_[hoverIndex_].c)),
                    QPointF(price.right(), y(comparison_ ? 100 * (bars_[hoverIndex_].c / bars_[base].c - 1)
                                                         : bars_[hoverIndex_].c)));
    }
    p->restore();
    p->setPen(fg);
    int labels = std::min(4, count_);
    for (int j = 0; j < labels; ++j) {
        int i = start_ + (labels == 1 ? 0 : j * (count_ - 1) / (labels - 1));
        QString date = bars_[i].values.value("date").toString();
        double xx = std::clamp(x(i) - 35, whole.left(), whole.right() - 70);
        p->drawText(QRectF(xx, whole.bottom() + 7, 76, 20), Qt::AlignCenter, date);
    }
    p->drawText(QPointF(price.left() + 4, price.top() + 12),
                comparison_ ? QStringLiteral("Акция / IMOEX · изменение от первой общей даты")
                            : QStringLiteral("Свечи · SMA 20 / 50 / 200"));
}
