// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "recordingcontext.h"

RecordingContext::RecordingContext()
  : m_state(IDLE)
  , m_pausedElapsed(0)
  , m_frameCount(0)
  , m_timerRunning(false)
{
}

RecordingContext::RecordingState RecordingContext::state() const
{
    return m_state;
}

void RecordingContext::setState(RecordingState state)
{
    m_state = state;
}

RecordingContext::RecordingConfig& RecordingContext::config()
{
    return m_config;
}

const RecordingContext::RecordingConfig& RecordingContext::config() const
{
    return m_config;
}

void RecordingContext::startTimer()
{
    m_timer.start();
    m_pausedElapsed = 0;
    m_frameCount = 0;
    m_timerRunning = true;
}

void RecordingContext::pauseTimer()
{
    if (m_timerRunning) {
        m_pausedElapsed += m_timer.elapsed();
        m_timerRunning = false;
    }
}

void RecordingContext::resumeTimer()
{
    if (!m_timerRunning) {
        m_timer.restart();
        m_timerRunning = true;
    }
}

void RecordingContext::stopTimer()
{
    if (m_timerRunning) {
        m_pausedElapsed += m_timer.elapsed();
    }
    m_timerRunning = false;
}

qint64 RecordingContext::elapsedMs() const
{
    if (m_timerRunning) {
        return m_pausedElapsed + m_timer.elapsed();
    }
    return m_pausedElapsed;
}

qint64 RecordingContext::frameCount() const
{
    return m_frameCount;
}

void RecordingContext::incrementFrameCount()
{
    m_frameCount++;
}
