#pragma once

#include "Track.h"

#include <QAbstractListModel>
#include <QVector>

#include <random>

// The play queue: an ordered list of tracks plus the notion of a "current"
// entry. Played tracks stay in the queue so that "previous" works, and so that
// the queue can be replayed with repeat enabled.
//
// With shuffle enabled the queue keeps a separate random play order. Tracks
// added while shuffling are slotted in at random positions among the tracks
// that have not been played yet, and a manually picked track becomes the new
// point from which shuffled playback continues.
class PlayQueue : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        TrackRole = Qt::UserRole + 1,
        IsCurrentRole,
        DurationRole,
    };
    enum class Repeat { Off, All, One };
    Q_ENUM(Repeat)

    static const char *const RowsMimeType;

    explicit PlayQueue(QObject *parent = nullptr);

    // QAbstractItemModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;

    int count() const { return int(m_entries.size()); }
    bool isEmpty() const { return m_entries.isEmpty(); }
    const Track &trackAt(int row) const { return m_entries[row].track; }
    QVector<Track> tracks() const;
    qint64 totalDurationMs() const;

    // Editing. `row` is clamped; returns the row of the first inserted track.
    int insert(int row, const QVector<Track> &tracks);
    int append(const QVector<Track> &tracks) { return insert(count(), tracks); }
    int insertAfterCurrent(const QVector<Track> &tracks);
    void removeRowsList(QList<int> rows);
    // Returns the new row of the first moved entry, or -1 if nothing moved.
    int moveRowsTo(QList<int> rows, int destination);
    void clear();
    void updateTrack(int row, const Track &track);

    // Playback position.
    int currentRow() const;
    const Track *currentTrack() const;
    void setCurrentRow(int row);
    // Advances and returns the new current row, or -1 at the end of the queue.
    // `automatic` is true when the previous track simply finished, which is the
    // only case where Repeat::One replays the same track.
    int advance(bool automatic);
    int goBack();

    bool shuffle() const { return m_shuffle; }
    void setShuffle(bool on);
    Repeat repeat() const { return m_repeat; }
    void setRepeat(Repeat mode);

signals:
    void currentRowChanged(int row);
    void shuffleChanged(bool on);
    void repeatChanged(PlayQueue::Repeat mode);
    void contentsChanged();

private:
    struct Entry {
        quint64 id;
        Track track;
    };

    int rowOfId(quint64 id) const;
    void setCurrentId(quint64 id);
    void rebuildShuffleOrder();
    void emitRowChanged(quint64 id);

    QVector<Entry> m_entries;
    quint64 m_nextId = 1;
    quint64 m_currentId = 0;
    // Linear mode: where playback resumes when the current entry was removed.
    int m_resumeRow = 0;

    bool m_shuffle = false;
    QVector<quint64> m_order; // shuffled ids
    int m_orderPos = -1;      // position of the current entry in m_order
    Repeat m_repeat = Repeat::Off;
    std::mt19937 m_rng;
};
