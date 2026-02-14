// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QElapsedTimer>
#include <QRect>
#include <QString>

class RecordingContext
{
public:
    enum RecordingState
    {
        IDLE,
        SELECTING_REGION,
        RECORDING,
        PAUSED,
        STOPPED
    };

    // Rate control mode
    enum RateControlMode
    {
        BITRATE_MODE,  // Use constant bitrate
        CRF_MODE       // Use constant rate factor (quality-based)
    };

    struct RecordingConfig
    {
        QRect region;
        QString outputPath;
        int framerate = 30;

        // Rate control
        RateControlMode rateControl = CRF_MODE;
        int bitrate = 4000000;  // 4 Mbps (used when rateControl == BITRATE_MODE)
        int crf = 23;           // 0-51, lower = better quality (used when rateControl == CRF_MODE)

        // Encoder settings
        QString codec = "libx264";
        QString preset = "ultrafast";  // ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow
        QString format = "mp4";
    };

    RecordingContext();

    RecordingState state() const;
    void setState(RecordingState state);

    RecordingConfig& config();
    const RecordingConfig& config() const;

    void startTimer();
    void pauseTimer();
    void resumeTimer();
    void stopTimer();

    qint64 elapsedMs() const;
    qint64 frameCount() const;
    void incrementFrameCount();

private:
    RecordingState m_state = IDLE;
    RecordingConfig m_config;
    QElapsedTimer m_timer;
    qint64 m_pausedElapsed = 0;
    qint64 m_frameCount = 0;
    bool m_timerRunning = false;
};
