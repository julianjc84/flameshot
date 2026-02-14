// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "framegrabber.h"

FrameGrabber::FrameGrabber(QObject* parent)
  : QObject(parent)
{
}

QRect FrameGrabber::region() const
{
    return m_region;
}
