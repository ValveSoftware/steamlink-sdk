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

#include <QtDataVisualization/QItemModelBarDataProxy>
#include <QtDataVisualization/Q3DBars>
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
    QItemModelBarDataProxy *m_proxy;
};

void tst_proxy::initTestCase()
{
}

void tst_proxy::cleanupTestCase()
{
}

void tst_proxy::init()
{
    m_proxy = new QItemModelBarDataProxy();
}

void tst_proxy::cleanup()
{
    delete m_proxy;
}

void tst_proxy::construct()
{
    QItemModelBarDataProxy *proxy = new QItemModelBarDataProxy();
    QVERIFY(proxy);
    delete proxy;

    QTableWidget *table = new QTableWidget();

    proxy = new QItemModelBarDataProxy(table->model());
    QVERIFY(proxy);
    delete proxy;

    proxy = new QItemModelBarDataProxy(table->model(), "val");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString(""));
    QCOMPARE(proxy->columnRole(), QString(""));
    QCOMPARE(proxy->valueRole(), QString("val"));
    QCOMPARE(proxy->rotationRole(), QString(""));
    QCOMPARE(proxy->rowCategories().length(), 0);
    QCOMPARE(proxy->columnCategories().length(), 0);
    delete proxy;

    proxy = new QItemModelBarDataProxy(table->model(), "row", "col", "val");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString("row"));
    QCOMPARE(proxy->columnRole(), QString("col"));
    QCOMPARE(proxy->valueRole(), QString("val"));
    QCOMPARE(proxy->rotationRole(), QString(""));
    QCOMPARE(proxy->rowCategories().length(), 0);
    QCOMPARE(proxy->columnCategories().length(), 0);
    delete proxy;

    proxy = new QItemModelBarDataProxy(table->model(), "row", "col", "val", "rot");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString("row"));
    QCOMPARE(proxy->columnRole(), QString("col"));
    QCOMPARE(proxy->valueRole(), QString("val"));
    QCOMPARE(proxy->rotationRole(), QString("rot"));
    QCOMPARE(proxy->rowCategories().length(), 0);
    QCOMPARE(proxy->columnCategories().length(), 0);
    delete proxy;

    proxy = new QItemModelBarDataProxy(table->model(), "row", "col", "val",
                                       QStringList() << "rowCat", QStringList() << "colCat");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString("row"));
    QCOMPARE(proxy->columnRole(), QString("col"));
    QCOMPARE(proxy->valueRole(), QString("val"));
    QCOMPARE(proxy->rotationRole(), QString(""));
    QCOMPARE(proxy->rowCategories().length(), 1);
    QCOMPARE(proxy->columnCategories().length(), 1);
    delete proxy;

    proxy = new QItemModelBarDataProxy(table->model(), "row", "col", "val", "rot",
                                       QStringList() << "rowCat", QStringList() << "colCat");
    QVERIFY(proxy);
    QCOMPARE(proxy->rowRole(), QString("row"));
    QCOMPARE(proxy->columnRole(), QString("col"));
    QCOMPARE(proxy->valueRole(), QString("val"));
    QCOMPARE(proxy->rotationRole(), QString("rot"));
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
    QCOMPARE(m_proxy->multiMatchBehavior(), QItemModelBarDataProxy::MMBLast);
    QCOMPARE(m_proxy->rotationRole(), QString());
    QCOMPARE(m_proxy->rotationRolePattern(), QRegExp());
    QCOMPARE(m_proxy->rotationRoleReplace(), QString());
    QCOMPARE(m_proxy->rowCategories(), QStringList());
    QCOMPARE(m_proxy->rowRole(), QString());
    QCOMPARE(m_proxy->rowRolePattern(), QRegExp());
    QCOMPARE(m_proxy->rowRoleReplace(), QString());
    QCOMPARE(m_proxy->useModelCategories(), false);
    QCOMPARE(m_proxy->valueRole(), QString());
    QCOMPARE(m_proxy->valueRolePattern(), QRegExp());
    QCOMPARE(m_proxy->valueRoleReplace(), QString());

    QCOMPARE(m_proxy->columnLabels().count(), 0);
    QCOMPARE(m_proxy->rowCount(), 0);
    QCOMPARE(m_proxy->rowLabels().count(), 0);
    QVERIFY(!m_proxy->series());

    QCOMPARE(m_proxy->type(), QAbstractDataProxy::DataTypeBar);
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
    m_proxy->setMultiMatchBehavior(QItemModelBarDataProxy::MMBAverage);
    m_proxy->setRotationRole("rotation");
    m_proxy->setRotationRolePattern(QRegExp("/-/"));
    m_proxy->setRotationRoleReplace("\\\\1");
    m_proxy->setRowCategories(QStringList() << "row1" << "row2");
    m_proxy->setRowRole("row");
    m_proxy->setRowRolePattern(QRegExp("/^(\\d\\d\\d\\d).*$/"));
    m_proxy->setRowRoleReplace("\\\\1");
    m_proxy->setUseModelCategories(true);
    m_proxy->setValueRole("value");
    m_proxy->setValueRolePattern(QRegExp("/-/"));
    m_proxy->setValueRoleReplace("\\\\1");

    QCOMPARE(m_proxy->autoColumnCategories(), false);
    QCOMPARE(m_proxy->autoRowCategories(), false);
    QCOMPARE(m_proxy->columnCategories().count(), 2);
    QCOMPARE(m_proxy->columnRole(), QString("column"));
    QCOMPARE(m_proxy->columnRolePattern(), QRegExp("/^.*-(\\d\\d)$/"));
    QCOMPARE(m_proxy->columnRoleReplace(), QString("\\\\1"));
    QVERIFY(m_proxy->itemModel());
    QCOMPARE(m_proxy->multiMatchBehavior(), QItemModelBarDataProxy::MMBAverage);
    QCOMPARE(m_proxy->rotationRole(), QString("rotation"));
    QCOMPARE(m_proxy->rotationRolePattern(), QRegExp("/-/"));
    QCOMPARE(m_proxy->rotationRoleReplace(), QString("\\\\1"));
    QCOMPARE(m_proxy->rowCategories().count(), 2);
    QCOMPARE(m_proxy->rowRole(), QString("row"));
    QCOMPARE(m_proxy->rowRolePattern(), QRegExp("/^(\\d\\d\\d\\d).*$/"));
    QCOMPARE(m_proxy->rowRoleReplace(), QString("\\\\1"));
    QCOMPARE(m_proxy->useModelCategories(), true);
    QCOMPARE(m_proxy->valueRole(), QString("value"));
    QCOMPARE(m_proxy->valueRolePattern(), QRegExp("/-/"));
    QCOMPARE(m_proxy->valueRoleReplace(), QString("\\\\1"));
}

void tst_proxy::multiMatch()
{
    Q3DBars graph;

    QTableWidget table;
    QStringList rows;
    rows << "row 1" << "row 2" << "row 3";
    QStringList columns;
    columns << "col 1";
    const char *values[1][3] = {{"0/0/3.5/30", "0/0/5.0/30", "0/0/6.5/30"}};

    table.setRowCount(1);
    table.setColumnCount(3);

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
    m_proxy->setValueRolePattern(QRegExp(QStringLiteral("^\\d*(\\/)(\\d*)\\/(\\d*[\\.\\,]?\\d*)\\/\\d*[\\.\\,]?\\d*$")));
    m_proxy->setValueRoleReplace(QStringLiteral("\\3"));
    m_proxy->setColumnRolePattern(QRegExp(QStringLiteral("^(\\d*)(\\/)(\\d*)\\/\\d*[\\.\\,]?\\d*\\/\\d*[\\.\\,]?\\d*$")));
    m_proxy->setColumnRoleReplace(QStringLiteral("\\1"));

    QBar3DSeries *series = new QBar3DSeries(m_proxy);

    graph.addSeries(series);

    QCoreApplication::processEvents();
    QCOMPARE(graph.valueAxis()->max(), 6.5f);
    m_proxy->setMultiMatchBehavior(QItemModelBarDataProxy::MMBFirst);
    QCoreApplication::processEvents();
    QCOMPARE(graph.valueAxis()->max(), 3.5f);
    m_proxy->setMultiMatchBehavior(QItemModelBarDataProxy::MMBLast);
    QCoreApplication::processEvents();
    QCOMPARE(graph.valueAxis()->max(), 6.5f);
    m_proxy->setMultiMatchBehavior(QItemModelBarDataProxy::MMBAverage);
    QCoreApplication::processEvents();
    QCOMPARE(graph.valueAxis()->max(), 5.0f);
    m_proxy->setMultiMatchBehavior(QItemModelBarDataProxy::MMBCumulative);
    QCoreApplication::processEvents();
    QCOMPARE(graph.valueAxis()->max(), 15.0f);

    QCOMPARE(m_proxy->columnLabels().count(), 1);
    QCOMPARE(m_proxy->rowCount(), 1);
    QCOMPARE(m_proxy->rowLabels().count(), 1);
    QVERIFY(m_proxy->series());

    m_proxy = 0; // Proxy gets deleted as graph gets deleted
}

QTEST_MAIN(tst_proxy)
#include "tst_proxy.moc"
