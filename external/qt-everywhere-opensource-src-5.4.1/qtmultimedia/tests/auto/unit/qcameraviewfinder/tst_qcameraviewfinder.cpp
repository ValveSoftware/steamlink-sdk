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
#include <QDebug>

#include "qcameraviewfinder.h"
#include "qcamera.h"
#include "qmediaobject.h"

#include "mockcameraservice.h"
#include "mockmediaserviceprovider.h"

class tst_QCameraViewFinder : public QObject
{
    Q_OBJECT
public slots:
    void initTestCase();
    void cleanupTestCase();

private slots:
    void testConstructor();
    void testMediaObject();

private:
    MockCameraService  *mockcameraservice;
    MockMediaServiceProvider *provider;
    QCamera *camera;
    QCameraViewfinder *viewFinder;
};

void tst_QCameraViewFinder::initTestCase()
{
    provider = new MockMediaServiceProvider;
    mockcameraservice = new MockCameraService;
    provider->service = mockcameraservice;
    QMediaServiceProvider::setDefaultServiceProvider(provider);

    camera = new QCamera;
    viewFinder = new QCameraViewfinder();
}

void tst_QCameraViewFinder::cleanupTestCase()
{
    delete mockcameraservice;
    delete provider;
}

void tst_QCameraViewFinder::testConstructor()
{
    /* Verify whether the object is created or not */
    QVERIFY(viewFinder != NULL);
    QCOMPARE(viewFinder->isVisible(),false);
    QCOMPARE(viewFinder->isEnabled(),true);
    viewFinder->show();
}

void tst_QCameraViewFinder::testMediaObject()
{
    QVERIFY(viewFinder != NULL);
    viewFinder->show();
    /* Sets the QVideoWidget  based camera viewfinder.*/
    camera->setViewfinder(viewFinder);
    QCOMPARE(viewFinder->isVisible(),true);

    /* Return the currently attached media object.*/
    QMediaObject *media = viewFinder->mediaObject();

    /* Verifying the object */
    QCOMPARE(media, camera);
}

QTEST_MAIN(tst_QCameraViewFinder)
#include "tst_qcameraviewfinder.moc"
