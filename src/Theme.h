#pragma once

#include <QColor>

class QApplication;

namespace Theme {

inline const QColor accent{0xe1, 0xa3, 0x4f};        // #e1a34f
inline const QColor accentHover{0xec, 0xb8, 0x70};
inline const QColor accentPressed{0xc4, 0x88, 0x38};
inline const QColor onAccent{0x1a, 0x1a, 0x1a};      // text/icons drawn on accent

inline const QColor base0{0x16, 0x16, 0x16};         // deepest: artwork well
inline const QColor base1{0x1d, 0x1d, 0x1d};         // window
inline const QColor base2{0x23, 0x23, 0x23};         // panels and lists
inline const QColor base3{0x2b, 0x2b, 0x2b};         // hover, inputs
inline const QColor base4{0x36, 0x36, 0x36};         // borders, grooves

inline const QColor text{0xdc, 0xdc, 0xdc};
inline const QColor textDim{0x8e, 0x8e, 0x8e};
inline const QColor textFaint{0x5c, 0x5c, 0x5c};

// Subtle warm tint used for selected rows.
inline QColor selection() { return QColor(0xe1, 0xa3, 0x4f, 38); }
inline QColor hover() { return QColor(255, 255, 255, 10); }

void apply(QApplication &app);

} // namespace Theme
