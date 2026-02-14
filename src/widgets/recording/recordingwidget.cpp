// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "recordingwidget.h"
#include "recordingoverlay.h"
#include "src/recording/recordingcontroller.h"
#include "src/utils/confighandler.h"
#include "src/utils/filenamehandler.h"
#include "src/utils/screengrabber.h"
#include "src/utils/systemnotification.h"
#include "src/config/generalconf.h"
#include "src/utils/colorutils.h"
#include "src/widgets/capture/overlaymessage.h"
#include "src/widgets/capture/capturebutton.h"
#include "src/widgets/capture/selectionwidget.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QVBoxLayout>

// Static aspect ratio presets
// Note: { "Fullscreen", -1, -1 } is a special case handled separately
const QVector<RecordingWidget::AspectRatio> RecordingWidget::s_aspectRatios = {
    { "Free", 0, 0 },
    { "Fullscreen", -1, -1 },
    { "16:9", 16, 9 },
    { "4:3", 4, 3 },
    { "1:1", 1, 1 },
    { "21:9", 21, 9 },
    { "9:16", 9, 16 },
};

RecordingWidget::RecordingWidget(const RecordingRequest& req, QWidget* parent)
  : QWidget(parent)
  , m_request(req)
  , m_controller(new RecordingController(this))
  , m_currentAspectRatio(s_aspectRatios[0])  // Free by default
{
    // Set up fullscreen transparent window
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::BypassWindowManagerHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::CrossCursor);

    // Get colors and settings from config
    ConfigHandler config;
    m_uiColor = config.uiColor();
    m_contrastUiColor = config.contrastUiColor();
    m_overlayColor = QColor(0, 0, 0, config.contrastOpacity());

    // Selection geometry display timer
    m_xywhTimer.setSingleShot(true);
    connect(&m_xywhTimer, &QTimer::timeout, this, [this]() {
        m_xywhDisplay = false;
        update();
    });

    // Load saved aspect ratio
    QString savedAspectRatio = config.videoAspectRatio();
    if (!savedAspectRatio.isEmpty()) {
        for (const auto& ratio : s_aspectRatios) {
            if (ratio.name == savedAspectRatio) {
                m_currentAspectRatio = ratio;
                break;
            }
        }
    }

    // Connect controller signals
    connect(m_controller,
            &RecordingController::recordingFinished,
            this,
            &RecordingWidget::onRecordingFinished);
    connect(m_controller,
            &RecordingController::recordingError,
            this,
            &RecordingWidget::onRecordingError);
    connect(m_controller,
            &RecordingController::encoderFallback,
            this,
            &RecordingWidget::onEncoderFallback);

    // Initialize background
    initBackground();

    // Initialize help overlay message (same style as screenshot capture)
    OverlayMessage::init(this, rect());
    QList<QPair<QString, QString>> keyMap;
    keyMap << std::pair(tr("Mouse"), tr("Select recording area"));
    keyMap << std::pair(tr("Enter"), tr("Start recording"));
    keyMap << std::pair(tr("Esc"), tr("Exit"));
    OverlayMessage::push(OverlayMessage::compileFromKeyMap(keyMap));

    // Create selection widget (hidden initially)
    m_selectionWidget = new SelectionWidget(m_uiColor, this);
    m_selectionWidget->hide();
    m_selectionWidget->setIdleCentralCursor(Qt::SizeAllCursor);

    connect(m_selectionWidget,
            &SelectionWidget::geometryChanged,
            this,
            &RecordingWidget::onSelectionChanged);
    connect(m_selectionWidget,
            &SelectionWidget::geometrySettled,
            this,
            &RecordingWidget::onSelectionSettled);

    // Create aspect ratio panel (hidden until selection exists)
    initAspectRatioPanel();

    // Handle initial region/mode
    bool useFullscreen = m_currentAspectRatio.isFullscreen() ||
                         m_request.mode() == RecordingRequest::FULLSCREEN_MODE;

    if (useFullscreen) {
        // Set selection to full screen and auto-start recording
        m_selectionWidget->setGeometry(rect());
        m_selectionWidget->show();
        updateAspectRatioPanel();
        confirmRegion();
    } else if (!m_request.initialRegion().isEmpty()) {
        m_selectionWidget->setGeometry(m_request.initialRegion());
        m_selectionWidget->show();
        updateAspectRatioPanel();
    }
}

RecordingWidget::~RecordingWidget()
{
    if (m_recordingActive) {
        m_controller->stopRecording();
    }
}

void RecordingWidget::initBackground()
{
    // Grab screenshot of entire desktop as background
    ScreenGrabber grabber;
    bool ok = false;
    QPixmap screenshot = grabber.grabEntireDesktop(ok);

    if (ok) {
        m_backgroundScreenshot = screenshot;
    }

    // Set window geometry to cover all screens
    QRect fullGeometry = grabber.desktopGeometry();
    setGeometry(fullGeometry);
}

void RecordingWidget::initAspectRatioPanel()
{
    QColor contrastCol = ColorUtils::contrastColor(m_uiColor);
    QColor textColor = ColorUtils::colorIsDark(m_uiColor) ? Qt::white : Qt::black;

    // Panel background uses the theme color
    QColor panelBg = m_uiColor;
    panelBg.setAlpha(220);

    m_aspectPanel = new QWidget(this);
    m_aspectPanel->setObjectName("aspectPanel");
    // Ensure the panel captures mouse events on its background
    m_aspectPanel->setAttribute(Qt::WA_StyledBackground);
    m_aspectPanel->setAttribute(Qt::WA_NoMousePropagation);
    m_aspectPanel->setStyleSheet(
        QString("#aspectPanel {"
                "   background: transparent;"
                "   padding: 8px;"
                "}"
                "QLabel {"
                "   color: %1;"
                "   font-size: 11px;"
                "}")
            .arg(textColor.name()));

    auto* layout = new QVBoxLayout(m_aspectPanel);
    layout->setSpacing(8);
    layout->setContentsMargins(10, 10, 10, 10);

    // Title
    auto* titleLabel = new QLabel(tr("Aspect Ratio"), m_aspectPanel);
    titleLabel->setStyleSheet(
        QString("font-weight: bold; font-size: 13px; color: %1;")
            .arg(textColor.name()));
    layout->addWidget(titleLabel);

    // Aspect ratio buttons in a flow layout
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);

    for (const auto& ratio : s_aspectRatios) {
        auto* btn = new CaptureButton(ratio.name, m_aspectPanel);
        bool isActive = (ratio.name == m_currentAspectRatio.name);
        btn->setColor(isActive ? m_contrastUiColor : m_uiColor);
        btn->setCheckable(true);
        btn->setChecked(isActive);

        connect(btn, &CaptureButton::clicked, this, [this, ratio, btn]() {
            // Update button colors: active gets contrast, others get normal
            for (auto* b : m_aspectButtons) {
                bool selected = (b == btn);
                b->setChecked(selected);
                b->setColor(selected ? m_contrastUiColor : m_uiColor);
            }
            setAspectRatio(ratio);
        });

        buttonLayout->addWidget(btn);
        m_aspectButtons.append(btn);
    }

    layout->addLayout(buttonLayout);

    // Size label
    m_sizeLabel = new QLabel(m_aspectPanel);
    m_sizeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_sizeLabel);

    // Start Recording button - uses theme color
    m_startButton = new CaptureButton(tr("Start Recording"), m_aspectPanel);
    m_startButton->setColor(m_uiColor);
    connect(m_startButton, &CaptureButton::clicked, this, [this]() {
        QRect sel = m_selectionWidget->geometry();
        if (sel.width() >= 10 && sel.height() >= 10) {
            confirmRegion();
        }
    });
    layout->addWidget(m_startButton);

    // Hints
    QColor hintColor = textColor;
    hintColor.setAlpha(180);
    auto* hintLabel = new QLabel(
        tr("Press Escape to cancel"),
        m_aspectPanel);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet(
        QString("color: %1; font-size: 10px;")
            .arg(hintColor.name(QColor::HexArgb)));
    layout->addWidget(hintLabel);

    m_aspectPanel->adjustSize();
    m_aspectPanel->hide();
}

void RecordingWidget::updateAspectRatioPanel()
{
    if (!m_selectionWidget->isVisible()) {
        m_aspectPanel->hide();
        return;
    }

    QRect sel = m_selectionWidget->geometry();

    // Update size label
    m_sizeLabel->setText(QString("%1 x %2").arg(sel.width()).arg(sel.height()));

    // Position panel below selection (or above if not enough space)
    int panelX = sel.center().x() - m_aspectPanel->width() / 2;
    int panelY = sel.bottom() + 15;

    // Check if panel would go off screen bottom
    if (panelY + m_aspectPanel->height() > height() - 20) {
        panelY = sel.top() - m_aspectPanel->height() - 15;
    }

    // Keep panel within screen bounds horizontally
    panelX = qMax(20, qMin(panelX, width() - m_aspectPanel->width() - 20));
    panelY = qMax(20, qMin(panelY, height() - m_aspectPanel->height() - 20));

    m_aspectPanel->move(panelX, panelY);
    m_aspectPanel->show();
    m_aspectPanel->raise();
}

void RecordingWidget::setAspectRatio(const AspectRatio& ratio)
{
    AspectRatio previousRatio = m_currentAspectRatio;
    m_currentAspectRatio = ratio;

    // Save the selection to config for next time
    // Intentionally save "Free" instead of "Fullscreen" so the next session
    // starts with a blank slate ready for user selection, avoiding confusion
    // from auto-starting fullscreen recording unexpectedly.
    ConfigHandler config;
    config.setVideoAspectRatio(ratio.isFullscreen() ? "Free" : ratio.name);

    if (ratio.isFullscreen()) {
        // Set selection to full screen
        m_selectionWidget->setGeometry(rect());
        m_selectionWidget->show();
        updateAspectRatioPanel();
    } else if (ratio.isNone() && previousRatio.isFullscreen()) {
        // Switching from Fullscreen to Free - hide selection so user can draw new one
        m_selectionWidget->hide();
        m_aspectPanel->hide();
        OverlayMessage::setVisibility(true);
        update();
    } else if (!ratio.isNone() && m_selectionWidget->isVisible()) {
        applyAspectRatio();
    }
}

void RecordingWidget::applyAspectRatio()
{
    if (m_currentAspectRatio.isNone()) {
        return;
    }

    if (m_currentAspectRatio.isFullscreen()) {
        // Fullscreen - set to entire screen
        m_selectionWidget->setGeometry(rect());
        updateAspectRatioPanel();
        return;
    }

    QRect sel = m_selectionWidget->geometry();
    QRect constrained = constrainToAspectRatio(sel, m_currentAspectRatio);
    m_selectionWidget->setGeometry(constrained);
    updateAspectRatioPanel();
}

QRect RecordingWidget::constrainToAspectRatio(const QRect& rect,
                                               const AspectRatio& ratio)
{
    if (ratio.isNone()) {
        return rect;
    }

    float targetRatio = ratio.ratio();
    float currentRatio = static_cast<float>(rect.width()) / rect.height();

    int newWidth = rect.width();
    int newHeight = rect.height();

    if (currentRatio > targetRatio) {
        // Too wide, adjust width
        newWidth = static_cast<int>(rect.height() * targetRatio);
    } else {
        // Too tall, adjust height
        newHeight = static_cast<int>(rect.width() / targetRatio);
    }

    // Center the new rectangle on the old center
    QPoint center = rect.center();
    QRect result(0, 0, newWidth, newHeight);
    result.moveCenter(center);

    return result;
}

QRect RecordingWidget::constrainedRectFromDrag(const QPoint& anchor,
                                                const QPoint& cursor,
                                                const AspectRatio& ratio)
{
    if (ratio.isNone()) {
        return QRect(anchor, cursor).normalized();
    }

    float targetRatio = ratio.ratio();

    // Calculate delta from anchor to cursor
    int dx = cursor.x() - anchor.x();
    int dy = cursor.y() - anchor.y();

    // Determine the dominant axis and calculate constrained size
    int width, height;

    // Use the larger delta to determine size, then constrain the other
    if (qAbs(dx) * 1.0f / targetRatio >= qAbs(dy)) {
        // Width is dominant
        width = qAbs(dx);
        height = static_cast<int>(width / targetRatio);
    } else {
        // Height is dominant
        height = qAbs(dy);
        width = static_cast<int>(height * targetRatio);
    }

    // Ensure minimum size
    if (width < 10) width = 10;
    if (height < 10) height = static_cast<int>(10 / targetRatio);
    if (height < 10) {
        height = 10;
        width = static_cast<int>(10 * targetRatio);
    }

    // Build the rectangle based on drag direction
    QRect result;

    if (dx >= 0 && dy >= 0) {
        // Dragging down-right: anchor is top-left
        result = QRect(anchor.x(), anchor.y(), width, height);
    } else if (dx < 0 && dy >= 0) {
        // Dragging down-left: anchor is top-right
        result = QRect(anchor.x() - width, anchor.y(), width, height);
    } else if (dx >= 0 && dy < 0) {
        // Dragging up-right: anchor is bottom-left
        result = QRect(anchor.x(), anchor.y() - height, width, height);
    } else {
        // Dragging up-left: anchor is bottom-right
        result = QRect(anchor.x() - width, anchor.y() - height, width, height);
    }

    return result;
}

QRect RecordingWidget::constrainedRectWithAnchor(const QRect& rect,
                                                   const AspectRatio& ratio,
                                                   const QPoint& anchor)
{
    if (ratio.isNone()) {
        return rect;
    }

    float targetRatio = ratio.ratio();
    int newWidth = rect.width();
    int newHeight = rect.height();

    // Adjust dimensions to match aspect ratio
    float currentRatio = static_cast<float>(rect.width()) / rect.height();
    if (currentRatio > targetRatio) {
        // Too wide, adjust width based on height
        newWidth = static_cast<int>(rect.height() * targetRatio);
    } else {
        // Too tall, adjust height based on width
        newHeight = static_cast<int>(rect.width() / targetRatio);
    }

    // Build rectangle anchored at the specified corner
    QRect result;

    // Determine which corner is the anchor based on position relative to rect center
    bool anchorLeft = anchor.x() <= rect.center().x();
    bool anchorTop = anchor.y() <= rect.center().y();

    if (anchorLeft && anchorTop) {
        // Anchor at top-left
        result = QRect(anchor.x(), anchor.y(), newWidth, newHeight);
    } else if (!anchorLeft && anchorTop) {
        // Anchor at top-right
        result = QRect(anchor.x() - newWidth, anchor.y(), newWidth, newHeight);
    } else if (anchorLeft && !anchorTop) {
        // Anchor at bottom-left
        result = QRect(anchor.x(), anchor.y() - newHeight, newWidth, newHeight);
    } else {
        // Anchor at bottom-right
        result = QRect(anchor.x() - newWidth, anchor.y() - newHeight, newWidth, newHeight);
    }

    return result;
}

void RecordingWidget::applyAspectRatioFromResize()
{
    if (m_currentAspectRatio.isNone()) {
        return;
    }

    if (m_currentAspectRatio.isFullscreen()) {
        // Fullscreen - always snap back to full screen
        m_selectionWidget->setGeometry(rect());
        updateAspectRatioPanel();
        return;
    }

    QRect oldRect = m_selectionBeforeResize;
    QRect newRect = m_selectionWidget->geometry();

    // Check if this is a move (size unchanged) vs resize
    if (oldRect.size() == newRect.size()) {
        return;
    }

    // Calculate how much each edge moved
    int dLeft = newRect.left() - oldRect.left();
    int dRight = newRect.right() - oldRect.right();
    int dTop = newRect.top() - oldRect.top();
    int dBottom = newRect.bottom() - oldRect.bottom();

    // Determine which edges moved (handle detection)
    bool leftMoved = qAbs(dLeft) > 2;
    bool rightMoved = qAbs(dRight) > 2;
    bool topMoved = qAbs(dTop) > 2;
    bool bottomMoved = qAbs(dBottom) > 2;

    // If all edges moved similarly, it's a move not resize
    if (leftMoved && rightMoved && topMoved && bottomMoved) {
        if (qAbs(dLeft - dRight) < 5 && qAbs(dTop - dBottom) < 5) {
            return;
        }
    }

    // Calculate dimension changes
    int dWidth = newRect.width() - oldRect.width();
    int dHeight = newRect.height() - oldRect.height();

    float targetRatio = m_currentAspectRatio.ratio();
    int newWidth, newHeight;

    // Determine which dimension is the "driver" based on what changed more
    // For side handles: only one dimension changes significantly
    // For corners: use the dominant change
    bool widthIsDominant = qAbs(dWidth) >= qAbs(dHeight);

    if (widthIsDominant) {
        // Width changed more - calculate height from width
        newWidth = newRect.width();
        newHeight = qRound(newWidth / targetRatio);
    } else {
        // Height changed more - calculate width from height
        newHeight = newRect.height();
        newWidth = qRound(newHeight * targetRatio);
    }

    // Determine anchor point (the corner that didn't move)
    QPoint anchor;
    if (!leftMoved && !topMoved) {
        anchor = oldRect.topLeft();
    } else if (!rightMoved && !topMoved) {
        anchor = oldRect.topRight();
    } else if (!leftMoved && !bottomMoved) {
        anchor = oldRect.bottomLeft();
    } else if (!rightMoved && !bottomMoved) {
        anchor = oldRect.bottomRight();
    } else if (!leftMoved) {
        // Left edge fixed (right edge or right corners moved)
        anchor = QPoint(oldRect.left(), topMoved ? oldRect.bottom() : oldRect.top());
    } else if (!rightMoved) {
        // Right edge fixed
        anchor = QPoint(oldRect.right(), topMoved ? oldRect.bottom() : oldRect.top());
    } else if (!topMoved) {
        // Top edge fixed
        anchor = QPoint(leftMoved ? oldRect.right() : oldRect.left(), oldRect.top());
    } else if (!bottomMoved) {
        // Bottom edge fixed
        anchor = QPoint(leftMoved ? oldRect.right() : oldRect.left(), oldRect.bottom());
    } else {
        // Fallback: use center
        anchor = oldRect.center();
    }

    // Build the constrained rectangle from the anchor
    QRect constrained;

    // Determine anchor position relative to the dragged area
    bool anchorIsLeft = anchor.x() <= newRect.center().x();
    bool anchorIsTop = anchor.y() <= newRect.center().y();

    if (anchorIsLeft && anchorIsTop) {
        constrained = QRect(anchor.x(), anchor.y(), newWidth, newHeight);
    } else if (!anchorIsLeft && anchorIsTop) {
        constrained = QRect(anchor.x() - newWidth + 1, anchor.y(), newWidth, newHeight);
    } else if (anchorIsLeft && !anchorIsTop) {
        constrained = QRect(anchor.x(), anchor.y() - newHeight + 1, newWidth, newHeight);
    } else {
        constrained = QRect(anchor.x() - newWidth + 1, anchor.y() - newHeight + 1, newWidth, newHeight);
    }

    m_selectionWidget->setGeometry(constrained);
    updateAspectRatioPanel();
}

void RecordingWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background screenshot
    if (!m_backgroundScreenshot.isNull()) {
        painter.drawPixmap(rect(), m_backgroundScreenshot);
    }

    // Draw overlay outside selection (same style as screenshot capture)
    painter.setBrush(m_overlayColor);
    QRect sel;
    if (m_selectionWidget->isVisible()) {
        sel = m_selectionWidget->geometry().normalized();
    }
    QRegion grey(rect());
    grey = grey.subtracted(sel);
    painter.setClipRegion(grey);
    painter.drawRect(-1, -1, rect().width() + 1, rect().height() + 1);
    painter.setClipRect(rect());

    // Draw selection geometry text (same as screenshot capture)
    if (m_selectionWidget->isVisible() && !sel.isEmpty() && m_xywhDisplay) {
        ConfigHandler config;
        GeneralConf::xywh_position position =
          static_cast<GeneralConf::xywh_position>(config.showSelectionGeometry());
        if (position != 0) {
            QString xy = QString("%1x%2+%3+%4")
                           .arg(sel.width())
                           .arg(sel.height())
                           .arg(sel.left())
                           .arg(sel.top());

            QFontMetrics fm = painter.fontMetrics();
            QRect xybox = fm.boundingRect(xy);
            xybox.adjust(0, 0, 10, 12);

            int x0, y0;
            switch (position) {
                case GeneralConf::xywh_top_left:
                    x0 = sel.left();
                    y0 = sel.top();
                    break;
                case GeneralConf::xywh_bottom_left:
                    x0 = sel.left();
                    y0 = sel.bottom() - xybox.height();
                    break;
                case GeneralConf::xywh_top_right:
                    x0 = sel.right() - xybox.width();
                    y0 = sel.top();
                    break;
                case GeneralConf::xywh_bottom_right:
                    x0 = sel.right() - xybox.width();
                    y0 = sel.bottom() - xybox.height();
                    break;
                case GeneralConf::xywh_center:
                default:
                    x0 = sel.left() + (sel.width() - xybox.width()) / 2;
                    y0 = sel.top() + (sel.height() - xybox.height()) / 2;
            }

            QColor uicolor = m_uiColor;
            uicolor.setAlpha(200);
            painter.fillRect(x0, y0, xybox.width(), xybox.height(), QBrush(uicolor));
            painter.setPen(ColorUtils::colorIsDark(uicolor) ? Qt::white : Qt::black);
            painter.drawText(x0, y0, xybox.width(), xybox.height(),
                             Qt::AlignVCenter | Qt::AlignHCenter, xy);
        }
    }
}


void RecordingWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_recordingActive) {
        // Ignore clicks on the aspect ratio panel
        if (m_aspectPanel->isVisible() &&
            m_aspectPanel->geometry().contains(event->pos())) {
            return;
        }

        // Check if clicking on the selection widget's resize handles
        if (m_selectionWidget->isVisible()) {
            auto side = m_selectionWidget->getMouseSide(event->pos());
            if (side != SelectionWidget::NO_SIDE) {
                // Let SelectionWidget handle the event
                return;
            }
        }

        // Start new selection
        m_initialSelection = true;
        m_dragStart = event->pos();
        m_selectionWidget->setGeometry(QRect(m_dragStart, QSize(0, 0)));
        m_selectionWidget->show();
        m_aspectPanel->hide();
        OverlayMessage::setVisibility(false);
        update();
    }
}

void RecordingWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_initialSelection) {
        QRect newSelection;

        // Apply aspect ratio constraint during drag if not "Free"
        if (!m_currentAspectRatio.isNone()) {
            // Calculate constrained selection anchored at drag start
            newSelection = constrainedRectFromDrag(
                m_dragStart, event->pos(), m_currentAspectRatio);
        } else {
            newSelection = QRect(m_dragStart, event->pos()).normalized();
        }

        m_selectionWidget->setGeometry(newSelection);
        m_xywhDisplay = true;
        update();
    } else if (!m_selectionWidget->isVisible()) {
        // Update cursor when no selection
        setCursor(Qt::CrossCursor);
    }
}

void RecordingWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_initialSelection) {
        m_initialSelection = false;

        QRect sel = m_selectionWidget->geometry();

        // Ensure minimum size
        if (sel.width() < 10 || sel.height() < 10) {
            m_selectionWidget->hide();
            m_aspectPanel->hide();
            OverlayMessage::setVisibility(true);
        } else {
            OverlayMessage::setVisibility(false);
            // Apply final aspect ratio constraint
            if (!m_currentAspectRatio.isNone()) {
                applyAspectRatio();
            }
            updateAspectRatioPanel();
        }

        update();
    }
}

void RecordingWidget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Escape:
            if (m_recordingActive) {
                stopRecording();
            }
            emit recordingCancelled();
            close();
            break;

        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (!m_recordingActive && m_selectionWidget->isVisible()) {
                confirmRegion();
            }
            break;

        // Arrow keys for selection movement/resize
        case Qt::Key_Left:
            if (m_selectionWidget->isVisible()) {
                if (event->modifiers() & Qt::ShiftModifier) {
                    m_selectionWidget->resizeLeft();
                } else {
                    m_selectionWidget->moveLeft();
                }
            }
            break;
        case Qt::Key_Right:
            if (m_selectionWidget->isVisible()) {
                if (event->modifiers() & Qt::ShiftModifier) {
                    m_selectionWidget->resizeRight();
                } else {
                    m_selectionWidget->moveRight();
                }
            }
            break;
        case Qt::Key_Up:
            if (m_selectionWidget->isVisible()) {
                if (event->modifiers() & Qt::ShiftModifier) {
                    m_selectionWidget->resizeUp();
                } else {
                    m_selectionWidget->moveUp();
                }
            }
            break;
        case Qt::Key_Down:
            if (m_selectionWidget->isVisible()) {
                if (event->modifiers() & Qt::ShiftModifier) {
                    m_selectionWidget->resizeDown();
                } else {
                    m_selectionWidget->moveDown();
                }
            }
            break;

        default:
            QWidget::keyPressEvent(event);
    }
}

void RecordingWidget::onSelectionChanged()
{
    // Track geometry at start of resize for anchor calculation
    if (!m_resizeInProgress && !m_initialSelection) {
        m_resizeInProgress = true;
        m_selectionBeforeResize = m_selectionWidget->geometry();
    }
    // Show geometry display with timeout (same as screenshot capture)
    m_xywhDisplay = true;
    int timeout = ConfigHandler().showSelectionGeometryHideTime();
    if (timeout != 0) {
        m_xywhTimer.start(timeout);
    }
    update();
    updateAspectRatioPanel();
}

void RecordingWidget::onSelectionSettled()
{
    // Clamp selection to screen bounds
    clampSelectionToScreen();

    // Apply aspect ratio after user finishes resizing
    if (!m_currentAspectRatio.isNone() && !m_initialSelection) {
        if (m_resizeInProgress) {
            applyAspectRatioFromResize();
        } else {
            applyAspectRatio();
        }
    }
    m_resizeInProgress = false;
    update();
}

void RecordingWidget::clampSelectionToScreen()
{
    if (!m_selectionWidget || !m_selectionWidget->isVisible()) {
        return;
    }

    QRect sel = m_selectionWidget->geometry();
    QRect screen = rect();  // Widget covers entire desktop

    // Clamp position to keep selection within screen
    int x = sel.x();
    int y = sel.y();
    int w = sel.width();
    int h = sel.height();

    // If selection is larger than screen, resize it
    if (w > screen.width()) {
        w = screen.width();
    }
    if (h > screen.height()) {
        h = screen.height();
    }

    // Clamp position
    if (x < screen.left()) {
        x = screen.left();
    }
    if (y < screen.top()) {
        y = screen.top();
    }
    if (x + w > screen.right() + 1) {
        x = screen.right() + 1 - w;
    }
    if (y + h > screen.bottom() + 1) {
        y = screen.bottom() + 1 - h;
    }

    QRect clamped(x, y, w, h);
    if (clamped != sel) {
        m_selectionWidget->setGeometry(clamped);
        updateAspectRatioPanel();
    }
}

void RecordingWidget::confirmRegion()
{
    if (!m_selectionWidget->isVisible()) {
        return;
    }

    QRect sel = m_selectionWidget->geometry();
    if (sel.width() < 10 || sel.height() < 10) {
        return;
    }

    startRecording();
}

void RecordingWidget::startRecording()
{
    // Hide selection UI and show recording overlay
    hide();

    QRect selection = m_selectionWidget->geometry();

    // Generate output path if not specified
    QString outputPath = m_request.outputPath();
    if (outputPath.isEmpty()) {
        outputPath = generateOutputPath();
    }

    // Get settings from config, with request values as overrides
    ConfigHandler configHandler;

    int framerate = m_request.framerate();
    if (framerate <= 0) {
        framerate = configHandler.videoFramerate();
    }

    int bitrate = m_request.bitrate();
    if (bitrate <= 0) {
        bitrate = configHandler.videoBitrate();
    }

    QString codec = m_request.codec();
    if (codec.isEmpty()) {
        codec = configHandler.videoCodec();
    }

    QString format = m_request.format();
    if (format.isEmpty()) {
        format = configHandler.videoFormat();
    }

    // Rate control settings
    bool useCrf = configHandler.videoUseCrf();
    int crf = configHandler.videoCrf();
    QString preset = configHandler.videoPreset();

    // Configure recording
    RecordingContext::RecordingConfig config;
    config.region = selection;
    config.outputPath = outputPath;
    config.framerate = framerate;
    config.rateControl = useCrf ? RecordingContext::CRF_MODE : RecordingContext::BITRATE_MODE;
    config.bitrate = bitrate;
    config.crf = crf;
    config.codec = codec;
    config.preset = preset;
    config.format = format;

    // Start recording
    if (!m_controller->startRecording(config)) {
        emit recordingError(tr("Failed to start recording"));
        close();
        return;
    }

    m_recordingActive = true;

    // Create and show recording overlay with region frame
    m_overlay = new RecordingOverlay(m_controller, selection);
    m_overlay->show();

    connect(m_controller, &RecordingController::regionChanged, m_overlay, &RecordingOverlay::updateRegion);
    connect(m_controller, &RecordingController::edgeVisibilityChanged, m_overlay, &RecordingOverlay::setEdgeVisibility);
    connect(m_overlay, &RecordingOverlay::stopRequested, this, &RecordingWidget::stopRecording);
    connect(m_overlay, &RecordingOverlay::abortRequested, this, &RecordingWidget::abortRecording);

    emit recordingStarted();
}

void RecordingWidget::stopRecording()
{
    if (!m_recordingActive) {
        return;
    }

    m_controller->stopRecording();
}

void RecordingWidget::abortRecording()
{
    if (!m_recordingActive) {
        return;
    }

    // Get output path before stopping
    QString outputPath = m_controller->context().config().outputPath;

    // Disconnect all controller signals to prevent callbacks on closed widgets
    disconnect(m_controller, nullptr, this, nullptr);

    // Stop recording (this will finalize the file)
    m_controller->stopRecording();
    m_recordingActive = false;

    // Abort any in-progress GIF conversion
    m_controller->abortGifConversion();

    // Delete the output file
    if (!outputPath.isEmpty() && QFile::exists(outputPath)) {
        QFile::remove(outputPath);
    }

    // Close overlay
    if (m_overlay) {
        m_overlay->close();
        m_overlay = nullptr;
    }

    emit recordingCancelled();
    close();
}

void RecordingWidget::onRecordingFinished(const QString& outputPath)
{
    m_recordingActive = false;

    if (m_overlay) {
        m_overlay->close();
        m_overlay = nullptr;
    }

    emit recordingFinished(outputPath);
    close();
}

void RecordingWidget::onRecordingError(const QString& error)
{
    m_recordingActive = false;

    if (m_overlay) {
        m_overlay->close();
        m_overlay = nullptr;
    }

    // Send system notification for the error
    SystemNotification().sendMessage(
        error,
        tr("Recording Error"),
        QString(),
        10000);  // 10 second timeout

    emit recordingError(error);
    close();
}

void RecordingWidget::onEncoderFallback(const QString& originalCodec,
                                         const QString& fallbackCodec)
{
    // Send system notification about encoder fallback
    QString message = tr("Encoder '%1' not available. Using '%2' instead.")
                          .arg(originalCodec)
                          .arg(fallbackCodec);

    SystemNotification().sendMessage(
        message,
        tr("Recording Started"),
        QString(),
        5000);  // 5 second timeout
}

QString RecordingWidget::generateOutputPath() const
{
    ConfigHandler config;

    // Get save path (default comes from VideoSaveDir in ConfigHandler)
    QString basePath = config.videoSavePath();

    // Get filename pattern from video settings
    QString pattern = config.videoFilenamePattern();

    // Check if GIF mode
    QString outputMode = config.videoOutputMode();
    QString format;
    if (outputMode == "gif") {
        format = "gif";
    } else {
        // Get format from video settings
        format = config.videoFormat();
        if (format.isEmpty()) {
            format = "mp4";
        }
    }

    // Use FileNameHandler to parse the pattern
    FileNameHandler handler;
    QString filename = handler.parseFilename(pattern) + "." + format;

    return QDir(basePath).filePath(filename);
}
