/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtCore/private/qjnihelpers_p.h>

class tst_QAndroidJniEnvironment : public QObject
{
    Q_OBJECT

private slots:
    void jniEnv();
    void javaVM();
};

void tst_QAndroidJniEnvironment::jniEnv()
{
    JavaVM *javaVM = QtAndroidPrivate::javaVM();
    QVERIFY(javaVM);

    {
        QAndroidJniEnvironment env;

        // JNI environment should now be attached to the current thread
        JNIEnv *jni = 0;
        QCOMPARE(javaVM->GetEnv((void**)&jni, JNI_VERSION_1_6), JNI_OK);

        JNIEnv *e = env;
        QVERIFY(e);

        QCOMPARE(env->GetVersion(), JNI_VERSION_1_6);

        // try to find an existing class
        QVERIFY(env->FindClass("java/lang/Object"));
        QVERIFY(!env->ExceptionCheck());

        // try to find a nonexistent class
        QVERIFY(!env->FindClass("this/doesnt/Exist"));
        QVERIFY(env->ExceptionCheck());
        env->ExceptionClear();
    }

    // The environment should automatically be detached when QAndroidJniEnvironment goes out of scope
    JNIEnv *jni = 0;
    QCOMPARE(javaVM->GetEnv((void**)&jni, JNI_VERSION_1_6), JNI_EDETACHED);
}

void tst_QAndroidJniEnvironment::javaVM()
{
    JavaVM *javaVM = QtAndroidPrivate::javaVM();
    QVERIFY(javaVM);

    QAndroidJniEnvironment env;
    QCOMPARE(env.javaVM(), javaVM);

    JavaVM *vm = 0;
    QCOMPARE(env->GetJavaVM(&vm), JNI_OK);
    QCOMPARE(env.javaVM(), vm);
}

QTEST_APPLESS_MAIN(tst_QAndroidJniEnvironment)

#include "tst_qandroidjnienvironment.moc"
