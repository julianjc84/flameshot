// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "recordingrequest.h"

RecordingRequest::RecordingRequest(RecordingMode mode, const QString& path)
  : m_mode(mode)
  , m_outputPath(path)
{
}

RecordingRequest::RecordingMode RecordingRequest::mode() const
{
    return m_mode;
}

QString RecordingRequest::outputPath() const
{
    return m_outputPath;
}

QRect RecordingRequest::initialRegion() const
{
    return m_initialRegion;
}

int RecordingRequest::framerate() const
{
    return m_framerate;
}

int RecordingRequest::bitrate() const
{
    return m_bitrate;
}

QString RecordingRequest::codec() const
{
    return m_codec;
}

QString RecordingRequest::format() const
{
    return m_format;
}

int RecordingRequest::screenNumber() const
{
    return m_screenNumber;
}

void RecordingRequest::setOutputPath(const QString& path)
{
    m_outputPath = path;
}

void RecordingRequest::setInitialRegion(const QRect& region)
{
    m_initialRegion = region;
}

void RecordingRequest::setFramerate(int fps)
{
    if (fps > 0 && fps <= 120) {
        m_framerate = fps;
    }
}

void RecordingRequest::setBitrate(int bps)
{
    if (bps > 0) {
        m_bitrate = bps;
    }
}

void RecordingRequest::setCodec(const QString& codec)
{
    if (!codec.isEmpty()) {
        m_codec = codec;
    }
}

void RecordingRequest::setFormat(const QString& format)
{
    if (!format.isEmpty()) {
        m_format = format;
    }
}

void RecordingRequest::setScreenNumber(int screen)
{
    m_screenNumber = screen;
}
