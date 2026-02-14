// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QImage>
#include <QObject>
#include <QProcess>
#include <QString>

/**
 * @brief Video encoder that uses ffmpeg CLI via QProcess
 *
 * This encoder pipes raw frames to an ffmpeg process which handles
 * the actual encoding. This approach:
 * - Removes compile-time FFmpeg library dependency
 * - Allows the app to run without FFmpeg (with graceful error)
 * - Lets users install ffmpeg themselves (avoids codec patent issues)
 */
class VideoEncoder : public QObject
{
    Q_OBJECT

public:
    // Rate control mode
    enum RateControlMode
    {
        BITRATE_MODE, // Use constant bitrate
        CRF_MODE      // Use constant rate factor (quality-based)
    };

    struct EncoderConfig
    {
        QString outputPath;
        int width = 0;
        int height = 0;
        int framerate = 30;

        // Rate control
        RateControlMode rateControl = CRF_MODE;
        int bitrate = 4000000; // 4 Mbps (used when rateControl == BITRATE_MODE)
        int crf = 23; // 0-51, lower = better quality (used when rateControl == CRF_MODE)

        // Encoder settings
        QString codec = "libx264";
        QString preset = "ultrafast"; // ultrafast...veryslow
        QString format = "mp4";
    };

    explicit VideoEncoder(QObject* parent = nullptr);
    ~VideoEncoder() override;

    bool initialize(const EncoderConfig& config);
    bool encodeFrame(const QImage& frame);
    bool finalize();

    QString errorString() const;
    qint64 encodedFrames() const;
    bool isInitialized() const;

    /**
     * @brief Build FFmpeg command preview from current config settings
     * @param width Video width (use 0 for placeholder)
     * @param height Video height (use 0 for placeholder)
     * @return The full FFmpeg command as a string
     *
     * This static method builds a preview of the FFmpeg command that would be
     * executed, using current settings from ConfigHandler. Useful for showing
     * users what command will run.
     */
    static QString buildCommandPreview(int width = 0, int height = 0);

    /**
     * @brief Parse a custom command string into arguments
     * @param command The custom FFmpeg command string
     * @return List of arguments suitable for QProcess
     */
    static QStringList parseCustomCommand(const QString& command);

signals:
    void encodingProgress(qint64 frames, qint64 timeMs);
    void encodingError(const QString& error);
    void encodingFinished(const QString& outputPath);

private slots:
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QStringList buildFFmpegArgs();
    void setError(const QString& error);
    void cleanup();

    EncoderConfig m_config;
    QString m_errorString;
    qint64 m_frameCount = 0;
    bool m_initialized = false;
    bool m_finalizing = false;

    QProcess* m_ffmpegProcess = nullptr;
};
