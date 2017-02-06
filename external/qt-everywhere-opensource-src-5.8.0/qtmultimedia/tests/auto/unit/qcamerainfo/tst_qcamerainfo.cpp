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

#include <QtTest/QtTest>
#include <QDebug>

#include <qcamera.h>
#include <qcamerainfo.h>

#include "mockcameraservice.h"
#include "mockmediaserviceprovider.h"

QT_USE_NAMESPACE

class tst_QCameraInfo: public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void init();
    void cleanup();

private slots:
    void constructor();
    void defaultCamera();
    void availableCameras();
    void equality_operators();

private:
    MockSimpleCameraService  *mockSimpleCameraService;
    MockCameraService *mockCameraService;
    MockMediaServiceProvider *provider;
};

void tst_QCameraInfo::initTestCase()
{
}

void tst_QCameraInfo::init()
{
    provider = new MockMediaServiceProvider;
    mockSimpleCameraService = new MockSimpleCameraService;
    mockCameraService = new MockCameraService;

    provider->service = mockCameraService;
    QMediaServiceProvider::setDefaultServiceProvider(provider);
}

void tst_QCameraInfo::cleanup()
{
    delete provider;
    delete mockCameraService;
    delete mockSimpleCameraService;
}

void tst_QCameraInfo::constructor()
{
    // Service doesn't implement QVideoDeviceSelectorControl
    // QCameraInfo should not be valid in this case
    provider->service = mockSimpleCameraService;

    {
        QCamera camera;
        QCameraInfo info(camera);
        QVERIFY(info.isNull());
        QVERIFY(info.deviceName().isEmpty());
        QVERIFY(info.description().isEmpty());
        QCOMPARE(info.position(), QCamera::UnspecifiedPosition);
        QCOMPARE(info.orientation(), 0);
    }

    // Service implements QVideoDeviceSelectorControl
    provider->service = mockCameraService;

    {
        // default camera
        QCamera camera;
        QCameraInfo info(camera);
        QVERIFY(!info.isNull());
        QCOMPARE(info.deviceName(), QStringLiteral("othercamera"));
        QCOMPARE(info.description(), QStringLiteral("othercamera desc"));
        QCOMPARE(info.position(), QCamera::UnspecifiedPosition);
        QCOMPARE(info.orientation(), 0);
    }

    QCamera camera("backcamera");
    QCameraInfo info(camera);
    QVERIFY(!info.isNull());
    QCOMPARE(info.deviceName(), QStringLiteral("backcamera"));
    QCOMPARE(info.description(), QStringLiteral("backcamera desc"));
    QCOMPARE(info.position(), QCamera::BackFace);
    QCOMPARE(info.orientation(), 90);

    QCameraInfo info2(info);
    QVERIFY(!info2.isNull());
    QCOMPARE(info2.deviceName(), QStringLiteral("backcamera"));
    QCOMPARE(info2.description(), QStringLiteral("backcamera desc"));
    QCOMPARE(info2.position(), QCamera::BackFace);
    QCOMPARE(info2.orientation(), 90);
}

void tst_QCameraInfo::defaultCamera()
{
    provider->service = mockCameraService;

    QCameraInfo info = QCameraInfo::defaultCamera();

    QVERIFY(!info.isNull());
    QCOMPARE(info.deviceName(), QStringLiteral("othercamera"));
    QCOMPARE(info.description(), QStringLiteral("othercamera desc"));
    QCOMPARE(info.position(), QCamera::UnspecifiedPosition);
    QCOMPARE(info.orientation(), 0);

    QCamera camera(info);
    QCOMPARE(QCameraInfo(camera), info);
}

void tst_QCameraInfo::availableCameras()
{
    provider->service = mockCameraService;

    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    QCOMPARE(cameras.count(), 2);

    QCameraInfo info = cameras.at(0);
    QVERIFY(!info.isNull());
    QCOMPARE(info.deviceName(), QStringLiteral("backcamera"));
    QCOMPARE(info.description(), QStringLiteral("backcamera desc"));
    QCOMPARE(info.position(), QCamera::BackFace);
    QCOMPARE(info.orientation(), 90);

    info = cameras.at(1);
    QVERIFY(!info.isNull());
    QCOMPARE(info.deviceName(), QStringLiteral("othercamera"));
    QCOMPARE(info.description(), QStringLiteral("othercamera desc"));
    QCOMPARE(info.position(), QCamera::UnspecifiedPosition);
    QCOMPARE(info.orientation(), 0);

    cameras = QCameraInfo::availableCameras(QCamera::BackFace);
    QCOMPARE(cameras.count(), 1);
    info = cameras.at(0);
    QVERIFY(!info.isNull());
    QCOMPARE(info.deviceName(), QStringLiteral("backcamera"));
    QCOMPARE(info.description(), QStringLiteral("backcamera desc"));
    QCOMPARE(info.position(), QCamera::BackFace);
    QCOMPARE(info.orientation(), 90);

    cameras = QCameraInfo::availableCameras(QCamera::FrontFace);
    QCOMPARE(cameras.count(), 0);
}

void tst_QCameraInfo::equality_operators()
{
    provider->service = mockCameraService;

    QCameraInfo defaultCamera = QCameraInfo::defaultCamera();
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();

    QVERIFY(defaultCamera == cameras.at(1));
    QVERIFY(defaultCamera != cameras.at(0));
    QVERIFY(cameras.at(0) != cameras.at(1));

    {
        QCamera camera(defaultCamera);
        QVERIFY(QCameraInfo(camera) == defaultCamera);
        QVERIFY(QCameraInfo(camera) == cameras.at(1));
    }

    {
        QCamera camera(cameras.at(0));
        QVERIFY(QCameraInfo(camera) == cameras.at(0));
    }
}


QTEST_MAIN(tst_QCameraInfo)

#include "tst_qcamerainfo.moc"
