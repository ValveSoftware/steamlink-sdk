/****************************************************************************
**
** Copyright (C) 2013 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include <QtCore/QLoggingCategory>
#include <QtAndroidExtras/QAndroidJniEnvironment>

#include "android/inputstreamthread_p.h"
#include "qbluetoothsocket_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

InputStreamThread::InputStreamThread(QBluetoothSocketPrivate *socket)
    : QObject(), m_socket_p(socket), expectClosure(false)
{
}

bool InputStreamThread::run()
{
    QMutexLocker lock(&m_mutex);

    javaInputStreamThread = QAndroidJniObject("org/qtproject/qt5/android/bluetooth/QtBluetoothInputStreamThread");
    if (!javaInputStreamThread.isValid() || !m_socket_p->inputStream.isValid())
        return false;

    javaInputStreamThread.callMethod<void>("setInputStream", "(Ljava/io/InputStream;)V",
                                           m_socket_p->inputStream.object<jobject>());
    javaInputStreamThread.setField<jlong>("qtObject", reinterpret_cast<long>(this));
    javaInputStreamThread.setField<jboolean>("logEnabled", QT_BT_ANDROID().isDebugEnabled());

    javaInputStreamThread.callMethod<void>("start");

    return true;
}

bool InputStreamThread::bytesAvailable() const
{
    QMutexLocker locker(&m_mutex);
    return m_socket_p->buffer.size();
}

qint64 InputStreamThread::readData(char *data, qint64 maxSize)
{
    QMutexLocker locker(&m_mutex);

    if (!m_socket_p->buffer.isEmpty())
        return m_socket_p->buffer.read(data, maxSize);

    return 0;
}

//inside the java thread
void InputStreamThread::javaThreadErrorOccurred(int errorCode)
{
    QMutexLocker lock(&m_mutex);

    if (!expectClosure)
        emit error(errorCode);
    else
        emit error(-1); //magic error, -1 means error was expected due to expected close()
}

//inside the java thread
void InputStreamThread::javaReadyRead(jbyteArray buffer, int bufferLength)
{
    QAndroidJniEnvironment env;

    QMutexLocker lock(&m_mutex);
    char *writePtr = m_socket_p->buffer.reserve(bufferLength);
    env->GetByteArrayRegion(buffer, 0, bufferLength, reinterpret_cast<jbyte*>(writePtr));
    emit dataAvailable();
}

void InputStreamThread::prepareForClosure()
{
    QMutexLocker lock(&m_mutex);
    expectClosure = true;
}

QT_END_NAMESPACE
