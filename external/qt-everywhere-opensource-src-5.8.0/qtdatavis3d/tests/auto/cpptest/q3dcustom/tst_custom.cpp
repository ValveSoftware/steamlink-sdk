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

#include <QtDataVisualization/QCustom3DItem>

using namespace QtDataVisualization;

class tst_custom: public QObject
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

private:
    QCustom3DItem *m_custom;
};

void tst_custom::initTestCase()
{
}

void tst_custom::cleanupTestCase()
{
}

void tst_custom::init()
{
    m_custom = new QCustom3DItem();
}

void tst_custom::cleanup()
{
    delete m_custom;
}

void tst_custom::construct()
{
    QCustom3DItem *custom = new QCustom3DItem();
    QVERIFY(custom);
    delete custom;

    custom = new QCustom3DItem(":/customitem.obj", QVector3D(1.0, 1.0, 1.0),
                               QVector3D(1.0, 1.0, 1.0), QQuaternion(1.0, 1.0, 10.0, 100.0),
                               QImage(":/customtexture.jpg"));
    QVERIFY(custom);
    QCOMPARE(custom->meshFile(), QString(":/customitem.obj"));
    QCOMPARE(custom->position(), QVector3D(1.0, 1.0, 1.0));
    QCOMPARE(custom->isPositionAbsolute(), false);
    QCOMPARE(custom->rotation(), QQuaternion(1.0, 1.0, 10.0, 100.0));
    QCOMPARE(custom->scaling(), QVector3D(1.0, 1.0, 1.0));
    QCOMPARE(custom->isScalingAbsolute(), true);
    QCOMPARE(custom->isShadowCasting(), true);
    QCOMPARE(custom->textureFile(), QString());
    QCOMPARE(custom->isVisible(), true);
    delete custom;
}

void tst_custom::initialProperties()
{
    QVERIFY(m_custom);

    QCOMPARE(m_custom->meshFile(), QString());
    QCOMPARE(m_custom->position(), QVector3D());
    QCOMPARE(m_custom->isPositionAbsolute(), false);
    QCOMPARE(m_custom->rotation(), QQuaternion());
    QCOMPARE(m_custom->scaling(), QVector3D(0.1f, 0.1f, 0.1f));
    QCOMPARE(m_custom->isScalingAbsolute(), true);
    QCOMPARE(m_custom->isShadowCasting(), true);
    QCOMPARE(m_custom->textureFile(), QString());
    QCOMPARE(m_custom->isVisible(), true);
}

void tst_custom::initializeProperties()
{
    QVERIFY(m_custom);

    m_custom->setMeshFile(":/customitem.obj");
    m_custom->setPosition(QVector3D(1.0, 1.0, 1.0));
    m_custom->setPositionAbsolute(true);
    m_custom->setRotation(QQuaternion(1.0, 1.0, 10.0, 100.0));
    m_custom->setScaling(QVector3D(1.0, 1.0, 1.0));
    m_custom->setScalingAbsolute(false);
    m_custom->setShadowCasting(false);
    m_custom->setTextureFile(":/customtexture.jpg");
    m_custom->setVisible(false);

    QCOMPARE(m_custom->meshFile(), QString(":/customitem.obj"));
    QCOMPARE(m_custom->position(), QVector3D(1.0, 1.0, 1.0));
    QCOMPARE(m_custom->isPositionAbsolute(), true);
    QCOMPARE(m_custom->rotation(), QQuaternion(1.0, 1.0, 10.0, 100.0));
    QCOMPARE(m_custom->scaling(), QVector3D(1.0, 1.0, 1.0));
    QCOMPARE(m_custom->isScalingAbsolute(), false);
    QCOMPARE(m_custom->isShadowCasting(), false);
    QCOMPARE(m_custom->textureFile(), QString(":/customtexture.jpg"));
    QCOMPARE(m_custom->isVisible(), false);

    m_custom->setTextureImage(QImage(QSize(10, 10), QImage::Format_ARGB32));
    QCOMPARE(m_custom->textureFile(), QString());
}

QTEST_MAIN(tst_custom)
#include "tst_custom.moc"
