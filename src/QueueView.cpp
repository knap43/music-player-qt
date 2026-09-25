#include "QueueView.h"

#include "Icons.h"
#include "LibraryView.h"
#include "PlayQueue.h"
#include "Theme.h"

#include <QContextMenuEvent>
#include <QDataStream>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>

namespace {

constexpr int kRowHeight = 46;

class QueueDelegate : public QStyledItemDelegate
{
public:
    explicit QueueDelegate(QueueView *view)
        : QStyledItemDelegate(view)
        , m_view(view)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return {200, kRowHeight};
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        p->save();
        const QRect r = option.rect;
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool current = index.data(PlayQueue::IsCurrentRole).toBool();
        const Track track = index.data(PlayQueue::TrackRole).value<Track>();

        if (selected)
            p->fillRect(r, Theme::selection());
        else if (hovered)
            p->fillRect(r, Theme::hover());
        if (current)
            p->fillRect(QRect(r.left(), r.top(), 3, r.height()), Theme::accent);

        const QFontMetrics fm(option.font);
        QFont small = option.font;
        small.setPointSizeF(option.font.pointSizeF() * 0.9);
        const QFontMetrics smallFm(small);

        // Leading column: row number, or a play/pause glyph for the current row.
        const QRect lead(r.left() + 6, r.top(), 34, r.height());
        if (current) {
            const QRectF glyph(lead.center().x() - 8, lead.center().y() - 8, 16, 16);
            Icons::paint(*p, m_view->isPlaying() ? Icons::Kind::Play : Icons::Kind::Pause, glyph,
                         Theme::accent);
        } else {
            p->setFont(small);
            p->setPen(Theme::textFaint);
            p->drawText(lead, Qt::AlignCenter, QString::number(index.row() + 1));
        }

        const QString duration = Format::duration(track.durationMs);
        const int durationWidth = fm.horizontalAdvance(duration);
        const int right = r.right() - 12;
        p->setFont(option.font);
        p->setPen(Theme::textDim);
        p->drawText(QRect(right - durationWidth, r.top(), durationWidth, r.height()),
                    Qt::AlignRight | Qt::AlignVCenter, duration);

        const int left = lead.right() + 8;
        const int width = right - durationWidth - 14 - left;
        const int blockHeight = fm.height() + smallFm.height() + 2;
        const int top = r.top() + (r.height() - blockHeight) / 2;

        QFont titleFont = option.font;
        titleFont.setBold(current);
        const QFontMetrics titleFm(titleFont);
        p->setFont(titleFont);
        p->setPen(current ? Theme::accent : Theme::text);
        p->drawText(QRect(left, top, width, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    titleFm.elidedText(track.displayTitle(), Qt::ElideRight, width));

        p->setFont(small);
        p->setPen(Theme::textDim);
        const QString subtitle = track.displayArtist() + QStringLiteral("  ·  ") + track.displayAlbum();
        p->drawText(QRect(left, top + fm.height() + 2, width, smallFm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    smallFm.elidedText(subtitle, Qt::ElideRight, width));
        p->restore();
    }

private:
    QueueView *m_view;
};

} // namespace

QueueView::QueueView(PlayQueue *queue, QWidget *parent)
    : QListView(parent)
    , m_queue(queue)
{
    setModel(queue);
    setItemDelegate(new QueueDelegate(this));
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::CopyAction);
    setDragDropOverwriteMode(false);
    setMouseTracking(true);
    setUniformItemSizes(true);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    connect(this, &QListView::activated, this, [this](const QModelIndex &index) {
        emit rowActivated(index.row());
    });
}

void QueueView::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    viewport()->update();
}

QList<int> QueueView::selectedRows() const
{
    QList<int> rows;
    for (const QModelIndex &index : selectionModel()->selectedIndexes())
        rows << index.row();
    std::sort(rows.begin(), rows.end());
    return rows;
}

void QueueView::removeSelected()
{
    const QList<int> rows = selectedRows();
    if (rows.isEmpty())
        return;
    const int first = rows.first();
    m_queue->removeRowsList(rows);
    if (m_queue->count() > 0)
        setCurrentIndex(model()->index(qMin(first, m_queue->count() - 1), 0));
}

int QueueView::dropRow(const QPoint &pos) const
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid())
        return m_queue->count();
    return pos.y() > visualRect(index).center().y() ? index.row() + 1 : index.row();
}

void QueueView::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    const int row = dropRow(event->position().toPoint());
    const QString rowsType = QString::fromLatin1(PlayQueue::RowsMimeType);
    const QString pathsType = QString::fromLatin1(LibraryView::PathsMimeType);

    bool handled = true;
    if (event->source() == this && mime->hasFormat(rowsType)) {
        QList<int> rows;
        QDataStream stream(mime->data(rowsType));
        stream >> rows;
        const int first = m_queue->moveRowsTo(rows, row);
        if (first >= 0) {
            selectionModel()->select(QItemSelection(model()->index(first, 0),
                                                    model()->index(first + int(rows.size()) - 1, 0)),
                                     QItemSelectionModel::ClearAndSelect);
        }
    } else if (mime->hasFormat(pathsType)) {
        const QString paths = QString::fromUtf8(mime->data(pathsType));
        emit trackPathsDropped(paths.split(QLatin1Char('\n'), Qt::SkipEmptyParts), row);
    } else if (mime->hasUrls()) {
        emit urlsDropped(mime->urls(), row);
    } else {
        handled = false;
    }

    // Always report a copy so that the drag source never deletes anything.
    if (handled) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        event->ignore();
    }
    stopAutoScroll();
    setState(NoState);
    viewport()->update();
}

void QueueView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if (m_queue->count() > 0)
        return;

    QPainter p(viewport());
    const QRect area = viewport()->rect().adjusted(32, 0, -32, 0);
    QFont title = font();
    title.setBold(true);
    title.setPointSizeF(title.pointSizeF() * 1.1);
    const int glyph = 40;
    const int top = area.center().y() - 70;
    Icons::paint(p, Icons::Kind::Note, QRectF(area.center().x() - glyph / 2.0, top, glyph, glyph),
                 Theme::base4);
    p.setFont(title);
    p.setPen(Theme::textDim);
    p.drawText(QRect(area.left(), top + glyph + 14, area.width(), 26), Qt::AlignHCenter,
               tr("The queue is empty"));
    p.setFont(font());
    p.setPen(Theme::textFaint);
    p.drawText(QRect(area.left(), top + glyph + 44, area.width(), 80),
               Qt::AlignHCenter | Qt::TextWordWrap,
               tr("Double-click an album or track in the library, drag it here, "
                  "or right-click it for more options."));
}

void QueueView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        removeSelected();
        event->accept();
        return;
    default:
        QListView::keyPressEvent(event);
    }
}

void QueueView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid() && !selectionModel()->isSelected(index))
        setCurrentIndex(index);

    QMenu menu(this);
    QAction *play = menu.addAction(tr("Play"), this, [this, index] { emit rowActivated(index.row()); });
    play->setEnabled(index.isValid());
    QAction *remove = menu.addAction(tr("Remove from Queue"), this, &QueueView::removeSelected);
    remove->setEnabled(!selectedRows().isEmpty());
    menu.addSeparator();
    QAction *clear = menu.addAction(tr("Clear Queue"), m_queue, &PlayQueue::clear);
    clear->setEnabled(m_queue->count() > 0);
    menu.exec(event->globalPos());
}
