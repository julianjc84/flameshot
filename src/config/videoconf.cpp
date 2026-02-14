// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "videoconf.h"
#include "src/recording/ffmpegutils.h"
#include "src/utils/confighandler.h"
#include "src/utils/filenamehandler.h"

#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

VideoConf::VideoConf(QWidget* parent)
  : QWidget(parent)
{
    // Create scroll area for the content (match other settings tabs)
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Create container widget for scroll area
    auto* scrollContent = new QWidget();
    scrollContent->setObjectName("videoContent");
    scrollArea->setObjectName("videoScrollArea");
    scrollArea->setStyleSheet(
      "#videoContent, #videoScrollArea { background: transparent; border: 0px; }");
    m_layout = new QVBoxLayout(scrollContent);
    m_layout->setContentsMargins(0, 0, 20, 0);
    m_layout->setAlignment(Qt::AlignTop);

    // Check if FFmpeg is installed
    m_ffmpegInstalled = FFmpegUtils::isInstalled();

    if (!m_ffmpegInstalled) {
        initFFmpegWarning();
    } else {
        initOutputMode();
        initSavePath();
        initFilenamePattern();
        initRecordingOptions();
        initFramerate();
        initCodec();
        initPreset();
        initRateControl();
        initFormat();
    }

    m_layout->addStretch();

    scrollArea->setWidget(scrollContent);

    // Main layout just contains the scroll area
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);

    if (m_ffmpegInstalled) {
        updateComponents();
    }
}

void VideoConf::initFFmpegWarning()
{
    auto* box = new QGroupBox(tr("FFmpeg Required"));
    auto* layout = new QVBoxLayout(box);

    // Warning icon and message
    auto* warningLabel = new QLabel(
        tr("FFmpeg is required for video recording but was not found on your system."),
        this);
    warningLabel->setWordWrap(true);
    warningLabel->setStyleSheet("font-weight: bold; color: #cc6600;");
    layout->addWidget(warningLabel);

    layout->addSpacing(10);

    // Install command
    QString installCmd = FFmpegUtils::installCommand();
    auto* installLabel = new QLabel(tr("Install FFmpeg:"), this);
    layout->addWidget(installLabel);

    auto* cmdLayout = new QHBoxLayout();
    auto* cmdEdit = new QLineEdit(installCmd, this);
    cmdEdit->setReadOnly(true);
    cmdEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2d2d2d;"
        "  color: #00ff00;"
        "  font-family: monospace;"
        "  padding: 8px;"
        "  border: 1px solid #555;"
        "  border-radius: 4px;"
        "}");

    auto* copyBtn = new QPushButton(tr("Copy"), this);
    connect(copyBtn, &QPushButton::clicked, this, [installCmd]() {
        QGuiApplication::clipboard()->setText(installCmd);
    });

    cmdLayout->addWidget(cmdEdit);
    cmdLayout->addWidget(copyBtn);
    layout->addLayout(cmdLayout);

    layout->addSpacing(10);

    // Other distros
    auto* otherLabel = new QLabel(tr("Other distributions:"), this);
    otherLabel->setStyleSheet("color: gray;");
    layout->addWidget(otherLabel);

    QMap<QString, QString> commands = FFmpegUtils::installCommands();
    for (auto it = commands.begin(); it != commands.end(); ++it) {
        auto* distroLabel = new QLabel(
            QString("  %1: %2").arg(it.key(), it.value()), this);
        distroLabel->setStyleSheet("color: gray; font-size: 11px;");
        layout->addWidget(distroLabel);
    }

    layout->addSpacing(15);

    // Check again button
    auto* checkBtn = new QPushButton(tr("Check Again"), this);
    connect(checkBtn, &QPushButton::clicked, this, [this]() {
        if (FFmpegUtils::isInstalled()) {
            // Reload the page - simplest is to inform user to reopen settings
            QLabel* successLabel = new QLabel(
                tr("FFmpeg found! Please close and reopen settings."),
                this);
            successLabel->setStyleSheet("color: #00aa00; font-weight: bold;");
            m_layout->insertWidget(1, successLabel);
        }
    });
    layout->addWidget(checkBtn);

    m_layout->addWidget(box);
}

void VideoConf::initOutputMode()
{
    auto* box = new QGroupBox(tr("Output Type"));
    auto* mainLayout = new QVBoxLayout(box);

    auto* radioLayout = new QHBoxLayout();
    m_videoModeRadio = new QRadioButton(tr("Video"), this);
    m_gifModeRadio = new QRadioButton(tr("GIF"), this);

    connect(m_videoModeRadio, &QRadioButton::toggled,
            this, &VideoConf::outputModeChanged);
    connect(m_gifModeRadio, &QRadioButton::toggled,
            this, &VideoConf::outputModeChanged);

    radioLayout->addWidget(m_videoModeRadio);
    radioLayout->addWidget(m_gifModeRadio);
    radioLayout->addStretch();
    mainLayout->addLayout(radioLayout);

    auto* hintLabel = new QLabel(
        tr("Video: MP4/MKV with configurable codec and quality\n"
           "GIF: Animated GIF with optimized palette (post-processed for quality)"),
        this);
    hintLabel->setStyleSheet("color: gray; font-size: 10px;");
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    m_layout->addWidget(box);
}

void VideoConf::updateComponents()
{
    // Wrap everything in try-catch to prevent UI from breaking
    try {
        ConfigHandler config;

        // Output mode
        if (m_videoModeRadio && m_gifModeRadio) {
            QString outputMode = config.videoOutputMode();
            m_videoModeRadio->setChecked(outputMode == "video");
            m_gifModeRadio->setChecked(outputMode == "gif");
            updateVideoOptionsVisibility();
        }

        // Save path
        if (m_savePath) {
            m_savePath->setText(config.videoSavePath());
        }

        // Filename pattern
        if (m_filenamePattern) {
            m_filenamePattern->setText(config.videoFilenamePattern());
            updateFilenamePreview();
        }

        // Framerate
        if (m_framerate) {
            m_framerate->setValue(config.videoFramerate());
        }

        // Rate control mode
        if (m_crfRadio && m_bitrateRadio) {
            bool useCrf = config.videoUseCrf();
            m_crfRadio->setChecked(useCrf);
            m_bitrateRadio->setChecked(!useCrf);
            updateRateControlUI();
        }

        // CRF value
        if (m_crfSlider && m_crfValueLabel) {
            int crf = config.videoCrf();
            m_crfSlider->setValue(crf);
            m_crfValueLabel->setText(QString::number(crf));
        }

        // Bitrate
        if (m_bitrate) {
            m_bitrate->setValue(config.videoBitrate() / 1000); // Convert to kbps for display
        }

        // Preset
        if (m_preset) {
            QString preset = config.videoPreset();
            int presetIndex = m_preset->findText(preset);
            if (presetIndex >= 0) {
                m_preset->setCurrentIndex(presetIndex);
            } else {
                m_preset->setCurrentIndex(0);
            }
        }

        // Format
        if (m_format) {
            QString format = config.videoFormat();
            int formatIndex = m_format->findText(format);
            if (formatIndex >= 0) {
                m_format->setCurrentIndex(formatIndex);
            } else {
                m_format->setCurrentIndex(0);
            }
        }

        // Codec - with fallback if saved codec not available
        if (m_codec && m_codec->count() > 0) {
            QString codec = config.videoCodec();
            int codecIndex = m_codec->findData(codec);
            if (codecIndex >= 0) {
                m_codec->setCurrentIndex(codecIndex);
            } else {
                // Saved codec not available, fall back to first available
                m_codec->setCurrentIndex(0);
                // Update config with the fallback codec
                QString fallbackCodec = m_codec->currentData().toString();
                if (!fallbackCodec.isEmpty()) {
                    ConfigHandler().setVideoCodec(fallbackCodec);
                }
            }
        }

        // Aspect ratio
        if (m_aspectRatio) {
            QString aspectRatio = config.videoAspectRatio();
            int aspectIndex = m_aspectRatio->findText(aspectRatio);
            if (aspectIndex >= 0) {
                m_aspectRatio->setCurrentIndex(aspectIndex);
            } else {
                m_aspectRatio->setCurrentIndex(0);
            }
        }

    } catch (...) {
        // If anything fails, the UI should still be functional
        // Just skip updating the components
    }
}

void VideoConf::initSavePath()
{
    auto* box = new QGroupBox(tr("Save Location"));
    auto* layout = new QHBoxLayout(box);

    m_savePath = new QLineEdit(this);
    m_savePath->setReadOnly(true);

    m_changeSaveButton = new QPushButton(tr("Change..."), this);
    connect(m_changeSaveButton, &QPushButton::clicked,
            this, &VideoConf::changeSavePath);

    layout->addWidget(m_savePath);
    layout->addWidget(m_changeSaveButton);

    m_layout->addWidget(box);
}

void VideoConf::initFilenamePattern()
{
    auto* box = new QGroupBox(tr("Filename Pattern"));
    auto* layout = new QVBoxLayout(box);

    auto* helpLabel = new QLabel(
      tr("Use the same patterns as screenshot filenames:\n"
         "%F = date, %T = time, %H = hour, %M = minute, %S = second"),
      this);
    helpLabel->setWordWrap(true);

    m_filenamePattern = new QLineEdit(this);
    connect(m_filenamePattern, &QLineEdit::editingFinished,
            this, &VideoConf::filenamePatternEdited);

    m_filenamePreview = new QLabel(this);
    m_filenamePreview->setStyleSheet("color: gray;");

    layout->addWidget(helpLabel);
    layout->addWidget(m_filenamePattern);
    layout->addWidget(m_filenamePreview);

    m_layout->addWidget(box);
}

void VideoConf::initRecordingOptions()
{
    auto* box = new QGroupBox(tr("Recording Options"));
    auto* layout = new QVBoxLayout(box);

    // Aspect ratio dropdown
    auto* aspectLayout = new QHBoxLayout();
    auto* aspectLabel = new QLabel(tr("Default aspect ratio:"), this);

    m_aspectRatio = new QComboBox(this);
    // Note: Fullscreen is available during recording (click the button) but not
    // saved as a preference, so it's not listed here. Recording always starts
    // with a blank slate ready for user selection.
    m_aspectRatio->addItems({
        "Free",
        "16:9",
        "4:3",
        "1:1",
        "21:9",
        "9:16"
    });
    connect(m_aspectRatio, &QComboBox::currentTextChanged,
            this, &VideoConf::aspectRatioChanged);

    aspectLayout->addWidget(aspectLabel);
    aspectLayout->addWidget(m_aspectRatio);
    aspectLayout->addStretch();
    layout->addLayout(aspectLayout);

    auto* aspectHint = new QLabel(
        tr("Free: Draw any region\n"
           "Ratios: Constrain selection to aspect ratio"),
        this);
    aspectHint->setStyleSheet("color: gray; font-size: 10px;");
    aspectHint->setWordWrap(true);
    layout->addWidget(aspectHint);

    m_layout->addWidget(box);
}

void VideoConf::initCodec()
{
    m_codecBox = new QGroupBox(tr("Encoder"));
    auto* layout = new QVBoxLayout(m_codecBox);

    auto* codecLayout = new QHBoxLayout();
    auto* codecLabel = new QLabel(tr("Video Encoder:"), this);

    m_codec = new QComboBox(this);

    // Get available encoders from FFmpeg
    QList<FFmpegUtils::EncoderInfo> encoders = FFmpegUtils::availableEncoders();

    for (const auto& encoder : encoders) {
        m_codec->addItem(encoder.displayName, encoder.id);
    }

    // Fallback if nothing found - always ensure at least one encoder
    if (m_codec->count() == 0) {
        m_codec->addItem("H.264 (Software)", "libx264");
    }

    connect(m_codec, &QComboBox::currentTextChanged,
            this, [this]() {
                if (m_codec && m_codec->currentData().isValid()) {
                    codecChanged(m_codec->currentData().toString());
                }
            });

    auto* codecHint = new QLabel(
      tr("Hardware encoders (NVENC, Quick Sync, VA-API) are faster but require compatible GPU"),
      this);
    codecHint->setStyleSheet("color: gray; font-size: 10px;");
    codecHint->setWordWrap(true);

    codecLayout->addWidget(codecLabel);
    codecLayout->addWidget(m_codec);
    codecLayout->addStretch();

    layout->addLayout(codecLayout);
    layout->addWidget(codecHint);

    m_layout->addWidget(m_codecBox);
}

void VideoConf::initPreset()
{
    m_presetBox = new QGroupBox(tr("Encoding Speed"));
    auto* layout = new QVBoxLayout(m_presetBox);

    auto* presetLayout = new QHBoxLayout();
    auto* presetLabel = new QLabel(tr("Preset:"), this);

    m_preset = new QComboBox(this);
    m_preset->addItems({
        "ultrafast",
        "superfast",
        "veryfast",
        "faster",
        "fast",
        "medium",
        "slow",
        "slower",
        "veryslow"
    });
    connect(m_preset, &QComboBox::currentTextChanged,
            this, &VideoConf::presetChanged);

    auto* presetHint = new QLabel(
      tr("Faster = larger files, less CPU usage\n"
         "Slower = smaller files, more CPU usage"),
      this);
    presetHint->setStyleSheet("color: gray; font-size: 10px;");

    presetLayout->addWidget(presetLabel);
    presetLayout->addWidget(m_preset);
    presetLayout->addStretch();

    layout->addLayout(presetLayout);
    layout->addWidget(presetHint);

    m_layout->addWidget(m_presetBox);
}

void VideoConf::initFramerate()
{
    m_framerateBox = new QGroupBox(tr("Framerate"));
    auto* layout = new QVBoxLayout(m_framerateBox);

    auto* fpsLayout = new QHBoxLayout();
    auto* fpsLabel = new QLabel(tr("Frames per second:"), this);
    m_framerate = new QSpinBox(this);
    m_framerate->setRange(1, 120);
    m_framerate->setValue(30);
    m_framerate->setSuffix(" fps");
    connect(m_framerate, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VideoConf::framerateChanged);

    fpsLayout->addWidget(fpsLabel);
    fpsLayout->addWidget(m_framerate);
    fpsLayout->addStretch();
    layout->addLayout(fpsLayout);

    auto* fpsHint = new QLabel(
        tr("For GIF: 10-15 fps recommended for smaller files"),
        this);
    fpsHint->setStyleSheet("color: gray; font-size: 10px;");
    layout->addWidget(fpsHint);

    m_layout->addWidget(m_framerateBox);
}

void VideoConf::initRateControl()
{
    m_rateControlBox = new QGroupBox(tr("Quality / File Size"));
    auto* layout = new QVBoxLayout(m_rateControlBox);

    // Radio buttons for mode selection
    auto* modeLayout = new QHBoxLayout();
    m_crfRadio = new QRadioButton(tr("Quality (CRF)"), this);
    m_bitrateRadio = new QRadioButton(tr("Bitrate"), this);
    m_crfRadio->setChecked(true);

    connect(m_crfRadio, &QRadioButton::toggled, this, &VideoConf::rateControlChanged);
    connect(m_bitrateRadio, &QRadioButton::toggled, this, &VideoConf::rateControlChanged);

    modeLayout->addWidget(m_crfRadio);
    modeLayout->addWidget(m_bitrateRadio);
    modeLayout->addStretch();
    layout->addLayout(modeLayout);

    // CRF slider
    auto* crfLayout = new QHBoxLayout();
    auto* crfLabel = new QLabel(tr("CRF:"), this);
    m_crfSlider = new QSlider(Qt::Horizontal, this);
    m_crfSlider->setRange(0, 51);
    m_crfSlider->setValue(23);
    m_crfSlider->setTickPosition(QSlider::TicksBelow);
    m_crfSlider->setTickInterval(5);
    m_crfSlider->setFocusPolicy(Qt::StrongFocus);

    m_crfValueLabel = new QLabel("23", this);
    m_crfValueLabel->setMinimumWidth(30);

    connect(m_crfSlider, &QSlider::valueChanged, this, [this](int value) {
        m_crfValueLabel->setText(QString::number(value));
        crfChanged(value);
    });

    crfLayout->addWidget(crfLabel);
    crfLayout->addWidget(m_crfSlider);
    crfLayout->addWidget(m_crfValueLabel);
    layout->addLayout(crfLayout);

    m_crfHintLabel = new QLabel(
      tr("0 = lossless, 18 = visually lossless, 23 = default, 28 = smaller files"),
      this);
    m_crfHintLabel->setStyleSheet("color: gray; font-size: 10px;");
    layout->addWidget(m_crfHintLabel);

    // Bitrate spinbox
    auto* bitrateLayout = new QHBoxLayout();
    auto* bitrateLabel = new QLabel(tr("Bitrate:"), this);
    m_bitrate = new QSpinBox(this);
    m_bitrate->setRange(100, 50000); // 100 kbps to 50 Mbps
    m_bitrate->setValue(4000); // 4 Mbps default
    m_bitrate->setSuffix(" kbps");
    m_bitrate->setSingleStep(500);
    connect(m_bitrate, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VideoConf::bitrateChanged);

    bitrateLayout->addWidget(bitrateLabel);
    bitrateLayout->addWidget(m_bitrate);
    bitrateLayout->addStretch();
    layout->addLayout(bitrateLayout);

    m_bitrateHintLabel = new QLabel(
      tr("Higher bitrate = better quality, larger files (4000 kbps recommended)"),
      this);
    m_bitrateHintLabel->setStyleSheet("color: gray; font-size: 10px;");
    layout->addWidget(m_bitrateHintLabel);

    m_layout->addWidget(m_rateControlBox);
}

void VideoConf::initFormat()
{
    m_formatBox = new QGroupBox(tr("Output Format"));
    auto* layout = new QHBoxLayout(m_formatBox);

    auto* formatLabel = new QLabel(tr("Container:"), this);
    m_format = new QComboBox(this);
    m_format->addItems({ "mp4", "mkv", "webm" });
    connect(m_format, &QComboBox::currentTextChanged,
            this, &VideoConf::formatChanged);

    layout->addWidget(formatLabel);
    layout->addWidget(m_format);
    layout->addStretch();

    m_layout->addWidget(m_formatBox);
}

void VideoConf::updateRateControlUI()
{
    if (!m_crfRadio) return;

    bool useCrf = m_crfRadio->isChecked();

    if (m_crfSlider) m_crfSlider->setEnabled(useCrf);
    if (m_crfValueLabel) m_crfValueLabel->setEnabled(useCrf);
    if (m_crfHintLabel) m_crfHintLabel->setEnabled(useCrf);

    if (m_bitrate) m_bitrate->setEnabled(!useCrf);
    if (m_bitrateHintLabel) m_bitrateHintLabel->setEnabled(!useCrf);
}

void VideoConf::updateFilenamePreview()
{
    if (!m_filenamePattern || !m_filenamePreview) return;

    FileNameHandler handler;
    QString format;
    if (m_gifModeRadio && m_gifModeRadio->isChecked()) {
        format = "gif";
    } else {
        format = m_format ? m_format->currentText() : "mp4";
    }
    QString preview = handler.parseFilename(m_filenamePattern->text()) + "." + format;
    m_filenamePreview->setText(tr("Preview: %1").arg(preview));
}

void VideoConf::changeSavePath()
{
    if (!m_savePath) return;

    QString path = chooseFolder(m_savePath->text());
    if (!path.isEmpty()) {
        m_savePath->setText(path);
        ConfigHandler().setVideoSavePath(path);
    }
}

void VideoConf::framerateChanged(int fps)
{
    ConfigHandler().setVideoFramerate(fps);
}

void VideoConf::crfChanged(int crf)
{
    ConfigHandler().setVideoCrf(crf);
}

void VideoConf::bitrateChanged(int kbps)
{
    ConfigHandler().setVideoBitrate(kbps * 1000); // Convert to bps
}

void VideoConf::presetChanged(const QString& preset)
{
    ConfigHandler().setVideoPreset(preset);
}

void VideoConf::formatChanged(const QString& format)
{
    ConfigHandler().setVideoFormat(format);
    updateFilenamePreview();
}

void VideoConf::codecChanged(const QString& codec)
{
    ConfigHandler().setVideoCodec(codec);

    // Update preset visibility based on codec
    // Presets only apply to software encoders (libx264, libx265)
    bool isSoftwareEncoder = codec == "libx264" || codec == "libx265";
    if (m_presetBox) {
        m_presetBox->setVisible(isSoftwareEncoder);
    }

}

void VideoConf::filenamePatternEdited()
{
    if (!m_filenamePattern) return;

    QString pattern = m_filenamePattern->text();
    if (pattern.isEmpty()) {
        pattern = "%F_%T";
        m_filenamePattern->setText(pattern);
    }
    ConfigHandler().setVideoFilenamePattern(pattern);
    updateFilenamePreview();
}

void VideoConf::rateControlChanged()
{
    if (!m_crfRadio) return;

    bool useCrf = m_crfRadio->isChecked();
    ConfigHandler().setVideoUseCrf(useCrf);
    updateRateControlUI();
}

void VideoConf::aspectRatioChanged(const QString& ratio)
{
    ConfigHandler().setVideoAspectRatio(ratio);
}

void VideoConf::outputModeChanged()
{
    bool isGifMode = m_gifModeRadio->isChecked();
    ConfigHandler().setVideoOutputMode(isGifMode ? "gif" : "video");
    updateVideoOptionsVisibility();
    updateFilenamePreview();

}

void VideoConf::updateVideoOptionsVisibility()
{
    if (!m_gifModeRadio) return;

    bool isGifMode = m_gifModeRadio->isChecked();

    // Hide video-specific options when GIF mode is selected
    // GIF doesn't need codec, preset, rate control, format, or custom command options
    if (m_codecBox) m_codecBox->setVisible(!isGifMode);
    if (m_presetBox) m_presetBox->setVisible(!isGifMode);
    if (m_rateControlBox) m_rateControlBox->setVisible(!isGifMode);
    if (m_formatBox) m_formatBox->setVisible(!isGifMode);
}

const QString VideoConf::chooseFolder(const QString& currentPath)
{
    QString path = currentPath;
    if (path.isEmpty()) {
        path = ConfigHandler().videoSavePath();
    }

    return QFileDialog::getExistingDirectory(
      this,
      tr("Choose a directory for video recordings"),
      path,
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}
