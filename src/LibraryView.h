#pragma once

#include "Library.h"

#include <QHash>
#include <QSet>
#include <QTreeWidget>

// Albums with their tracks as expandable children. Albums and tracks can be
// sent to the queue by double-clicking, pressing Enter, the context menu, or
// dragging them onto the queue.
class LibraryView : public QTreeWidget
{
    Q_OBJECT
public:
    enum class Action { PlayNow, PlayNext, Append, Replace };
    Q_ENUM(Action)

    enum ItemRole {
        KindRole = Qt::UserRole + 1, // 0 = album, 1 = track
        AlbumIndexRole,
        TrackIndexRole,
    };

    static const char *const PathsMimeType;

    explicit LibraryView(QWidget *parent = nullptr);

    void setAlbums(const QVector<Album> &albums);
    void setThumbnail(const QString &albumKey, const QImage &thumbnail);
    void setFilter(const QString &text);

    const Album *albumAt(const QModelIndex &index) const;
    const Track *trackAt(const QModelIndex &index) const;
    QVector<Track> selectedTracks() const;
    bool isOnChevron(const QModelIndex &index, const QPoint &pos) const;

signals:
    void tracksRequested(const QVector<Track> &tracks, LibraryView::Action action);

protected:
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;
    Qt::DropActions supportedDropActions() const override { return Qt::CopyAction; }
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QVector<Track> tracksFor(const QList<QTreeWidgetItem *> &items) const;

    QVector<Album> m_albums;
    QHash<QString, QTreeWidgetItem *> m_itemByKey;
    QSet<QTreeWidgetItem *> m_expandedByFilter;
};
