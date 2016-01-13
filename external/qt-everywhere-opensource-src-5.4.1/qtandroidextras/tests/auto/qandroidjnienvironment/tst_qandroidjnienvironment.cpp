/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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
