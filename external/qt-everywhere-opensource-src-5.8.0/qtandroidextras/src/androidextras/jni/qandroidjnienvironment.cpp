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

#include "qandroidjnienvironment.h"
#include <QtCore/private/qjni_p.h>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qthreadstorage.h>

QT_BEGIN_NAMESPACE

/*!
    \class QAndroidJniEnvironment
    \inmodule QtAndroidExtras
    \brief The QAndroidJniEnvironment provides access to the JNI Environment.
    \since 5.2
*/

/*!
    \fn QAndroidJniEnvironment::QAndroidJniEnvironment()

    Constructs a new QAndroidJniEnvironment object and attach the current thread to the Java VM.

    \snippet code/src_androidextras_qandroidjnienvironment.cpp Create QAndroidJniEnvironment
*/

/*!
    \fn QAndroidJniEnvironment::~QAndroidJniEnvironment()

    Detaches the current thread from the Java VM and destroys the QAndroidJniEnvironment object.
*/

/*!
    \fn JavaVM *QAndroidJniEnvironment::javaVM()

    Returns the Java VM interface.
*/

/*!
    \fn JNIEnv *QAndroidJniEnvironment::operator->()

    Provides access to the QAndroidJniEnvironment's JNIEnv pointer.
*/

/*!
    \fn QAndroidJniEnvironment::operator JNIEnv *() const

    Returns the JNI Environment pointer.
 */


QAndroidJniEnvironment::QAndroidJniEnvironment()
    : d(new QJNIEnvironmentPrivate)
{
}

QAndroidJniEnvironment::~QAndroidJniEnvironment()
{
}

JavaVM *QAndroidJniEnvironment::javaVM()
{
    return QtAndroidPrivate::javaVM();
}

JNIEnv *QAndroidJniEnvironment::operator->()
{
    return d->jniEnv;
}

QAndroidJniEnvironment::operator JNIEnv*() const
{
    return d->jniEnv;
}

QT_END_NAMESPACE
