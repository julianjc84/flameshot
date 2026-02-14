// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "recordingcontroller.h"
#include "framegrabber.h"
#include "videoencoder.h"

#if defined(Q_OS_WIN)
#include "windowsframegrabber.h"
#elif defined(Q_OS_MACOS)
#include "macframegrabber.h"
#else
#include "x11framegrabber.h"
#ifdef HAVE_PIPEWIRE
#include "waylandframegrabber.h"
#endif
#endif

#include "src/utils/confighandler.h"
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
#include "src/utils/desktopinfo.h"
#endif
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

RecordingController::RecordingController(QObject* parent)
  : QObject(parent)
{
    m_captureTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_captureTimer, &QTimer::timeout, this, &RecordingController::captureFrame);
}

RecordingController::~RecordingController()
{
    if (m_context.state() == RecordingContext::RECORDING ||
        m_context.state() == RecordingContext::PAUSED) {
        stopRecording();
    }
    abortGifConversion();
    cleanup();
}

bool RecordingController::startRecording(
  const RecordingContext::RecordingConfig& config)
{
    if (m_context.state() != RecordingContext::IDLE) {
        emit recordingError(tr("Recording already in progress"));
        return false;
    }

    // Check if we're in GIF mode
    ConfigHandler configHandler;
    QString outputMode = configHandler.videoOutputMode();
    m_gifMode = (outputMode == "gif");

    RecordingContext::RecordingConfig modifiedConfig = config;

    if (m_gifMode) {
        // Store the intended final GIF path
        m_finalGifPath = config.outputPath;
        // Ensure it ends with .gif
        if (!m_finalGifPath.endsWith(".gif", Qt::CaseInsensitive)) {
            // Replace extension with .gif
            int lastDot = m_finalGifPath.lastIndexOf('.');
            if (lastDot > 0) {
                m_finalGifPath = m_finalGifPath.left(lastDot) + ".gif";
            } else {
                m_finalGifPath += ".gif";
            }
        }

        // Create temp paths for intermediate files
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString uniqueId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        m_tempVideoPath = QDir(tempDir).filePath(
            QString("flameshot_rec_%1.mp4").arg(uniqueId));
        m_tempPalettePath = QDir(tempDir).filePath(
            QString("flameshot_palette_%1.png").arg(uniqueId));

        // Modify config to record to temp video with optimized settings for GIF
        modifiedConfig.outputPath = m_tempVideoPath;
        modifiedConfig.codec = "libx264";
        modifiedConfig.preset = "ultrafast";
        modifiedConfig.format = "mp4";
        modifiedConfig.rateControl = RecordingContext::CRF_MODE;
        modifiedConfig.crf = 18;  // High quality for conversion
    }

    if (modifiedConfig.framerate <= 0) {
        emit recordingError(tr("Invalid framerate"));
        return false;
    }

    m_context.config() = modifiedConfig;

    if (!initializeGrabber()) {
        return false;
    }

    if (!initializeEncoder()) {
        cleanup();
        return false;
    }

    // Calculate frame interval in milliseconds
    int frameIntervalMs = 1000 / modifiedConfig.framerate;
    m_captureTimer.setInterval(frameIntervalMs);

    m_context.startTimer();
    setState(RecordingContext::RECORDING);
    m_captureTimer.start();

    return true;
}

void RecordingController::pauseRecording()
{
    if (m_context.state() != RecordingContext::RECORDING) {
        return;
    }

    m_captureTimer.stop();
    m_context.pauseTimer();
    setState(RecordingContext::PAUSED);
}

void RecordingController::resumeRecording()
{
    if (m_context.state() != RecordingContext::PAUSED) {
        return;
    }

    m_context.resumeTimer();
    setState(RecordingContext::RECORDING);
    m_captureTimer.start();
}

void RecordingController::stopRecording()
{
    if (m_context.state() != RecordingContext::RECORDING &&
        m_context.state() != RecordingContext::PAUSED) {
        return;
    }

    m_captureTimer.stop();
    m_context.stopTimer();

    if (m_encoder) {
        m_encoder->finalize();
    }

    cleanup();
    setState(RecordingContext::IDLE);

    if (m_gifMode && !m_tempVideoPath.isEmpty()) {
        // Start GIF conversion process
        startGifConversion();
    } else {
        // Regular video mode - emit finished immediately
        QString outputPath = m_context.config().outputPath;
        if (!outputPath.isEmpty()) {
            emit recordingFinished(outputPath);
        }
    }
}

RecordingContext::RecordingState RecordingController::state() const
{
    return m_context.state();
}

RecordingContext& RecordingController::context()
{
    return m_context;
}

const RecordingContext& RecordingController::context() const
{
    return m_context;
}

qint64 RecordingController::elapsedMs() const
{
    return m_context.elapsedMs();
}

qint64 RecordingController::frameCount() const
{
    return m_context.frameCount();
}

void RecordingController::captureFrame()
{
    if (m_context.state() != RecordingContext::RECORDING) {
        return;
    }

    if (!m_grabber || !m_encoder) {
        emit recordingError(tr("Recording components not initialized"));
        stopRecording();
        return;
    }

    // Edge-pan the capture region to follow the cursor
    updateRegionForCursor();

    QImage frame = m_grabber->grabFrame();
    if (frame.isNull()) {
        emit recordingError(tr("Failed to capture frame"));
        stopRecording();
        return;
    }

    if (!m_encoder->encodeFrame(frame)) {
        emit recordingError(m_encoder->errorString());
        stopRecording();
        return;
    }

    m_context.incrementFrameCount();
    emit recordingProgress(m_context.frameCount(), m_context.elapsedMs());
}

void RecordingController::updateRegionForCursor()
{
    QPoint cursor = QCursor::pos();
    QRect region = m_grabber->region();
    QRect original = region;

    // Shift region when cursor enters the edge padding zone
    if (cursor.x() < region.left() + EDGE_PADDING) {
        region.moveLeft(cursor.x() - EDGE_PADDING);
    }
    if (cursor.x() > region.right() - EDGE_PADDING) {
        region.moveRight(cursor.x() + EDGE_PADDING);
    }
    if (cursor.y() < region.top() + EDGE_PADDING) {
        region.moveTop(cursor.y() - EDGE_PADDING);
    }
    if (cursor.y() > region.bottom() - EDGE_PADDING) {
        region.moveBottom(cursor.y() + EDGE_PADDING);
    }

    // Clamp to screen bounds
    if (!m_screenBounds.isNull()) {
        if (region.left() < m_screenBounds.left()) {
            region.moveLeft(m_screenBounds.left());
        }
        if (region.top() < m_screenBounds.top()) {
            region.moveTop(m_screenBounds.top());
        }
        if (region.right() > m_screenBounds.right()) {
            region.moveRight(m_screenBounds.right());
        }
        if (region.bottom() > m_screenBounds.bottom()) {
            region.moveBottom(m_screenBounds.bottom());
        }
    }

    if (region != original) {
        m_grabber->setRegion(region);
        emit regionChanged(region);
    }

    // Hide edges the cursor is approaching (wider threshold than panning)
    // so they're hidden well before X11 grabs the frame
    bool showTop = cursor.y() > region.top() + HIDE_PADDING;
    bool showBottom = cursor.y() < region.bottom() - HIDE_PADDING;
    bool showLeft = cursor.x() > region.left() + HIDE_PADDING;
    bool showRight = cursor.x() < region.right() - HIDE_PADDING;
    emit edgeVisibilityChanged(showTop, showBottom, showLeft, showRight);
}

bool RecordingController::initializeGrabber()
{
#if defined(Q_OS_WIN)
    m_grabber = std::make_unique<WindowsFrameGrabber>(this);
#elif defined(Q_OS_MACOS)
    m_grabber = std::make_unique<MacFrameGrabber>(this);
#else
    DesktopInfo info;

#ifdef HAVE_PIPEWIRE
    if (info.waylandDetected()) {
        m_grabber = std::make_unique<WaylandFrameGrabber>(this);
    } else {
        m_grabber = std::make_unique<X11FrameGrabber>(this);
    }
#else
    if (info.waylandDetected()) {
        emit recordingError(
          tr("Wayland screen recording requires PipeWire support. "
             "Please rebuild with PipeWire enabled."));
        return false;
    }
    m_grabber = std::make_unique<X11FrameGrabber>(this);
#endif
#endif

    if (!m_grabber->isAvailable()) {
        emit recordingError(
          tr("Screen recording is not available on this system"));
        return false;
    }

    if (!m_grabber->initialize(m_context.config().region)) {
        emit recordingError(tr("Failed to initialize screen capture"));
        return false;
    }

    connect(m_grabber.get(),
            &FrameGrabber::errorOccurred,
            this,
            &RecordingController::recordingError);

    // Determine screen bounds for edge-panning clamping
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen->geometry().intersects(m_context.config().region)) {
            m_screenBounds = screen->geometry();
            break;
        }
    }
    if (m_screenBounds.isNull()) {
        QScreen* primary = QGuiApplication::primaryScreen();
        if (primary) {
            m_screenBounds = primary->geometry();
        }
    }

    return true;
}

bool RecordingController::initializeEncoder()
{
    m_encoder = std::make_unique<VideoEncoder>(this);

    VideoEncoder::EncoderConfig encoderConfig;
    encoderConfig.outputPath = m_context.config().outputPath;
    encoderConfig.width = m_context.config().region.width();
    encoderConfig.height = m_context.config().region.height();
    encoderConfig.framerate = m_context.config().framerate;

    // Rate control settings
    encoderConfig.rateControl = (m_context.config().rateControl == RecordingContext::CRF_MODE)
                                  ? VideoEncoder::CRF_MODE
                                  : VideoEncoder::BITRATE_MODE;
    encoderConfig.bitrate = m_context.config().bitrate;
    encoderConfig.crf = m_context.config().crf;

    // Encoder settings
    encoderConfig.codec = m_context.config().codec;
    encoderConfig.preset = m_context.config().preset;
    encoderConfig.format = m_context.config().format;

    QString originalCodec = encoderConfig.codec;

    if (!m_encoder->initialize(encoderConfig)) {
        // If hardware encoder failed and it's not already libx264, try fallback
        if (originalCodec != "libx264" && !originalCodec.isEmpty()) {
            // Reset encoder and try with libx264
            m_encoder.reset();
            m_encoder = std::make_unique<VideoEncoder>(this);

            encoderConfig.codec = "libx264";
            // Reset pixel format will be handled by encoder based on codec

            if (m_encoder->initialize(encoderConfig)) {
                // Fallback succeeded - emit signal to notify user
                emit encoderFallback(originalCodec, "libx264");

                connect(m_encoder.get(),
                        &VideoEncoder::encodingError,
                        this,
                        &RecordingController::recordingError);
                return true;
            }
        }

        // Either already libx264 or fallback also failed
        emit recordingError(m_encoder->errorString());
        return false;
    }

    connect(m_encoder.get(),
            &VideoEncoder::encodingError,
            this,
            &RecordingController::recordingError);

    return true;
}

void RecordingController::cleanup()
{
    if (m_grabber) {
        m_grabber->cleanup();
        m_grabber.reset();
    }

    m_encoder.reset();
}

void RecordingController::setState(RecordingContext::RecordingState state)
{
    if (m_context.state() != state) {
        m_context.setState(state);
        emit stateChanged(state);
    }
}

void RecordingController::abortGifConversion()
{
    if (m_gifProcess) {
        m_gifProcess->disconnect();
        m_gifProcess->kill();
        m_gifProcess->waitForFinished(3000);
        m_gifProcess->deleteLater();
        m_gifProcess = nullptr;
    }
    m_gifConversionStep = 0;
    cleanupTempFiles();
    m_gifMode = false;
    m_tempVideoPath.clear();
    m_tempPalettePath.clear();
    m_finalGifPath.clear();
}

void RecordingController::startGifConversion()
{
    m_gifConversionStep = 1;  // Starting palette generation

    m_gifProcess = new QProcess(this);
    connect(m_gifProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus) {
                onGifConversionFinished(exitCode);
            });

    // Step 1: Generate palette
    int fps = m_context.config().framerate;
    QStringList args;
    args << "-y"  // Overwrite output
         << "-i" << m_tempVideoPath
         << "-vf" << QString("fps=%1,palettegen=stats_mode=diff").arg(fps)
         << m_tempPalettePath;

    m_gifProcess->start("ffmpeg", args);
}

void RecordingController::onGifConversionFinished(int exitCode)
{
    if (exitCode != 0) {
        QString errorOutput = m_gifProcess->readAllStandardError();
        emit recordingError(tr("GIF conversion failed: %1").arg(errorOutput));
        cleanupTempFiles();
        m_gifProcess->deleteLater();
        m_gifProcess = nullptr;
        return;
    }

    if (m_gifConversionStep == 1) {
        // Palette generated, now create the GIF
        m_gifConversionStep = 2;

        int fps = m_context.config().framerate;
        QStringList args;
        args << "-y"  // Overwrite output
             << "-i" << m_tempVideoPath
             << "-i" << m_tempPalettePath
             << "-lavfi" << QString("fps=%1[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5").arg(fps)
             << m_finalGifPath;

        m_gifProcess->start("ffmpeg", args);
    } else if (m_gifConversionStep == 2) {
        // GIF creation complete
        m_gifConversionStep = 0;
        m_gifProcess->deleteLater();
        m_gifProcess = nullptr;

        // Clean up temp files
        cleanupTempFiles();

        // Emit success
        emit recordingFinished(m_finalGifPath);

        // Reset GIF mode variables
        m_gifMode = false;
        m_tempVideoPath.clear();
        m_tempPalettePath.clear();
        m_finalGifPath.clear();
    }
}

void RecordingController::cleanupTempFiles()
{
    if (!m_tempVideoPath.isEmpty() && QFile::exists(m_tempVideoPath)) {
        QFile::remove(m_tempVideoPath);
    }
    if (!m_tempPalettePath.isEmpty() && QFile::exists(m_tempPalettePath)) {
        QFile::remove(m_tempPalettePath);
    }
}
