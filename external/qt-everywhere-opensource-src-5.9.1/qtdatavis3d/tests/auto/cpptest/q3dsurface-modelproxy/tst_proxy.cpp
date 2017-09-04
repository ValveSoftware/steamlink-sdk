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

#include <QtDataVisualization/QItemModelSurfaceDataProxy>
#include <QtDataVisualization/Q3DSurface>
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

    void multiMatch();

private:
    QItemModelSurfaceDataProxy *m_proxy;
};

void tst_proxy::initTestCase()
{
}

void tst_proxy::cleanupTestCase()
{
}

void tst_proxy::init()
{
    m_proxy = new QItemModelSurfaceDataProxy();
}

void tst_proxy::cleanup()
{
    delete m_proxy;
}


void tst_proxy::construct()
{
    QItemModelSurfaceDataProxy *proxy = new QItemModelSurfaceDataProxy();
    QVERIFY(proxy);
    delete proxy;

    QTableWidget table;

    proxy = new QItemModelSurfaceDataProxy(table.model());
    QVERIFY(proxy);
    delete proxy;

    proxy = new QItemModelSurfaceDataProxy(table.model(), "y");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString(""));
    QCOMPARE(proxy->columnRole(), QString(""));
    QCOMPARE(proxy->xPosRole(), QString(""));
    QCOMPARE(proxy->yPosRole(), QString("y"));
    QCOMPARE(proxy->zPosRole(), QString(""));
    QCOMPARE(proxy->rowCategories().length(), 0);
    QCOMPARE(proxy->columnCategories().length(), 0);
    delete proxy;

    proxy = new QItemModelSurfaceDataProxy(table.model(), "row", "column", "y");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString("row"));
    QCOMPARE(proxy->columnRole(), QString("column"));
    QCOMPARE(proxy->xPosRole(), QString("column"));
    QCOMPARE(proxy->yPosRole(), QString("y"));
    QCOMPARE(proxy->zPosRole(), QString("row"));
    QCOMPARE(proxy->rowCategories().length(), 0);
    QCOMPARE(proxy->columnCategories().length(), 0);
    delete proxy;

    proxy = new QItemModelSurfaceDataProxy(table.model(), "row", "column", "x", "y", "z");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString("row"));
    QCOMPARE(proxy->columnRole(), QString("column"));
    QCOMPARE(proxy->xPosRole(), QString("x"));
    QCOMPARE(proxy->yPosRole(), QString("y"));
    QCOMPARE(proxy->zPosRole(), QString("z"));
    QCOMPARE(proxy->rowCategories().length(), 0);
    QCOMPARE(proxy->columnCategories().length(), 0);
    delete proxy;

    proxy = new QItemModelSurfaceDataProxy(table.model(), "row", "column", "y",
                                           QStringList() << "rowCat", QStringList() << "colCat");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString("row"));
    QCOMPARE(proxy->columnRole(), QString("column"));
    QCOMPARE(proxy->xPosRole(), QString("column"));
    QCOMPARE(proxy->yPosRole(), QString("y"));
    QCOMPARE(proxy->zPosRole(), QString("row"));
    QCOMPARE(proxy->rowCategories().length(), 1);
    QCOMPARE(proxy->columnCategories().length(), 1);
    delete proxy;

    proxy = new QItemModelSurfaceDataProxy(table.model(), "row", "column", "x", "y", "z",
                                           QStringList() << "rowCat", QStringList() << "colCat");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString("row"));
    QCOMPARE(proxy->columnRole(), QString("column"));
    QCOMPARE(proxy->xPosRole(), QString("x"));
    QCOMPARE(proxy->yPosRole(), QString("y"));
    QCOMPARE(proxy->zPosRole(), QString("z"));
    QCOMPARE(proxy->rowCategories().length(), 1);
    QCOMPARE(proxy->columnCategories().length(), 1);
    delete proxy;
}

void tst_proxy::initialProperties()
{
    QVERIFY(m_proxy);

    QCOMPARE(m_proxy->autoColumnCategories(), true);
    QCOMPARE(m_proxy->autoRowCategories(), true);
    QCOMPARE(m_proxy->columnCategories(), QStringList());
    QCOMPARE(m_proxy->columnRole(), QString());
    QCOMPARE(m_proxy->columnRolePattern(), QRegExp());
    QCOMPARE(m_proxy->columnRoleReplace(), QString());
    QVERIFY(!m_proxy->itemModel());
    QCOMPARE(m_proxy->multiMatchBehavior(), QItemModelSurfaceDataProxy::MMBLast);
    QCOMPARE(m_proxy->rowCategories(), QStringList());
    QCOMPARE(m_proxy->rowRole(), QString());
    QCOMPARE(m_proxy->rowRolePattern(), QRegExp());
    QCOMPARE(m_proxy->rowRoleReplace(), QString());
    QCOMPARE(m_proxy->useModelCategories(), false);
    QCOMPARE(m_proxy->xPosRole(), QString());
    QCOMPARE(m_proxy->xPosRolePattern(), QRegExp());
    QCOMPARE(m_proxy->xPosRoleReplace(), QString());
    QCOMPARE(m_proxy->yPosRole(), QString());
    QCOMPARE(m_proxy->yPosRolePattern(), QRegExp());
    QCOMPARE(m_proxy->yPosRoleReplace(), QString());
    QCOMPARE(m_proxy->zPosRole(), QString());
    QCOMPARE(m_proxy->zPosRolePattern(), QRegExp());
    QCOMPARE(m_proxy->zPosRoleReplace(), QString());

    QCOMPARE(m_proxy->columnCount(), 0);
    QCOMPARE(m_proxy->rowCount(), 0);
    QVERIFY(!m_proxy->series());

    QCOMPARE(m_proxy->type(), QAbstractDataProxy::DataTypeSurface);
}

void tst_proxy::initializeProperties()
{
    QVERIFY(m_proxy);

    QTableWidget table;

    m_proxy->setAutoColumnCategories(false);
    m_proxy->setAutoRowCategories(false);
    m_proxy->setColumnCategories(QStringList() << "col1" << "col2");
    m_proxy->setColumnRole("column");
    m_proxy->setColumnRolePattern(QRegExp("/^.*-(\\d\\d)$/"));
    m_proxy->setColumnRoleReplace("\\\\1");
    m_proxy->setItemModel(table.model());
    m_proxy->setMultiMatchBehavior(QItemModelSurfaceDataProxy::MMBAverage);
    m_proxy->setRowCategories(QStringList() << "row1" << "row2");
    m_proxy->setRowRole("row");
    m_proxy->setRowRolePattern(QRegExp("/^(\\d\\d\\d\\d).*$/"));
    m_proxy->setRowRoleReplace("\\\\1");
    m_proxy->setUseModelCategories(true);
    m_proxy->setXPosRole("X");
    m_proxy->setXPosRolePattern(QRegExp("/-/"));
    m_proxy->setXPosRoleReplace("\\\\1");
    m_proxy->setYPosRole("Y");
    m_proxy->setYPosRolePattern(QRegExp("/-/"));
    m_proxy->setYPosRoleReplace("\\\\1");
    m_proxy->setZPosRole("Z");
    m_proxy->setZPosRolePattern(QRegExp("/-/"));
    m_proxy->setZPosRoleReplace("\\\\1");

    QCOMPARE(m_proxy->autoColumnCategories(), false);
    QCOMPARE(m_proxy->autoRowCategories(), false);
    QCOMPARE(m_proxy->columnCategories().count(), 2);
    QCOMPARE(m_proxy->columnRole(), QString("column"));
    QCOMPARE(m_proxy->columnRolePattern(), QRegExp("/^.*-(\\d\\d)$/"));
    QCOMPARE(m_proxy->columnRoleReplace(), QString("\\\\1"));
    QVERIFY(m_proxy->itemModel());
    QCOMPARE(m_proxy->multiMatchBehavior(), QItemModelSurfaceDataProxy::MMBAverage);
    QCOMPARE(m_proxy->rowCategories().count(), 2);
    QCOMPARE(m_proxy->rowRole(), QString("row"));
    QCOMPARE(m_proxy->rowRolePattern(), QRegExp("/^(\\d\\d\\d\\d).*$/"));
    QCOMPARE(m_proxy->rowRoleReplace(), QString("\\\\1"));
    QCOMPARE(m_proxy->useModelCategories(), true);
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

void tst_proxy::multiMatch()
{
    Q3DSurface graph;

    QTableWidget table;
    QStringList rows;
    rows << "row 1" << "row 2";
    QStringList columns;
    columns << "col 1" << "col 2" << "col 3" << "col 4";
    const char *values[4][2] = {{"0/0/5.5/30", "0/0/10.5/30"},
                               {"0/1/5.5/30", "0/1/0.5/30"},
                               {"1/0/5.5/30", "1/0/0.5/30"},
                               {"1/1/0.0/30", "1/1/0.0/30"}};

    table.setRowCount(2);
    table.setColumnCount(4);

    for (int col = 0; col < columns.size(); col++) {
        for (int row = 0; row < rows.size(); row++) {
            QModelIndex index = table.model()->index(col, row);
            table.model()->setData(index, values[col][row]);
        }
    }

    m_proxy->setItemModel(table.model());
    m_proxy->setRowRole(table.model()->roleNames().value(Qt::DisplayRole));
    m_proxy->setColumnRole(table.model()->roleNames().value(Qt::DisplayRole));
    m_proxy->setRowRolePattern(QRegExp(QStringLiteral("^(\\d*)\\/(\\d*)\\/\\d*[\\.\\,]?\\d*\\/\\d*[\\.\\,]?\\d*$")));
    m_proxy->setRowRoleReplace(QStringLiteral("\\2"));
    m_proxy->setYPosRolePattern(QRegExp(QStringLiteral("^\\d*(\\/)(\\d*)\\/(\\d*[\\.\\,]?\\d*)\\/\\d*[\\.\\,]?\\d*$")));
    m_proxy->setYPosRoleReplace(QStringLiteral("\\3"));
    m_proxy->setColumnRolePattern(QRegExp(QStringLiteral("^(\\d*)(\\/)(\\d*)\\/\\d*[\\.\\,]?\\d*\\/\\d*[\\.\\,]?\\d*$")));
    m_proxy->setColumnRoleReplace(QStringLiteral("\\1"));

    QSurface3DSeries *series = new QSurface3DSeries(m_proxy);

    graph.addSeries(series);

    QCoreApplication::processEvents();
    QCOMPARE(graph.axisY()->max(), 10.5f);
    m_proxy->setMultiMatchBehavior(QItemModelSurfaceDataProxy::MMBFirst);
    QCoreApplication::processEvents();
    QCOMPARE(graph.axisY()->max(), 5.5f);
    m_proxy->setMultiMatchBehavior(QItemModelSurfaceDataProxy::MMBLast);
    QCoreApplication::processEvents();
    QCOMPARE(graph.axisY()->max(), 10.5f);
    m_proxy->setMultiMatchBehavior(QItemModelSurfaceDataProxy::MMBAverage);
    QCoreApplication::processEvents();
    QCOMPARE(graph.axisY()->max(), 8.0f);
    m_proxy->setMultiMatchBehavior(QItemModelSurfaceDataProxy::MMBCumulativeY);
    QCoreApplication::processEvents();
    QCOMPARE(graph.axisY()->max(), 16.0f);

    QCOMPARE(m_proxy->columnCount(), 2);
    QCOMPARE(m_proxy->rowCount(), 3);
    QVERIFY(m_proxy->series());

    m_proxy = 0; // Graph deletes proxy
}

QTEST_MAIN(tst_proxy)
#include "tst_proxy.moc"
