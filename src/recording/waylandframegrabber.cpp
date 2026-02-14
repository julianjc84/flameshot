// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "waylandframegrabber.h"

#ifdef HAVE_PIPEWIRE
extern "C" {
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
}
#endif

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

WaylandFrameGrabber::WaylandFrameGrabber(QObject* parent)
  : FrameGrabber(parent)
{
#ifdef HAVE_PIPEWIRE
    pw_init(nullptr, nullptr);
#endif
}

WaylandFrameGrabber::~WaylandFrameGrabber()
{
    cleanup();
#ifdef HAVE_PIPEWIRE
    pw_deinit();
#endif
}

bool WaylandFrameGrabber::initialize(const QRect& region)
{
    m_region = region;

#ifdef HAVE_PIPEWIRE
    if (!initPortalScreencast()) {
        emit errorOccurred(tr("Failed to initialize XDG Desktop Portal screencast"));
        return false;
    }

    if (!initPipeWire(m_nodeId)) {
        emit errorOccurred(tr("Failed to initialize PipeWire stream"));
        return false;
    }

    m_initialized = true;
    return true;
#else
    emit errorOccurred(
      tr("Wayland screen recording requires PipeWire support.\n"
         "Please rebuild Flameshot with PipeWire enabled."));
    return false;
#endif
}

QImage WaylandFrameGrabber::grabFrame()
{
#ifdef HAVE_PIPEWIRE
    if (!m_initialized) {
        emit errorOccurred(tr("Frame grabber not initialized"));
        return QImage();
    }

    // Process PipeWire events to get new frames
    // In a full implementation, this would process the PipeWire loop
    // and return the latest captured frame

    if (m_latestFrame.isNull()) {
        emit errorOccurred(tr("No frame available"));
        return QImage();
    }

    // Crop to requested region if needed
    QImage result = m_latestFrame;
    if (!m_region.isEmpty() && m_region != m_latestFrame.rect()) {
        result = m_latestFrame.copy(m_region);
    }

    emit frameReady(result);
    return result;
#else
    emit errorOccurred(tr("PipeWire support not available"));
    return QImage();
#endif
}

void WaylandFrameGrabber::cleanup()
{
#ifdef HAVE_PIPEWIRE
    cleanupPipeWire();
#endif
    m_initialized = false;
    m_latestFrame = QImage();
}

bool WaylandFrameGrabber::isAvailable() const
{
#ifdef HAVE_PIPEWIRE
    // Check if we're running under Wayland
    QString sessionType = qgetenv("XDG_SESSION_TYPE");
    QString waylandDisplay = qgetenv("WAYLAND_DISPLAY");

    return sessionType.toLower() == "wayland" || !waylandDisplay.isEmpty();
#else
    return false;
#endif
}

bool WaylandFrameGrabber::initPortalScreencast()
{
#ifdef HAVE_PIPEWIRE
    QDBusInterface portalInterface(
      QStringLiteral("org.freedesktop.portal.Desktop"),
      QStringLiteral("/org/freedesktop/portal/desktop"),
      QStringLiteral("org.freedesktop.portal.ScreenCast"),
      QDBusConnection::sessionBus());

    if (!portalInterface.isValid()) {
        qWarning() << "XDG Desktop Portal ScreenCast interface not available";
        return false;
    }

    // TODO: Implement full portal screencast flow:
    // 1. CreateSession
    // 2. SelectSources
    // 3. Start
    // 4. Get PipeWire node ID from response

    // For now, this is a placeholder that will be expanded
    // when full PipeWire support is implemented
    return false;
#else
    return false;
#endif
}

bool WaylandFrameGrabber::initPipeWire(uint32_t nodeId)
{
#ifdef HAVE_PIPEWIRE
    Q_UNUSED(nodeId)

    // TODO: Implement PipeWire stream initialization
    // This involves:
    // 1. Creating pw_main_loop
    // 2. Creating pw_context
    // 3. Connecting to PipeWire core
    // 4. Creating stream with video format negotiation
    // 5. Setting up frame callbacks

    return false;
#else
    Q_UNUSED(nodeId)
    return false;
#endif
}

void WaylandFrameGrabber::cleanupPipeWire()
{
#ifdef HAVE_PIPEWIRE
    if (m_pwStream) {
        pw_stream_destroy(m_pwStream);
        m_pwStream = nullptr;
    }
    if (m_pwCore) {
        pw_core_disconnect(m_pwCore);
        m_pwCore = nullptr;
    }
    if (m_pwContext) {
        pw_context_destroy(m_pwContext);
        m_pwContext = nullptr;
    }
    if (m_pwLoop) {
        pw_main_loop_destroy(m_pwLoop);
        m_pwLoop = nullptr;
    }
#endif
}
