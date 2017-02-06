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
