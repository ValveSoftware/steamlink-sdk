/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwaylandeventthread_p.h"
#include <QtCore/QSocketNotifier>
#include <QCoreApplication>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

QT_BEGIN_NAMESPACE

QWaylandEventThread::QWaylandEventThread(QObject *parent)
    : QObject(parent)
    , m_display(0)
    , m_fileDescriptor(-1)
    , m_readNotifier(0)
    , m_displayLock(new QMutex)
{
}

QWaylandEventThread::~QWaylandEventThread()
{
    delete m_displayLock;
    wl_display_disconnect(m_display);
}

void QWaylandEventThread::displayConnect()
{
    m_displayLock->lock();
    QMetaObject::invokeMethod(this, "waylandDisplayConnect", Qt::QueuedConnection);
}

// ### be careful what you do, this function may also be called from other
// threads to clean up & exit.
void QWaylandEventThread::checkError() const
{
    int ecode = wl_display_get_error(m_display);
    if ((ecode == EPIPE || ecode == ECONNRESET)) {
        // special case this to provide a nicer error
        qWarning("The Wayland connection broke. Did the Wayland compositor die?");
    } else {
        qErrnoWarning(ecode, "The Wayland connection experienced a fatal error");
    }
}

void QWaylandEventThread::readWaylandEvents()
{
    if (wl_display_dispatch(m_display) < 0) {
        checkError();
        m_readNotifier->setEnabled(false);
        emit fatalError();
        return;
    }

    emit newEventsRead();
}

void QWaylandEventThread::waylandDisplayConnect()
{
    m_display = wl_display_connect(NULL);
    if (m_display == NULL) {
        qErrnoWarning(errno, "Failed to create display");
        ::exit(1);
    }
    m_displayLock->unlock();

    m_fileDescriptor = wl_display_get_fd(m_display);

    m_readNotifier = new QSocketNotifier(m_fileDescriptor, QSocketNotifier::Read, this);
    connect(m_readNotifier, SIGNAL(activated(int)), this, SLOT(readWaylandEvents()));
}

wl_display *QWaylandEventThread::display() const
{
    QMutexLocker displayLock(m_displayLock);
    return m_display;
}

QT_END_NAMESPACE
