// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "recordingcontext.h"
#include <QObject>
#include <QRect>
#include <QTimer>
#include <memory>

class FrameGrabber;
class VideoEncoder;
class QProcess;

class RecordingController : public QObject
{
    Q_OBJECT

public:
    explicit RecordingController(QObject* parent = nullptr);
    ~RecordingController() override;

    bool startRecording(const RecordingContext::RecordingConfig& config);
    void pauseRecording();
    void resumeRecording();
    void stopRecording();
    void abortGifConversion();

    RecordingContext::RecordingState state() const;
    RecordingContext& context();
    const RecordingContext& context() const;

    qint64 elapsedMs() const;
    qint64 frameCount() const;

signals:
    void stateChanged(RecordingContext::RecordingState state);
    void recordingProgress(qint64 frames, qint64 elapsedMs);
    void recordingFinished(const QString& outputPath);
    void recordingError(const QString& error);
    void encoderFallback(const QString& originalCodec, const QString& fallbackCodec);
    void regionChanged(const QRect& newRegion);
    void edgeVisibilityChanged(bool top, bool bottom, bool left, bool right);

private slots:
    void captureFrame();
    void onGifConversionFinished(int exitCode);

private:
    bool initializeGrabber();
    bool initializeEncoder();
    void cleanup();
    void setState(RecordingContext::RecordingState state);
    void updateRegionForCursor();
    void startGifConversion();
    void cleanupTempFiles();

    RecordingContext m_context;
    std::unique_ptr<FrameGrabber> m_grabber;
    std::unique_ptr<VideoEncoder> m_encoder;
    QTimer m_captureTimer;
    QRect m_screenBounds;
    static const int EDGE_PADDING = 50;
    static const int HIDE_PADDING = 100;

    // GIF conversion
    bool m_gifMode = false;
    QString m_tempVideoPath;
    QString m_tempPalettePath;
    QString m_finalGifPath;
    QProcess* m_gifProcess = nullptr;
    int m_gifConversionStep = 0;  // 0 = not started, 1 = palette gen, 2 = gif creation
};
