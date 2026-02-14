// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "framegrabber.h"
#include <QScreen>

class X11FrameGrabber : public FrameGrabber
{
    Q_OBJECT

public:
    explicit X11FrameGrabber(QObject* parent = nullptr);
    ~X11FrameGrabber() override;

    bool initialize(const QRect& region) override;
    QImage grabFrame() override;
    void cleanup() override;

    bool isAvailable() const override;
    QString backendName() const override { return QStringLiteral("X11"); }

private:
    QScreen* m_screen = nullptr;
    qreal m_devicePixelRatio = 1.0;
};
