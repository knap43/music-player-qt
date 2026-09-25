#pragma once

#include <QImage>
#include <QRect>
#include <QSize>

namespace ImageUtils {

// A heavily blurred, low-resolution copy of the image, meant to be scaled up
// smoothly as a backdrop.
QImage blurred(const QImage &source, int workSize = 72, int radius = 5, int passes = 3);

// Source rectangle that crops `imageSize` to fill `target` (like CSS "cover").
QRect coverCrop(const QSize &imageSize, const QSize &target);

// Thumbnail with the image centred and cropped to a square.
QImage squareThumbnail(const QImage &source, int size);

} // namespace ImageUtils
