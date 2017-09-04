/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

#include <QtDataVisualization/Q3DCamera>

using namespace QtDataVisualization;

class tst_camera: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void construct();

    void initialProperties();
    void initializeProperties();
    void invalidProperties();

    void changePresets();

private:
    Q3DCamera *m_camera;
};

void tst_camera::initTestCase()
{
}

void tst_camera::cleanupTestCase()
{
}

void tst_camera::init()
{
    m_camera = new Q3DCamera();
}

void tst_camera::cleanup()
{
    delete m_camera;
}

void tst_camera::construct()
{
    Q3DCamera *camera = new Q3DCamera();
    QVERIFY(camera);
    delete camera;
}

void tst_camera::initialProperties()
{
    QVERIFY(m_camera);

    QCOMPARE(m_camera->cameraPreset(), Q3DCamera::CameraPresetNone);
    QCOMPARE(m_camera->maxZoomLevel(), 500.0f);
    QCOMPARE(m_camera->minZoomLevel(), 10.0f);
    QCOMPARE(m_camera->target(), QVector3D(0.0, 0.0, 0.0));
    QCOMPARE(m_camera->wrapXRotation(), true);
    QCOMPARE(m_camera->wrapYRotation(), false);
    QCOMPARE(m_camera->xRotation(), 0.0f);
    QCOMPARE(m_camera->yRotation(), 0.0f);
    QCOMPARE(m_camera->zoomLevel(), 100.0f);

    // Common (from Q3DObject)
    QVERIFY(!m_camera->parentScene());
    QCOMPARE(m_camera->position(), QVector3D(0, 0, 0));
}

void tst_camera::initializeProperties()
{
    QVERIFY(m_camera);

    m_camera->setMaxZoomLevel(1000.0f);
    m_camera->setMinZoomLevel(100.0f);
    m_camera->setTarget(QVector3D(1.0, -1.0, 1.0));
    m_camera->setWrapXRotation(false);
    m_camera->setWrapYRotation(true);
    m_camera->setXRotation(30.0f);
    m_camera->setYRotation(30.0f);
    m_camera->setZoomLevel(500.0f);

    QCOMPARE(m_camera->maxZoomLevel(), 1000.0f);
    QCOMPARE(m_camera->minZoomLevel(), 100.0f);
    QCOMPARE(m_camera->target(), QVector3D(1.0, -1.0, 1.0));
    QCOMPARE(m_camera->wrapXRotation(), false);
    QCOMPARE(m_camera->wrapYRotation(), true);
    QCOMPARE(m_camera->xRotation(), 30.0f);
    QCOMPARE(m_camera->yRotation(), 30.0f);
    QCOMPARE(m_camera->zoomLevel(), 500.0f);

    m_camera->setPosition(QVector3D(1.0, 1.0, 1.0));

    // Common (from Q3DObject)
    QCOMPARE(m_camera->position(), QVector3D(1.0, 1.0, 1.0));
}

void tst_camera::invalidProperties()
{
    m_camera->setTarget(QVector3D(-1.5, -1.5, -1.5));
    QCOMPARE(m_camera->target(), QVector3D(-1.0, -1.0, -1.0));

    m_camera->setTarget(QVector3D(1.5, 1.5, 1.5));
    QCOMPARE(m_camera->target(), QVector3D(1.0, 1.0, 1.0));

    m_camera->setMinZoomLevel(0.1f);
    QCOMPARE(m_camera->minZoomLevel(), 1.0f);
}

void tst_camera::changePresets()
{
    m_camera->setCameraPreset(Q3DCamera::CameraPresetBehind); // Will be overridden by the the following sets
    m_camera->setMaxZoomLevel(1000.0f);
    m_camera->setMinZoomLevel(100.0f);
    m_camera->setTarget(QVector3D(1.0, -1.0, 1.0));
    m_camera->setWrapXRotation(false);
    m_camera->setWrapYRotation(true);
    m_camera->setXRotation(30.0f);
    m_camera->setYRotation(30.0f);
    m_camera->setZoomLevel(500.0f);

    QCOMPARE(m_camera->cameraPreset(), Q3DCamera::CameraPresetNone);
    QCOMPARE(m_camera->maxZoomLevel(), 1000.0f);
    QCOMPARE(m_camera->minZoomLevel(), 100.0f);
    QCOMPARE(m_camera->target(), QVector3D(1.0, -1.0, 1.0));
    QCOMPARE(m_camera->wrapXRotation(), false);
    QCOMPARE(m_camera->wrapYRotation(), true);
    QCOMPARE(m_camera->xRotation(), 30.0f);
    QCOMPARE(m_camera->yRotation(), 30.0f);
    QCOMPARE(m_camera->zoomLevel(), 500.0f);

    m_camera->setCameraPreset(Q3DCamera::CameraPresetBehind); // Sets target and rotations

    QCOMPARE(m_camera->cameraPreset(), Q3DCamera::CameraPresetBehind);
    QCOMPARE(m_camera->maxZoomLevel(), 1000.0f);
    QCOMPARE(m_camera->minZoomLevel(), 100.0f);
    QCOMPARE(m_camera->target(), QVector3D(0.0, 0.0, 0.0));
    QCOMPARE(m_camera->wrapXRotation(), false);
    QCOMPARE(m_camera->wrapYRotation(), true);
    QCOMPARE(m_camera->xRotation(), 180.0f);
    QCOMPARE(m_camera->yRotation(), 22.5f);
    QCOMPARE(m_camera->zoomLevel(), 500.0f);

    m_camera->setCameraPosition(10.0f, 15.0f, 125.0f); // Overrides preset

    QCOMPARE(m_camera->cameraPreset(), Q3DCamera::CameraPresetNone);
    QCOMPARE(m_camera->maxZoomLevel(), 1000.0f);
    QCOMPARE(m_camera->minZoomLevel(), 100.0f);
    QCOMPARE(m_camera->target(), QVector3D(0.0, 0.0, 0.0));
    QCOMPARE(m_camera->wrapXRotation(), false);
    QCOMPARE(m_camera->wrapYRotation(), true);
    QCOMPARE(m_camera->xRotation(), 10.0f);
    QCOMPARE(m_camera->yRotation(), 15.0f);
    QCOMPARE(m_camera->zoomLevel(), 125.0f);
}

QTEST_MAIN(tst_camera)
#include "tst_camera.moc"
