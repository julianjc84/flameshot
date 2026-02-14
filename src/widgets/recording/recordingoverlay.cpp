// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "recordingoverlay.h"
#include "src/recording/recordingcontroller.h"
#include "src/utils/colorutils.h"
#include "src/utils/confighandler.h"
#include "src/widgets/capture/capturebutton.h"

#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

// RecordingFrameEdge implementation
RecordingFrameEdge::RecordingFrameEdge(const QColor& color, QWidget* parent)
  : QWidget(parent)
  , m_color(color)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::Tool | Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
}

void RecordingFrameEdge::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), m_color);
}

// RecordingOverlay implementation
RecordingOverlay::RecordingOverlay(RecordingController* controller,
                                   const QRect& recordingRegion,
                                   QWidget* parent)
  : QWidget(parent)
  , m_controller(controller)
  , m_recordingRegion(recordingRegion)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::Tool | Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);

    setupUI();
    setupFrame();

    // Connect to controller
    connect(m_controller,
            &RecordingController::stateChanged,
            this,
            &RecordingOverlay::onStateChanged);
    connect(m_controller,
            &RecordingController::recordingProgress,
            this,
            [this](qint64, qint64) { updateDisplay(); });

    // Start update timer
    m_updateTimer.setInterval(100);
    connect(&m_updateTimer, &QTimer::timeout, this, &RecordingOverlay::updateDisplay);
    m_updateTimer.start();

    // Position the control panel at the bottom of the recording region
    positionControlPanel();
}

RecordingOverlay::~RecordingOverlay()
{
    m_updateTimer.stop();
    cleanupFrame();
}

void RecordingOverlay::cleanupFrame()
{
    if (m_topEdge) {
        m_topEdge->close();
        delete m_topEdge;
        m_topEdge = nullptr;
    }
    if (m_bottomEdge) {
        m_bottomEdge->close();
        delete m_bottomEdge;
        m_bottomEdge = nullptr;
    }
    if (m_leftEdge) {
        m_leftEdge->close();
        delete m_leftEdge;
        m_leftEdge = nullptr;
    }
    if (m_rightEdge) {
        m_rightEdge->close();
        delete m_rightEdge;
        m_rightEdge = nullptr;
    }
}

void RecordingOverlay::setupUI()
{
    ConfigHandler config;
    m_uiColor = config.uiColor();
    QColor contrastCol = ColorUtils::contrastColor(m_uiColor);
    QColor textColor = ColorUtils::colorIsDark(m_uiColor) ? Qt::white : Qt::black;

    // Label styling uses the same text color as buttons
    setStyleSheet(
      QString("QLabel { color: %1; }").arg(textColor.name()));

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);

    // Recording indicator (red dot)
    m_indicator = new QLabel(this);
    m_indicator->setFixedSize(12, 12);
    m_indicator->setStyleSheet(
      "background-color: #ff3333;"
      "border-radius: 6px;");
    layout->addWidget(m_indicator);

    // Time display
    m_timeLabel = new QLabel("00:00:00", this);
    m_timeLabel->setStyleSheet(
      QString("font-family: monospace; font-weight: bold; color: %1;")
        .arg(textColor.name()));
    layout->addWidget(m_timeLabel);

    layout->addSpacing(10);

    // Pause button - uses theme color
    m_pauseBtn = new CaptureButton(tr("Pause"), this);
    m_pauseBtn->setColor(m_uiColor);
    m_pauseBtn->setFixedWidth(70);
    connect(m_pauseBtn, &CaptureButton::clicked, this, &RecordingOverlay::onPauseClicked);
    layout->addWidget(m_pauseBtn);

    // Stop button (save recording) - uses theme color
    m_stopBtn = new CaptureButton(tr("Stop"), this);
    m_stopBtn->setColor(m_uiColor);
    m_stopBtn->setFixedWidth(60);
    m_stopBtn->setToolTip(tr("Stop and save recording"));
    connect(m_stopBtn, &CaptureButton::clicked, this, &RecordingOverlay::onStopClicked);
    layout->addWidget(m_stopBtn);

    // Abort button (cancel recording) - uses theme color
    auto* abortBtn = new CaptureButton(tr("Abort"), this);
    abortBtn->setColor(m_uiColor);
    abortBtn->setFixedWidth(60);
    abortBtn->setToolTip(tr("Cancel recording without saving"));
    connect(abortBtn, &CaptureButton::clicked, this, &RecordingOverlay::onAbortClicked);
    layout->addWidget(abortBtn);

    setLayout(layout);
    adjustSize();
}

void RecordingOverlay::setupFrame()
{
    // Create frame color - red to indicate recording
    QColor frameColor(255, 50, 50, 255);

    // Create the 4 edge widgets positioned OUTSIDE the recording region
    // These are separate windows so they don't get captured in the recording

    // Top edge - above the recording region
    m_topEdge = new RecordingFrameEdge(frameColor);
    m_topEdge->setGeometry(
        m_recordingRegion.left() - FRAME_THICKNESS,
        m_recordingRegion.top() - FRAME_THICKNESS,
        m_recordingRegion.width() + 2 * FRAME_THICKNESS,
        FRAME_THICKNESS);
    m_topEdge->show();

    // Bottom edge - below the recording region
    m_bottomEdge = new RecordingFrameEdge(frameColor);
    m_bottomEdge->setGeometry(
        m_recordingRegion.left() - FRAME_THICKNESS,
        m_recordingRegion.bottom() + 1,
        m_recordingRegion.width() + 2 * FRAME_THICKNESS,
        FRAME_THICKNESS);
    m_bottomEdge->show();

    // Left edge - to the left of the recording region
    m_leftEdge = new RecordingFrameEdge(frameColor);
    m_leftEdge->setGeometry(
        m_recordingRegion.left() - FRAME_THICKNESS,
        m_recordingRegion.top(),
        FRAME_THICKNESS,
        m_recordingRegion.height());
    m_leftEdge->show();

    // Right edge - to the right of the recording region
    m_rightEdge = new RecordingFrameEdge(frameColor);
    m_rightEdge->setGeometry(
        m_recordingRegion.right() + 1,
        m_recordingRegion.top(),
        FRAME_THICKNESS,
        m_recordingRegion.height());
    m_rightEdge->show();
}

void RecordingOverlay::positionControlPanel()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    // Place in the bottom-right corner of the screen with some margin
    int margin = 10;
    int panelX = screenGeometry.right() - width() - margin;
    int panelY = screenGeometry.bottom() - height() - margin;

    move(panelX, panelY);
}

void RecordingOverlay::updateRegion(const QRect& newRegion)
{
    m_recordingRegion = newRegion;

    // Reposition frame edges around the new region
    if (m_topEdge) {
        m_topEdge->setGeometry(
            m_recordingRegion.left() - FRAME_THICKNESS,
            m_recordingRegion.top() - FRAME_THICKNESS,
            m_recordingRegion.width() + 2 * FRAME_THICKNESS,
            FRAME_THICKNESS);
    }
    if (m_bottomEdge) {
        m_bottomEdge->setGeometry(
            m_recordingRegion.left() - FRAME_THICKNESS,
            m_recordingRegion.bottom() + 1,
            m_recordingRegion.width() + 2 * FRAME_THICKNESS,
            FRAME_THICKNESS);
    }
    if (m_leftEdge) {
        m_leftEdge->setGeometry(
            m_recordingRegion.left() - FRAME_THICKNESS,
            m_recordingRegion.top(),
            FRAME_THICKNESS,
            m_recordingRegion.height());
    }
    if (m_rightEdge) {
        m_rightEdge->setGeometry(
            m_recordingRegion.right() + 1,
            m_recordingRegion.top(),
            FRAME_THICKNESS,
            m_recordingRegion.height());
    }
}

void RecordingOverlay::setEdgeVisibility(bool top, bool bottom, bool left, bool right)
{
    if (m_topEdge) m_topEdge->setVisible(top);
    if (m_bottomEdge) m_bottomEdge->setVisible(bottom);
    if (m_leftEdge) m_leftEdge->setVisible(left);
    if (m_rightEdge) m_rightEdge->setVisible(right);
}

void RecordingOverlay::updateDisplay()
{
    if (!m_controller) {
        return;
    }

    qint64 elapsed = m_controller->elapsedMs();
    m_timeLabel->setText(formatTime(elapsed));
}

void RecordingOverlay::onPauseClicked()
{
    if (!m_controller) {
        return;
    }

    if (m_controller->state() == RecordingContext::RECORDING) {
        m_controller->pauseRecording();
        emit pauseRequested();
    } else if (m_controller->state() == RecordingContext::PAUSED) {
        m_controller->resumeRecording();
        emit resumeRequested();
    }

    updatePauseButton();
}

void RecordingOverlay::onStopClicked()
{
    emit stopRequested();
}

void RecordingOverlay::onAbortClicked()
{
    emit abortRequested();
}

void RecordingOverlay::onStateChanged(RecordingContext::RecordingState state)
{
    updatePauseButton();

    // Update indicator color for state
    if (state == RecordingContext::PAUSED) {
        m_indicator->setStyleSheet(
          "background-color: #ffaa00; border-radius: 6px;");
    } else if (state == RecordingContext::RECORDING) {
        m_indicator->setStyleSheet(
          "background-color: #ff3333; border-radius: 6px;");
    }
}

void RecordingOverlay::updatePauseButton()
{
    if (!m_controller) {
        return;
    }

    if (m_controller->state() == RecordingContext::PAUSED) {
        m_pauseBtn->setText(tr("Resume"));
    } else {
        m_pauseBtn->setText(tr("Pause"));
    }
}

QString RecordingOverlay::formatTime(qint64 ms) const
{
    qint64 totalSeconds = ms / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    return QString("%1:%2:%3")
      .arg(hours, 2, 10, QChar('0'))
      .arg(minutes, 2, 10, QChar('0'))
      .arg(seconds, 2, 10, QChar('0'));
}

void RecordingOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    // No panel background — CaptureButtons have their own themed backgrounds
}

void RecordingOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->pos();
    }
}

void RecordingOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
}

void RecordingOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}
