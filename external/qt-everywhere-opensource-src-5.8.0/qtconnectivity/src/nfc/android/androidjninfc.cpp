/****************************************************************************
**
** Copyright (C) 2016 Centria research and development
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "androidjninfc_p.h"

#include <android/log.h>

#include "androidmainnewintentlistener_p.h"

#include "qglobal.h"
#include "qbytearray.h"
#include "qdebug.h"

static const char *nfcClassName = "org/qtproject/qt5/android/nfc/QtNfc";

static AndroidNfc::MainNfcNewIntentListener mainListener;

QT_BEGIN_ANDROIDNFC_NAMESPACE

bool startDiscovery()
{
    return QAndroidJniObject::callStaticMethod<jboolean>(nfcClassName,"start");
}

bool isAvailable()
{
    return QAndroidJniObject::callStaticMethod<jboolean>(nfcClassName,"isAvailable");
}

bool stopDiscovery()
{
    return QAndroidJniObject::callStaticMethod<jboolean>(nfcClassName,"stop");
}

QAndroidJniObject getStartIntent()
{
    QAndroidJniObject ret = QAndroidJniObject::callStaticObjectMethod(nfcClassName, "getStartIntent", "()Landroid/content/Intent;");
    return ret;
}

bool registerListener(AndroidNfcListenerInterface *listener)
{
    return mainListener.registerListener(listener);
}

bool unregisterListener(AndroidNfcListenerInterface *listener)
{
    return mainListener.unregisterListener(listener);
}

QAndroidJniObject getTag(const QAndroidJniObject &intent)
{
    QAndroidJniObject extraTag = QAndroidJniObject::getStaticObjectField("android/nfc/NfcAdapter", "EXTRA_TAG", "Ljava/lang/String;");
    QAndroidJniObject tag = intent.callObjectMethod("getParcelableExtra", "(Ljava/lang/String;)Landroid/os/Parcelable;", extraTag.object<jstring>());
    Q_ASSERT_X(tag.isValid(), "getTag", "could not get Tag object");
    return tag;
}

QT_END_ANDROIDNFC_NAMESPACE

Q_DECL_EXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /*reserved*/)
{
    static bool initialized = false;
    if (initialized)
        return JNI_VERSION_1_6;
    initialized = true;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    return JNI_VERSION_1_6;
}
