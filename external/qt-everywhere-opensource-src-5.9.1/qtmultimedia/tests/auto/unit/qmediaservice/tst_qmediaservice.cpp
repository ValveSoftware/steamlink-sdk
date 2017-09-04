/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>

#include <qvideodeviceselectorcontrol.h>
#include <qmediacontrol.h>
#include <qmediaservice.h>

#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

class QtTestMediaService;

class QtTestMediaControlA : public QMediaControl
{
    Q_OBJECT
};

#define QtTestMediaControlA_iid "com.nokia.QtTestMediaControlA"
Q_MEDIA_DECLARE_CONTROL(QtTestMediaControlA, QtTestMediaControlA_iid)

class QtTestMediaControlB : public QMediaControl
{
    Q_OBJECT
};

#define QtTestMediaControlB_iid "com.nokia.QtTestMediaControlB"
Q_MEDIA_DECLARE_CONTROL(QtTestMediaControlB, QtTestMediaControlB_iid)


class QtTestMediaControlC : public QMediaControl
{
    Q_OBJECT
};

#define QtTestMediaControlC_iid "com.nokia.QtTestMediaControlC"
Q_MEDIA_DECLARE_CONTROL(QtTestMediaControlC, QtTestMediaControlA_iid) // Yes A.

class QtTestMediaControlD : public QMediaControl
{
    Q_OBJECT
};

#define QtTestMediaControlD_iid "com.nokia.QtTestMediaControlD"
Q_MEDIA_DECLARE_CONTROL(QtTestMediaControlD, QtTestMediaControlD_iid)

//unimplemented service
#define QtTestMediaControlE_iid "com.nokia.QtTestMediaControlF"
class QtTestMediaControlE : public QMediaControl
{
    Q_OBJECT
};

/* implementation of child class by inheriting The QMediaService base class for media service implementations. */
class QtTestMediaService : public QMediaService
{
    Q_OBJECT
public:
    int refA;
    int refB;
    int refC;
    QtTestMediaControlA controlA;
    QtTestMediaControlB controlB;
    QtTestMediaControlC controlC;

    //constructor
    QtTestMediaService(): QMediaService(0), refA(0), refB(0), refC(0)
    {
    }

    //requestControl() pure virtual function of QMediaService class.
    QMediaControl *requestControl(const char *name)
    {
        if (strcmp(name, QtTestMediaControlA_iid) == 0) {
            refA += 1;

            return &controlA;
        } else if (strcmp(name, QtTestMediaControlB_iid) == 0) {
            refB += 1;

            return &controlB;
        } else if (strcmp(name, QtTestMediaControlC_iid) == 0) {
            refA += 1;

            return &controlA;
        } else {
            return 0;
        }
    }

    //releaseControl() pure virtual function of QMediaService class.
    void releaseControl(QMediaControl *control)
    {
        if (control == &controlA)
            refA -= 1;
        else if (control == &controlB)
            refB -= 1;
        else if (control == &controlC)
            refC -= 1;
    }

    //requestControl() function of QMediaService class.
    using QMediaService::requestControl;
};

/* Test case implementation for QMediaService class which provides a common base class for media service implementations.*/
class tst_QMediaService : public QObject
{
    Q_OBJECT
private slots:
    void tst_destructor();
    void tst_releaseControl();
    void tst_requestControl();
    void tst_requestControlTemplate();

    void initTestCase();

    void control_iid();
    void control();

};

/*MaemoAPI-1668 :destructor property test. */
void tst_QMediaService::tst_destructor()
{
    QtTestMediaService *service = new QtTestMediaService;
    delete service;
}

void tst_QMediaService::initTestCase()
{
}

/*MaemoAPI-1669 :releaseControl() API property test. */
void tst_QMediaService::tst_releaseControl()
{
    //test class instance creation
    QtTestMediaService service;

    //Get a pointer to the media control implementing interface and verify.
    QMediaControl* controlA = service.requestControl(QtTestMediaControlA_iid);
    QCOMPARE(controlA, &service.controlA);
    service.releaseControl(controlA); //Controls must be returned to the service when no longer needed
    QVERIFY(service.refA == 0);

    //Get a pointer to the media control implementing interface and verify.
    QMediaControl* controlB = service.requestControl(QtTestMediaControlB_iid);
    QCOMPARE(controlB, &service.controlB);
    service.releaseControl(controlB); //Controls must be returned to the service when no longer needed
    QVERIFY(service.refB == 0);
}

/*MaemoAPI-1670 :requestControl() API property test. */
void tst_QMediaService::tst_requestControl()
{
    //test class instance creation
    QtTestMediaService service;

    //Get a pointer to the media control implementing interface and verify.
    QMediaControl* controlA = service.requestControl(QtTestMediaControlA_iid);
    QCOMPARE(controlA, &service.controlA);
    service.releaseControl(controlA); //Controls must be returned to the service when no longer needed

    //Get a pointer to the media control implementing interface and verify.
    QMediaControl* controlB = service.requestControl(QtTestMediaControlB_iid);
    QCOMPARE(controlB, &service.controlB);
    service.releaseControl(controlB); //Controls must be returned to the service when no longer needed

    //If the service does not implement the control, a null pointer is returned instead.
    QMediaControl* controlE = service.requestControl(QtTestMediaControlE_iid);
    QVERIFY(!controlE); //should return null pointer
    service.releaseControl(controlE); //Controls must be returned to the service when no longer needed

    //If the service is unavailable a null pointer is returned instead.
    QMediaControl* control = service.requestControl("");
    QVERIFY(!control); //should return null pointer
    service.releaseControl(control); //Controls must be returned to the service when no longer needed
}

/*MaemoAPI-1671 :requestControl() API property test. */
void tst_QMediaService::tst_requestControlTemplate()
{
    //test class instance creation
    QtTestMediaService service;

    //Get a pointer to the media control of type T implemented by a media service.
    QtTestMediaControlA *controlA = service.requestControl<QtTestMediaControlA *>();
    QCOMPARE(controlA, &service.controlA);
    service.releaseControl(controlA);

    //Get a pointer to the media control of type T implemented by a media service.
    QtTestMediaControlB *controlB = service.requestControl<QtTestMediaControlB *>();
    QCOMPARE(controlB, &service.controlB);
    service.releaseControl(controlB);

    QVERIFY(!service.requestControl<QtTestMediaControlC *>());  // Faulty implementation returns A.
    QCOMPARE(service.refA, 0);  // Verify the control was released.

    QVERIFY(!service.requestControl<QtTestMediaControlD *>());  // No control of that type.
}


void tst_QMediaService::control_iid()
{
    const char *nullString = 0;

    // Default implementation.
    QCOMPARE(qmediacontrol_iid<QtTestMediaControlE *>(), nullString);

    // Partial template.
    QVERIFY(qstrcmp(qmediacontrol_iid<QtTestMediaControlA *>(), QtTestMediaControlA_iid) == 0);
}

void tst_QMediaService::control()
{
    QtTestMediaService service;

    QtTestMediaControlA *controlA = service.requestControl<QtTestMediaControlA *>();
    QCOMPARE(controlA, &service.controlA);
    service.releaseControl(controlA);

    QtTestMediaControlB *controlB = service.requestControl<QtTestMediaControlB *>();
    QCOMPARE(controlB, &service.controlB);
    service.releaseControl(controlB);

    QVERIFY(!service.requestControl<QtTestMediaControlC *>());  // Faulty implementation returns A, but is wrong class
    QCOMPARE(service.refA, 0);  // Verify the control was released.

    QVERIFY(!service.requestControl<QtTestMediaControlD *>());  // No control of that type.
}

QT_END_NAMESPACE

QT_USE_NAMESPACE

QTEST_MAIN(tst_QMediaService)

#include "tst_qmediaservice.moc"
