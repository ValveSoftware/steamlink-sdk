/****************************************************************************
**
** Copyright (C) 2016 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include <android/log.h>
#include "android/androidbroadcastreceiver_p.h"
#include <QtCore/QLoggingCategory>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtGui/QGuiApplication>
#include <QtAndroidExtras/QAndroidJniEnvironment>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)


AndroidBroadcastReceiver::AndroidBroadcastReceiver(QObject* parent)
    : QObject(parent), valid(false)
{
    // get Qt Context
    // TODO: replace with QtAndroidPrivate::context() introduced by Qt 5.8
    contextObject = QAndroidJniObject(QtAndroidPrivate::activity()
                                      ? QtAndroidPrivate::activity() : QtAndroidPrivate::service());

    broadcastReceiverObject = QAndroidJniObject("org/qtproject/qt5/android/bluetooth/QtBluetoothBroadcastReceiver");
    if (!broadcastReceiverObject.isValid())
        return;
    broadcastReceiverObject.setField<jlong>("qtObject", reinterpret_cast<long>(this));

    intentFilterObject = QAndroidJniObject("android/content/IntentFilter");
    if (!intentFilterObject.isValid())
        return;

    valid = true;
}

AndroidBroadcastReceiver::~AndroidBroadcastReceiver()
{
}

bool AndroidBroadcastReceiver::isValid() const
{
    return valid;
}

void AndroidBroadcastReceiver::unregisterReceiver()
{
    if (!valid)
        return;

    broadcastReceiverObject.callMethod<void>("unregisterReceiver");
}

void AndroidBroadcastReceiver::addAction(const QAndroidJniObject &action)
{
    if (!valid || !action.isValid())
        return;

    intentFilterObject.callMethod<void>("addAction", "(Ljava/lang/String;)V", action.object<jstring>());

    contextObject.callObjectMethod(
                "registerReceiver",
                "(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;",
                broadcastReceiverObject.object<jobject>(),
                intentFilterObject.object<jobject>());
}

QT_END_NAMESPACE
