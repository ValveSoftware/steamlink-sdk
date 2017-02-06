/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <directshoweventloop.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>

class DirectShowPostedEvent
{
public:
    DirectShowPostedEvent(QObject *receiver, QEvent *event)
        : receiver(receiver)
        , event(event)
        , next(0)
    {
    }

    ~DirectShowPostedEvent()
    {
        delete event;
    }

    QObject *receiver;
    QEvent *event;
    DirectShowPostedEvent *next;
};

DirectShowEventLoop::DirectShowEventLoop(QObject *parent)
    : QObject(parent)
    , m_postsHead(0)
    , m_postsTail(0)
    , m_eventHandle(::CreateEvent(0, 0, 0, 0))
    , m_waitHandle(::CreateEvent(0, 0, 0, 0))
{
}

DirectShowEventLoop::~DirectShowEventLoop()
{
    ::CloseHandle(m_eventHandle);
    ::CloseHandle(m_waitHandle);

    for (DirectShowPostedEvent *post = m_postsHead; post; post = m_postsHead) {
        m_postsHead = m_postsHead->next;

        delete post;
    }
}

void DirectShowEventLoop::wait(QMutex *mutex)
{
    ::ResetEvent(m_waitHandle);

    mutex->unlock();

    HANDLE handles[] = { m_eventHandle, m_waitHandle };
    while (::WaitForMultipleObjects(2, handles, false, INFINITE) == WAIT_OBJECT_0)
        processEvents();

    mutex->lock();
}

void DirectShowEventLoop::wake()
{
    ::SetEvent(m_waitHandle);
}

void DirectShowEventLoop::postEvent(QObject *receiver, QEvent *event)
{
    QMutexLocker locker(&m_mutex);

    DirectShowPostedEvent *post = new DirectShowPostedEvent(receiver, event);

    if (m_postsTail)
        m_postsTail->next = post;
    else
        m_postsHead = post;

    m_postsTail = post;

    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
    ::SetEvent(m_eventHandle);
}

void DirectShowEventLoop::customEvent(QEvent *event)
{
    if (event->type() == QEvent::User) {
        processEvents();
    } else {
        QObject::customEvent(event);
    }
}

void DirectShowEventLoop::processEvents()
{
    QMutexLocker locker(&m_mutex);

    ::ResetEvent(m_eventHandle);

    while(m_postsHead) {
        DirectShowPostedEvent *post = m_postsHead;
        m_postsHead = m_postsHead->next;

        if (!m_postsHead)
            m_postsTail = 0;

        locker.unlock();
        QCoreApplication::sendEvent(post->receiver, post->event);
        delete post;
        locker.relock();
    }
}
