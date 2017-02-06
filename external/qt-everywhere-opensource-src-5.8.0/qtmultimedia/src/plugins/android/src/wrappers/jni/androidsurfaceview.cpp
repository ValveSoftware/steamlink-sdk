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

#include "androidsurfaceview.h"

#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qvector.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmutex.h>
#include <QtGui/qwindow.h>

QT_BEGIN_NAMESPACE

static const char QtSurfaceHolderCallbackClassName[] = "org/qtproject/qt5/android/multimedia/QtSurfaceHolderCallback";
typedef QVector<AndroidSurfaceHolder *> SurfaceHolders;
Q_GLOBAL_STATIC(SurfaceHolders, surfaceHolders)
Q_GLOBAL_STATIC(QMutex, shLock)

AndroidSurfaceHolder::AndroidSurfaceHolder(QJNIObjectPrivate object)
    : m_surfaceHolder(object)
    , m_surfaceCreated(false)
{
    if (!m_surfaceHolder.isValid())
        return;

    {
        QMutexLocker locker(shLock);
        surfaceHolders->append(this);
    }

    QJNIObjectPrivate callback(QtSurfaceHolderCallbackClassName, "(J)V", reinterpret_cast<jlong>(this));
    m_surfaceHolder.callMethod<void>("addCallback",
                                     "(Landroid/view/SurfaceHolder$Callback;)V",
                                     callback.object());
}

AndroidSurfaceHolder::~AndroidSurfaceHolder()
{
    QMutexLocker locker(shLock);
    const int i = surfaceHolders->indexOf(this);
    if (Q_UNLIKELY(i == -1))
        return;

    surfaceHolders->remove(i);
}

jobject AndroidSurfaceHolder::surfaceHolder() const
{
    return m_surfaceHolder.object();
}

bool AndroidSurfaceHolder::isSurfaceCreated() const
{
    QMutexLocker locker(shLock);
    return m_surfaceCreated;
}

void AndroidSurfaceHolder::handleSurfaceCreated(JNIEnv*, jobject, jlong id)
{
    QMutexLocker locker(shLock);
    const int i = surfaceHolders->indexOf(reinterpret_cast<AndroidSurfaceHolder *>(id));
    if (Q_UNLIKELY(i == -1))
        return;

    (*surfaceHolders)[i]->m_surfaceCreated = true;
    Q_EMIT (*surfaceHolders)[i]->surfaceCreated();
}

void AndroidSurfaceHolder::handleSurfaceDestroyed(JNIEnv*, jobject, jlong id)
{
    QMutexLocker locker(shLock);
    const int i = surfaceHolders->indexOf(reinterpret_cast<AndroidSurfaceHolder *>(id));
    if (Q_UNLIKELY(i == -1))
        return;

    (*surfaceHolders)[i]->m_surfaceCreated = false;
}

bool AndroidSurfaceHolder::initJNI(JNIEnv *env)
{
    jclass clazz = QJNIEnvironmentPrivate::findClass(QtSurfaceHolderCallbackClassName,
                                                     env);

    static const JNINativeMethod methods[] = {
        {"notifySurfaceCreated", "(J)V", (void *)AndroidSurfaceHolder::handleSurfaceCreated},
        {"notifySurfaceDestroyed", "(J)V", (void *)AndroidSurfaceHolder::handleSurfaceDestroyed}
    };

    if (clazz && env->RegisterNatives(clazz,
                                      methods,
                                      sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
            return false;
    }

    return true;
}

AndroidSurfaceView::AndroidSurfaceView()
    : m_window(0)
    , m_surfaceHolder(0)
    , m_pendingVisible(-1)
{
    QtAndroidPrivate::runOnAndroidThreadSync([this] {
        m_surfaceView = QJNIObjectPrivate("android/view/SurfaceView",
                                          "(Landroid/content/Context;)V",
                                          QtAndroidPrivate::activity());
    }, QJNIEnvironmentPrivate());

    Q_ASSERT(m_surfaceView.isValid());

    QJNIObjectPrivate holder = m_surfaceView.callObjectMethod("getHolder",
                                                                "()Landroid/view/SurfaceHolder;");
    if (!holder.isValid()) {
        m_surfaceView = QJNIObjectPrivate();
    } else {
        m_surfaceHolder = new AndroidSurfaceHolder(holder);
        connect(m_surfaceHolder, &AndroidSurfaceHolder::surfaceCreated,
                this, &AndroidSurfaceView::surfaceCreated);
        { // Lock now to avoid a race with handleSurfaceCreated()
            QMutexLocker locker(shLock);
            m_window = QWindow::fromWinId(WId(m_surfaceView.object()));

            if (m_pendingVisible != -1)
                m_window->setVisible(m_pendingVisible);
            if (m_pendingGeometry.isValid())
                m_window->setGeometry(m_pendingGeometry);
        }
    }
}

AndroidSurfaceView::~AndroidSurfaceView()
{
    delete m_surfaceHolder;
    delete m_window;
}

AndroidSurfaceHolder *AndroidSurfaceView::holder() const
{
    return m_surfaceHolder;
}

void AndroidSurfaceView::setVisible(bool v)
{
    if (m_window)
        m_window->setVisible(v);
    else
        m_pendingVisible = int(v);
}

void AndroidSurfaceView::setGeometry(int x, int y, int width, int height)
{
    if (m_window)
        m_window->setGeometry(x, y, width, height);
    else
        m_pendingGeometry = QRect(x, y, width, height);
}

QT_END_NAMESPACE
