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

#include <QtDataVisualization/QItemModelScatterDataProxy>
#include <QtDataVisualization/Q3DScatter>
#include <QtWidgets/QTableWidget>

using namespace QtDataVisualization;

class tst_proxy: public QObject
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

    void addModel();

private:
    QItemModelScatterDataProxy *m_proxy;
};

void tst_proxy::initTestCase()
{
}

void tst_proxy::cleanupTestCase()
{
}

void tst_proxy::init()
{
    m_proxy = new QItemModelScatterDataProxy();
}

void tst_proxy::cleanup()
{
    delete m_proxy;
}

void tst_proxy::construct()
{
    QItemModelScatterDataProxy *proxy = new QItemModelScatterDataProxy();
    QVERIFY(proxy);
    delete proxy;

    QTableWidget *table = new QTableWidget();

    proxy = new QItemModelScatterDataProxy(table->model());
    QVERIFY(proxy);
    delete proxy;

    proxy = new QItemModelScatterDataProxy(table->model(), "x", "y", "z");
    QVERIFY(proxy);
    QCOMPARE(proxy->xPosRole(), QString("x"));
    QCOMPARE(proxy->yPosRole(), QString("y"));
    QCOMPARE(proxy->zPosRole(), QString("z"));
    QCOMPARE(proxy->rotationRole(), QString(""));
    delete proxy;

    proxy = new QItemModelScatterDataProxy(table->model(), "x", "y", "z", "rot");
    QVERIFY(proxy);
    QCOMPARE(proxy->xPosRole(), QString("x"));
    QCOMPARE(proxy->yPosRole(), QString("y"));
    QCOMPARE(proxy->zPosRole(), QString("z"));
    QCOMPARE(proxy->rotationRole(), QString("rot"));
    delete proxy;
}

void tst_proxy::initialProperties()
{
    QVERIFY(m_proxy);

    QVERIFY(!m_proxy->itemModel());
    QCOMPARE(m_proxy->rotationRole(), QString());
    QCOMPARE(m_proxy->rotationRolePattern(), QRegExp());
    QCOMPARE(m_proxy->rotationRoleReplace(), QString());
    QCOMPARE(m_proxy->xPosRole(), QString());
    QCOMPARE(m_proxy->xPosRolePattern(), QRegExp());
    QCOMPARE(m_proxy->xPosRoleReplace(), QString());
    QCOMPARE(m_proxy->yPosRole(), QString());
    QCOMPARE(m_proxy->yPosRolePattern(), QRegExp());
    QCOMPARE(m_proxy->yPosRoleReplace(), QString());
    QCOMPARE(m_proxy->zPosRole(), QString());
    QCOMPARE(m_proxy->zPosRolePattern(), QRegExp());
    QCOMPARE(m_proxy->zPosRoleReplace(), QString());

    QCOMPARE(m_proxy->itemCount(), 0);
    QVERIFY(!m_proxy->series());

    QCOMPARE(m_proxy->type(), QAbstractDataProxy::DataTypeScatter);
}

void tst_proxy::initializeProperties()
{
    QVERIFY(m_proxy);

    QTableWidget table;

    m_proxy->setItemModel(table.model());
    m_proxy->setRotationRole("rotation");
    m_proxy->setRotationRolePattern(QRegExp("/-/"));
    m_proxy->setRotationRoleReplace("\\\\1");
    m_proxy->setXPosRole("X");
    m_proxy->setXPosRolePattern(QRegExp("/-/"));
    m_proxy->setXPosRoleReplace("\\\\1");
    m_proxy->setYPosRole("Y");
    m_proxy->setYPosRolePattern(QRegExp("/-/"));
    m_proxy->setYPosRoleReplace("\\\\1");
    m_proxy->setZPosRole("Z");
    m_proxy->setZPosRolePattern(QRegExp("/-/"));
    m_proxy->setZPosRoleReplace("\\\\1");

    QVERIFY(m_proxy->itemModel());
    QCOMPARE(m_proxy->rotationRole(), QString("rotation"));
    QCOMPARE(m_proxy->rotationRolePattern(), QRegExp("/-/"));
    QCOMPARE(m_proxy->rotationRoleReplace(), QString("\\\\1"));
    QCOMPARE(m_proxy->xPosRole(), QString("X"));
    QCOMPARE(m_proxy->xPosRolePattern(), QRegExp("/-/"));
    QCOMPARE(m_proxy->xPosRoleReplace(), QString("\\\\1"));
    QCOMPARE(m_proxy->yPosRole(), QString("Y"));
    QCOMPARE(m_proxy->yPosRolePattern(), QRegExp("/-/"));
    QCOMPARE(m_proxy->yPosRoleReplace(), QString("\\\\1"));
    QCOMPARE(m_proxy->zPosRole(), QString("Z"));
    QCOMPARE(m_proxy->zPosRolePattern(), QRegExp("/-/"));
    QCOMPARE(m_proxy->zPosRoleReplace(), QString("\\\\1"));
}

void tst_proxy::addModel()
{
    QTableWidget table;
    QStringList rows;
    rows << "row 1";
    QStringList columns;
    columns << "col 1";
    const char *values[1][2] = {{"0/0/5.5/30", "0/0/10.5/30"}};

    table.setRowCount(2);
    table.setColumnCount(1);

    for (int col = 0; col < columns.size(); col++) {
        for (int row = 0; row < rows.size(); row++) {
            QModelIndex index = table.model()->index(col, row);
            table.model()->setData(index, values[col][row]);
        }
    }

    m_proxy->setItemModel(table.model());
    m_proxy->setXPosRole(table.model()->roleNames().value(Qt::DisplayRole));
    m_proxy->setZPosRole(table.model()->roleNames().value(Qt::DisplayRole));
    m_proxy->setXPosRolePattern(QRegExp(QStringLiteral("^(\\d*)\\/(\\d*)\\/\\d*[\\.\\,]?\\d*\\/\\d*[\\.\\,]?\\d*$")));
    m_proxy->setXPosRoleReplace(QStringLiteral("\\2"));
    m_proxy->setYPosRolePattern(QRegExp(QStringLiteral("^\\d*(\\/)(\\d*)\\/(\\d*[\\.\\,]?\\d*)\\/\\d*[\\.\\,]?\\d*$")));
    m_proxy->setYPosRoleReplace(QStringLiteral("\\3"));
    m_proxy->setZPosRolePattern(QRegExp(QStringLiteral("^(\\d*)(\\/)(\\d*)\\/\\d*[\\.\\,]?\\d*\\/\\d*[\\.\\,]?\\d*$")));
    m_proxy->setZPosRoleReplace(QStringLiteral("\\1"));

    QScatter3DSeries *series = new QScatter3DSeries(m_proxy);
    Q_UNUSED(series)

    QCoreApplication::processEvents();

    QCOMPARE(m_proxy->itemCount(), 2);
    QVERIFY(m_proxy->series());

    delete series;
    m_proxy = 0; // proxy gets deleted with series
}

QTEST_MAIN(tst_proxy)
#include "tst_proxy.moc"
