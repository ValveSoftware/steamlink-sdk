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
#include <QtAndroidExtras/QtAndroid>

class tst_QAndroidFunctions : public QObject
{
    Q_OBJECT
private slots:
    void testAndroidSdkVersion();
    void testAndroidActivity();
    void testRunOnAndroidThread();
};

void tst_QAndroidFunctions::testAndroidSdkVersion()
{
    QVERIFY(QtAndroid::androidSdkVersion() > 0);
}

void tst_QAndroidFunctions::testAndroidActivity()
{
    QAndroidJniObject activity = QtAndroid::androidActivity();
    QVERIFY(activity.isValid());
    QVERIFY(activity.callMethod<jboolean>("isTaskRoot"));
}

void tst_QAndroidFunctions::testRunOnAndroidThread()
{
    int a = 0;

    // test async operation
    QtAndroid::runOnAndroidThread([&a]{
        a = 1;
    });
    QTRY_COMPARE(a, 1); // wait for async op. to finish

    // test sync operation
    QtAndroid::runOnAndroidThreadSync([&a]{
        a = 2;
    });
    QCOMPARE(a, 2);

    // test async/async lock
    QtAndroid::runOnAndroidThread([&a]{
        QtAndroid::runOnAndroidThread([&a]{
            a = 3;
        });
    });
    QTRY_COMPARE(a, 3);  // wait for async op. to finish

    // test async/sync lock
    QtAndroid::runOnAndroidThread([&a]{
        QtAndroid::runOnAndroidThreadSync([&a]{
            a = 5;
        });
    });
    QTRY_COMPARE(a, 5);

    // test sync/sync lock
    QtAndroid::runOnAndroidThreadSync([&a]{
        QtAndroid::runOnAndroidThreadSync([&a]{
            a = 4;
        });
    });
    QCOMPARE(a, 4);


    // test sync/async lock
    QtAndroid::runOnAndroidThreadSync([&a]{
        QtAndroid::runOnAndroidThread([&a]{
            a = 6;
        });
    });
    QCOMPARE(a, 6);
}

QTEST_APPLESS_MAIN(tst_QAndroidFunctions)

#include "tst_qandroidfunctions.moc"
