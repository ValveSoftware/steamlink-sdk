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

#include <QtDataVisualization/QLogValue3DAxisFormatter>

using namespace QtDataVisualization;

class tst_axis: public QObject
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
    QLogValue3DAxisFormatter *m_formatter;
};

void tst_axis::initTestCase()
{
}

void tst_axis::cleanupTestCase()
{
}

void tst_axis::init()
{
    m_formatter = new QLogValue3DAxisFormatter();
}

void tst_axis::cleanup()
{
    delete m_formatter;
}

void tst_axis::construct()
{
    QLogValue3DAxisFormatter *formatter = new QLogValue3DAxisFormatter();
    QVERIFY(formatter);
    delete formatter;
}

void tst_axis::initialProperties()
{
    QVERIFY(m_formatter);

    QCOMPARE(m_formatter->autoSubGrid(), true);
    QCOMPARE(m_formatter->base(), 10.0);
    QCOMPARE(m_formatter->showEdgeLabels(), true);
}

void tst_axis::initializeProperties()
{
    QVERIFY(m_formatter);

    m_formatter->setAutoSubGrid(false);
    m_formatter->setBase(0.1);
    m_formatter->setShowEdgeLabels(false);

    QCOMPARE(m_formatter->autoSubGrid(), false);
    QCOMPARE(m_formatter->base(), 0.1);
    QCOMPARE(m_formatter->showEdgeLabels(), false);
}

void tst_axis::invalidProperties()
{
    m_formatter->setBase(-1.0);
    QCOMPARE(m_formatter->base(), 10.0);

    m_formatter->setBase(1.0);
    QCOMPARE(m_formatter->base(), 10.0);
}

QTEST_MAIN(tst_axis)
#include "tst_axis.moc"
