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

#include <QtDataVisualization/QBar3DSeries>

using namespace QtDataVisualization;

class tst_series: public QObject
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
    QBar3DSeries *m_series;
};

void tst_series::initTestCase()
{
}

void tst_series::cleanupTestCase()
{
}

void tst_series::init()
{
    m_series = new QBar3DSeries();
}

void tst_series::cleanup()
{
    delete m_series;
}

void tst_series::construct()
{
    QBar3DSeries *series = new QBar3DSeries();
    QVERIFY(series);
    delete series;

    QBarDataProxy *proxy = new QBarDataProxy();

    series = new QBar3DSeries(proxy);
    QVERIFY(series);
    QCOMPARE(series->dataProxy(), proxy);
    delete series;
}

void tst_series::initialProperties()
{
    QVERIFY(m_series);

    QVERIFY(m_series->dataProxy());
    QCOMPARE(m_series->meshAngle(), 0.0f);
    QCOMPARE(m_series->selectedBar(), m_series->invalidSelectionPosition());

    // Common properties
    QCOMPARE(m_series->baseColor(), QColor(Qt::black));
    QCOMPARE(m_series->baseGradient(), QLinearGradient());
    QCOMPARE(m_series->colorStyle(), Q3DTheme::ColorStyleUniform);
    QCOMPARE(m_series->itemLabel(), QString(""));
    QCOMPARE(m_series->itemLabelFormat(), QString("@valueLabel"));
    QCOMPARE(m_series->isItemLabelVisible(), true);
    QCOMPARE(m_series->mesh(), QAbstract3DSeries::MeshBevelBar);
    QCOMPARE(m_series->meshRotation(), QQuaternion(1, 0, 0, 0));
    QCOMPARE(m_series->isMeshSmooth(), false);
    QCOMPARE(m_series->multiHighlightColor(), QColor(Qt::black));
    QCOMPARE(m_series->multiHighlightGradient(), QLinearGradient());
    QCOMPARE(m_series->name(), QString(""));
    QCOMPARE(m_series->singleHighlightColor(), QColor(Qt::black));
    QCOMPARE(m_series->singleHighlightGradient(), QLinearGradient());
    QCOMPARE(m_series->type(), QAbstract3DSeries::SeriesTypeBar);
    QCOMPARE(m_series->userDefinedMesh(), QString(""));
    QCOMPARE(m_series->isVisible(), true);
}

void tst_series::initializeProperties()
{
    QVERIFY(m_series);

    m_series->setDataProxy(new QBarDataProxy());
    m_series->setMeshAngle(15.0f);
    m_series->setSelectedBar(QPoint(0, 0));

    QCOMPARE(m_series->meshAngle(), 15.0f);
    QCOMPARE(m_series->selectedBar(), QPoint(0, 0));

    QLinearGradient gradient1;
    gradient1.setColorAt(0.0, Qt::red);
    gradient1.setColorAt(1.0, Qt::blue);
    QLinearGradient gradient2;
    gradient2.setColorAt(0.0, Qt::yellow);
    gradient2.setColorAt(1.0, Qt::green);
    QLinearGradient gradient3;
    gradient3.setColorAt(0.0, Qt::white);
    gradient3.setColorAt(1.0, Qt::gray);

    // Common properties
    m_series->setBaseColor(QColor(Qt::blue));
    m_series->setBaseGradient(gradient1);
    m_series->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    m_series->setItemLabelFormat("%f");
    m_series->setItemLabelVisible(false);
    m_series->setMesh(QAbstract3DSeries::MeshCone);
    m_series->setMeshSmooth(true);
    m_series->setMultiHighlightColor(QColor(Qt::green));
    m_series->setMultiHighlightGradient(gradient2);
    m_series->setName("name");
    m_series->setSingleHighlightColor(QColor(Qt::red));
    m_series->setSingleHighlightGradient(gradient3);
    m_series->setUserDefinedMesh(":/customitem.obj");
    m_series->setVisible(false);

    QCOMPARE(m_series->baseColor(), QColor(Qt::blue));
    QCOMPARE(m_series->baseGradient(), gradient1);
    QCOMPARE(m_series->baseGradient().stops().at(0).second, QColor(Qt::red));
    QCOMPARE(m_series->colorStyle(), Q3DTheme::ColorStyleRangeGradient);
    QCOMPARE(m_series->itemLabelFormat(), QString("%f"));
    QCOMPARE(m_series->isItemLabelVisible(), false);
    QCOMPARE(m_series->mesh(), QAbstract3DSeries::MeshCone);
    QCOMPARE(m_series->isMeshSmooth(), true);
    QCOMPARE(m_series->multiHighlightColor(), QColor(Qt::green));
    QCOMPARE(m_series->multiHighlightGradient(), gradient2);
    QCOMPARE(m_series->multiHighlightGradient().stops().at(0).second, QColor(Qt::yellow));
    QCOMPARE(m_series->name(), QString("name"));
    QCOMPARE(m_series->singleHighlightColor(), QColor(Qt::red));
    QCOMPARE(m_series->singleHighlightGradient(), gradient3);
    QCOMPARE(m_series->singleHighlightGradient().stops().at(0).second, QColor(Qt::white));
    QCOMPARE(m_series->userDefinedMesh(), QString(":/customitem.obj"));
    QCOMPARE(m_series->isVisible(), false);
}

void tst_series::invalidProperties()
{
    m_series->setMesh(QAbstract3DSeries::MeshMinimal);

    QCOMPARE(m_series->mesh(), QAbstract3DSeries::MeshBevelBar);
}

QTEST_MAIN(tst_series)
#include "tst_series.moc"
