// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

/**
 * @brief Utility class for checking FFmpeg availability and capabilities
 *
 * This class provides static methods to check if ffmpeg is installed,
 * query available encoders, and get installation instructions.
 */
class FFmpegUtils
{
public:
    struct EncoderInfo
    {
        QString id;          // e.g., "libx264"
        QString displayName; // e.g., "H.264 (Software)"
        QString description; // e.g., "libx264 H.264 / AVC / MPEG-4 AVC"
        bool isHardware;     // true for nvenc, qsv, vaapi
    };

    /**
     * @brief Check if ffmpeg is installed and accessible
     */
    static bool isInstalled();

    /**
     * @brief Get ffmpeg version string
     */
    static QString version();

    /**
     * @brief Get list of available video encoders
     */
    static QList<EncoderInfo> availableEncoders();

    /**
     * @brief Get installation command for current distribution
     */
    static QString installCommand();

    /**
     * @brief Get installation commands for various distributions
     */
    static QMap<QString, QString> installCommands();

    /**
     * @brief Check if a specific encoder is available
     */
    static bool hasEncoder(const QString& encoderId);

    /**
     * @brief Get the path to ffmpeg binary
     */
    static QString ffmpegPath();

private:
    static QStringList runFFmpeg(const QStringList& args, int timeoutMs = 5000);
};
