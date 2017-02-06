/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
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

#include "../qabstractaxis/tst_qabstractaxis.h"
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>

class tst_QBarCategoriesAxis: public tst_QAbstractAxis
{
Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qbarcategoryaxis_data();
    void qbarcategoryaxis();

    void append2_data();
    void append2();
    void append_data();
    void append();
    void at_data();
    void at();
    void categories_data();
    void categories();
    void clear_data();
    void clear();
    void count_data();
    void count();
    void insert_data();
    void insert();
    void remove_data();
    void remove();
    void max_raw_data();
    void max_raw();
    void max_data();
    void max();
    void max_animation_data();
    void max_animation();
    void min_raw_data();
    void min_raw();
    void min_data();
    void min();
    void min_animation_data();
    void min_animation();
    void range_raw_data();
    void range_raw();
    void range_data();
    void range();
    void range_animation_data();
    void range_animation();
    void noautoscale_data();
    void noautoscale();
    void autoscale_data();
    void autoscale();

private:
    QBarCategoryAxis* m_baraxis;
    QBarSeries* m_series;
};

void tst_QBarCategoriesAxis::initTestCase()
{
}

void tst_QBarCategoriesAxis::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QBarCategoriesAxis::init()
{
    m_baraxis = new QBarCategoryAxis();
    m_series = new QBarSeries();

    QBarSet *set0 = new QBarSet("Jane");
    QBarSet *set1 = new QBarSet("John");
    QBarSet *set2 = new QBarSet("Axel");
    QBarSet *set3 = new QBarSet("Mary");
    QBarSet *set4 = new QBarSet("Samantha");

    *set0 << 1 << 2 << 3 << 4 << 5 << 6;
    *set1 << 5 << 0 << 0 << 4 << 0 << 7;
    *set2 << 3 << 5 << 8 << 13 << 8 << 5;
    *set3 << 5 << 6 << 7 << 3 << 4 << 5;
    *set4 << 9 << 7 << 5 << 3 << 1 << 2;

    m_series->append(set0);
    m_series->append(set1);
    m_series->append(set2);
    m_series->append(set3);
    m_series->append(set4);

    QStringList categories;
    categories << "Jan" << "Feb" << "Mar" << "Apr" << "May" << "Jun";

    foreach(QString category, categories)
        m_baraxis->append(category);

    tst_QAbstractAxis::init(m_baraxis, m_series);
    m_chart->addSeries(m_series);
    m_chart->createDefaultAxes();
}

void tst_QBarCategoriesAxis::cleanup()
{
    delete m_series;
    delete m_baraxis;
    m_series = 0;
    m_baraxis = 0;
    tst_QAbstractAxis::cleanup();
}

void tst_QBarCategoriesAxis::qbarcategoryaxis_data()
{
}

void tst_QBarCategoriesAxis::qbarcategoryaxis()
{
    qabstractaxis();
    QBarCategoryAxis axis;
    axis.append(QStringList());
    axis.append(QString());
    QCOMPARE(axis.count(), 0);
    QStringList test;
    QCOMPARE(axis.categories(),test);
    axis.clear();
    QCOMPARE(axis.count(), 0);
    axis.insert(-1, QString());
    QCOMPARE(axis.max(), QString());
    QCOMPARE(axis.min(), QString());
    axis.remove(QString());
    axis.setCategories(QStringList());
    axis.setMax(QString());
    axis.setMin(QString());
    axis.setRange(QString(), QString());
    QCOMPARE(axis.type(), QAbstractAxis::AxisTypeBarCategory);
}

void tst_QBarCategoriesAxis::append_data()
{
    QTest::addColumn<QStringList>("categories");
    QTest::newRow("Jan Feb Mar Apr") << (QStringList() << "Jan" << "Feb" << "Mar" << "Apr");
    QTest::newRow("Jul Aug Sep") << (QStringList() << "Jul" << "Aug" << "Sep");
}

void tst_QBarCategoriesAxis::append()
{
    QFETCH(QStringList, categories);

    QBarCategoryAxis axis;

    QSignalSpy spy0(&axis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(&axis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(&axis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(&axis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(&axis, SIGNAL(countChanged()));

    axis.append(categories);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);

    m_chart->setAxisX(&axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(axis.categories(), categories);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 1);
}

void tst_QBarCategoriesAxis::append2_data()
{
    QTest::addColumn<QStringList>("categories");
    QTest::newRow("Jan Feb Mar Apr") << (QStringList() << "Jan" << "Feb" << "Mar" << "Apr");
    QTest::newRow("Jul Aug Sep") << (QStringList() << "Jul" << "Aug" << "Sep");
}

void tst_QBarCategoriesAxis::append2()
{
    QFETCH(QStringList, categories);

    QBarCategoryAxis axis;

    QSignalSpy spy0(&axis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(&axis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(&axis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(&axis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(&axis, SIGNAL(countChanged()));

    foreach(QString category, categories)
        axis.append(category);

    QCOMPARE(spy0.count(), categories.count());
    QCOMPARE(spy1.count(), categories.count());
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), categories.count());

    m_chart->setAxisX(&axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(axis.categories(), categories);

    QCOMPARE(spy0.count(), categories.count());
    QCOMPARE(spy1.count(), categories.count());
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), categories.count());
    QCOMPARE(spy4.count(), categories.count());
}

void tst_QBarCategoriesAxis::at_data()
{
    QTest::addColumn<QStringList>("categories");
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("string");
    QTest::newRow("Jul Aug Sep 0 Jul") <<  (QStringList() << "Jul" << "Aug" << "Sep") << 0 << "Jul";
    QTest::newRow("Jul Aug Sep 2 Sep") <<  (QStringList() << "Jul" << "Aug" << "Sep") << 2 << "Sep";
    QTest::newRow("Jul Aug Sep 1 Aug") <<  (QStringList() << "Jul" << "Aug" << "Sep") << 1 << "Aug";
}

void tst_QBarCategoriesAxis::at()
{
    QFETCH(int, index);
    QFETCH(QString, string);
    QFETCH(QStringList, categories);

    QBarCategoryAxis axis;
    axis.append(categories);

    QSignalSpy spy0(&axis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(&axis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(&axis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(&axis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(&axis, SIGNAL(countChanged()));

    QCOMPARE(axis.at(index), string);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);

    m_chart->setAxisX(&axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(axis.at(index), string);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);
}

void tst_QBarCategoriesAxis::categories_data()
{
    QTest::addColumn<QStringList>("categories");
    QTest::newRow("Jul Aug Sep") << (QStringList() << "Jul" << "Aug" << "Sep");
}

void tst_QBarCategoriesAxis::categories()
{
    QFETCH(QStringList, categories);

    QBarCategoryAxis axis;

    QSignalSpy spy0(&axis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(&axis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(&axis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(&axis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(&axis, SIGNAL(countChanged()));

    axis.setCategories(categories);
    QCOMPARE(axis.categories(), categories);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);

    m_chart->setAxisX(&axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(axis.categories(), categories);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 1);
}

void tst_QBarCategoriesAxis::clear_data()
{
    QTest::addColumn<QStringList>("categories");
    QTest::newRow("Jul Aug Sep") << (QStringList() << "Jul" << "Aug" << "Sep");
}

void tst_QBarCategoriesAxis::clear()
{
    QFETCH(QStringList, categories);

    QBarCategoryAxis axis;

    axis.setCategories(categories);
    QCOMPARE(axis.categories(), categories);

    QSignalSpy spy0(&axis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(&axis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(&axis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(&axis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(&axis, SIGNAL(countChanged()));

    axis.clear();
    QCOMPARE(axis.categories(), QStringList());

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);

    m_chart->setAxisX(&axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    QCOMPARE(spy0.count(), 2);
    QCOMPARE(spy1.count(), 2);
    QCOMPARE(spy2.count(), 2);
    QCOMPARE(spy3.count(), 2);

    axis.clear();
    QCOMPARE(axis.categories().count(),0);
    QCOMPARE(spy0.count(), 3);
    QCOMPARE(spy1.count(), 3);
    QCOMPARE(spy2.count(), 3);
    QCOMPARE(spy3.count(), 3);
    QCOMPARE(spy4.count(), 3);
}

void tst_QBarCategoriesAxis::count_data()
{
    QTest::addColumn<QStringList>("categories");
    QTest::addColumn<int>("count");
    QTest::newRow("Jul Aug Sep") << (QStringList() << "Jul" << "Aug" << "Sep") << 3;
    QTest::newRow("Jul Aug ") << (QStringList() << "Jul" << "Aug") << 2;
}

void tst_QBarCategoriesAxis::count()
{
    QFETCH(QStringList, categories);
    QFETCH(int, count);

    QBarCategoryAxis axis;
    axis.setCategories(categories);

    QSignalSpy spy0(&axis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(&axis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(&axis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(&axis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(&axis, SIGNAL(countChanged()));

    QCOMPARE(axis.count(), count);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 0);

    m_chart->setAxisX(&axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(axis.count(), count);
}

void tst_QBarCategoriesAxis::insert_data()
{
    QTest::addColumn<QStringList>("categories");
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("category");
    QTest::newRow("Jul Aug Sep 0 Jun") <<  (QStringList() << "Jul" << "Aug" << "Sep") << 1 << "Jun";
    QTest::newRow("Jul Aug Sep 2 Summer") <<  (QStringList() << "Jul" << "Aug" << "Sep") << 2 << "Summer";
}

void tst_QBarCategoriesAxis::insert()
{
    QFETCH(QStringList, categories);
    QFETCH(int, index);
    QFETCH(QString, category);

    QBarCategoryAxis axis;
    axis.append(categories);

    QSignalSpy spy0(&axis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(&axis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(&axis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(&axis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(&axis, SIGNAL(countChanged()));

    axis.insert(index, category);
    QCOMPARE(axis.at(index),category);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 0);
    QCOMPARE(spy4.count(), 1);

    m_chart->setAxisX(&axis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
}

void tst_QBarCategoriesAxis::remove_data()
{
    QTest::addColumn<QStringList>("categories");
    QTest::addColumn<QString>("category");
    QTest::addColumn<QStringList>("result");
    QTest::newRow("Jul Aug Sep 0") << (QStringList() << "Jul" << "Aug" << "Sep") << "Jul" << (QStringList() << "Aug" << "Sep");
    QTest::newRow("Jul Aug Sep 1") << (QStringList() << "Jul" << "Aug" << "Sep") << "Aug"<< (QStringList() << "Jul" << "Sep");
}

void tst_QBarCategoriesAxis::remove()
{
    QFETCH(QStringList, categories);
    QFETCH(QString, category);
    QFETCH(QStringList, result);

    QBarCategoryAxis axis;
    axis.append(categories);

    int maxCount = axis.max() == category;
    int minCount = axis.min() == category;
    int rangeCount = maxCount + minCount;

    QSignalSpy spy0(&axis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(&axis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(&axis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(&axis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(&axis, SIGNAL(countChanged()));

    axis.remove(category);
    QCOMPARE(axis.categories(),result);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), maxCount);
    QCOMPARE(spy2.count(), minCount);
    QCOMPARE(spy3.count(), rangeCount);
    QCOMPARE(spy4.count(), 1);
}

void tst_QBarCategoriesAxis::max_raw_data()
{
    //"Jan" << "Feb" << "Mar" << "Apr" << "May" << "Jun";
    QTest::addColumn<QString>("max");
    QTest::newRow("Feb") << "Feb";
    QTest::newRow("Apr") << "Apr";
    QTest::newRow("May") << "May";
}

void tst_QBarCategoriesAxis::max_raw()
{
    QFETCH(QString, max);

    QSignalSpy spy0(m_baraxis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(m_baraxis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(m_baraxis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(m_baraxis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(m_baraxis, SIGNAL(countChanged()));

    m_baraxis->setMax(max);
    QCOMPARE(m_baraxis->max(), max);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 0);
}

void tst_QBarCategoriesAxis::max_data()
{
    max_raw_data();
}

void tst_QBarCategoriesAxis::max()
{
    m_chart->setAxisX(m_baraxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    max_raw();
}

void tst_QBarCategoriesAxis::max_animation_data()
{
    max_data();
}

void tst_QBarCategoriesAxis::max_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    max();
}

void tst_QBarCategoriesAxis::min_raw_data()
{
    //"Jan" << "Feb" << "Mar" << "Apr" << "May" << "Jun";
    QTest::addColumn<QString>("min");
    QTest::newRow("Feb") << "Feb";
    QTest::newRow("Apr") << "Apr";
    QTest::newRow("May") << "May";
}

void tst_QBarCategoriesAxis::min_raw()
{
    QFETCH(QString, min);

    QSignalSpy spy0(m_baraxis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(m_baraxis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(m_baraxis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(m_baraxis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(m_baraxis, SIGNAL(countChanged()));

    m_baraxis->setMin(min);
    QCOMPARE(m_baraxis->min(), min);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 0);
}

void tst_QBarCategoriesAxis::min_data()
{
    min_raw_data();
}

void tst_QBarCategoriesAxis::min()
{
    min_raw();
    m_chart->setAxisX(m_baraxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

}

void tst_QBarCategoriesAxis::min_animation_data()
{
    min_data();
}

void tst_QBarCategoriesAxis::min_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    min();
}


void tst_QBarCategoriesAxis::range_raw_data()
{
    //"Jan" << "Feb" << "Mar" << "Apr" << "May" << "Jun";
    QTest::addColumn<QString>("min");
    QTest::addColumn<QString>("max");
    QTest::newRow("Feb - Apr") << "Feb" << "Apr";
    QTest::newRow("Feb - May") << "Feb" << "May";
    QTest::newRow("Mar - Apr") << "Mar" << "Apr";
}

void tst_QBarCategoriesAxis::range_raw()
{
    QFETCH(QString, min);
    QFETCH(QString, max);

    QSignalSpy spy0(m_baraxis, SIGNAL(categoriesChanged()));
    QSignalSpy spy1(m_baraxis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy2(m_baraxis, SIGNAL(minChanged(QString)));
    QSignalSpy spy3(m_baraxis, SIGNAL(rangeChanged(QString,QString)));
    QSignalSpy spy4(m_baraxis, SIGNAL(countChanged()));

    m_baraxis->setRange(min, max);
    QCOMPARE(m_baraxis->min(), min);
    QCOMPARE(m_baraxis->max(), max);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(spy3.count(), 1);
    QCOMPARE(spy4.count(), 0);
}

void tst_QBarCategoriesAxis::range_data()
{
    range_raw_data();
}

void tst_QBarCategoriesAxis::range()
{
    range_raw();
    m_chart->setAxisX(m_baraxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

}

void tst_QBarCategoriesAxis::range_animation_data()
{
    range_data();
}

void tst_QBarCategoriesAxis::range_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    range();
}


void tst_QBarCategoriesAxis::noautoscale_data()
{
    QTest::addColumn<QString>("min");
    QTest::addColumn<QString>("max");
    QTest::newRow("Feb - Mar") << "Feb" << "Mar";
    QTest::newRow("Feb - May") << "Feb" << "May";
    QTest::newRow("Apr - May") << "Apr" << "May";
}

void tst_QBarCategoriesAxis::noautoscale()
{
    QFETCH(QString, min);
    QFETCH(QString, max);

    QSignalSpy spy0(m_baraxis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy1(m_baraxis, SIGNAL(minChanged(QString)));
    QSignalSpy spy2(m_baraxis, SIGNAL(rangeChanged(QString,QString)));

    m_baraxis->setRange(min, max);
    QCOMPARE(m_baraxis->min(),min);
    QCOMPARE(m_baraxis->max(),max);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);

    m_chart->setAxisX(m_baraxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_baraxis->min(),min);
    QCOMPARE(m_baraxis->max(),max);
}

void tst_QBarCategoriesAxis::autoscale_data()
{

}

void tst_QBarCategoriesAxis::autoscale()
{
    delete m_baraxis;
    m_baraxis = new QBarCategoryAxis();

    QSignalSpy spy0(m_baraxis, SIGNAL(maxChanged(QString)));
    QSignalSpy spy1(m_baraxis, SIGNAL(minChanged(QString)));
    QSignalSpy spy2(m_baraxis, SIGNAL(rangeChanged(QString,QString)));

    QCOMPARE(m_baraxis->min(),QString());
    QCOMPARE(m_baraxis->max(),QString());
    m_chart->setAxisX(m_baraxis, m_series);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_baraxis->min(),QString("1"));
    QCOMPARE(m_baraxis->max(),QString("6"));
}


QTEST_MAIN(tst_QBarCategoriesAxis)
#include "tst_qbarcategoryaxis.moc"

