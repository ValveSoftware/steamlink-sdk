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

#include <QtDataVisualization/QCustom3DLabel>

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
    void invalidProperties();

private:
    QCustom3DLabel *m_custom;
};

void tst_custom::initTestCase()
{
}

void tst_custom::cleanupTestCase()
{
}

void tst_custom::init()
{
    m_custom = new QCustom3DLabel();
}

void tst_custom::cleanup()
{
    delete m_custom;
}

void tst_custom::construct()
{
    QCustom3DLabel *custom = new QCustom3DLabel();
    QVERIFY(custom);
    delete custom;

    custom = new QCustom3DLabel("label", QFont("Times New Roman", 10.0), QVector3D(1.0, 1.0, 1.0),
                                QVector3D(1.0, 1.0, 1.0), QQuaternion(1.0, 1.0, 10.0, 100.0));
    QVERIFY(custom);
    QCOMPARE(custom->backgroundColor(), QColor(Qt::gray));
    QCOMPARE(custom->isBackgroundEnabled(), true);
    QCOMPARE(custom->isBorderEnabled(), true);
    QCOMPARE(custom->isFacingCamera(), false);
    QCOMPARE(custom->font(), QFont("Times New Roman", 10));
    QCOMPARE(custom->text(), QString("label"));
    QCOMPARE(custom->textColor(), QColor(Qt::white));
    QCOMPARE(custom->meshFile(), QString(":/defaultMeshes/plane"));
    QCOMPARE(custom->position(), QVector3D(1.0, 1.0, 1.0));
    QCOMPARE(custom->isPositionAbsolute(), false);
    QCOMPARE(custom->rotation(), QQuaternion(1.0, 1.0, 10.0, 100.0));
    QCOMPARE(custom->scaling(), QVector3D(1.0, 1.0, 1.0));
    QCOMPARE(custom->isScalingAbsolute(), true);
    QCOMPARE(custom->isShadowCasting(), false);
    QCOMPARE(custom->textureFile(), QString());
    QCOMPARE(custom->isVisible(), true);
    delete custom;
}

void tst_custom::initialProperties()
{
    QVERIFY(m_custom);

    QCOMPARE(m_custom->backgroundColor(), QColor(Qt::gray));
    QCOMPARE(m_custom->isBackgroundEnabled(), true);
    QCOMPARE(m_custom->isBorderEnabled(), true);
    QCOMPARE(m_custom->isFacingCamera(), false);
    QCOMPARE(m_custom->font(), QFont("Arial", 20));
    QCOMPARE(m_custom->text(), QString());
    QCOMPARE(m_custom->textColor(), QColor(Qt::white));

    // Common (from QCustom3DItem)
    QCOMPARE(m_custom->meshFile(), QString(":/defaultMeshes/plane"));
    QCOMPARE(m_custom->position(), QVector3D());
    QCOMPARE(m_custom->isPositionAbsolute(), false);
    QCOMPARE(m_custom->rotation(), QQuaternion());
    QCOMPARE(m_custom->scaling(), QVector3D(0.1f, 0.1f, 0.1f));
    QCOMPARE(m_custom->isScalingAbsolute(), true);
    QCOMPARE(m_custom->isShadowCasting(), false);
    QCOMPARE(m_custom->textureFile(), QString());
    QCOMPARE(m_custom->isVisible(), true);
}

void tst_custom::initializeProperties()
{
    QVERIFY(m_custom);

    m_custom->setBackgroundColor(QColor(Qt::red));
    m_custom->setBackgroundEnabled(false);
    m_custom->setBorderEnabled(false);
    m_custom->setFacingCamera(true);
    m_custom->setFont(QFont("Times New Roman", 10));
    m_custom->setText(QString("This is a Custom Label"));
    m_custom->setTextColor(QColor(Qt::blue));

    QCOMPARE(m_custom->backgroundColor(), QColor(Qt::red));
    QCOMPARE(m_custom->isBackgroundEnabled(), false);
    QCOMPARE(m_custom->isBorderEnabled(), false);
    QCOMPARE(m_custom->isFacingCamera(), true);
    QCOMPARE(m_custom->font(), QFont("Times New Roman", 10));
    QCOMPARE(m_custom->text(), QString("This is a Custom Label"));
    QCOMPARE(m_custom->textColor(), QColor(Qt::blue));

    // Common (from QCustom3DItem)
    m_custom->setPosition(QVector3D(1.0, 1.0, 1.0));
    m_custom->setPositionAbsolute(true);
    m_custom->setRotation(QQuaternion(1.0, 1.0, 10.0, 100.0));
    m_custom->setScaling(QVector3D(1.0, 1.0, 1.0));
    m_custom->setShadowCasting(true);
    m_custom->setVisible(false);

    QCOMPARE(m_custom->position(), QVector3D(1.0, 1.0, 1.0));
    QCOMPARE(m_custom->isPositionAbsolute(), true);
    QCOMPARE(m_custom->rotation(), QQuaternion(1.0, 1.0, 10.0, 100.0));
    QCOMPARE(m_custom->scaling(), QVector3D(1.0, 1.0, 1.0));
    QCOMPARE(m_custom->isShadowCasting(), true);
    QCOMPARE(m_custom->isVisible(), false);
}

void tst_custom::invalidProperties()
{
    m_custom->setScalingAbsolute(false);
    QCOMPARE(m_custom->isScalingAbsolute(), true);
}

QTEST_MAIN(tst_custom)
#include "tst_custom.moc"
