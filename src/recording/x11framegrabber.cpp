// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "x11framegrabber.h"
#include <QGuiApplication>
#include <QPixmap>

X11FrameGrabber::X11FrameGrabber(QObject* parent)
  : FrameGrabber(parent)
{
}

X11FrameGrabber::~X11FrameGrabber()
{
    cleanup();
}

bool X11FrameGrabber::initialize(const QRect& region)
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

QImage X11FrameGrabber::grabFrame()
{
    if (!m_screen) {
        emit errorOccurred(tr("Screen not initialized"));
        return QImage();
    }

    // Grab the entire desktop or specific screen area
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

    // Handle high DPI scaling
    if (m_devicePixelRatio != 1.0) {
        image = image.scaled(m_region.width(),
                            m_region.height(),
                            Qt::IgnoreAspectRatio,
                            Qt::SmoothTransformation);
    }

    emit frameReady(image);
    return image;
}

void X11FrameGrabber::cleanup()
{
    m_screen = nullptr;
}

bool X11FrameGrabber::isAvailable() const
{
    // X11 grabbing is available when not running under Wayland
    QString sessionType = qgetenv("XDG_SESSION_TYPE");
    QString waylandDisplay = qgetenv("WAYLAND_DISPLAY");

    // Available if we're on X11 or if Wayland detection fails
    return sessionType.toLower() != "wayland" && waylandDisplay.isEmpty();
}
