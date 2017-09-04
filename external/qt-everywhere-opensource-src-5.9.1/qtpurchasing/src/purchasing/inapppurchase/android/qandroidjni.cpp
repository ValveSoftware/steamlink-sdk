/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3-COMM$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qandroidinapppurchasebackend_p.h"

#include <QtCore/qdatetime.h>
#include <QtAndroidExtras/qandroidjniobject.h>
#include <jni.h>

QT_USE_NAMESPACE

static void queryFailed(JNIEnv *, jclass, jlong nativePointer, jstring productId)
{
    QAndroidInAppPurchaseBackend *backend = reinterpret_cast<QAndroidInAppPurchaseBackend *>(nativePointer);
    QMetaObject::invokeMethod(backend,
                              "registerQueryFailure",
                              Qt::AutoConnection,
                              Q_ARG(QString, QAndroidJniObject(productId).toString()));
}

static void purchasedProductsQueried(JNIEnv *, jclass, jlong nativePointer)
{
    QAndroidInAppPurchaseBackend *backend = reinterpret_cast<QAndroidInAppPurchaseBackend *>(nativePointer);
    QMetaObject::invokeMethod(backend,
                              "registerReady",
                              Qt::AutoConnection);
}

static void registerProduct(JNIEnv *, jclass, jlong nativePointer, jstring productId, jstring price, jstring title, jstring description)
{
    QAndroidInAppPurchaseBackend *backend = reinterpret_cast<QAndroidInAppPurchaseBackend *>(nativePointer);
    QMetaObject::invokeMethod(backend,
                              "registerProduct",
                              Qt::AutoConnection,
                              Q_ARG(QString, QAndroidJniObject(productId).toString()),
                              Q_ARG(QString, QAndroidJniObject(price).toString()),
                              Q_ARG(QString, QAndroidJniObject(title).toString()),
                              Q_ARG(QString, QAndroidJniObject(description).toString()));
}

static void registerPurchased(JNIEnv *, jclass, jlong nativePointer, jstring identifier,
                              jstring signature, jstring data, jstring purchaseToken, jstring orderId, jlong timestamp)
{
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(qint64(timestamp));
    dateTime.setTimeSpec(Qt::LocalTime);

    QAndroidInAppPurchaseBackend *backend = reinterpret_cast<QAndroidInAppPurchaseBackend *>(nativePointer);
    QMetaObject::invokeMethod(backend,
                              "registerPurchased",
                              Qt::AutoConnection,
                              Q_ARG(QString, QAndroidJniObject(identifier).toString()),
                              Q_ARG(QString, QAndroidJniObject(signature).toString()),
                              Q_ARG(QString, QAndroidJniObject(data).toString()),
                              Q_ARG(QString, QAndroidJniObject(purchaseToken).toString()),
                              Q_ARG(QString, QAndroidJniObject(orderId).toString()),
                              Q_ARG(QDateTime, dateTime));
}

static void purchaseSucceeded(JNIEnv *, jclass, jlong nativePointer, jint requestCode,
                              jstring signature, jstring data, jstring purchaseToken, jstring orderId, jlong timestamp)
{
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(qint64(timestamp));
    dateTime.setTimeSpec(Qt::LocalTime);

    QAndroidInAppPurchaseBackend *backend = reinterpret_cast<QAndroidInAppPurchaseBackend *>(nativePointer);
    QMetaObject::invokeMethod(backend,
                              "purchaseSucceeded",
                              Qt::AutoConnection,
                              Q_ARG(int, int(requestCode)),
                              Q_ARG(QString, QAndroidJniObject(signature).toString()),
                              Q_ARG(QString, QAndroidJniObject(data).toString()),
                              Q_ARG(QString, QAndroidJniObject(purchaseToken).toString()),
                              Q_ARG(QString, QAndroidJniObject(orderId).toString()),
                              Q_ARG(QDateTime, dateTime));
}

static void purchaseFailed(JNIEnv *, jclass, jlong nativePointer, jint requestCode, jint failureReason, jstring errorString)
{
    QAndroidInAppPurchaseBackend *backend = reinterpret_cast<QAndroidInAppPurchaseBackend *>(nativePointer);
    QMetaObject::invokeMethod(backend,
                              "purchaseFailed",
                              Qt::AutoConnection,
                              Q_ARG(int, int(requestCode)),
                              Q_ARG(int, int(failureReason)),
                              Q_ARG(QString, QAndroidJniObject(errorString).toString()));
}

static JNINativeMethod methods[] = {
    {"queryFailed", "(JLjava/lang/String;)V", (void *)queryFailed},
    {"purchasedProductsQueried", "(J)V", (void *)purchasedProductsQueried},
    {"registerProduct", "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", (void *)registerProduct},
    {"registerPurchased", "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J)V", (void *)registerPurchased},
    {"purchaseSucceeded", "(JILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J)V", (void *)purchaseSucceeded},
    {"purchaseFailed", "(JIILjava/lang/String;)V", (void *)purchaseFailed}
};

jint JNICALL JNI_OnLoad(JavaVM *vm, void *)
{
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4) != JNI_OK)
        return JNI_FALSE;

    jclass clazz = env->FindClass("org/qtproject/qt5/android/purchasing/QtInAppPurchase");
    if (env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0])) < 0)
        return JNI_FALSE;

    return JNI_VERSION_1_4;
}
