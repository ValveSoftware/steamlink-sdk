/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
