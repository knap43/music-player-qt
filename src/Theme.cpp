#include "Theme.h"

#include "Icons.h"

#include <QApplication>
#include <QPalette>
#include <QProxyStyle>
#include <QStyleFactory>

namespace Theme {

namespace {

// Fusion, with the few round built-in glyphs swapped for square ones.
class SquareStyle : public QProxyStyle
{
public:
    SquareStyle()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {
    }

    QIcon standardIcon(StandardPixmap icon, const QStyleOption *option,
                       const QWidget *widget) const override
    {
        if (icon == SP_LineEditClearButton)
            return Icons::icon(Icons::Kind::Clear, textDim);
        return QProxyStyle::standardIcon(icon, option, widget);
    }
};

QString styleSheet()
{
    // No border-radius anywhere: every corner in the application is square.
    QString css = QStringLiteral(R"(
* { border-radius: 0px; }

QMainWindow, QDialog { background: @base1; }
QWidget { color: @text; }

QMenuBar { background: @base0; border-bottom: 1px solid @base3; padding: 2px 4px; }
QMenuBar::item { background: transparent; padding: 5px 10px; }
QMenuBar::item:selected { background: @base3; color: @accent; }
QMenuBar::item:pressed { background: @accent; color: @onAccent; }

QMenu { background: @base2; border: 1px solid @base4; padding: 4px 0px; }
QMenu::item { padding: 6px 28px 6px 28px; background: transparent; }
QMenu::item:selected { background: @accent; color: @onAccent; }
QMenu::item:disabled { color: @textFaint; }
QMenu::separator { height: 1px; background: @base4; margin: 4px 0px; }
QMenu::indicator { width: 9px; height: 9px; left: 10px; }
QMenu::indicator:non-exclusive:unchecked { border: 1px solid @textDim; background: transparent; }
QMenu::indicator:non-exclusive:checked { border: 1px solid @accent; background: @accent; }
QMenu::indicator:non-exclusive:checked:selected { border: 1px solid @onAccent; background: @onAccent; }

QToolTip { background: @base2; color: @text; border: 1px solid @accent; padding: 4px 6px; }

QStatusBar { background: @base0; color: @textDim; border-top: 1px solid @base3; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @textDim; padding: 0px 6px; }

QSplitter::handle { background: @base0; }

QWidget#panel { background: @base2; }
QWidget#nowPlaying { background: @base1; }
QLabel#panelTitle { color: @accent; font-weight: 700; letter-spacing: 2px; }
QLabel#panelInfo { color: @textDim; }
QLabel#trackTitle { color: @text; }
QLabel#trackSubtitle { color: @textDim; }
QLabel#timeLabel { color: @textDim; }
QLabel#emptyHint { color: @textDim; }

QTreeView, QListView { background: @base2; border: none; outline: 0; }

QLineEdit {
    background: @base1; border: 1px solid @base4; padding: 6px 8px;
    selection-background-color: @accent; selection-color: @onAccent;
}
QLineEdit:focus { border: 1px solid @accent; }

QPushButton {
    background: @base3; border: 1px solid @base4; padding: 6px 14px; color: @text;
}
QPushButton:hover { border: 1px solid @accent; color: @accent; }
QPushButton:pressed { background: @accent; color: @onAccent; }
QPushButton:disabled { color: @textFaint; border: 1px solid @base3; }
QPushButton#accentButton { background: @accent; border: 1px solid @accent; color: @onAccent; font-weight: 700; }
QPushButton#accentButton:hover { background: @accentHover; border: 1px solid @accentHover; }
QPushButton#accentButton:pressed { background: @accentPressed; border: 1px solid @accentPressed; }
QPushButton#accentButton:disabled { background: @base3; border: 1px solid @base3; color: @textFaint; }

QToolButton { background: transparent; border: none; padding: 6px; }
QToolButton:hover { background: @base3; }
QToolButton:pressed { background: @base4; }
QToolButton:checked { border-bottom: 2px solid @accent; }
QToolButton:disabled { background: transparent; }
QToolButton#playButton { background: @accent; border: none; }
QToolButton#playButton:hover { background: @accentHover; }
QToolButton#playButton:pressed { background: @accentPressed; }
QToolButton#textToggle { color: @textDim; font-weight: 700; letter-spacing: 1px; padding: 6px 10px; }
QToolButton#textToggle:checked { color: @accent; }
QToolButton#textToggle:disabled { color: @textFaint; }

QSlider { background: transparent; }
QSlider::groove:horizontal { height: 4px; background: @base4; border: none; }
QSlider::sub-page:horizontal { background: @accent; }
QSlider::add-page:horizontal { background: @base4; }
QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0px; background: @text; border: none; }
QSlider::handle:horizontal:hover { background: @accent; }
QSlider::sub-page:horizontal:disabled { background: @base4; }
QSlider::handle:horizontal:disabled { background: @base4; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }
QScrollBar::handle:vertical { background: @base4; min-height: 32px; }
QScrollBar::handle:vertical:hover, QScrollBar::handle:vertical:pressed { background: @accent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0px; }
QScrollBar::handle:horizontal { background: @base4; min-width: 32px; }
QScrollBar::handle:horizontal:hover, QScrollBar::handle:horizontal:pressed { background: @accent; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; border: none; background: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QProgressBar { background: @base3; border: none; max-height: 4px; min-width: 120px; }
QProgressBar::chunk { background: @accent; }

QMessageBox { background: @base1; }
)");

    const QList<QPair<QString, QColor>> tokens{
        {QStringLiteral("@accentHover"), accentHover},
        {QStringLiteral("@accentPressed"), accentPressed},
        {QStringLiteral("@accent"), accent},
        {QStringLiteral("@onAccent"), onAccent},
        {QStringLiteral("@base0"), base0},
        {QStringLiteral("@base1"), base1},
        {QStringLiteral("@base2"), base2},
        {QStringLiteral("@base3"), base3},
        {QStringLiteral("@base4"), base4},
        {QStringLiteral("@textDim"), textDim},
        {QStringLiteral("@textFaint"), textFaint},
        {QStringLiteral("@text"), text},
    };
    for (const auto &[token, color] : tokens)
        css.replace(token, color.name());
    return css;
}

} // namespace

void apply(QApplication &app)
{
    app.setStyle(new SquareStyle);

    QPalette p;
    p.setColor(QPalette::Window, base1);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base2);
    p.setColor(QPalette::AlternateBase, base3);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::PlaceholderText, textFaint);
    p.setColor(QPalette::Button, base3);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, accent);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, onAccent);
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::ToolTipBase, base2);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Light, base4);
    p.setColor(QPalette::Midlight, base3);
    p.setColor(QPalette::Mid, base2);
    p.setColor(QPalette::Dark, base0);
    p.setColor(QPalette::Shadow, Qt::black);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, textFaint);
    app.setPalette(p);

    app.setStyleSheet(styleSheet());
}

} // namespace Theme
