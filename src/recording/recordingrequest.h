// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QRect>
#include <QString>

class RecordingRequest
{
public:
    enum RecordingMode
    {
        REGION_MODE,     // User selects region interactively
        FULLSCREEN_MODE, // Record entire desktop
        SCREEN_MODE,     // Record specific monitor
    };

    RecordingRequest(RecordingMode mode = REGION_MODE,
                     const QString& path = QString());

    RecordingMode mode() const;
    QString outputPath() const;
    QRect initialRegion() const;
    int framerate() const;
    int bitrate() const;
    QString codec() const;
    QString format() const;
    int screenNumber() const;

    void setOutputPath(const QString& path);
    void setInitialRegion(const QRect& region);
    void setFramerate(int fps);
    void setBitrate(int bps);
    void setCodec(const QString& codec);
    void setFormat(const QString& format);
    void setScreenNumber(int screen);

private:
    RecordingMode m_mode;
    QString m_outputPath;
    QRect m_initialRegion;
    int m_framerate = 0;      // 0 means use config
    int m_bitrate = 0;        // 0 means use config
    QString m_codec;          // empty means use config
    QString m_format;         // empty means use config
    int m_screenNumber = -1;  // -1 means current screen
};
