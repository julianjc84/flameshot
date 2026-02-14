// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include <QWidget>

class QVBoxLayout;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QRadioButton;
class QSlider;
class QGroupBox;

class VideoConf : public QWidget
{
    Q_OBJECT

public:
    explicit VideoConf(QWidget* parent = nullptr);

public slots:
    void updateComponents();

private slots:
    void changeSavePath();
    void framerateChanged(int fps);
    void crfChanged(int crf);
    void bitrateChanged(int kbps);
    void presetChanged(const QString& preset);
    void formatChanged(const QString& format);
    void codecChanged(const QString& codec);
    void filenamePatternEdited();
    void rateControlChanged();
    void aspectRatioChanged(const QString& ratio);
    void outputModeChanged();
private:
    void initFFmpegWarning();
    void initOutputMode();
    void initSavePath();
    void initFilenamePattern();
    void initFramerate();
    void initRateControl();
    void initPreset();
    void initFormat();
    void initCodec();
    void initRecordingOptions();
    void updateRateControlUI();
    void updateFilenamePreview();
    void updateVideoOptionsVisibility();
    const QString chooseFolder(const QString& currentPath = "");

    bool m_ffmpegInstalled = false;
    QVBoxLayout* m_layout;

    // Output mode (Video/GIF)
    QRadioButton* m_videoModeRadio;
    QRadioButton* m_gifModeRadio;

    // Save location
    QLineEdit* m_savePath;
    QPushButton* m_changeSaveButton;

    // Filename
    QLineEdit* m_filenamePattern;
    QLabel* m_filenamePreview;

    // Framerate
    QSpinBox* m_framerate;

    // Rate control
    QGroupBox* m_rateControlBox;
    QRadioButton* m_crfRadio;
    QRadioButton* m_bitrateRadio;
    QSlider* m_crfSlider;
    QLabel* m_crfValueLabel;
    QLabel* m_crfHintLabel;
    QSpinBox* m_bitrate;
    QLabel* m_bitrateHintLabel;

    // Preset
    QComboBox* m_preset;

    // Format
    QComboBox* m_format;

    // Codec/Encoder
    QComboBox* m_codec;

    // Recording options
    QComboBox* m_aspectRatio;

    // Group boxes for visibility toggling (video-only options)
    QGroupBox* m_codecBox;
    QGroupBox* m_presetBox;
    QGroupBox* m_formatBox;
    QGroupBox* m_framerateBox;

};
