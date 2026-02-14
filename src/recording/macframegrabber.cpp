// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "macframegrabber.h"
#include <QGuiApplication>
#include <QPixmap>

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

    // Find the screen that contains the region
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen->geometry().intersects(region)) {
            m_screen = screen;
            break;
        }
    }

    if (!m_screen) {
        m_screen = QGuiApplication::primaryScreen();
    }

    if (!m_screen) {
        emit errorOccurred(tr("No screen available"));
        return false;
    }

    m_devicePixelRatio = m_screen->devicePixelRatio();

    return true;
}

QImage MacFrameGrabber::grabFrame()
{
    if (!m_screen) {
        emit errorOccurred(tr("Screen not initialized"));
        return QImage();
    }

    // Grab the screen area using Qt's platform abstraction
    // (CGWindowListCreateImage on macOS)
    QPixmap pixmap = m_screen->grabWindow(
      0, // WId 0 = entire screen
      m_region.x() - m_screen->geometry().x(),
      m_region.y() - m_screen->geometry().y(),
      m_region.width(),
      m_region.height());

    if (pixmap.isNull()) {
        emit errorOccurred(tr("Failed to grab screen"));
        return QImage();
    }

    QImage image = pixmap.toImage();

    // Handle Retina/high DPI scaling
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
    m_screen = nullptr;
}

bool MacFrameGrabber::isAvailable() const
{
    return true;
}
