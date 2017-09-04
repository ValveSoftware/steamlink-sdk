/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include <QtDebug>

#include "qpatternistlocale_p.h"

#include "qiodevicedelegate_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

QIODeviceDelegate::QIODeviceDelegate(QIODevice *const source) : m_source(source)
{
    Q_ASSERT(m_source);

    connect(source, SIGNAL(aboutToClose()),         SIGNAL(aboutToClose()));
    connect(source, SIGNAL(bytesWritten(qint64)),   SIGNAL(bytesWritten(qint64)));
    connect(source, SIGNAL(readChannelFinished()),  SIGNAL(readChannelFinished()));
    connect(source, SIGNAL(readyRead()),            SIGNAL(readyRead()));

    /* According to Thiago these two signals are very similar, but QtNetworkAccess uses finished()
     * instead for a minor but significant reason. */
    connect(source, SIGNAL(readChannelFinished()), SIGNAL(finished()));

    /* For instance QFile emits no signals, so how do we know if the device has all data available
     * and it therefore is safe and correct to emit finished()? isSequential() tells us whether it's
     * not random access, and whether it's safe to emit finished(). */
    if(m_source->isSequential())
        QMetaObject::invokeMethod(this, "readyRead", Qt::QueuedConnection);
    else
        QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);

    setOpenMode(QIODevice::ReadOnly);

    /* Set up the timeout timer. */
    connect(&m_timeout, SIGNAL(timeout()), SLOT(networkTimeout()));

    m_timeout.setSingleShot(true);
    m_timeout.start(Timeout);
}

void QIODeviceDelegate::networkTimeout()
{
    setErrorString(QtXmlPatterns::tr("Network timeout."));
    error(QNetworkReply::TimeoutError);
}

void QIODeviceDelegate::abort()
{
    /* Do nothing, just to please QNetworkReply's pure virtual. */
}

bool QIODeviceDelegate::atEnd() const
{
    return m_source->atEnd();
}

qint64 QIODeviceDelegate::bytesAvailable() const
{
    return m_source->bytesAvailable();
}

qint64 QIODeviceDelegate::bytesToWrite() const
{
    return m_source->bytesToWrite();
}

bool QIODeviceDelegate::canReadLine() const
{
    return m_source->canReadLine();
}

void QIODeviceDelegate::close()
{
    return m_source->close();
}

bool QIODeviceDelegate::isSequential() const
{
    return m_source->isSequential();
}

bool QIODeviceDelegate::open(OpenMode mode)
{
    const bool success = m_source->open(mode);
    setOpenMode(m_source->openMode());
    return success;
}

qint64 QIODeviceDelegate::pos() const
{
    return m_source->pos();
}

bool QIODeviceDelegate::reset()
{
    return m_source->reset();
}

bool QIODeviceDelegate::seek(qint64 pos)
{
    return m_source->seek(pos);
}

qint64 QIODeviceDelegate::size() const
{
    return m_source->size();
}

bool QIODeviceDelegate::waitForBytesWritten(int msecs)
{
    return m_source->waitForBytesWritten(msecs);
}

bool QIODeviceDelegate::waitForReadyRead(int msecs)
{
    return m_source->waitForReadyRead(msecs);
}

qint64 QIODeviceDelegate::readData(char *data, qint64 maxSize)
{
    return m_source->read(data, maxSize);
}

QT_END_NAMESPACE

