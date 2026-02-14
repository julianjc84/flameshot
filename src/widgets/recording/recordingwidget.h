// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "src/recording/recordingrequest.h"
#include <QTimer>
#include <QWidget>

class RecordingController;
class RecordingOverlay;
class SelectionWidget;
class CaptureButton;
class QPushButton;
class QLabel;

class RecordingWidget : public QWidget
{
    Q_OBJECT

public:
    // Preset aspect ratios
    struct AspectRatio
    {
        QString name;
        int width;
        int height;

        float ratio() const
        {
            return height > 0 ? static_cast<float>(width) / height : 0;
        }
        bool isNone() const { return width == 0 && height == 0; }
        bool isFullscreen() const { return width == -1 && height == -1; }
    };

    explicit RecordingWidget(const RecordingRequest& req = RecordingRequest(),
                             QWidget* parent = nullptr);
    ~RecordingWidget() override;

signals:
    void recordingStarted();
    void recordingFinished(const QString& outputPath);
    void recordingCancelled();
    void recordingError(const QString& error);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void startRecording();
    void stopRecording();
    void abortRecording();
    void onRecordingFinished(const QString& outputPath);
    void onRecordingError(const QString& error);
    void onEncoderFallback(const QString& originalCodec,
                           const QString& fallbackCodec);
    void onSelectionChanged();
    void onSelectionSettled();

private:
    void initBackground();
    void initAspectRatioPanel();
    void confirmRegion();
    QString generateOutputPath() const;
    void updateAspectRatioPanel();
    void setAspectRatio(const AspectRatio& ratio);
    void applyAspectRatio();
    void applyAspectRatioFromResize();
    QRect constrainToAspectRatio(const QRect& rect, const AspectRatio& ratio);
    QRect constrainedRectFromDrag(const QPoint& anchor,
                                   const QPoint& cursor,
                                   const AspectRatio& ratio);
    QRect constrainedRectWithAnchor(const QRect& rect,
                                     const AspectRatio& ratio,
                                     const QPoint& anchor);
    void clampSelectionToScreen();

    RecordingRequest m_request;
    RecordingController* m_controller;
    RecordingOverlay* m_overlay = nullptr;

    // Selection
    SelectionWidget* m_selectionWidget = nullptr;
    QPixmap m_backgroundScreenshot;
    bool m_recordingActive = false;
    bool m_initialSelection = false;
    bool m_resizeInProgress = false;
    QPoint m_dragStart;
    QRect m_selectionBeforeResize;

    // Aspect ratio panel
    QWidget* m_aspectPanel = nullptr;
    QVector<CaptureButton*> m_aspectButtons;
    QLabel* m_sizeLabel = nullptr;
    CaptureButton* m_startButton = nullptr;
    AspectRatio m_currentAspectRatio;
    static const QVector<AspectRatio> s_aspectRatios;

    // Colors
    QColor m_uiColor;
    QColor m_contrastUiColor;
    QColor m_overlayColor;

    // Selection geometry display
    bool m_xywhDisplay = false;
    QTimer m_xywhTimer;
};
