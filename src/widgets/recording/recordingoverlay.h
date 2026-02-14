// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "src/recording/recordingcontext.h"
#include <QTimer>
#include <QWidget>

class CaptureButton;
class QLabel;
class RecordingController;

// Thin border widget for one edge of the recording frame
class RecordingFrameEdge : public QWidget
{
    Q_OBJECT
public:
    explicit RecordingFrameEdge(const QColor& color, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_color;
};

class RecordingOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingOverlay(RecordingController* controller,
                              const QRect& recordingRegion,
                              QWidget* parent = nullptr);
    ~RecordingOverlay() override;

signals:
    void pauseRequested();
    void resumeRequested();
    void stopRequested();
    void abortRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

public slots:
    void updateRegion(const QRect& newRegion);
    void setEdgeVisibility(bool top, bool bottom, bool left, bool right);

private slots:
    void updateDisplay();
    void onPauseClicked();
    void onStopClicked();
    void onAbortClicked();
    void onStateChanged(RecordingContext::RecordingState state);

private:
    void setupUI();
    void setupFrame();
    void cleanupFrame();
    void positionControlPanel();
    QString formatTime(qint64 ms) const;
    void updatePauseButton();

    RecordingController* m_controller;
    QRect m_recordingRegion;
    QColor m_uiColor;
    QLabel* m_timeLabel;
    QLabel* m_indicator;
    CaptureButton* m_pauseBtn;
    CaptureButton* m_stopBtn;
    QTimer m_updateTimer;

    // Frame edges (outside recording area)
    RecordingFrameEdge* m_topEdge = nullptr;
    RecordingFrameEdge* m_bottomEdge = nullptr;
    RecordingFrameEdge* m_leftEdge = nullptr;
    RecordingFrameEdge* m_rightEdge = nullptr;

    // For dragging
    bool m_dragging = false;
    QPoint m_dragOffset;

    static const int FRAME_THICKNESS = 3;
};
