/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "avfdisplaylink.h"
#include <QtCore/qcoreapplication.h>

#ifdef QT_DEBUG_AVF
#include <QtCore/qdebug.h>
#endif

QT_USE_NAMESPACE

static CVReturn CVDisplayLinkCallback(CVDisplayLinkRef displayLink,
                                 const CVTimeStamp *inNow,
                                 const CVTimeStamp *inOutputTime,
                                 CVOptionFlags flagsIn,
                                 CVOptionFlags *flagsOut,
                                 void *displayLinkContext)
{
    Q_UNUSED(displayLink);
    Q_UNUSED(inNow);
    Q_UNUSED(flagsIn);
    Q_UNUSED(flagsOut);

    AVFDisplayLink *link = (AVFDisplayLink *)displayLinkContext;

    link->displayLinkEvent(inOutputTime);
    return kCVReturnSuccess;
}

AVFDisplayLink::AVFDisplayLink(QObject *parent)
    : QObject(parent)
    , m_pendingDisplayLinkEvent(false)
    , m_isActive(false)
{
    // create display link for the main display
    CVDisplayLinkCreateWithCGDisplay(kCGDirectMainDisplay, &m_displayLink);
    if (m_displayLink) {
        // set the current display of a display link.
        CVDisplayLinkSetCurrentCGDisplay(m_displayLink, kCGDirectMainDisplay);

        // set the renderer output callback function
        CVDisplayLinkSetOutputCallback(m_displayLink, &CVDisplayLinkCallback, this);
    }
}

AVFDisplayLink::~AVFDisplayLink()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif

    if (m_displayLink) {
        CVDisplayLinkStop(m_displayLink);
        CVDisplayLinkRelease(m_displayLink);
        m_displayLink = NULL;
    }
}

bool AVFDisplayLink::isValid() const
{
    return m_displayLink != 0;
}

bool AVFDisplayLink::isActive() const
{
    return m_isActive;
}

void AVFDisplayLink::start()
{
    if (m_displayLink && !m_isActive) {
       CVDisplayLinkStart(m_displayLink);
       m_isActive = true;
    }
}

void AVFDisplayLink::stop()
{
    if (m_displayLink && m_isActive) {
        CVDisplayLinkStop(m_displayLink);
        m_isActive = false;
    }
}


void AVFDisplayLink::displayLinkEvent(const CVTimeStamp *ts)
{
    // This function is called from a
    // thread != gui thread. So we post the event.
    // But we need to make sure that we don't post faster
    // than the event loop can eat:
    m_displayLinkMutex.lock();
    bool pending = m_pendingDisplayLinkEvent;
    m_pendingDisplayLinkEvent = true;
    m_frameTimeStamp = *ts;
    m_displayLinkMutex.unlock();

    if (!pending)
        qApp->postEvent(this, new QEvent(QEvent::User), Qt::HighEventPriority);
}

bool AVFDisplayLink::event(QEvent *event)
{
    switch (event->type()){
        case QEvent::User:  {
                m_displayLinkMutex.lock();
                m_pendingDisplayLinkEvent = false;
                CVTimeStamp ts = m_frameTimeStamp;
                m_displayLinkMutex.unlock();

                Q_EMIT tick(ts);

                return false;
            }
            break;
        default:
            break;
    }
    return QObject::event(event);
}
