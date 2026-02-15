// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "macframegrabber.h"
#include <CoreGraphics/CoreGraphics.h>
#include <QGuiApplication>
#include <QScreen>

MacFrameGrabber::MacFrameGrabber(QObject* parent)
  : FrameGrabber(parent)
{
}

MacFrameGrabber::~MacFrameGrabber()
{
    cleanup();
}

bool MacFrameGrabber::initialize(const QRect& region)
{
    m_region = region;

    // Check and request screen recording permission (macOS 10.15+)
    if (__builtin_available(macOS 10.15, *)) {
        m_hasPermission = CGPreflightScreenCaptureAccess();
        if (!m_hasPermission) {
            CGRequestScreenCaptureAccess();
            m_hasPermission = CGPreflightScreenCaptureAccess();
        }
    } else {
        m_hasPermission = true;
    }

    if (!m_hasPermission) {
        emit errorOccurred(
          tr("Screen recording permission denied. Please grant access in "
             "System Preferences > Security & Privacy > Privacy > Screen "
             "Recording."));
        return false;
    }

    // Find the display that contains the capture region
    CGDirectDisplayID displays[16];
    uint32_t displayCount = 0;
    CGGetActiveDisplayList(16, displays, &displayCount);

    m_displayId = CGMainDisplayID();
    for (uint32_t i = 0; i < displayCount; ++i) {
        CGRect bounds = CGDisplayBounds(displays[i]);
        QRect displayRect(bounds.origin.x, bounds.origin.y,
                          bounds.size.width, bounds.size.height);
        if (displayRect.intersects(region)) {
            m_displayId = displays[i];
            break;
        }
    }

    // Get device pixel ratio from QScreen for Retina handling
    m_devicePixelRatio = 1.0;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen->geometry().intersects(region)) {
            m_devicePixelRatio = screen->devicePixelRatio();
            break;
        }
    }

    return true;
}

QImage MacFrameGrabber::grabFrame()
{
    if (!m_hasPermission) {
        emit errorOccurred(tr("Screen recording permission not granted"));
        return QImage();
    }

    CGRect captureRect = CGRectMake(m_region.x(), m_region.y(),
                                    m_region.width(), m_region.height());

    CGImageRef cgImage = CGWindowListCreateImage(
      captureRect,
      kCGWindowListOptionOnScreenOnly,
      kCGNullWindowID,
      kCGWindowImageDefault);

    if (!cgImage) {
        emit errorOccurred(tr("Failed to capture screen"));
        return QImage();
    }

    // Convert CGImage to QImage
    size_t width = CGImageGetWidth(cgImage);
    size_t height = CGImageGetHeight(cgImage);

    QImage image(static_cast<int>(width), static_cast<int>(height),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
      image.bits(),
      width,
      height,
      8,
      image.bytesPerLine(),
      colorSpace,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);

    if (ctx) {
        CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), cgImage);
        CGContextRelease(ctx);
    }

    CGColorSpaceRelease(colorSpace);
    CGImageRelease(cgImage);

    // Handle Retina/high DPI scaling — captured image is at native resolution
    if (m_devicePixelRatio != 1.0) {
        image = image.scaled(m_region.width(),
                             m_region.height(),
                             Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    }

    emit frameReady(image);
    return image;
}

void MacFrameGrabber::cleanup()
{
    m_displayId = 0;
    m_hasPermission = false;
}

bool MacFrameGrabber::isAvailable() const
{
    if (__builtin_available(macOS 10.15, *)) {
        return CGPreflightScreenCaptureAccess();
    }
    return true;
}
