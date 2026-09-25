#include "LibraryView.h"

#include "Icons.h"
#include "Theme.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QHeaderView>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QUrl>

const char *const LibraryView::PathsMimeType = "application/x-amber-track-paths";

namespace {

constexpr int kAlbumRowHeight = 68;
constexpr int kTrackRowHeight = 30;
constexpr int kThumb = 52;
constexpr int kPad = 8;
constexpr int kTextLeft = kPad + kThumb + 12;
constexpr int kChevronWidth = 30;

enum Kind { AlbumItem = 0, TrackItem = 1 };

class LibraryDelegate : public QStyledItemDelegate
{
public:
    explicit LibraryDelegate(LibraryView *view)
        : QStyledItemDelegate(view)
        , m_view(view)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        const bool album = index.data(LibraryView::KindRole).toInt() == AlbumItem;
        return {200, album ? kAlbumRowHeight : kTrackRowHeight};
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing, false);
        const QRect r = option.rect;
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;

        if (selected)
            p->fillRect(r, Theme::selection());
        else if (hovered)
            p->fillRect(r, Theme::hover());

        if (index.data(LibraryView::KindRole).toInt() == AlbumItem)
            paintAlbum(p, option, index, selected);
        else
            paintTrack(p, option, index, selected);
        p->restore();
    }

private:
    void paintAlbum(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index,
                    bool selected) const
    {
        const Album *album = m_view->albumAt(index);
        if (!album)
            return;
        const QRect r = option.rect;

        if (selected)
            p->fillRect(QRect(r.left(), r.top(), 3, r.height()), Theme::accent);

        const QRect thumb(r.left() + kPad, r.top() + (r.height() - kThumb) / 2, kThumb, kThumb);
        const QPixmap pm = index.data(Qt::DecorationRole).value<QPixmap>();
        if (!pm.isNull()) {
            p->setRenderHint(QPainter::SmoothPixmapTransform);
            p->drawPixmap(thumb, pm);
        } else {
            p->fillRect(thumb, Theme::base3);
            Icons::paint(*p, Icons::Kind::Note, QRectF(thumb).adjusted(14, 14, -14, -14), Theme::base4);
        }

        const int textRight = r.right() - kChevronWidth;
        const int textWidth = textRight - (r.left() + kTextLeft);
        QFont titleFont = option.font;
        titleFont.setBold(true);
        QFont smallFont = option.font;
        smallFont.setPointSizeF(option.font.pointSizeF() * 0.88);

        const QFontMetrics titleFm(titleFont);
        const QFontMetrics fm(option.font);
        const QFontMetrics smallFm(smallFont);
        const int blockHeight = titleFm.height() + fm.height() + smallFm.height() + 2;
        int y = r.top() + (r.height() - blockHeight) / 2;

        p->setFont(titleFont);
        p->setPen(selected ? Theme::accent : Theme::text);
        p->drawText(QRect(r.left() + kTextLeft, y, textWidth, titleFm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    titleFm.elidedText(album->title, Qt::ElideRight, textWidth));
        y += titleFm.height() + 1;

        p->setFont(option.font);
        p->setPen(Theme::textDim);
        p->drawText(QRect(r.left() + kTextLeft, y, textWidth, fm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(album->location, Qt::ElideRight, textWidth));
        y += fm.height() + 1;

        QStringList meta;
        meta << (album->tracks.size() == 1
                     ? LibraryView::tr("1 track")
                     : LibraryView::tr("%1 tracks").arg(album->tracks.size()));
        meta << Format::duration(album->durationMs());
        p->setFont(smallFont);
        p->setPen(Theme::textFaint);
        p->drawText(QRect(r.left() + kTextLeft, y, textWidth, smallFm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    smallFm.elidedText(meta.join(QStringLiteral("  ·  ")), Qt::ElideRight, textWidth));

        // Expand/collapse chevron.
        const bool expanded = m_view->isExpanded(index);
        const QPointF c(r.right() - kChevronWidth / 2.0, r.center().y() + 0.5);
        QPolygonF arrow;
        if (expanded)
            arrow << QPointF(c.x() - 5, c.y() - 2.5) << QPointF(c.x() + 5, c.y() - 2.5) << QPointF(c.x(), c.y() + 3.5);
        else
            arrow << QPointF(c.x() - 2.5, c.y() - 5) << QPointF(c.x() + 3.5, c.y()) << QPointF(c.x() - 2.5, c.y() + 5);
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen);
        p->setBrush(expanded ? Theme::accent : Theme::textDim);
        p->drawPolygon(arrow);

        p->setPen(Theme::base1);
        p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
    }

    void paintTrack(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index,
                    bool selected) const
    {
        const Track *track = m_view->trackAt(index);
        if (!track)
            return;
        const QRect r = option.rect;
        p->fillRect(QRect(r.left(), r.top(), 3, r.height()),
                    selected ? Theme::accent : Theme::base1);

        const QFontMetrics fm(option.font);
        const QString duration = Format::duration(track->durationMs);
        const int durationWidth = fm.horizontalAdvance(duration);
        const int numberWidth = fm.horizontalAdvance(QStringLiteral("00")) + 4;
        const int numberLeft = r.left() + kTextLeft - numberWidth - 10;

        p->setFont(option.font);
        p->setPen(Theme::textFaint);
        // Position in the folder, which is the album's track order.
        p->drawText(QRect(numberLeft, r.top(), numberWidth, r.height()), Qt::AlignRight | Qt::AlignVCenter,
                    QStringLiteral("%1").arg(index.data(LibraryView::TrackIndexRole).toInt() + 1, 2, 10,
                                             QLatin1Char('0')));

        const int right = r.right() - kPad - 4;
        p->setPen(Theme::textDim);
        p->drawText(QRect(right - durationWidth, r.top(), durationWidth, r.height()),
                    Qt::AlignRight | Qt::AlignVCenter, duration);

        const int titleLeft = r.left() + kTextLeft;
        const int titleWidth = right - durationWidth - 12 - titleLeft;
        QString title = track->displayTitle();
        QString extra;
        if (!track->artist.isEmpty())
            extra = QStringLiteral("  ") + track->artist;

        const QString titleShown = fm.elidedText(title, Qt::ElideRight, titleWidth);
        p->setPen(selected ? Theme::accent : Theme::text);
        p->drawText(QRect(titleLeft, r.top(), titleWidth, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    titleShown);
        const int used = fm.horizontalAdvance(titleShown);
        if (!extra.isEmpty() && used < titleWidth - 20) {
            p->setPen(Theme::textFaint);
            p->drawText(QRect(titleLeft + used, r.top(), titleWidth - used, r.height()),
                        Qt::AlignLeft | Qt::AlignVCenter,
                        fm.elidedText(extra, Qt::ElideRight, titleWidth - used));
        }
    }

    LibraryView *m_view;
};

QString normalized(const QString &s)
{
    return s.toCaseFolded();
}

} // namespace

LibraryView::LibraryView(QWidget *parent)
    : QTreeWidget(parent)
{
    setColumnCount(1);
    header()->hide();
    setRootIsDecorated(false);
    setIndentation(0);
    setUniformRowHeights(false);
    setAnimated(false);
    setExpandsOnDoubleClick(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    setMouseTracking(true);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setItemDelegate(new LibraryDelegate(this));
}

void LibraryView::setAlbums(const QVector<Album> &albums)
{
    // Remember which albums were open so a rescan does not collapse them.
    QSet<QString> expanded;
    for (auto it = m_itemByKey.cbegin(); it != m_itemByKey.cend(); ++it)
        if (it.value()->isExpanded() && !m_expandedByFilter.contains(it.value()))
            expanded.insert(it.key());

    clear();
    m_itemByKey.clear();
    m_expandedByFilter.clear();
    m_albums = albums;

    QList<QTreeWidgetItem *> items;
    items.reserve(m_albums.size());
    for (int a = 0; a < m_albums.size(); ++a) {
        const Album &album = m_albums[a];
        auto *albumItem = new QTreeWidgetItem;
        albumItem->setData(0, Qt::DisplayRole, album.title);
        albumItem->setData(0, KindRole, AlbumItem);
        albumItem->setData(0, AlbumIndexRole, a);
        albumItem->setToolTip(0, QDir::toNativeSeparators(album.key));
        for (int t = 0; t < album.tracks.size(); ++t) {
            auto *trackItem = new QTreeWidgetItem(albumItem);
            trackItem->setData(0, Qt::DisplayRole, album.tracks[t].displayTitle());
            trackItem->setData(0, KindRole, TrackItem);
            trackItem->setData(0, AlbumIndexRole, a);
            trackItem->setData(0, TrackIndexRole, t);
            trackItem->setToolTip(0, album.tracks[t].path);
        }
        m_itemByKey.insert(album.key, albumItem);
        items << albumItem;
    }
    addTopLevelItems(items);
    for (const QString &key : std::as_const(expanded))
        if (QTreeWidgetItem *item = m_itemByKey.value(key))
            item->setExpanded(true);
}

void LibraryView::setThumbnail(const QString &albumKey, const QImage &thumbnail)
{
    if (QTreeWidgetItem *item = m_itemByKey.value(albumKey))
        item->setData(0, Qt::DecorationRole, QPixmap::fromImage(thumbnail));
}

void LibraryView::setFilter(const QString &text)
{
    const QStringList tokens = normalized(text).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    auto matches = [&tokens](const QString &haystack) {
        for (const QString &token : tokens)
            if (!haystack.contains(token))
                return false;
        return true;
    };

    setUpdatesEnabled(false);
    for (int a = 0; a < topLevelItemCount(); ++a) {
        QTreeWidgetItem *albumItem = topLevelItem(a);
        const Album &album = m_albums[albumItem->data(0, AlbumIndexRole).toInt()];

        if (tokens.isEmpty()) {
            albumItem->setHidden(false);
            for (int t = 0; t < albumItem->childCount(); ++t)
                albumItem->child(t)->setHidden(false);
            if (m_expandedByFilter.contains(albumItem))
                albumItem->setExpanded(false);
            continue;
        }

        const QString albumText = normalized(album.title + QLatin1Char(' ') + album.location);
        const bool albumMatches = matches(albumText);
        bool anyTrack = false;
        for (int t = 0; t < albumItem->childCount(); ++t) {
            const Track &track = album.tracks[t];
            const bool trackMatches = albumMatches
                || matches(albumText + QLatin1Char(' ')
                           + normalized(track.displayTitle() + QLatin1Char(' ') + track.artist));
            albumItem->child(t)->setHidden(!trackMatches);
            anyTrack = anyTrack || trackMatches;
        }
        albumItem->setHidden(!anyTrack);
        if (anyTrack && !albumMatches && !albumItem->isExpanded()) {
            albumItem->setExpanded(true);
            m_expandedByFilter.insert(albumItem);
        } else if (albumMatches && m_expandedByFilter.contains(albumItem)) {
            albumItem->setExpanded(false);
            m_expandedByFilter.remove(albumItem);
        }
    }
    if (tokens.isEmpty())
        m_expandedByFilter.clear();
    setUpdatesEnabled(true);
}

const Album *LibraryView::albumAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    const int a = index.data(AlbumIndexRole).toInt();
    return a >= 0 && a < m_albums.size() ? &m_albums[a] : nullptr;
}

const Track *LibraryView::trackAt(const QModelIndex &index) const
{
    const Album *album = albumAt(index);
    if (!album || index.data(KindRole).toInt() != TrackItem)
        return nullptr;
    const int t = index.data(TrackIndexRole).toInt();
    return t >= 0 && t < album->tracks.size() ? &album->tracks[t] : nullptr;
}

QVector<Track> LibraryView::tracksFor(const QList<QTreeWidgetItem *> &items) const
{
    // Keep library order, and avoid duplicates when an album and some of its
    // tracks are selected together.
    QList<QTreeWidgetItem *> sorted = items;
    std::sort(sorted.begin(), sorted.end(), [this](QTreeWidgetItem *a, QTreeWidgetItem *b) {
        const QModelIndex ia = indexFromItem(a), ib = indexFromItem(b);
        const int albumA = ia.data(AlbumIndexRole).toInt(), albumB = ib.data(AlbumIndexRole).toInt();
        if (albumA != albumB)
            return albumA < albumB;
        const bool trackA = ia.data(KindRole).toInt() == TrackItem;
        const bool trackB = ib.data(KindRole).toInt() == TrackItem;
        if (trackA != trackB)
            return !trackA;
        return ia.data(TrackIndexRole).toInt() < ib.data(TrackIndexRole).toInt();
    });

    QVector<Track> tracks;
    QSet<QString> seen;
    for (QTreeWidgetItem *item : std::as_const(sorted)) {
        const QModelIndex index = indexFromItem(item);
        const Album *album = albumAt(index);
        if (!album)
            continue;
        if (index.data(KindRole).toInt() == AlbumItem) {
            for (int t = 0; t < album->tracks.size(); ++t) {
                // With a search active, only the visible tracks are meant.
                if (item->child(t) && item->child(t)->isHidden())
                    continue;
                if (!seen.contains(album->tracks[t].path)) {
                    seen.insert(album->tracks[t].path);
                    tracks << album->tracks[t];
                }
            }
        } else if (const Track *track = trackAt(index)) {
            if (!seen.contains(track->path)) {
                seen.insert(track->path);
                tracks << *track;
            }
        }
    }
    return tracks;
}

QVector<Track> LibraryView::selectedTracks() const
{
    return tracksFor(selectedItems());
}

QStringList LibraryView::mimeTypes() const
{
    return {QString::fromLatin1(PathsMimeType), QStringLiteral("text/uri-list")};
}

QMimeData *LibraryView::mimeData(const QList<QTreeWidgetItem *> &items) const
{
    const QVector<Track> tracks = tracksFor(items);
    QStringList paths;
    QList<QUrl> urls;
    for (const Track &t : tracks) {
        paths << t.path;
        urls << QUrl::fromLocalFile(t.path);
    }
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(PathsMimeType), paths.join(QLatin1Char('\n')).toUtf8());
    mime->setUrls(urls);
    return mime;
}

bool LibraryView::isOnChevron(const QModelIndex &index, const QPoint &pos) const
{
    if (!index.isValid() || index.data(KindRole).toInt() != AlbumItem)
        return false;
    return pos.x() >= visualRect(index).right() - kChevronWidth - 4;
}

void LibraryView::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && isOnChevron(index, event->position().toPoint())
        && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
        setExpanded(index, !isExpanded(index));
        event->accept();
        return;
    }
    QTreeWidget::mousePressEvent(event);
}

void LibraryView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (event->button() != Qt::LeftButton || !index.isValid()) {
        QTreeWidget::mouseDoubleClickEvent(event);
        return;
    }
    if (isOnChevron(index, event->position().toPoint())) {
        // Treat a fast double click on the chevron as two toggles.
        setExpanded(index, !isExpanded(index));
        event->accept();
        return;
    }
    const QVector<Track> tracks = tracksFor({itemFromIndex(index)});
    if (!tracks.isEmpty())
        emit tracksRequested(tracks, Action::Append);
    event->accept();
}

void LibraryView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const QVector<Track> tracks = selectedTracks();
        if (!tracks.isEmpty()) {
            const bool now = event->modifiers() & Qt::ControlModifier;
            emit tracksRequested(tracks, now ? Action::PlayNow : Action::Append);
        }
        event->accept();
        return;
    }
    QTreeWidget::keyPressEvent(event);
}

void LibraryView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid() && !selectionModel()->isSelected(index))
        setCurrentIndex(index);
    const QVector<Track> tracks = selectedTracks();

    QMenu menu(this);
    auto add = [&](const QString &text, Action action) {
        QAction *a = menu.addAction(text, this, [this, tracks, action] { emit tracksRequested(tracks, action); });
        a->setEnabled(!tracks.isEmpty());
    };
    add(tr("Play Now"), Action::PlayNow);
    add(tr("Play Next"), Action::PlayNext);
    add(tr("Add to Queue"), Action::Append);
    add(tr("Replace Queue"), Action::Replace);
    menu.addSeparator();
    menu.addAction(tr("Expand All"), this, &QTreeView::expandAll);
    menu.addAction(tr("Collapse All"), this, [this] {
        collapseAll();
        m_expandedByFilter.clear();
    });
    menu.exec(event->globalPos());
}
