#pragma once

#include <QListView>
#include <QUrl>

class PlayQueue;

// The queue list. Rows can be reordered by dragging; albums and tracks can be
// dropped in from the library, and audio files or folders from a file manager.
class QueueView : public QListView
{
    Q_OBJECT
public:
    explicit QueueView(PlayQueue *queue, QWidget *parent = nullptr);

    bool isPlaying() const { return m_playing; }
    void setPlaying(bool playing);
    QList<int> selectedRows() const;

signals:
    void rowActivated(int row);
    void trackPathsDropped(const QStringList &paths, int row);
    void urlsDropped(const QList<QUrl> &urls, int row);

public slots:
    void removeSelected();

protected:
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    int dropRow(const QPoint &pos) const;

    PlayQueue *m_queue;
    bool m_playing = false;
};
