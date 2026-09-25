#pragma once

#include <QColor>
#include <QIcon>

class QPainter;
class QRectF;

// Icons are drawn with QPainter so they look identical on every desktop and
// follow the palette, instead of depending on the user's icon theme.
namespace Icons {

enum class Kind {
    Play,
    Pause,
    Next,
    Previous,
    Shuffle,
    Repeat,
    RepeatOne,
    Volume,
    Muted,
    Note,
    Search,
    Clear,
};

// `normal` is used for the off state, `on` (if valid) for checked buttons.
QIcon icon(Kind kind, const QColor &normal, const QColor &on = QColor());

// Paints a glyph into `rect` (a 24x24 design grid scaled to fit).
void paint(QPainter &painter, Kind kind, const QRectF &rect, const QColor &color);

QIcon appIcon();

} // namespace Icons
