// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QImage>
#include <QObject>
#include <QRect>

class FrameGrabber : public QObject
{
    Q_OBJECT

public:
    explicit FrameGrabber(QObject* parent = nullptr);
    ~FrameGrabber() override = default;

    virtual bool initialize(const QRect& region) = 0;
    virtual QImage grabFrame() = 0;
    virtual void cleanup() = 0;

    virtual bool isAvailable() const = 0;
    virtual QString backendName() const = 0;

    virtual void setRegion(const QRect& region) { m_region = region; }
    QRect region() const;

signals:
    void frameReady(const QImage& frame);
    void errorOccurred(const QString& error);

protected:
    QRect m_region;
};
