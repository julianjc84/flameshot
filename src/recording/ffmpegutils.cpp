// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "ffmpegutils.h"

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

bool FFmpegUtils::isInstalled()
{
    QString path = ffmpegPath();
    return !path.isEmpty();
}

QString FFmpegUtils::ffmpegPath()
{
    // Check if ffmpeg is in PATH
    QString path = QStandardPaths::findExecutable("ffmpeg");
    if (!path.isEmpty()) {
        return path;
    }

    // Check common locations
    QStringList commonPaths = {
#if defined(Q_OS_WIN)
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe",
#elif defined(Q_OS_MACOS)
        "/opt/homebrew/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
#else
        "/usr/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
        "/opt/ffmpeg/bin/ffmpeg",
#endif
    };

    for (const QString& p : commonPaths) {
        if (QFile::exists(p)) {
            return p;
        }
    }

    return QString();
}

QString FFmpegUtils::version()
{
    QStringList output = runFFmpeg({ "-version" });
    if (output.isEmpty()) {
        return QString();
    }

    // First line contains version info
    // e.g., "ffmpeg version 4.4.2-0ubuntu0.22.04.1 Copyright (c) ..."
    QString firstLine = output.first();
    QRegularExpression re("ffmpeg version ([\\d.]+)");
    QRegularExpressionMatch match = re.match(firstLine);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return firstLine;
}

QList<FFmpegUtils::EncoderInfo> FFmpegUtils::availableEncoders()
{
    QList<EncoderInfo> encoders;

    // Define encoders we're interested in with display names
    struct KnownEncoder
    {
        const char* id;
        const char* displayName;
        bool isHardware;
    };

    static const KnownEncoder knownEncoders[] = {
        { "libx264", "H.264 (Software)", false },
        { "libx265", "H.265/HEVC (Software)", false },
        { "h264_nvenc", "H.264 NVENC (NVIDIA GPU)", true },
        { "hevc_nvenc", "H.265 NVENC (NVIDIA GPU)", true },
        { "h264_qsv", "H.264 Quick Sync (Intel GPU)", true },
        { "hevc_qsv", "H.265 Quick Sync (Intel GPU)", true },
        { "h264_vaapi", "H.264 VA-API (Intel/AMD GPU)", true },
        { "hevc_vaapi", "H.265 VA-API (Intel/AMD GPU)", true },
        { "libvpx", "VP8 (Software)", false },
        { "libvpx-vp9", "VP9 (Software)", false },
#ifdef Q_OS_MACOS
        { "h264_videotoolbox", "H.264 VideoToolbox (Apple GPU)", true },
        { "hevc_videotoolbox", "H.265 VideoToolbox (Apple GPU)", true },
#endif
    };

    // Query ffmpeg for available encoders
    QStringList output = runFFmpeg({ "-encoders" });

    // Parse output to find which encoders are available
    // Format: " V..... libx264    libx264 H.264 / AVC / MPEG-4 AVC"
    for (const QString& line : output) {
        for (const auto& known : knownEncoders) {
            if (line.contains(QString(" %1 ").arg(known.id)) ||
                line.contains(QString(" %1\n").arg(known.id)) ||
                line.endsWith(QString(" %1").arg(known.id))) {

                // Extract description from line
                QRegularExpression re(QString("%1\\s+(.*)").arg(known.id));
                QRegularExpressionMatch match = re.match(line);
                QString description = match.hasMatch() ? match.captured(1).trimmed() : "";

                EncoderInfo info;
                info.id = known.id;
                info.displayName = known.displayName;
                info.description = description;
                info.isHardware = known.isHardware;
                encoders.append(info);
                break;
            }
        }
    }

    return encoders;
}

bool FFmpegUtils::hasEncoder(const QString& encoderId)
{
    QList<EncoderInfo> encoders = availableEncoders();
    for (const auto& enc : encoders) {
        if (enc.id == encoderId) {
            return true;
        }
    }
    return false;
}

QString FFmpegUtils::installCommand()
{
#if defined(Q_OS_WIN)
    return "winget install ffmpeg";
#elif defined(Q_OS_MACOS)
    return "brew install ffmpeg";
#else
    // Try to detect the distribution
    QFile osRelease("/etc/os-release");
    QString distroId;

    if (osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(osRelease.readAll());
        QRegularExpression re("^ID=(.*)$", QRegularExpression::MultilineOption);
        QRegularExpressionMatch match = re.match(content);
        if (match.hasMatch()) {
            distroId = match.captured(1).remove('"').toLower();
        }
        osRelease.close();
    }

    // Return appropriate command based on distro
    if (distroId == "ubuntu" || distroId == "debian" || distroId == "linuxmint" ||
        distroId == "pop" || distroId == "elementary" || distroId == "zorin") {
        return "sudo apt install ffmpeg";
    } else if (distroId == "fedora") {
        return "sudo dnf install ffmpeg";
    } else if (distroId == "arch" || distroId == "manjaro" || distroId == "endeavouros") {
        return "sudo pacman -S ffmpeg";
    } else if (distroId == "opensuse" || distroId == "opensuse-leap" ||
               distroId == "opensuse-tumbleweed") {
        return "sudo zypper install ffmpeg";
    } else if (distroId == "gentoo") {
        return "sudo emerge ffmpeg";
    } else if (distroId == "void") {
        return "sudo xbps-install ffmpeg";
    } else if (distroId == "alpine") {
        return "sudo apk add ffmpeg";
    }

    // Default to apt (most common)
    return "sudo apt install ffmpeg";
#endif
}

QMap<QString, QString> FFmpegUtils::installCommands()
{
    QMap<QString, QString> commands;
#if defined(Q_OS_WIN)
    commands["Windows (winget)"] = "winget install ffmpeg";
    commands["Windows (choco)"] = "choco install ffmpeg";
    commands["Windows (scoop)"] = "scoop install ffmpeg";
#elif defined(Q_OS_MACOS)
    commands["macOS (Homebrew)"] = "brew install ffmpeg";
    commands["macOS (MacPorts)"] = "sudo port install ffmpeg";
#else
    commands["Debian/Ubuntu/Mint"] = "sudo apt install ffmpeg";
    commands["Fedora"] = "sudo dnf install ffmpeg";
    commands["Arch/Manjaro"] = "sudo pacman -S ffmpeg";
    commands["openSUSE"] = "sudo zypper install ffmpeg";
    commands["Gentoo"] = "sudo emerge ffmpeg";
#endif
    return commands;
}

QStringList FFmpegUtils::runFFmpeg(const QStringList& args, int timeoutMs)
{
    QString ffmpeg = ffmpegPath();
    if (ffmpeg.isEmpty()) {
        return QStringList();
    }

    QProcess process;
    process.start(ffmpeg, args);

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        return QStringList();
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QString errorOutput = QString::fromUtf8(process.readAllStandardError());

    // FFmpeg often writes to stderr, so combine both
    QString combined = output + errorOutput;

    return combined.split('\n', Qt::SkipEmptyParts);
}
