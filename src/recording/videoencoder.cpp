// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "videoencoder.h"
#include "ffmpegutils.h"
#include "src/utils/confighandler.h"

#include <QRegularExpression>

VideoEncoder::VideoEncoder(QObject* parent)
  : QObject(parent)
{
}

VideoEncoder::~VideoEncoder()
{
    if (m_initialized) {
        finalize();
    }
    cleanup();
}

bool VideoEncoder::initialize(const EncoderConfig& config)
{
    if (m_initialized) {
        setError(tr("Encoder already initialized"));
        return false;
    }

    // Check if ffmpeg is installed
    if (!FFmpegUtils::isInstalled()) {
        setError(tr("FFmpeg is not installed.\n\n"
                    "Please install FFmpeg to enable video recording:\n%1")
                     .arg(FFmpegUtils::installCommand()));
        return false;
    }

    m_config = config;

    if (m_config.width <= 0 || m_config.height <= 0) {
        setError(tr("Invalid video dimensions"));
        return false;
    }

    if (m_config.outputPath.isEmpty()) {
        setError(tr("Output path not specified"));
        return false;
    }

    // Ensure dimensions are even (required by most codecs)
    m_config.width = (m_config.width + 1) & ~1;
    m_config.height = (m_config.height + 1) & ~1;

    // Check if using custom command
    ConfigHandler configHandler;
    bool useCustomCommand = configHandler.videoUseCustomCommand();
    QString customCommand = configHandler.videoCustomCommand();

    QStringList args;
    QString ffmpegPath;

    if (useCustomCommand && !customCommand.isEmpty()) {
        // Parse the custom command and replace placeholders
        QString processedCommand = customCommand;
        processedCommand.replace("{width}", QString::number(m_config.width));
        processedCommand.replace("{height}", QString::number(m_config.height));
        processedCommand.replace("{output}", m_config.outputPath);

        args = parseCustomCommand(processedCommand);

        // First argument should be ffmpeg path, extract it
        if (args.isEmpty()) {
            setError(tr("Custom command is empty"));
            return false;
        }

        ffmpegPath = args.takeFirst(); // Remove and get first argument

        // If it's just "ffmpeg", resolve the full path
        if (ffmpegPath == "ffmpeg" || !ffmpegPath.contains("/")) {
            QString resolvedPath = FFmpegUtils::ffmpegPath();
            if (!resolvedPath.isEmpty()) {
                ffmpegPath = resolvedPath;
            }
        }
    } else {
        // Check if the requested encoder is available
        if (!FFmpegUtils::hasEncoder(m_config.codec)) {
            // Try to fall back to libx264
            if (m_config.codec != "libx264" && FFmpegUtils::hasEncoder("libx264")) {
                QString originalCodec = m_config.codec;
                m_config.codec = "libx264";
                // Note: Could emit a signal here to notify about fallback
            } else {
                setError(tr("Encoder '%1' is not available.\n\n"
                            "Available encoders can be listed with:\n"
                            "ffmpeg -encoders | grep -E '^ V'")
                             .arg(m_config.codec));
                return false;
            }
        }

        // Build ffmpeg arguments from settings
        args = buildFFmpegArgs();
        ffmpegPath = FFmpegUtils::ffmpegPath();
    }

    // Start ffmpeg process
    m_ffmpegProcess = new QProcess(this);

    connect(m_ffmpegProcess,
            &QProcess::errorOccurred,
            this,
            &VideoEncoder::onProcessError);
    connect(m_ffmpegProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &VideoEncoder::onProcessFinished);

    m_ffmpegProcess->start(ffmpegPath, args);

    if (!m_ffmpegProcess->waitForStarted(5000)) {
        setError(tr("Failed to start FFmpeg process: %1")
                     .arg(m_ffmpegProcess->errorString()));
        cleanup();
        return false;
    }

    m_initialized = true;
    m_frameCount = 0;

    return true;
}

QStringList VideoEncoder::buildFFmpegArgs()
{
    QStringList args;

    // Overwrite output without asking
    args << "-y";

    // Input: raw video from stdin
    args << "-f" << "rawvideo";
    args << "-pix_fmt" << "bgra"; // QImage::Format_ARGB32 is BGRA in memory
    args << "-s" << QString("%1x%2").arg(m_config.width).arg(m_config.height);
    args << "-r" << QString::number(m_config.framerate);
    args << "-i" << "pipe:0"; // Read from stdin

    // Video codec
    args << "-c:v" << m_config.codec;

    // Output pixel format (yuv420p for compatibility, nv12 for some hw encoders)
    if (m_config.codec.contains("qsv") || m_config.codec.contains("vaapi")) {
        args << "-pix_fmt" << "nv12";
    } else {
        args << "-pix_fmt" << "yuv420p";
    }

    // Encoder-specific settings
    if (m_config.codec == "libx264" || m_config.codec == "libx265") {
        // Software encoder settings
        args << "-preset" << m_config.preset;

        if (m_config.rateControl == CRF_MODE) {
            args << "-crf" << QString::number(m_config.crf);
        } else {
            args << "-b:v" << QString::number(m_config.bitrate);
        }

        // Low latency tuning for screen recording
        args << "-tune" << "zerolatency";

    } else if (m_config.codec.contains("nvenc")) {
        // NVIDIA NVENC settings
        args << "-preset" << "p4"; // Balanced
        args << "-tune" << "ll";   // Low latency

        if (m_config.rateControl == CRF_MODE) {
            args << "-cq" << QString::number(m_config.crf);
            args << "-rc" << "vbr";
        } else {
            args << "-b:v" << QString::number(m_config.bitrate);
            args << "-rc" << "cbr";
        }

    } else if (m_config.codec.contains("qsv")) {
        // Intel Quick Sync settings
        args << "-preset" << "medium";

        if (m_config.rateControl == CRF_MODE) {
            args << "-global_quality" << QString::number(m_config.crf);
        } else {
            args << "-b:v" << QString::number(m_config.bitrate);
        }

    } else if (m_config.codec.contains("vaapi")) {
        // VA-API settings
        if (m_config.rateControl == CRF_MODE) {
            args << "-qp" << QString::number(m_config.crf);
        } else {
            args << "-b:v" << QString::number(m_config.bitrate);
        }
    }

    // Output file
    args << m_config.outputPath;

    return args;
}

bool VideoEncoder::encodeFrame(const QImage& frame)
{
    if (!m_initialized || !m_ffmpegProcess) {
        setError(tr("Encoder not initialized"));
        return false;
    }

    if (m_ffmpegProcess->state() != QProcess::Running) {
        setError(tr("FFmpeg process is not running"));
        return false;
    }

    // Convert QImage to correct format if needed
    QImage convertedFrame = frame;
    if (frame.format() != QImage::Format_ARGB32 &&
        frame.format() != QImage::Format_ARGB32_Premultiplied) {
        convertedFrame = frame.convertToFormat(QImage::Format_ARGB32);
    }

    // Scale image if dimensions don't match
    if (convertedFrame.width() != m_config.width ||
        convertedFrame.height() != m_config.height) {
        convertedFrame = convertedFrame.scaled(m_config.width,
                                                m_config.height,
                                                Qt::IgnoreAspectRatio,
                                                Qt::SmoothTransformation);
    }

    // Write raw frame data to ffmpeg stdin
    // ARGB32 format: 4 bytes per pixel
    qint64 frameSize = m_config.width * m_config.height * 4;
    qint64 bytesWritten = m_ffmpegProcess->write(
        reinterpret_cast<const char*>(convertedFrame.constBits()),
        frameSize);

    if (bytesWritten != frameSize) {
        setError(tr("Failed to write frame data to FFmpeg"));
        return false;
    }

    m_frameCount++;
    qint64 timeMs = (m_frameCount * 1000) / m_config.framerate;
    emit encodingProgress(m_frameCount, timeMs);

    return true;
}

bool VideoEncoder::finalize()
{
    if (!m_initialized || !m_ffmpegProcess) {
        return true;
    }

    m_finalizing = true;

    // Close stdin to signal end of input to ffmpeg
    m_ffmpegProcess->closeWriteChannel();

    // Wait for ffmpeg to finish encoding
    if (!m_ffmpegProcess->waitForFinished(30000)) { // 30 second timeout
        m_ffmpegProcess->kill();
        setError(tr("FFmpeg process timed out"));
        cleanup();
        return false;
    }

    int exitCode = m_ffmpegProcess->exitCode();
    if (exitCode != 0) {
        QString errorOutput = QString::fromUtf8(m_ffmpegProcess->readAllStandardError());
        setError(tr("FFmpeg exited with error code %1:\n%2")
                     .arg(exitCode)
                     .arg(errorOutput.right(500))); // Last 500 chars of error
        cleanup();
        return false;
    }

    m_initialized = false;
    emit encodingFinished(m_config.outputPath);

    cleanup();
    return true;
}

void VideoEncoder::onProcessError(QProcess::ProcessError error)
{
    if (m_finalizing) {
        return; // Ignore errors during finalization
    }

    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = tr("FFmpeg failed to start. Is it installed?");
            break;
        case QProcess::Crashed:
            errorMsg = tr("FFmpeg crashed unexpectedly");
            break;
        case QProcess::WriteError:
            errorMsg = tr("Failed to write to FFmpeg process");
            break;
        default:
            errorMsg = tr("FFmpeg process error: %1").arg(error);
            break;
    }

    setError(errorMsg);
}

void VideoEncoder::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_finalizing) {
        return; // Handled in finalize()
    }

    if (status == QProcess::CrashExit || exitCode != 0) {
        QString errorOutput = QString::fromUtf8(m_ffmpegProcess->readAllStandardError());
        setError(tr("FFmpeg process ended unexpectedly (exit code %1):\n%2")
                     .arg(exitCode)
                     .arg(errorOutput.right(500)));
    }
}

void VideoEncoder::cleanup()
{
    if (m_ffmpegProcess) {
        if (m_ffmpegProcess->state() != QProcess::NotRunning) {
            m_ffmpegProcess->kill();
            m_ffmpegProcess->waitForFinished(1000);
        }
        m_ffmpegProcess->deleteLater();
        m_ffmpegProcess = nullptr;
    }

    m_initialized = false;
    m_finalizing = false;
}

void VideoEncoder::setError(const QString& error)
{
    m_errorString = error;
    emit encodingError(error);
}

QString VideoEncoder::errorString() const
{
    return m_errorString;
}

qint64 VideoEncoder::encodedFrames() const
{
    return m_frameCount;
}

bool VideoEncoder::isInitialized() const
{
    return m_initialized;
}

QString VideoEncoder::buildCommandPreview(int width, int height)
{
    ConfigHandler config;
    QStringList args;

    QString ffmpegPath = FFmpegUtils::ffmpegPath();
    if (ffmpegPath.isEmpty()) {
        ffmpegPath = "ffmpeg";
    }

    // Get config values
    int framerate = config.videoFramerate();
    QString codec = config.videoCodec();
    QString preset = config.videoPreset();
    bool useCrf = config.videoUseCrf();
    int crf = config.videoCrf();
    int bitrate = config.videoBitrate();

    // Use placeholders for width/height if not specified
    QString widthStr = width > 0 ? QString::number(width) : "{width}";
    QString heightStr = height > 0 ? QString::number(height) : "{height}";

    // Ensure dimensions are even (required by most codecs)
    if (width > 0) {
        width = (width + 1) & ~1;
        widthStr = QString::number(width);
    }
    if (height > 0) {
        height = (height + 1) & ~1;
        heightStr = QString::number(height);
    }

    // Build command
    args << ffmpegPath;

    // Overwrite output without asking
    args << "-y";

    // Input: raw video from stdin
    args << "-f" << "rawvideo";
    args << "-pix_fmt" << "bgra";
    args << "-s" << QString("%1x%2").arg(widthStr, heightStr);
    args << "-r" << QString::number(framerate);
    args << "-i" << "pipe:0";

    // Video codec
    args << "-c:v" << codec;

    // Output pixel format
    if (codec.contains("qsv") || codec.contains("vaapi")) {
        args << "-pix_fmt" << "nv12";
    } else {
        args << "-pix_fmt" << "yuv420p";
    }

    // Encoder-specific settings
    if (codec == "libx264" || codec == "libx265") {
        args << "-preset" << preset;

        if (useCrf) {
            args << "-crf" << QString::number(crf);
        } else {
            args << "-b:v" << QString::number(bitrate);
        }

        args << "-tune" << "zerolatency";

    } else if (codec.contains("nvenc")) {
        args << "-preset" << "p4";
        args << "-tune" << "ll";

        if (useCrf) {
            args << "-cq" << QString::number(crf);
            args << "-rc" << "vbr";
        } else {
            args << "-b:v" << QString::number(bitrate);
            args << "-rc" << "cbr";
        }

    } else if (codec.contains("qsv")) {
        args << "-preset" << "medium";

        if (useCrf) {
            args << "-global_quality" << QString::number(crf);
        } else {
            args << "-b:v" << QString::number(bitrate);
        }

    } else if (codec.contains("vaapi")) {
        if (useCrf) {
            args << "-qp" << QString::number(crf);
        } else {
            args << "-b:v" << QString::number(bitrate);
        }
    }

    // Output file placeholder
    args << "{output}";

    return args.join(" ");
}

QStringList VideoEncoder::parseCustomCommand(const QString& command)
{
    QStringList args;
    QString current;
    bool inQuote = false;
    QChar quoteChar;

    for (int i = 0; i < command.length(); ++i) {
        QChar c = command[i];

        if (inQuote) {
            if (c == quoteChar) {
                inQuote = false;
            } else {
                current += c;
            }
        } else {
            if (c == '"' || c == '\'') {
                inQuote = true;
                quoteChar = c;
            } else if (c.isSpace()) {
                if (!current.isEmpty()) {
                    args << current;
                    current.clear();
                }
            } else {
                current += c;
            }
        }
    }

    if (!current.isEmpty()) {
        args << current;
    }

    return args;
}
