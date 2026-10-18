#pragma once
#include <QMargins>
#include <QRect>
#include <algorithm>
// QWindow geometry describes the client area; keep its native frame on screen too.
inline QRect restoreWindowGeometry(QRect desired, QRect available, QMargins frame) {
    const QRect client = available.adjusted(frame.left(), frame.top(), -frame.right(), -frame.bottom());
    const int width = std::clamp(desired.width(), 640, std::max(640, client.width()));
    const int height = std::clamp(desired.height(), 480, std::max(480, client.height()));
    return {std::clamp(desired.x(), client.left(), std::max(client.left(), client.right()-width+1)),
            std::clamp(desired.y(), client.top(), std::max(client.top(), client.bottom()-height+1)), width, height};
}
