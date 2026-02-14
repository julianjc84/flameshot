// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "framegrabber.h"

// Forward declarations for PipeWire
struct pw_main_loop;
struct pw_context;
struct pw_core;
struct pw_stream;
struct spa_hook;

class WaylandFrameGrabber : public FrameGrabber
{
    Q_OBJECT

public:
    explicit WaylandFrameGrabber(QObject* parent = nullptr);
    ~WaylandFrameGrabber() override;

    bool initialize(const QRect& region) override;
    QImage grabFrame() override;
    void cleanup() override;

    bool isAvailable() const override;
    QString backendName() const override { return QStringLiteral("Wayland/PipeWire"); }

private:
    bool initPortalScreencast();
    bool initPipeWire(uint32_t nodeId);
    void cleanupPipeWire();

    // Portal state
    QString m_sessionHandle;
    uint32_t m_nodeId = 0;

    // PipeWire state
    pw_main_loop* m_pwLoop = nullptr;
    pw_context* m_pwContext = nullptr;
    pw_core* m_pwCore = nullptr;
    pw_stream* m_pwStream = nullptr;

    QImage m_latestFrame;
    bool m_initialized = false;
};
