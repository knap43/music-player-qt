#include "PlayQueue.h"

#include <QDataStream>
#include <QIODevice>
#include <QMimeData>

#include <algorithm>

const char *const PlayQueue::RowsMimeType = "application/x-amber-queue-rows";

PlayQueue::PlayQueue(QObject *parent)
    : QAbstractListModel(parent)
    , m_rng(std::random_device{}())
{
}

int PlayQueue::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant PlayQueue::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return {};
    const Entry &entry = m_entries[index.row()];
    switch (role) {
    case Qt::DisplayRole:
        return entry.track.displayTitle();
    case Qt::ToolTipRole:
        return QStringLiteral("%1\n%2 — %3\n%4")
            .arg(entry.track.displayTitle(), entry.track.displayArtist(),
                 entry.track.displayAlbum(), entry.track.path);
    case TrackRole:
        return QVariant::fromValue(entry.track);
    case IsCurrentRole:
        return entry.id == m_currentId;
    case DurationRole:
        return entry.track.durationMs;
    default:
        return {};
    }
}

Qt::ItemFlags PlayQueue::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QStringList PlayQueue::mimeTypes() const
{
    return {QString::fromLatin1(RowsMimeType), QStringLiteral("application/x-amber-track-paths"),
            QStringLiteral("text/uri-list")};
}

QMimeData *PlayQueue::mimeData(const QModelIndexList &indexes) const
{
    QList<int> rows;
    for (const QModelIndex &index : indexes)
        rows << index.row();
    std::sort(rows.begin(), rows.end());
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream << rows;
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(RowsMimeType), encoded);
    return mime;
}

Qt::DropActions PlayQueue::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions PlayQueue::supportedDragActions() const
{
    // Reordering is handled by QueueView itself; never let a drag out of the
    // queue be interpreted as a move by someone else (e.g. a file manager).
    return Qt::CopyAction;
}

QVector<Track> PlayQueue::tracks() const
{
    QVector<Track> result;
    result.reserve(count());
    for (const Entry &e : m_entries)
        result.push_back(e.track);
    return result;
}

qint64 PlayQueue::totalDurationMs() const
{
    qint64 total = 0;
    for (const Entry &e : m_entries)
        total += e.track.durationMs;
    return total;
}

int PlayQueue::rowOfId(quint64 id) const
{
    if (id == 0)
        return -1;
    for (int i = 0; i < count(); ++i)
        if (m_entries[i].id == id)
            return i;
    return -1;
}

int PlayQueue::currentRow() const
{
    return rowOfId(m_currentId);
}

const Track *PlayQueue::currentTrack() const
{
    const int row = currentRow();
    return row >= 0 ? &m_entries[row].track : nullptr;
}

void PlayQueue::emitRowChanged(quint64 id)
{
    const int row = rowOfId(id);
    if (row >= 0)
        emit dataChanged(index(row), index(row), {IsCurrentRole});
}

void PlayQueue::setCurrentId(quint64 id)
{
    const quint64 old = m_currentId;
    m_currentId = id;
    if (old != id) {
        emitRowChanged(old);
        emitRowChanged(id);
    }
    emit currentRowChanged(currentRow());
}

int PlayQueue::insert(int row, const QVector<Track> &tracks)
{
    row = qBound(0, row, count());
    if (tracks.isEmpty())
        return row;

    beginInsertRows(QModelIndex(), row, row + int(tracks.size()) - 1);
    QVector<Entry> entries;
    entries.reserve(tracks.size());
    for (const Track &t : tracks)
        entries.push_back({m_nextId++, t});
    m_entries.insert(row, entries.size(), Entry{});
    std::copy(entries.cbegin(), entries.cend(), m_entries.begin() + row);
    endInsertRows();

    if (m_currentId == 0 && row < m_resumeRow)
        m_resumeRow += int(tracks.size());

    if (m_shuffle) {
        // Scatter the new tracks among the ones that have not played yet.
        for (const Entry &e : entries) {
            const int lo = m_orderPos + 1;
            std::uniform_int_distribution<int> dist(lo, int(m_order.size()));
            m_order.insert(dist(m_rng), e.id);
        }
    }

    emit contentsChanged();
    return row;
}

int PlayQueue::insertAfterCurrent(const QVector<Track> &tracks)
{
    int row = currentRow();
    row = row >= 0 ? row + 1 : m_resumeRow;
    const int first = insert(row, tracks);
    if (m_shuffle) {
        // "Play next" must mean next, even when shuffling.
        QVector<quint64> ids;
        for (int i = 0; i < tracks.size(); ++i)
            ids << m_entries[first + i].id;
        for (quint64 id : ids)
            m_order.removeOne(id);
        for (int i = 0; i < ids.size(); ++i)
            m_order.insert(m_orderPos + 1 + i, ids[i]);
    }
    return first;
}

void PlayQueue::removeRowsList(QList<int> rows)
{
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    if (rows.isEmpty())
        return;

    const int current = currentRow();
    for (int row : std::as_const(rows)) {
        if (row < 0 || row >= count())
            continue;
        const quint64 id = m_entries[row].id;

        if (id == m_currentId) {
            m_currentId = 0;
            m_resumeRow = row;
        } else if (m_currentId == 0 && row < m_resumeRow) {
            --m_resumeRow;
        }

        const int orderIndex = int(m_order.indexOf(id));
        if (orderIndex >= 0) {
            m_order.remove(orderIndex);
            if (orderIndex <= m_orderPos)
                --m_orderPos;
        }

        beginRemoveRows(QModelIndex(), row, row);
        m_entries.remove(row);
        endRemoveRows();
    }

    if (current >= 0 && m_currentId == 0)
        emit currentRowChanged(-1);
    else if (current != currentRow())
        emit currentRowChanged(currentRow());
    emit contentsChanged();
}

int PlayQueue::moveRowsTo(QList<int> rows, int destination)
{
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    rows.erase(std::remove_if(rows.begin(), rows.end(),
                              [this](int r) { return r < 0 || r >= count(); }),
               rows.end());
    if (rows.isEmpty())
        return -1;

    destination = qBound(0, destination, count());
    int before = 0;
    for (int r : std::as_const(rows))
        if (r < destination)
            ++before;

    beginResetModel();
    QVector<Entry> moved;
    for (int i = int(rows.size()) - 1; i >= 0; --i) {
        moved.prepend(m_entries[rows[i]]);
        m_entries.remove(rows[i]);
    }
    const int target = destination - before;
    for (int i = 0; i < moved.size(); ++i)
        m_entries.insert(target + i, moved[i]);
    endResetModel();

    emit currentRowChanged(currentRow());
    emit contentsChanged();
    return target;
}

void PlayQueue::clear()
{
    beginResetModel();
    m_entries.clear();
    m_order.clear();
    m_orderPos = -1;
    m_currentId = 0;
    m_resumeRow = 0;
    endResetModel();
    emit currentRowChanged(-1);
    emit contentsChanged();
}

void PlayQueue::updateTrack(int row, const Track &track)
{
    if (row < 0 || row >= count())
        return;
    m_entries[row].track = track;
    emit dataChanged(index(row), index(row));
    emit contentsChanged();
}

void PlayQueue::setCurrentRow(int row)
{
    if (row < 0 || row >= count()) {
        setCurrentId(0);
        return;
    }
    const quint64 id = m_entries[row].id;
    if (m_shuffle) {
        // Continue shuffled playback from the chosen track.
        const int at = int(m_order.indexOf(id));
        if (at >= 0) {
            m_order.remove(at);
            if (at <= m_orderPos)
                --m_orderPos;
        }
        m_order.insert(m_orderPos + 1, id);
        ++m_orderPos;
    }
    setCurrentId(id);
}

int PlayQueue::advance(bool automatic)
{
    if (isEmpty())
        return -1;

    if (automatic && m_repeat == Repeat::One && m_currentId != 0) {
        emit currentRowChanged(currentRow());
        return currentRow();
    }

    if (m_shuffle) {
        int pos = m_orderPos + 1;
        if (pos >= m_order.size()) {
            if (m_repeat == Repeat::Off)
                return -1;
            const quint64 last = m_currentId;
            rebuildShuffleOrder();
            // Avoid playing the same track twice in a row across a reshuffle.
            if (m_order.size() > 1 && m_order.first() == last)
                std::swap(m_order[0], m_order[1 + int(m_rng() % (m_order.size() - 1))]);
            pos = 0;
        }
        m_orderPos = pos;
        setCurrentId(m_order[pos]);
        return currentRow();
    }

    const int current = currentRow();
    int next = current >= 0 ? current + 1 : m_resumeRow;
    if (next >= count()) {
        if (m_repeat == Repeat::Off)
            return -1;
        next = 0;
    }
    setCurrentId(m_entries[next].id);
    return next;
}

int PlayQueue::goBack()
{
    if (isEmpty())
        return -1;

    if (m_shuffle) {
        int pos = m_orderPos - 1;
        if (pos < 0) {
            if (m_repeat == Repeat::Off || m_order.isEmpty())
                return -1;
            pos = int(m_order.size()) - 1;
        }
        m_orderPos = pos;
        setCurrentId(m_order[pos]);
        return currentRow();
    }

    const int current = currentRow();
    int previous = (current >= 0 ? current : m_resumeRow) - 1;
    if (previous < 0) {
        if (m_repeat == Repeat::Off)
            return -1;
        previous = count() - 1;
    }
    setCurrentId(m_entries[previous].id);
    return previous;
}

void PlayQueue::rebuildShuffleOrder()
{
    m_order.clear();
    for (const Entry &e : std::as_const(m_entries))
        if (e.id != m_currentId)
            m_order.push_back(e.id);
    std::shuffle(m_order.begin(), m_order.end(), m_rng);
    m_orderPos = -1;
}

void PlayQueue::setShuffle(bool on)
{
    if (on == m_shuffle)
        return;
    m_shuffle = on;
    if (on) {
        rebuildShuffleOrder();
        if (m_currentId != 0) {
            m_order.prepend(m_currentId);
            m_orderPos = 0;
        }
    } else {
        m_order.clear();
        m_orderPos = -1;
    }
    emit shuffleChanged(on);
}

void PlayQueue::setRepeat(Repeat mode)
{
    if (mode == m_repeat)
        return;
    m_repeat = mode;
    emit repeatChanged(mode);
}
