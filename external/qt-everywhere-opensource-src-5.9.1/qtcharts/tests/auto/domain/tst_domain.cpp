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
#include <QtTest/QtTest>
#include <private/xydomain_p.h>
#include <private/qabstractaxis_p.h>
#include <tst_definitions.h>

QT_CHARTS_USE_NAMESPACE

Q_DECLARE_METATYPE(XYDomain*)
Q_DECLARE_METATYPE(QSizeF)
Q_DECLARE_METATYPE(QMargins)


class AxisMock: public QAbstractAxisPrivate
{
Q_OBJECT
public:
    AxisMock(Qt::Alignment alignment):QAbstractAxisPrivate(0){ setAlignment(alignment);};
    void initializeGraphics(QGraphicsItem* item)
    {
        Q_UNUSED(item);
    };

    void initializeDomain(AbstractDomain* domain)
    {
        Q_UNUSED(domain);
    };
    void setMin(const QVariant &min)
    {
        Q_UNUSED(min);
    }
    qreal min() { return m_min;}
    void setMax(const QVariant &max)
    {
        Q_UNUSED(max);
    }
    qreal max() { return m_max; }
    void setRange(const QVariant &min, const QVariant &max)
    {
        Q_UNUSED(min);
        Q_UNUSED(max);
    };

    void setRange(qreal min, qreal max)
    {
          m_min=min;
          m_max=max;
          emit rangeChanged(min,max);
    };

    int count () const { return m_count; }

    void handleDomainUpdated(){};
public:
    int m_count;
    qreal m_min;
    qreal m_max;
};

class tst_Domain: public QObject
{
Q_OBJECT

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void domain();
    void handleHorizontalAxisRangeChanged_data();
    void handleHorizontalAxisRangeChanged();
    void handleVerticalAxisRangeChanged_data();
    void handleVerticalAxisRangeChanged();
    void isEmpty_data();
    void isEmpty();
    void maxX_data();
    void maxX();
    void maxY_data();
    void maxY();
    void minX_data();
    void minX();
    void minY_data();
    void minY();
    void operatorEquals_data();
    void operatorEquals();
    void setRange_data();
    void setRange();
    void setRangeX_data();
    void setRangeX();
    void setRangeY_data();
    void setRangeY();
    void spanX_data();
    void spanX();
    void spanY_data();
    void spanY();
    void zoomIn_data();
    void zoomIn();
    void zoomOut_data();
    void zoomOut();
    void move_data();
    void move();
};

void tst_Domain::initTestCase()
{
}

void tst_Domain::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_Domain::init()
{
}

void tst_Domain::cleanup()
{
}

void tst_Domain::domain()
{
    XYDomain domain;

    QCOMPARE(domain.isEmpty(), true);
    QCOMPARE(domain.maxX(), 0.0);
    QCOMPARE(domain.maxY(), 0.0);
    QCOMPARE(domain.minX(), 0.0);
    QCOMPARE(domain.minY(), 0.0);
}

void tst_Domain::handleHorizontalAxisRangeChanged_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::newRow("-1 1") << -1.0 << 1.0;
    QTest::newRow("0 1") << 0.0 << 1.0;
    QTest::newRow("-1 0") << -1.0 << 0.0;
}

void tst_Domain::handleHorizontalAxisRangeChanged()
{
    QFETCH(qreal, min);
    QFETCH(qreal, max);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    AxisMock axis(Qt::AlignBottom);
    QObject::connect(&axis,SIGNAL(rangeChanged(qreal,qreal)),&domain,SLOT(handleHorizontalAxisRangeChanged(qreal,qreal)));
    axis.setRange(min,max);

    QVERIFY(qFuzzyCompare(domain.minX(), min));
    QVERIFY(qFuzzyCompare(domain.maxX(), max));

    QList<QVariant> arg1 = spy1.first();
    QVERIFY(qFuzzyCompare(arg1.at(0).toReal(), min));
    QVERIFY(qFuzzyCompare(arg1.at(1).toReal(), max));

    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), 1);
    TRY_COMPARE(spy2.count(), 0);

}

void tst_Domain::handleVerticalAxisRangeChanged_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::newRow("-1 1") << -1.0 << 1.0;
    QTest::newRow("0 1") << 0.0 << 1.0;
    QTest::newRow("-1 0") << -1.0 << 0.0;
}

void tst_Domain::handleVerticalAxisRangeChanged()
{
    QFETCH(qreal, min);
    QFETCH(qreal, max);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    AxisMock axis(Qt::AlignLeft);
    QObject::connect(&axis, SIGNAL(rangeChanged(qreal,qreal)), &domain, SLOT(handleVerticalAxisRangeChanged(qreal,qreal)));
    axis.setRange(min,max);

    QVERIFY(qFuzzyCompare(domain.minY(), min));
    QVERIFY(qFuzzyCompare(domain.maxY(), max));

    QList<QVariant> arg1 = spy2.first();
    QVERIFY(qFuzzyCompare(arg1.at(0).toReal(), min));
    QVERIFY(qFuzzyCompare(arg1.at(1).toReal(), max));

    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), 1);
}

void tst_Domain::isEmpty_data()
{
    QTest::addColumn<qreal>("minX");
    QTest::addColumn<qreal>("maxX");
    QTest::addColumn<qreal>("minY");
    QTest::addColumn<qreal>("maxY");
    QTest::addColumn<QSizeF>("size");
    QTest::addColumn<bool>("isEmpty");
    QTest::newRow("0 0 0 0") << 0.0 << 0.0 << 0.0 << 0.0 << QSizeF(1,1) << true;
    QTest::newRow("0 1 0 0") << 0.0 << 1.0 << 0.0 << 0.0 << QSizeF(1,1) << true;
    QTest::newRow("0 0 0 1") << 0.0 << 1.0 << 0.0 << 0.0 << QSizeF(1,1) << true;
    QTest::newRow("0 1 0 1") << 0.0 << 1.0 << 0.0 << 1.0 << QSizeF(1,1) << false;
    QTest::newRow("0 1 0 1") << 0.0 << 1.0 << 0.0 << 1.0 << QSizeF(-11,1) << true;
}

void tst_Domain::isEmpty()
{
    QFETCH(qreal, minX);
    QFETCH(qreal, maxX);
    QFETCH(qreal, minY);
    QFETCH(qreal, maxY);
    QFETCH(QSizeF, size);
    QFETCH(bool, isEmpty);

    XYDomain domain;
    domain.setRange(minX, maxX, minY, maxY);
    domain.setSize(size);
    QCOMPARE(domain.isEmpty(), isEmpty);
}

void tst_Domain::maxX_data()
{
    QTest::addColumn<qreal>("maxX1");
    QTest::addColumn<qreal>("maxX2");
    QTest::addColumn<int>("count");
    QTest::newRow("1") << 0.0 << 1.0 << 1;
    QTest::newRow("1.0") << 1.0 << 1.0 << 1;
    QTest::newRow("2.0") << 1.0 << 0.0 << 2;
}

void tst_Domain::maxX()
{
    QFETCH(qreal, maxX1);
    QFETCH(qreal, maxX2);
    QFETCH(int, count);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.setMaxX(maxX1);
    QCOMPARE(domain.maxX(), maxX1);
    domain.setMaxX(maxX2);
    QCOMPARE(domain.maxX(), maxX2);

    TRY_COMPARE(spy0.count(), count);
    TRY_COMPARE(spy1.count(), count);
    TRY_COMPARE(spy2.count(), 0);

}

void tst_Domain::maxY_data()
{
    QTest::addColumn<qreal>("maxY1");
    QTest::addColumn<qreal>("maxY2");
    QTest::addColumn<int>("count");
    QTest::newRow("1") << 0.0 << 1.0 << 1;
    QTest::newRow("1.0") << 1.0 << 1.0 << 1;
    QTest::newRow("2.0") << 1.0 << 0.0 << 2;
}

void tst_Domain::maxY()
{
    QFETCH(qreal, maxY1);
    QFETCH(qreal, maxY2);
    QFETCH(int, count);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.setMaxY(maxY1);
    QCOMPARE(domain.maxY(), maxY1);
    domain.setMaxY(maxY2);
    QCOMPARE(domain.maxY(), maxY2);

    TRY_COMPARE(spy0.count(), count);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), count);
}

void tst_Domain::minX_data()
{
    QTest::addColumn<qreal>("minX1");
    QTest::addColumn<qreal>("minX2");
    QTest::addColumn<int>("count");
    QTest::newRow("1") << 0.0 << 1.0 << 1;
    QTest::newRow("1.0") << 1.0 << 1.0 << 1;
    QTest::newRow("2.0") << 1.0 << 0.0 << 2;
}

void tst_Domain::minX()
{
    QFETCH(qreal, minX1);
    QFETCH(qreal, minX2);
    QFETCH(int, count);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.setMinX(minX1);
    QCOMPARE(domain.minX(), minX1);
    domain.setMinX(minX2);
    QCOMPARE(domain.minX(), minX2);

    TRY_COMPARE(spy0.count(), count);
    TRY_COMPARE(spy1.count(), count);
    TRY_COMPARE(spy2.count(), 0);
}

void tst_Domain::minY_data()
{
    QTest::addColumn<qreal>("minY1");
    QTest::addColumn<qreal>("minY2");
    QTest::addColumn<int>("count");
    QTest::newRow("1") << 0.0 << 1.0 << 1;
    QTest::newRow("1.0") << 1.0 << 1.0 << 1;
    QTest::newRow("2.0") << 1.0 << 0.0 << 2;
}

void tst_Domain::minY()
{
    QFETCH(qreal, minY1);
    QFETCH(qreal, minY2);
    QFETCH(int, count);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.setMinY(minY1);
    QCOMPARE(domain.minY(), minY1);
    domain.setMinY(minY2);
    QCOMPARE(domain.minY(), minY2);

    TRY_COMPARE(spy0.count(), count);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), count);
}

void tst_Domain::operatorEquals_data()
{

    QTest::addColumn<XYDomain*>("domain1");
    QTest::addColumn<XYDomain*>("domain2");
    QTest::addColumn<bool>("equals");
    QTest::addColumn<bool>("notEquals");
    XYDomain* a;
    XYDomain* b;
    a = new XYDomain();
    a->setRange(0, 100, 0, 100);
    b = new XYDomain();
    b->setRange(0, 100, 0, 100);
    QTest::newRow("equals") << a << b << true << false;
    a = new XYDomain();
    a->setRange(0, 100, 0, 100);
    b = new XYDomain();
    b->setRange(0, 100, 0, 1);
    QTest::newRow("equals") << a << b << false << true;
    a = new XYDomain();
    a->setRange(0, 100, 0, 100);
    b = new XYDomain();
    b->setRange(0, 1, 0, 100);
    QTest::newRow("equals") << a << b << false << true;

}

void tst_Domain::operatorEquals()
{
    QFETCH(XYDomain*, domain1);
    QFETCH(XYDomain*, domain2);
    QFETCH(bool, equals);
    QFETCH(bool, notEquals);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    QCOMPARE(*domain1==*domain2, equals);
    QCOMPARE(*domain1!=*domain2, notEquals);

    TRY_COMPARE(spy0.count(), 0);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), 0);

    delete domain1;
    delete domain2;
}

void tst_Domain::setRange_data()
{
    QTest::addColumn<qreal>("minX");
    QTest::addColumn<qreal>("maxX");
    QTest::addColumn<qreal>("minY");
    QTest::addColumn<qreal>("maxY");
    QTest::newRow("1,2,1,2") << 1.0 << 2.0 << 1.0 << 2.0;
    QTest::newRow("1,3,1,3") << 1.0 << 3.0 << 1.0 << 3.0;
    QTest::newRow("-1,5,-2,-1") << -1.0 << 5.0 << -2.0 << -1.0;
}

void tst_Domain::setRange()
{
    QFETCH(qreal, minX);
    QFETCH(qreal, maxX);
    QFETCH(qreal, minY);
    QFETCH(qreal, maxY);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.setRange(minX, maxX, minY, maxY);

    QCOMPARE(domain.minX(), minX);
    QCOMPARE(domain.maxX(), maxX);
    QCOMPARE(domain.minY(), minY);
    QCOMPARE(domain.maxY(), maxY);

    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), 1);
    TRY_COMPARE(spy2.count(), 1);

}

void tst_Domain::setRangeX_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::newRow("-1 1") << -1.0 << 1.0;
    QTest::newRow("0 1") << 0.0 << 1.0;
    QTest::newRow("-1 0") << -1.0 << 0.0;
}

void tst_Domain::setRangeX()
{
    QFETCH(qreal, min);
    QFETCH(qreal, max);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.setRangeX(min, max);

    QVERIFY(qFuzzyCompare(domain.minX(), min));
    QVERIFY(qFuzzyCompare(domain.maxX(), max));

    QList<QVariant> arg1 = spy1.first();
    QVERIFY(qFuzzyCompare(arg1.at(0).toReal(), min));
    QVERIFY(qFuzzyCompare(arg1.at(1).toReal(), max));

    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), 1);
    TRY_COMPARE(spy2.count(), 0);
}

void tst_Domain::setRangeY_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::newRow("-1 1") << -1.0 << 1.0;
    QTest::newRow("0 1") << 0.0 << 1.0;
    QTest::newRow("-1 0") << -1.0 << 0.0;
}

void tst_Domain::setRangeY()
{
    QFETCH(qreal, min);
    QFETCH(qreal, max);

    XYDomain domain;

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.setRangeY(min, max);

    QVERIFY(qFuzzyCompare(domain.minY(), min));
    QVERIFY(qFuzzyCompare(domain.maxY(), max));

    QList<QVariant> arg1 = spy2.first();
    QVERIFY(qFuzzyCompare(arg1.at(0).toReal(), min));
    QVERIFY(qFuzzyCompare(arg1.at(1).toReal(), max));

    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), 1);
}

void tst_Domain::spanX_data()
{
    QTest::addColumn<qreal>("minX");
    QTest::addColumn<qreal>("maxX");
    QTest::addColumn<qreal>("spanX");
    QTest::newRow("1 2 1") << 1.0 << 2.0 << 1.0;
    QTest::newRow("0 2 2") << 1.0 << 2.0 << 1.0;
}

void tst_Domain::spanX()
{
    QFETCH(qreal, minX);
    QFETCH(qreal, maxX);
    QFETCH(qreal, spanX);

    XYDomain domain;

    domain.setRangeX(minX, maxX);

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    QCOMPARE(domain.spanX(), spanX);

    TRY_COMPARE(spy0.count(), 0);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), 0);
}

void tst_Domain::spanY_data()
{
    QTest::addColumn<qreal>("minY");
    QTest::addColumn<qreal>("maxY");
    QTest::addColumn<qreal>("spanY");
    QTest::newRow("1 2 1") << 1.0 << 2.0 << 1.0;
    QTest::newRow("0 2 2") << 1.0 << 2.0 << 1.0;
}

void tst_Domain::spanY()
{
    QFETCH(qreal, minY);
    QFETCH(qreal, maxY);
    QFETCH(qreal, spanY);

    XYDomain domain;

    domain.setRangeY(minY, maxY);

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    QCOMPARE(domain.spanY(), spanY);

    TRY_COMPARE(spy0.count(), 0);
    TRY_COMPARE(spy1.count(), 0);
    TRY_COMPARE(spy2.count(), 0);
}

void tst_Domain::zoomIn_data()
{
    QTest::addColumn<QMargins>("range");
    QTest::addColumn<QSizeF>("size");
    QTest::addColumn<QMargins>("zoom");
    QTest::addColumn<QMargins>("result");

    QTest::newRow("first") << QMargins(0,0,1000,1000) << QSizeF(1000, 1000) <<
    						  QMargins(100, 100, 900, 900) << QMargins(100,100,900,900);
    QTest::newRow("second") << QMargins(0,0,2000,2000) << QSizeF(1000, 1000) <<
    						  QMargins(100, 100, 900, 900) << QMargins(200,200,1800,1800);
}

void tst_Domain::zoomIn()
{
    QFETCH(QMargins, range);
    QFETCH(QSizeF, size);
    QFETCH(QMargins, zoom);
    QFETCH(QMargins, result);

    XYDomain domain;
    domain.setRange(range.left(), range.right(), range.top(),range.bottom());
    domain.setSize(size);

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.zoomIn(QRectF(zoom.left(),zoom.top(),zoom.right()-zoom.left(),zoom.bottom()-zoom.top()));

    QCOMPARE(domain.minX(),qreal(result.left()));
    QCOMPARE(domain.maxX(),qreal(result.right()));
    QCOMPARE(domain.minY(),qreal(result.top()));
    QCOMPARE(domain.maxY(),qreal(result.bottom()));

    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), 1);
    TRY_COMPARE(spy2.count(), 1);
}

void tst_Domain::zoomOut_data()
{
    QTest::addColumn<QMargins>("range");
    QTest::addColumn<QSizeF>("size");
    QTest::addColumn<QMargins>("zoom");
    QTest::addColumn<QMargins>("result");

    QTest::newRow("first") << QMargins(100,100,900,900) << QSizeF(1000, 1000) <<
    						  QMargins(100, 100, 900, 900) << QMargins(0,0,1000,1000);
    QTest::newRow("second") << QMargins(200,200,1800,1800) << QSizeF(1000, 1000) <<
    						  QMargins(100, 100, 900, 900) << QMargins(0,0,2000,2000);
}

void tst_Domain::zoomOut()
{
    QFETCH(QMargins, range);
    QFETCH(QSizeF, size);
    QFETCH(QMargins, zoom);
    QFETCH(QMargins, result);

    XYDomain domain;
    domain.setRange(range.left(), range.right(), range.top(),range.bottom());
    domain.setSize(size);

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.zoomOut(QRectF(zoom.left(),zoom.top(),zoom.right()-zoom.left(),zoom.bottom()-zoom.top()));

    QCOMPARE(domain.minX(),qreal(result.left()));
    QCOMPARE(domain.maxX(),qreal(result.right()));
    QCOMPARE(domain.minY(),qreal(result.top()));
    QCOMPARE(domain.maxY(),qreal(result.bottom()));

    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), 1);
    TRY_COMPARE(spy2.count(), 1);
}

void tst_Domain::move_data()
{
	QTest::addColumn<QMargins>("range");
	QTest::addColumn<QSizeF>("size");
    QTest::addColumn<int>("dx");
    QTest::addColumn<int>("dy");
    QTest::addColumn<QMargins>("result");

    QTest::newRow("first") << QMargins(0,0,1000,1000) << QSizeF(1000, 1000) <<
       						  10 << 10 << QMargins(10,10,1010,1010);
    QTest::newRow("second") << QMargins(0,0,1000,1000) << QSizeF(1000, 1000) <<
       						  -10 << -10 << QMargins(-10,-10,990,990);
}

void tst_Domain::move()
{
    QFETCH(QMargins, range);
    QFETCH(QSizeF, size);
    QFETCH(int, dx);
    QFETCH(int, dy);
    QFETCH(QMargins, result);

    XYDomain domain;
    domain.setRange(range.left(), range.right(), range.top(),range.bottom());
    domain.setSize(size);

    QSignalSpy spy0(&domain, SIGNAL(updated()));
    QSignalSpy spy1(&domain, SIGNAL(rangeHorizontalChanged(qreal,qreal)));
    QSignalSpy spy2(&domain, SIGNAL(rangeVerticalChanged(qreal,qreal)));

    domain.move(dx, dy);

    QCOMPARE(domain.minX(),qreal(result.left()));
    QCOMPARE(domain.maxX(),qreal(result.right()));
    QCOMPARE(domain.minY(),qreal(result.top()));
    QCOMPARE(domain.maxY(),qreal(result.bottom()));

    TRY_COMPARE(spy0.count(), 1);
    TRY_COMPARE(spy1.count(), (dx != 0 ? 1 : 0));
    TRY_COMPARE(spy2.count(), (dy != 0 ? 1 : 0));
}

QTEST_MAIN(tst_Domain)
#include "tst_domain.moc"
