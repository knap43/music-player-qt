#pragma once

#include "Lyrics.h"
#include "Track.h"

#include <QImage>
#include <QString>

// Thin wrapper around TagLib. All functions are thread-safe and may be called
// from worker threads.
namespace Metadata {

// Tags and duration. Fields TagLib cannot provide are guessed from the path.
Track readTrack(const QString &path);

// Embedded artwork first, then an image sitting next to the file.
QImage readCover(const QString &path);
QImage readEmbeddedCover(const QString &path);
QImage findFolderCover(const QString &directory);

// A sidecar .lrc file first, then embedded synchronised (ID3v2 SYLT) and
// unsynchronised (USLT / LYRICS comment) lyrics.
Lyrics readLyrics(const QString &path);

} // namespace Metadata
