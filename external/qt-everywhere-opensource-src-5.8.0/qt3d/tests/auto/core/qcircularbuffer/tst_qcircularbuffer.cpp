/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QObject>
#include <QtTest/QtTest>

#include <Qt3DCore/private/qcircularbuffer_p.h>

using namespace Qt3DCore;

class tst_QCircularBuffer : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void construction();
    void destruction();

    void append();
    void at();
    void capacity();
    void clear();
    void contains();
    void count();
    void data();
    void dataOneTwo();
    void endsWith();
    void erase();
    void fill();
    void first();
    void fromList();
    void fromVector();
    void indexOf();
    void insert();
    void insertIterator();
    void isEmpty();
    void isFull();
    void isLinearised();
    void last();
    void lastIndexOf();
    void linearise();
    void prepend();
    void sharable();

    void removeStaticLinearised();
    void removeStaticNonLinearised();
    void removeMovableLinearised();
    void removeMovableNonLinearisedUpper();
    void removeMovableNonLinearisedLower();

    void replace();
    void resize();

    void setCapacityStatic();
    void setCapacityMovable();

    void size();
    void sizeAvailable();
    void startsWith();
    void toList();
    void toVector();
    void value();

    void operatorEquality();
    void operatorInequality();
    void operatorPlus();
    void operatorPlusEquals();
    void operatorStream();

    void const_iterator();
    void iterator();

    void testAppendFifo();
};

class MyComplexType
{
public:
    MyComplexType(int xx = 0)
        : x(xx),
          magicCheck(ms_magic)
    {
        ++ms_activeCount;
    }

    MyComplexType(const MyComplexType &other)
        : x(other.x),
          magicCheck(other.magicCheck)
    {
        ++ms_activeCount;
    }

    ~MyComplexType()
    {
        // Check that the constructor was actually called
        QVERIFY(ms_magic == magicCheck);
        --ms_activeCount;
    }

    MyComplexType &operator=(const MyComplexType &other)
    {
        if (this == &other)
            return *this;
        x = other.x;
        magicCheck = other.magicCheck;
        return *this;
    }

    int x;
    int magicCheck;

    static const int ms_magic = 0xfefefefe;
    static int ms_activeCount;
};

int MyComplexType::ms_activeCount = 0;

void tst_QCircularBuffer::construction()
{
    // Create an empty circular buffer
    QCircularBuffer<int> circ1;
    QVERIFY(circ1.capacity() == 0);
    QVERIFY(circ1.size() == 0);

    // Create a circular buffer with capacity for 10 items
    QCircularBuffer<int> circ2(10);
    QVERIFY(circ2.capacity() == 10);

    // Create a circular buffer with capacity for 3 items, all of which
    // are initialised to a specific value
    QCircularBuffer<int> circ3(3, 69);
    QVERIFY(circ3.capacity() == 3);
    QVERIFY(circ3.size() == 3);
    QVERIFY(circ3.at(0) == 69);
    QVERIFY(circ3.at(1) == 69);
    QVERIFY(circ3.at(2) == 69);

    // Create a circular buffer with capacity for 5 items. Pre-populate the
    // first 2 items to a known value.
    QCircularBuffer<int> circ4(5, 2, 10);
    QVERIFY(circ4.capacity() == 5);
    QVERIFY(circ4.size() == 2);
    QVERIFY(circ4.at(0) == 10);
    QVERIFY(circ4.at(1) == 10);
    QVERIFY(circ4.refCount().load() == 1);

    // Copy construct from circ4. Both circ4 and circ5 should now have a
    // refCount() of 2 since we are using implicit sharing.
    QCircularBuffer<int> circ5(circ4);
    QVERIFY(circ4.refCount().load() == 2);
    QVERIFY(circ5.refCount().load() == 2);
    QVERIFY(circ5.capacity() == 5);
    QVERIFY(circ5.size() == 2);
    QVERIFY(circ5.at(0) == 10);
    QVERIFY(circ5.at(1) == 10);
}

void tst_QCircularBuffer::destruction()
{
    // Make sure that we only call the dtor for items that
    // have actually been constructed. There was a bug where we
    // were accidentally calling the dtor for all items in the
    // memory block (d->capacity items) rather than constructed
    // items (d->size items). The complex dtor of QVector showed
    // this up nicely with a crash. So now we test for this, but
    // in a non-crashy manner.

    // Reset the count of active (constructed items)
    MyComplexType::ms_activeCount = 0;

    // Populate a circular buffer and remove an item. The check
    // in the dtor makes sure that the item was constructed
    QCircularBuffer<MyComplexType> *cir = new QCircularBuffer<MyComplexType>(8);
    cir->append(MyComplexType(1));
    cir->append(MyComplexType(2));
    cir->append(MyComplexType(3));
    cir->remove(0);
    Q_UNUSED(cir);

    // Check that the dtor was called 2 times fewer than the constructor.
    // At this stage will still have 2 items in the circular buffer.
    QVERIFY(MyComplexType::ms_activeCount == 2);

    // Destroy the circular buffer and check that the active count
    // is 0. (Same number of calls to dtor as have been made to the constructors)
    delete cir;
    cir = 0;
    QVERIFY(MyComplexType::ms_activeCount == 0);
}

void tst_QCircularBuffer::append()
{
    // Create a circular buffer with capacity for 3 items
    QCircularBuffer<int> circ(3);

    // Append some items
    circ.append(1);
    circ.append(2);
    circ.append(3);

    // The buffer should now contain the items 1, 2, 3 and be full
    QVERIFY(circ.at(0) == 1);
    QVERIFY(circ.at(1) == 2);
    QVERIFY(circ.at(2) == 3);
    QVERIFY(circ.isFull() == true);

    // Although the buffer is full we can still append items to it.
    // The earliest items will be overwritten
    circ.append(4);
    circ.append(5);

    // The buffer should now contain the items 3, 4, 5 and still be full
    QVERIFY(circ.at(0) == 3);
    QVERIFY(circ.at(1) == 4);
    QVERIFY(circ.at(2) == 5);
    QVERIFY(circ.isFull() == true);

    // Try appending beyond when the buffer becomes non-linearised
    QCircularBuffer<int> circ2(10);
    for (int i = 0; i < 15; ++i)
        circ2.append(i + 1);
    for (int i = 0; i < circ2.size(); ++i)
        QVERIFY(circ2.at(i) == 6 + i);
}

void tst_QCircularBuffer::at()
{
    QCircularBuffer<int> circ(3, 10);
    QVERIFY(circ.at(0) == 10);
    QVERIFY(circ.at(1) == 10);
    QVERIFY(circ.at(2) == 10);

    circ.append(20);
    QVERIFY(circ.at(2) == 20);
}

void tst_QCircularBuffer::capacity()
{
    QCircularBuffer<int> circ(10);
    QVERIFY(circ.capacity() == 10);
}

void tst_QCircularBuffer::clear()
{
    QCircularBuffer<int> circ(3, 10);
    QVERIFY(circ.size() == 3);
    QVERIFY(circ.capacity() == 3);

    circ.clear();
    QVERIFY(circ.size() == 0);
    QVERIFY(circ.capacity() == 3);
    QVERIFY(circ.refCount().load() == 1);
}

void tst_QCircularBuffer::contains()
{
    // Try simple test with a linearised buffer
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    QVERIFY(circ.isLinearised() == true);
    QVERIFY(circ.contains(2) == true);
    QVERIFY(circ.contains(5) == false);

    // Now append some more items to make the buffer non-linearised and
    // test again.
    circ.append(5);
    circ.append(6);
    circ.append(7);
    QVERIFY(circ.isLinearised() == false);
    QVERIFY(circ.contains(6) == true);
    QVERIFY(circ.contains(7) == true);
    QVERIFY(circ.contains(8) == false);
}

void tst_QCircularBuffer::count()
{
    // Test both versions of count() with a linearised buffer
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(4);

    QVERIFY(circ.isLinearised() == true);
    QVERIFY(circ.count() == 5);
    QVERIFY(circ.count(4) == 2);

    // Now with an non-linearised buffer
    circ.append(4);
    circ.append(4);
    QVERIFY(circ.isLinearised() == false);
    QVERIFY(circ.count() == 5);
    QVERIFY(circ.count(4) == 4);
}

void tst_QCircularBuffer::data()
{
    //
    // data() and constData() tests
    //

    // Test with a simple linearised buffer first
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(5);

    QCircularBuffer<int>::array_range range = circ.data();
    int *p = range.first;
    QVERIFY(range.second == 5);
    for (int i = 0; i < range.second; ++i)
        QVERIFY(p[ i ] == i + 1);

    // Now with an non-linearised one. Note that the non-const version will
    // linearise the buffer.
    circ.append(6);
    circ.append(7);
    QVERIFY(circ.isLinearised() == false);
    range = circ.data();
    p = range.first;
    QVERIFY(range.second == 5);
    for (int i = 0; i < range.second; ++i)
        QVERIFY(p[ i ] == i + 3);
    QVERIFY(circ.isLinearised() == true);

    // Let's repeat the exercise with the const version of the data function
    QCircularBuffer<int> circ2(5);
    circ2.append(1);
    circ2.append(2);
    circ2.append(3);
    circ2.append(4);
    circ2.append(5);

    QCircularBuffer<int>::const_array_range range2 = const_cast<const QCircularBuffer<int> *>(&circ2)->data();
    const int *p2 = range2.first;
    for (int i = 0; i < range2.second; ++i)
        QVERIFY(p2[ i ] == i + 1);

    // We should get a null pointer for a non-linearised buffer
    circ2.append(6);
    circ2.append(7);
    QVERIFY(circ2.isLinearised() == false);
    range2 = const_cast<const QCircularBuffer<int> *>(&circ2)->data();
    QVERIFY(range2.first == 0);
    QVERIFY(range2.second == 0);

    // Let's repeat the exercise with the constData function
    QCircularBuffer<int> circ3(5);
    circ3.append(1);
    circ3.append(2);
    circ3.append(3);
    circ3.append(4);
    circ3.append(5);

    QCircularBuffer<int>::const_array_range range3 = circ3.constData();
    const int *p3 = range3.first;
    for (int i = 0; i < range3.second; ++i)
        QVERIFY(p3[ i ] == i + 1);

    // We should get a null pointer for a non-linearised buffer
    circ3.append(6);
    circ3.append(7);
    QVERIFY(circ3.isLinearised() == false);
    range3 = circ3.constData();
    QVERIFY(range3.first == 0);
    QVERIFY(range3.second == 0);
}

void tst_QCircularBuffer::dataOneTwo()
{
    // Start off with a linearised buffer
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(5);

    QCircularBuffer<int>::array_range range = circ.dataOne();
    int *p = range.first;
    QVERIFY(range.second == 5);
    for (int i = 0; i < range.second; ++i)
        QVERIFY(p[ i ] == i + 1);

    // Test the const version too
    QCircularBuffer<int>::const_array_range constRange = const_cast< const QCircularBuffer<int> *>(&circ)->dataOne();
    const int *constp = constRange.first;
    QVERIFY(constRange.second == 5);
    for (int i = 0; i < constRange.second; ++i)
        QVERIFY(constp[ i ] == i + 1);

    // And the constDataOne() function
    constRange = circ.constDataOne();
    constp = constRange.first;
    QVERIFY(constRange.second == 5);
    for (int i = 0; i < constRange.second; ++i)
        QVERIFY(constp[ i ] == i + 1);

    // dataTwo should return a null array when the buffer is linearised
    QCircularBuffer<int>::array_range rangeb = circ.dataTwo();
    QVERIFY(rangeb.first == 0);
    QVERIFY(rangeb.second == 0);

    // Test the const version too
    QCircularBuffer<int>::const_array_range constRangeb = const_cast< const QCircularBuffer<int> *>(&circ)->dataTwo();
    QVERIFY(constRangeb.first == 0);
    QVERIFY(constRangeb.second == 0);

    // And the constDataTwo() function
    constRangeb = circ.constDataTwo();
    QVERIFY(constRangeb.first == 0);
    QVERIFY(constRangeb.second == 0);

    // Now make the buffer non-linearised by adding more items
    circ.append(6);
    circ.append(7);
    QVERIFY(circ.isLinearised() == false);

    // dataOne() should return the first contiguous part of the array
    range = circ.dataOne();
    p = range.first;
    QVERIFY(range.second == 3);
    for (int i = 0; i < range.second; ++i)
        QVERIFY(p[ i ] == i + 3);
    QVERIFY(circ.isLinearised() == false);

    // Test the const version too
    constRange = const_cast< const QCircularBuffer<int> *>(&circ)->dataOne();
    constp = constRange.first;
    QVERIFY(constRange.second == 3);
    for (int i = 0; i < constRange.second; ++i)
        QVERIFY(constp[ i ] == i + 3);
    QVERIFY(circ.isLinearised() == false);

    // And the constDataOne() function
    constRange = circ.constDataOne();
    constp = constRange.first;
    QVERIFY(constRange.second == 3);
    for (int i = 0; i < constRange.second; ++i)
        QVERIFY(constp[ i ] == i + 3);
    QVERIFY(circ.isLinearised() == false);

    // dataTwo should return the second contiguous part of the array
    rangeb = circ.dataTwo();
    QVERIFY(rangeb.second == 2);
    int *pb = rangeb.first;
    for (int i = 0; i < rangeb.second; ++i)
        QVERIFY(pb[ i ] == i + 6);
    QVERIFY(circ.isLinearised() == false);

    // Test the const version too
    constRangeb = const_cast< const QCircularBuffer<int> *>(&circ)->dataTwo();
    QVERIFY(constRangeb.second == 2);
    const int *constpb = constRangeb.first;
    for (int i = 0; i < constRangeb.second; ++i)
        QVERIFY(constpb[ i ] == i + 6);
    QVERIFY(circ.isLinearised() == false);

    // And finally, the constDataTwo() function.
    constRangeb = circ.constDataTwo();
    QVERIFY(constRangeb.second == 2);
    constpb = constRangeb.first;
    for (int i = 0; i < constRangeb.second; ++i)
        QVERIFY(constpb[ i ] == i + 6);
    QVERIFY(circ.isLinearised() == false);
}

void tst_QCircularBuffer::endsWith()
{
    // Test with a linearised buffer
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(5);
    QVERIFY(circ.endsWith(5) == true);
    QVERIFY(circ.endsWith(3) == false);

    // Now with a non-linearised buffer
    circ.append(6);
    circ.append(7);
    QVERIFY(circ.endsWith(7) == true);
    QVERIFY(circ.endsWith(3) == false);

    // Now with an empty buffer
    QCircularBuffer<int> circ2(1);
    QVERIFY(!circ2.endsWith(1));
    circ2.append(1);
    QVERIFY(circ2.endsWith(1));
    circ2.erase(circ2.begin());
    QVERIFY(!circ2.endsWith(1));
}

void tst_QCircularBuffer::erase()
{
    QCircularBuffer<int> circ(10);
    for (int i = 0; i < circ.capacity(); ++i)
        circ.append(i + 1);

    QCircularBuffer<int>::iterator pos = circ.begin() + 2;
    QCircularBuffer<int>::iterator it = circ.erase(pos);

    QVERIFY(circ.size() == circ.capacity() - 1);
    for (int i = 0; i < circ.size(); ++i) {
        if (i < 2)
            QVERIFY(circ.at(i) == i + 1);
        else
            QVERIFY(circ.at(i) == i + 2);
    }

    QVERIFY(*it == 4);

    QCircularBuffer<int>::iterator start = circ.begin() + 2;
    QCircularBuffer<int>::iterator end = circ.end();
    it = circ.erase(start, end);
    QVERIFY(circ.size() == 2);
    QVERIFY(circ.at(0) == 1);
    QVERIFY(circ.at(1) == 2);
    QVERIFY(it == circ.end());
}

void tst_QCircularBuffer::fill()
{
    QCircularBuffer<int> circ(5, 0);
    circ.fill(2);
    QVERIFY(circ.capacity() == 5);
    QVERIFY(circ.size() == 5);
    for (int i = 0; i < 5; ++i)
        QVERIFY(circ.at(i) == 2);

    // Fill the first 3 entries with value 7
    circ.fill(7, 3);
    QVERIFY(circ.capacity() == 5);
    QVERIFY(circ.size() == 3);
    for (int i = 0; i < 3; ++i)
        QVERIFY(circ.at(i) == 7);

    // We abuse the data pointer to check that values beyond the valid array
    // have been reset when we resized down.
    QCircularBuffer<int>::array_range range = circ.data();
    QVERIFY(range.second == 3);
    QVERIFY(range.first[ 3 ] == 0);
    QVERIFY(range.first[ 4 ] == 0);
}

void tst_QCircularBuffer::first()
{
    QCircularBuffer<int> circ(3);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    QVERIFY(circ.at(0) == 1);
    QVERIFY(circ.first() == 1);

    circ.append(4);
    circ.append(5);
    QVERIFY(circ.first() == 3);
}

void tst_QCircularBuffer::fromList()
{
    QList<int> list = (QList<int>() << 1 << 2 << 3 << 4 << 5);
    QCircularBuffer<int> circ = QCircularBuffer<int>::fromList(list);
    QVERIFY(circ.capacity() == list.size());
    QVERIFY(circ.size() == list.size());
    QVERIFY(circ.isLinearised() == true);
    for (int i = 0; i < circ.size(); ++i)
        QVERIFY(circ.at(i) == list.at(i));
}

void tst_QCircularBuffer::fromVector()
{
    QVector<int> vector = (QVector<int>() << 1 << 2 << 3 << 4 << 5);
    QCircularBuffer<int> circ = QCircularBuffer<int>::fromVector(vector);
    QVERIFY(circ.capacity() == vector.size());
    QVERIFY(circ.size() == vector.size());
    QVERIFY(circ.isLinearised() == true);
    for (int i = 0; i < circ.size(); ++i)
        QVERIFY(circ.at(i) == vector.at(i));
}

void tst_QCircularBuffer::indexOf()
{
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(2);
    circ.append(5);

    QVERIFY(circ.indexOf(3) == 2);
    QVERIFY(circ.indexOf(2) == 1);
    QVERIFY(circ.indexOf(2, 2) == 3);

    circ.append(6);
    circ.append(6);
    QVERIFY(circ.indexOf(2) == 1);
    QVERIFY(circ.indexOf(6) == 3);
    QVERIFY(circ.indexOf(6, 4) == 4);
}

void tst_QCircularBuffer::insert()
{
    // Try a simple insert in the upper part of the buffer
    QCircularBuffer<MyComplexType> circ(8);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(5);
    circ.append(6);

    QVERIFY(circ.capacity() == 8);
    QVERIFY(circ.size() == 6);

    circ.insert(4, 1, 0);
    QVERIFY(circ.capacity() == 8);
    QVERIFY(circ.size() == 7);
    QVERIFY(circ.at(0).x == 1);
    QVERIFY(circ.at(1).x == 2);
    QVERIFY(circ.at(2).x == 3);
    QVERIFY(circ.at(3).x == 4);
    QVERIFY(circ.at(4).x == 0);
    QVERIFY(circ.at(5).x == 5);
    QVERIFY(circ.at(6).x == 6);

    // Try a multiple insert in the upper part of the buffer
    QCircularBuffer<MyComplexType> circ2(8);
    circ2.append(1);
    circ2.append(2);
    circ2.append(3);
    circ2.append(4);
    circ2.append(5);
    circ2.append(6);

    QVERIFY(circ2.capacity() == 8);
    QVERIFY(circ2.size() == 6);

    circ2.insert(4, 7, 0);
    QVERIFY(circ2.capacity() == 8);
    QVERIFY(circ2.size() == 8);
    QVERIFY(circ2.at(0).x == 0);
    QVERIFY(circ2.at(1).x == 0);
    QVERIFY(circ2.at(2).x == 0);
    QVERIFY(circ2.at(3).x == 0);
    QVERIFY(circ2.at(4).x == 0);
    QVERIFY(circ2.at(5).x == 0);
    QVERIFY(circ2.at(6).x == 5);
    QVERIFY(circ2.at(7).x == 6);

    // Try a simple insert in the upper part of the buffer using a movable type.
    // This tests the case where the part of the buffer to be moved up does not
    // wrap around before or after the move.
    QCircularBuffer<int> circ3(8);
    circ3.append(1);
    circ3.append(2);
    circ3.append(3);
    circ3.append(4);
    circ3.append(5);
    circ3.append(6);

    circ3.insert(4, 1, 0);
    QVERIFY(circ3.capacity() == 8);
    QVERIFY(circ3.size() == 7);
    QVERIFY(circ3.at(0) == 1);
    QVERIFY(circ3.at(1) == 2);
    QVERIFY(circ3.at(2) == 3);
    QVERIFY(circ3.at(3) == 4);
    QVERIFY(circ3.at(4) == 0);
    QVERIFY(circ3.at(5) == 5);
    QVERIFY(circ3.at(6) == 6);

    // Do the same again but this time such that the destination buffer wraps around.
    QCircularBuffer<int> circ4(8);
    circ4.append(1);
    circ4.append(2);
    circ4.append(3);
    circ4.append(4);
    circ4.append(5);
    circ4.append(6);

    circ4.insert(4, 3, 0);
    QVERIFY(circ4.capacity() == 8);
    QVERIFY(circ4.size() == 8);
    QVERIFY(circ4.at(0) == 2);
    QVERIFY(circ4.at(1) == 3);
    QVERIFY(circ4.at(2) == 4);
    QVERIFY(circ4.at(3) == 0);
    QVERIFY(circ4.at(4) == 0);
    QVERIFY(circ4.at(5) == 0);
    QVERIFY(circ4.at(6) == 5);
    QVERIFY(circ4.at(7) == 6);

    // And now when the src buffer wraps
    QCircularBuffer<int> circ5(8);
    circ5.append(1);
    circ5.append(2);
    circ5.append(3);
    circ5.append(4);
    circ5.append(5);
    circ5.append(6);
    circ5.append(7);
    circ5.append(8);
    circ5.append(9);
    circ5.append(10);

    circ5.insert(4, 3, 0);
    QVERIFY(circ5.capacity() == 8);
    QVERIFY(circ5.size() == 8);
    QVERIFY(circ5.at(0) == 6);
    QVERIFY(circ5.at(1) == 0);
    QVERIFY(circ5.at(2) == 0);
    QVERIFY(circ5.at(3) == 0);
    QVERIFY(circ5.at(4) == 7);
    QVERIFY(circ5.at(5) == 8);
    QVERIFY(circ5.at(6) == 9);
    QVERIFY(circ5.at(7) == 10);

    // Finally when both src and dst buffers wrap
    QCircularBuffer<int> circ6(10);
    circ6.append(1);
    circ6.append(2);
    circ6.append(3);
    circ6.append(4);
    circ6.append(5);
    circ6.append(6);
    circ6.append(7);
    circ6.append(8);
    circ6.append(9);
    circ6.append(10);
    circ6.append(11);
    circ6.append(12);

    circ6.insert(5, 2, 0);
    QVERIFY(circ6.capacity() == 10);
    QVERIFY(circ6.size() == 10);
    QVERIFY(circ6.at(0) == 5);
    QVERIFY(circ6.at(1) == 6);
    QVERIFY(circ6.at(2) == 7);
    QVERIFY(circ6.at(3) == 0);
    QVERIFY(circ6.at(4) == 0);
    QVERIFY(circ6.at(5) == 8);
    QVERIFY(circ6.at(6) == 9);
    QVERIFY(circ6.at(7) == 10);
    QVERIFY(circ6.at(8) == 11);
    QVERIFY(circ6.at(9) == 12);


    // Try a simple insert in the lower part of the buffer
    QCircularBuffer<MyComplexType> circ7(8);
    circ7.append(1);
    circ7.append(2);
    circ7.append(3);
    circ7.append(4);
    circ7.append(5);
    circ7.append(6);

    QVERIFY(circ7.capacity() == 8);
    QVERIFY(circ7.size() == 6);

    circ7.insert(1, 1, 0);
    QVERIFY(circ7.capacity() == 8);
    QVERIFY(circ7.size() == 7);
    QVERIFY(circ7.at(0).x == 1);
    QVERIFY(circ7.at(1).x == 0);
    QVERIFY(circ7.at(2).x == 2);
    QVERIFY(circ7.at(3).x == 3);
    QVERIFY(circ7.at(4).x == 4);
    QVERIFY(circ7.at(5).x == 5);
    QVERIFY(circ7.at(6).x == 6);

    // Try a multiple insert in the lower part of the buffer
    QCircularBuffer<MyComplexType> circ8(8);
    circ8.append(1);
    circ8.append(2);
    circ8.append(3);
    circ8.append(4);
    circ8.append(5);
    circ8.append(6);

    QVERIFY(circ8.capacity() == 8);
    QVERIFY(circ8.size() == 6);

    circ8.insert(1, 4, 0);
    QVERIFY(circ8.capacity() == 8);
    QVERIFY(circ8.size() == 8);
    QVERIFY(circ8.at(0).x == 0);
    QVERIFY(circ8.at(1).x == 0);
    QVERIFY(circ8.at(2).x == 0);
    QVERIFY(circ8.at(3).x == 2);
    QVERIFY(circ8.at(4).x == 3);
    QVERIFY(circ8.at(5).x == 4);
    QVERIFY(circ8.at(6).x == 5);
    QVERIFY(circ8.at(7).x == 6);

    // Try a simple insert in the lower part of the buffer using a movable type.
    // This tests the case where the part of the buffer to be moved up does not
    // wrap around before or after the move.
    QCircularBuffer<int> circ9(10);
    circ9.append(1);
    circ9.append(2);
    circ9.append(3);
    circ9.append(4);
    circ9.append(5);
    circ9.append(6);
    circ9.append(7);
    circ9.append(8);
    circ9.append(9);

    circ9.insert(3, 2, 0);
    QVERIFY(circ9.capacity() == 10);
    QVERIFY(circ9.size() == 10);
    QVERIFY(circ9.at(0) == 2);
    QVERIFY(circ9.at(1) == 3);
    QVERIFY(circ9.at(2) == 0);
    QVERIFY(circ9.at(3) == 0);
    QVERIFY(circ9.at(4) == 4);
    QVERIFY(circ9.at(5) == 5);
    QVERIFY(circ9.at(6) == 6);
    QVERIFY(circ9.at(7) == 7);
    QVERIFY(circ9.at(8) == 8);
    QVERIFY(circ9.at(9) == 9);

    // And now when the src buffer wraps
    QCircularBuffer<int> circ10(10);
    circ10.append(3);
    circ10.append(4);
    circ10.append(5);
    circ10.append(6);
    circ10.append(7);
    circ10.append(8);
    circ10.append(9);
    circ10.append(10);
    circ10.prepend(2);
    circ10.prepend(1);

    // Verify that the input buffer is as expected
    QVERIFY(circ10.capacity() == 10);
    QVERIFY(circ10.size() == 10);
    QCircularBuffer<int>::const_array_range p2 = circ10.constDataTwo();
    QVERIFY(p2.second == 8);
    QVERIFY(p2.first[ 0 ] == 3);
    QVERIFY(p2.first[ 1 ] == 4);
    QVERIFY(p2.first[ 7 ] == 10);
    QCircularBuffer<int>::const_array_range p = circ10.constDataOne();
    QVERIFY(p.second == 2);
    QVERIFY(p.first[ 0 ] == 1);
    QVERIFY(p.first[ 1 ] == 2);

    circ10.insert(3, 1, 0);
    QVERIFY(circ10.capacity() == 10);
    QVERIFY(circ10.size() == 10);
    QVERIFY(circ10.at(0) == 2);
    QVERIFY(circ10.at(1) == 3);
    QVERIFY(circ10.at(2) == 0);
    QVERIFY(circ10.at(3) == 4);
    QVERIFY(circ10.at(4) == 5);
    QVERIFY(circ10.at(5) == 6);
    QVERIFY(circ10.at(6) == 7);
    QVERIFY(circ10.at(7) == 8);
    QVERIFY(circ10.at(8) == 9);
    QVERIFY(circ10.at(9) == 10);

    // Finally when both src and dst buffers wrap
    QCircularBuffer<int> circ11(13);
    circ11.append(4);
    circ11.append(5);
    circ11.append(6);
    circ11.append(7);
    circ11.append(8);
    circ11.append(9);
    circ11.append(10);
    circ11.append(11);
    circ11.append(12);
    circ11.append(13);
    circ11.prepend(3);
    circ11.prepend(2);
    circ11.prepend(1);

    // Verify that the input buffer is as expected
    QVERIFY(circ11.capacity() == 13);
    QVERIFY(circ11.size() == 13);
    p2 = circ11.constDataTwo();
    QVERIFY(p2.second == 10);
    QVERIFY(p2.first[ 0 ] == 4);
    QVERIFY(p2.first[ 1 ] == 5);
    QVERIFY(p2.first[ 9 ] == 13);
    p = circ11.constDataOne();
    QVERIFY(p.second == 3);
    QVERIFY(p.first[ 0 ] == 1);
    QVERIFY(p.first[ 1 ] == 2);
    QVERIFY(p.first[ 2 ] == 3);

    circ11.insert(5, 1, 0);
    QVERIFY(circ11.capacity() == 13);
    QVERIFY(circ11.size() == 13);
    QVERIFY(circ11.at(0) == 2);
    QVERIFY(circ11.at(1) == 3);
    QVERIFY(circ11.at(2) == 4);
    QVERIFY(circ11.at(3) == 5);
    QVERIFY(circ11.at(4) == 0);
    QVERIFY(circ11.at(5) == 6);
    QVERIFY(circ11.at(6) == 7);
    QVERIFY(circ11.at(7) == 8);
    QVERIFY(circ11.at(8) == 9);
    QVERIFY(circ11.at(9) == 10);
    QVERIFY(circ11.at(10) == 11);
    QVERIFY(circ11.at(11) == 12);
    QVERIFY(circ11.at(12) == 13);

    // Try inserting at the end of the buffer. Should work just like append()
    QCircularBuffer<int> circ12(10);
    for (int i = 0; i < 5; ++i)
        circ12.append(i + 1);
    for (int i = 5; i < 10; ++i)
        circ12.insert(circ12.size(), (i + 1) * 10);
    for (int i = 0; i < circ12.size(); ++i) {
        if (i < 5)
            QVERIFY(circ12.at(i) == i + 1);
        else
            QVERIFY(circ12.at(i) == (i + 1) * 10);
    }

    // Try inserting at the end of the buffer with non-movable type. Should work just like append()
    QCircularBuffer<MyComplexType> circ13(10);
    for (int i = 0; i < 5; ++i)
        circ13.append(i + 1);
    for (int i = 5; i < 10; ++i)
        circ13.insert(circ13.size(), (i + 1) * 10);
    for (int i = 0; i < circ13.size(); ++i) {
        if (i < 5)
            QVERIFY(circ13.at(i).x == i + 1);
        else
            QVERIFY(circ13.at(i).x == (i + 1) * 10);
    }

    // Insert multiple items at end of buffer
    QCircularBuffer<int> circ14(10);
    for (int i = 0; i < 5; ++i)
        circ14.append(i + 1);
    circ14.insert(circ14.size(), 7, 100);
    for (int i = 0; i < circ14.size(); ++i) {
        if (i < 3)
            QVERIFY(circ14.at(i) == i + 3);
        else
            QVERIFY(circ14.at(i) == 100);
    }

    // Insert multiple items at end of buffer of non-movable type
    QCircularBuffer<MyComplexType> circ15(10);
    for (int i = 0; i < 5; ++i)
        circ15.append(i + 1);
    circ15.insert(circ15.size(), 7, 100);
    for (int i = 0; i < circ15.size(); ++i) {
        if (i < 3)
            QVERIFY(circ15.at(i).x == i + 3);
        else
            QVERIFY(circ15.at(i).x == 100);
    }

    // Try inserting at the start of the buffer. Should work just like prepend()
    QCircularBuffer<int> circ16(10);
    for (int i = 0; i < 5; ++i)
        circ16.append(10);
    for (int i = 5; i < 10; ++i)
        circ16.insert(0, 20);
    for (int i = 0; i < circ16.size(); ++i) {
        if (i < 5)
            QVERIFY(circ16.at(i) == 20);
        else
            QVERIFY(circ16.at(i) == 10);
    }

    // Try inserting at the start of the buffer with non-movable type. Should work just like prepend()
    QCircularBuffer<MyComplexType> circ17(10);
    for (int i = 0; i < 5; ++i)
        circ17.append(10);
    for (int i = 5; i < 10; ++i)
        circ17.insert(0, 20);
    for (int i = 0; i < circ17.size(); ++i) {
        if (i < 5)
            QVERIFY(circ17.at(i).x == 20);
        else
            QVERIFY(circ17.at(i).x == 10);
    }

    // Try inserting multiple items at the start of the buffer. Should work just like prepend()
    QCircularBuffer<int> circ18(10);
    for (int i = 0; i < 5; ++i)
        circ18.append(10);
    circ18.insert(0, 5, 20);
    for (int i = 0; i < circ18.size(); ++i) {
        if (i < 5)
            QVERIFY(circ18.at(i) == 20);
        else
            QVERIFY(circ18.at(i) == 10);
    }

    // Try inserting multiple items at the start of the buffer. Should work just like prepend()
    QCircularBuffer<MyComplexType> circ19(10);
    for (int i = 0; i < 5; ++i)
        circ19.append(10);
    circ19.insert(0, 5, 20);
    for (int i = 0; i < circ19.size(); ++i) {
        if (i < 5)
            QVERIFY(circ19.at(i).x == 20);
        else
            QVERIFY(circ19.at(i).x == 10);
    }
}

void tst_QCircularBuffer::insertIterator()
{
    // Test the overload of insert that take an iterator as opposed to an index
    QCircularBuffer<int> circ(10);
    for (int i = 0; i < 5; ++i)
        circ.append(i + 1);

    QCircularBuffer<int>::iterator it = circ.begin() + 2;
    QCircularBuffer<int>::iterator it2 = circ.insert(it, 3, 10);
    for (int i = 0; i < circ.size(); ++i) {
        if (i < 2)
            QVERIFY(circ.at(i) == i + 1);
        else if (i > 4)
            QVERIFY(circ.at(i) == i - 2);
        else
            QVERIFY(circ.at(i) == 10);
    }

    for (int i = 0; i < 3; ++i) {
        QVERIFY(*it2 == 10);
        ++it2;
    }
}

void tst_QCircularBuffer::isEmpty()
{
    QCircularBuffer<int> circ(3);
    QVERIFY(circ.isEmpty() == true);
    circ.append(1);
    circ.append(2);
    QVERIFY(circ.isEmpty() == false);
    circ.clear();
    QVERIFY(circ.isEmpty() == true);
}

void tst_QCircularBuffer::isFull()
{
    QCircularBuffer<int> circ(3);
    QVERIFY(circ.isFull() == false);
    circ.append(1);
    QVERIFY(circ.isFull() == false);
    circ.append(2);
    QVERIFY(circ.isFull() == false);
    circ.append(3);
    QVERIFY(circ.isFull() == true);
    circ.clear();
    QVERIFY(circ.isFull() == false);
}

void tst_QCircularBuffer::isLinearised()
{
    QCircularBuffer<int> circ(3);
    QVERIFY(circ.isLinearised() == true);
    circ.append(1);
    QVERIFY(circ.isLinearised() == true);
    circ.append(2);
    QVERIFY(circ.isLinearised() == true);
    circ.append(3);
    QVERIFY(circ.isLinearised() == true);
    circ.append(4);
    QVERIFY(circ.isLinearised() == false);
    circ.clear();
    QVERIFY(circ.isLinearised() == true);
}

void tst_QCircularBuffer::last()
{
    QCircularBuffer<int> circ(3);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    QVERIFY(circ.at(2) == 3);
    QVERIFY(circ.last() == 3);

    circ.append(4);
    circ.append(5);
    QVERIFY(circ.last() == 5);
}

void tst_QCircularBuffer::lastIndexOf()
{
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(2);
    circ.append(5);

    QVERIFY(circ.lastIndexOf(3) == 2);
    QVERIFY(circ.lastIndexOf(2) == 3);
    QVERIFY(circ.lastIndexOf(2, 2) == 1);

    circ.append(6);
    QVERIFY(circ.lastIndexOf(2) == 2);
    QVERIFY(circ.lastIndexOf(6) == 4);
    QVERIFY(circ.lastIndexOf(2, 1) == 0);
}

void tst_QCircularBuffer::linearise()
{
    // Start with a linearised buffer
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(5);
    QVERIFY(circ.isLinearised() == true);

    // Add more items to make it non-linearised
    circ.append(6);
    circ.append(7);
    QVERIFY(circ.isLinearised() == false);
    QVERIFY(circ.at(0) == 3);
    QVERIFY(circ.at(1) == 4);
    QVERIFY(circ.at(2) == 5);
    QVERIFY(circ.at(3) == 6);
    QVERIFY(circ.at(4) == 7);

    // Now try to linearise it again
    circ.linearise();
    QVERIFY(circ.isLinearised() == true);
    QVERIFY(circ.at(0) == 3);
    QVERIFY(circ.at(1) == 4);
    QVERIFY(circ.at(2) == 5);
    QVERIFY(circ.at(3) == 6);
    QVERIFY(circ.at(4) == 7);
}

void tst_QCircularBuffer::prepend()
{
    // Create a circular buffer with capacity for 3 items
    QCircularBuffer<int> circ(3);

    // Prepend some items
    circ.prepend(1);
    circ.prepend(2);
    circ.prepend(3);

    // The buffer should now contain the items 1, 2, 3 and be full
    QVERIFY(circ.at(0) == 3);
    QVERIFY(circ.at(1) == 2);
    QVERIFY(circ.at(2) == 1);
    QVERIFY(circ.isFull() == true);

    // Although the buffer is full we can still prepend items to it.
    // The latest items will be overwritten
    circ.prepend(4);
    circ.prepend(5);

    // The buffer should now contain the items 3, 4, 5 and still be full
    QVERIFY(circ.at(0) == 5);
    QVERIFY(circ.at(1) == 4);
    QVERIFY(circ.at(2) == 3);
    QVERIFY(circ.isFull() == true);
}

void tst_QCircularBuffer::sharable()
{
    QCircularBuffer<int> circ(3);

    circ.push_back(1);
    circ.push_back(2);
    circ.push_back(3);

    {
        // take a reference to internal data and remember the current value:
        int &oldValueRef = circ.first();
        const int oldValue = oldValueRef;

        // make a copy of the container:
        const QCircularBuffer<int> c2 = circ;

        // modify copied-from container:
        oldValueRef = -1;

        // check that the copied-to container didn't change
        QEXPECT_FAIL("", "QCircularBuffer doesn't implement non-sharable state", Continue);
        QCOMPARE(c2.first(), oldValue);
    }
}

void tst_QCircularBuffer::removeStaticLinearised()
{
    // Try some simple removes on a linearised buffer with a non-movable type
    QCircularBuffer<MyComplexType> circ(6);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(5);
    circ.append(6);

    QVERIFY(circ.capacity() == 6);
    QVERIFY(circ.size() == 6);

    circ.remove(4, 2);
    QVERIFY(circ.capacity() == 6);
    QVERIFY(circ.size() == 4);
    QVERIFY(circ.at(0).x == 1);
    QVERIFY(circ.at(1).x == 2);
    QVERIFY(circ.at(2).x == 3);
    QVERIFY(circ.at(3).x == 4);

    // Restore the original
    circ.append(5);
    circ.append(6);

    // Now try removing in the upper part of the buffer but not all the
    // way to the end of it
    circ.remove(3, 1);
    QVERIFY(circ.capacity() == 6);
    QVERIFY(circ.size() == 5);
    QVERIFY(circ.at(0).x == 1);
    QVERIFY(circ.at(1).x == 2);
    QVERIFY(circ.at(2).x == 3);
    QVERIFY(circ.at(3).x == 5);
    QVERIFY(circ.at(4).x == 6);

    // Restore the original
    circ.clear();
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(5);
    circ.append(6);

    // Now try to remove some points from the lower half
    circ.remove(1, 2);
    QVERIFY(circ.capacity() == 6);
    QVERIFY(circ.size() == 4);
    QVERIFY(circ.at(0).x == 1);
    QVERIFY(circ.at(1).x == 4);
    QVERIFY(circ.at(2).x == 5);
    QVERIFY(circ.at(3).x == 6);

    // Now try repeatedly removing the first item of a partially full buffer
    QCircularBuffer<MyComplexType> circ2(20);
    for (int i = 0; i < 15; ++i)
        circ2.append(i);

    QVERIFY(circ2.size() == 15);
    for (int i = 0; i < 15; ++i)
        QVERIFY(circ2.at(i).x == i);

    for (int i = 0; i < 10; ++i) {
        MyComplexType value = circ2.first();
        QVERIFY(value.x == i);
        circ2.remove(0);
    }
}

void tst_QCircularBuffer::removeStaticNonLinearised()
{
    // Try the same type of tests with a non-linearised buffer.

    // First remove from upper part of the buffer
    QCircularBuffer<MyComplexType> circ(10);
    for (int i = 0; i < 15; ++i)
        circ.append(i + 1);

    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 10);
    QVERIFY(circ.isLinearised() == false);
    circ.remove(8, 2);
    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 8);
    QVERIFY(circ.at(0).x == 6);
    QVERIFY(circ.at(1).x == 7);
    QVERIFY(circ.at(2).x == 8);
    QVERIFY(circ.at(3).x == 9);
    QVERIFY(circ.at(4).x == 10);
    QVERIFY(circ.at(5).x == 11);
    QVERIFY(circ.at(6).x == 12);
    QVERIFY(circ.at(7).x == 13);

    // Restore the original
    circ.append(14);
    circ.append(15);

    // Remove some points from the lower half
    circ.remove(1, 4);
    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 6);
    QVERIFY(circ.at(0).x == 6);
    QVERIFY(circ.at(1).x == 11);
    QVERIFY(circ.at(2).x == 12);
    QVERIFY(circ.at(3).x == 13);
    QVERIFY(circ.at(4).x == 14);
    QVERIFY(circ.at(5).x == 15);

    // Remove all the way to the end
    QCircularBuffer<MyComplexType> circ2(6);
    for (int i = 0; i < 8; ++i)
        circ2.append(i + 1);
    circ2.remove(2, 4);
    QVERIFY(circ2.capacity() == 6);
    QVERIFY(circ2.size() == 2);
    QVERIFY(circ2.at(0).x == 3);
    QVERIFY(circ2.at(1).x == 4);

    // Try another couple of cases to be thorough
    QCircularBuffer<MyComplexType> circ3(6);
    for (int i = 0; i < 8; ++i)
        circ3.append(i + 1);
    circ3.remove(3, 2);
    QVERIFY(circ3.capacity() == 6);
    QVERIFY(circ3.size() == 4);
    QVERIFY(circ3.at(0).x == 3);
    QVERIFY(circ3.at(1).x == 4);
    QVERIFY(circ3.at(2).x == 5);
    QVERIFY(circ3.at(3).x == 8);

    QCircularBuffer<MyComplexType> circ4(6);
    for (int i = 0; i < 8; ++i)
        circ4.append(i + 1);
    circ4.remove(1, 2);
    QVERIFY(circ4.capacity() == 6);
    QVERIFY(circ4.size() == 4);
    QVERIFY(circ4.at(0).x == 3);
    QVERIFY(circ4.at(1).x == 6);
    QVERIFY(circ4.at(2).x == 7);
    QVERIFY(circ4.at(3).x == 8);
}

void tst_QCircularBuffer::removeMovableLinearised()
{
    // Try to do some removes on linearised buffers of a non-movable type

    // Remove from the lower half of the buffer
    QCircularBuffer<int> circ(6);
    for (int i = 0; i < 6; ++i)
        circ.append(i + 1);
    circ.remove(1, 2);
    QVERIFY(circ.capacity() == 6);
    QVERIFY(circ.size() == 4);
    QVERIFY(circ.at(0) == 1);
    QVERIFY(circ.at(1) == 4);
    QVERIFY(circ.at(2) == 5);
    QVERIFY(circ.at(3) == 6);

    // Remove form the upper half of the buffer
    QCircularBuffer<int> circ2(6);
    for (int i = 0; i < 6; ++i)
        circ2.append(i + 1);
    circ2.remove(3, 2);
    QVERIFY(circ2.capacity() == 6);
    QVERIFY(circ2.size() == 4);
    QVERIFY(circ2.at(0) == 1);
    QVERIFY(circ2.at(1) == 2);
    QVERIFY(circ2.at(2) == 3);
    QVERIFY(circ2.at(3) == 6);
}

void tst_QCircularBuffer::removeMovableNonLinearisedUpper()
{
    // Src region wraps and the dst region does not wrap
    QCircularBuffer<int> circ(20);
    for (int i = 0; i < 23; ++i)
        circ.append(i + 1);
    circ.remove(11, 5);
    QVERIFY(circ.size() == 15);
    for (int i = 0; i < 11; ++i)
        QVERIFY(circ.at(i) == i + 4);
    QVERIFY(circ.at(11) == 20);
    QVERIFY(circ.at(12) == 21);
    QVERIFY(circ.at(13) == 22);
    QVERIFY(circ.at(14) == 23);

    // Src region does not wrap and the dst region wraps
    QCircularBuffer<int> circ2(10);
    for (int i = 1; i <= 13; i++)
        circ2.append(i);
    circ2.remove(5, 2);
    QVERIFY(circ2.capacity() == 10);
    QVERIFY(circ2.size() == 8);
    QVERIFY(circ2.at(0) == 4);
    QVERIFY(circ2.at(1) == 5);
    QVERIFY(circ2.at(2) == 6);
    QVERIFY(circ2.at(3) == 7);
    QVERIFY(circ2.at(4) == 8);
    QVERIFY(circ2.at(5) == 11);
    QVERIFY(circ2.at(6) == 12);
    QVERIFY(circ2.at(7) == 13);

    // Src and dst regions wrap
    QCircularBuffer<int> circ3(10);
    for (int i = 1; i <= 13; i++)
        circ3.append(i);
    circ3.remove(5, 1);
    QVERIFY(circ3.capacity() == 10);
    QVERIFY(circ3.size() == 9);
    QVERIFY(circ3.at(0) == 4);
    QVERIFY(circ3.at(1) == 5);
    QVERIFY(circ3.at(2) == 6);
    QVERIFY(circ3.at(3) == 7);
    QVERIFY(circ3.at(4) == 8);
    QVERIFY(circ3.at(5) == 10);
    QVERIFY(circ3.at(6) == 11);
    QVERIFY(circ3.at(7) == 12);
    QVERIFY(circ3.at(8) == 13);

    // Neither the src nor dst regions wrap
    QCircularBuffer<int> circ4(20);
    for (int i = 0; i < 26; ++i)
        circ4.append(i + 1);
    circ4.remove(15, 3);
    QVERIFY(circ4.size() == 17);
    for (int i = 0; i < 15; ++i)
        QVERIFY(circ4.at(i) == i + 7);
    QVERIFY(circ4.at(15) == 25);
    QVERIFY(circ4.at(16) == 26);
}

void tst_QCircularBuffer::removeMovableNonLinearisedLower()
{
    // Src region wraps and the dst region does not wrap.
    // This case also tests zeroing the free'd region when it is disjoint.
    QCircularBuffer<int> circ(20);
    for (int i = 0; i < 36; ++i)
        circ.append(i + 1);
    circ.remove(6, 7);
    QVERIFY(circ.size() == 13);
    for (int i = 0; i < 6; ++i)
        QVERIFY(circ.at(i) == i + 17);
    for (int i = 6; i < circ.size(); ++i)
        QVERIFY(circ.at(i) == i + 24);

    // Src region does not wrap and the dst region wraps
    // This case also tests zeroing the free'd region when it is contiguous.
    QCircularBuffer<int> circ2(20);
    for (int i = 0; i < 36; ++i)
        circ2.append(i + 1);
    circ2.remove(4, 3);
    QVERIFY(circ2.size() == 17);
    for (int i = 0; i < 4; ++i)
        QVERIFY(circ2.at(i) == i + 17);
    for (int i = 4; i < circ2.size(); ++i)
        QVERIFY(circ2.at(i) == i + 20);

    // Src and dst regions wrap
    QCircularBuffer<int> circ3(20);
    for (int i = 0; i < 36; ++i)
        circ3.append(i + 1);
    circ3.remove(6, 3);
    QVERIFY(circ3.size() == 17);
    for (int i = 0; i < 6; ++i)
        QVERIFY(circ3.at(i) == i + 17);
    for (int i = 6; i < circ3.size(); ++i)
        QVERIFY(circ3.at(i) == i + 20);

    // Neither the src nor dst regions wrap
    QCircularBuffer<int> circ4(20);
    for (int i = 0; i < 36; ++i)
        circ4.append(i + 1);
    circ4.remove(2, 1);
    QVERIFY(circ4.size() == 19);
    for (int i = 0; i < 2; ++i)
        QVERIFY(circ4.at(i) == i + 17);
    for (int i = 2; i < circ4.size(); ++i)
        QVERIFY(circ4.at(i) == i + 18);
}

void tst_QCircularBuffer::replace()
{
    QCircularBuffer<int> circ(5, 7);
    for (int i = 0; i < 5; ++i)
        QVERIFY(circ.at(i) == 7);

    circ.replace(2, 100);
    for (int i = 0; i < 5; ++i) {
        if (i != 2)
            QVERIFY(circ.at(i) == 7);
        else
            QVERIFY(circ.at(2) == 100);
    }
}

void tst_QCircularBuffer::resize()
{
    QCircularBuffer<int> circ(10);
    for (int i = 0; i < circ.capacity(); ++i)
        circ.append(i + 1);
    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 10);

    circ.resize(6);
    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 6);
    for (int i = 0; i < circ.size(); ++i)
        QVERIFY(circ.at(i) == i + 1);

    circ.resize(9);
    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 9);
    for (int i = 0; i < circ.size(); ++i) {
        if (i < 6)
            QVERIFY(circ.at(i) == i + 1);
        else
            QVERIFY(circ.at(i) == 0);
    }
}

void tst_QCircularBuffer::setCapacityStatic()
{
    // New capacity < old capacity & src region wraps
    QCircularBuffer<MyComplexType> circ(20);
    for (int i = 0; i < 36; ++i)
        circ.append(i + 1);
    circ.setCapacity(10);
    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 10);
    for (int i = 0; i < circ.size(); ++i)
        QVERIFY(circ.at(i).x == i + 17);

    // New capacity < old capacity & src region does not wrap
    QCircularBuffer<MyComplexType> circ2(20);
    for (int i = 0; i < 36; ++i)
        circ2.append(i + 1);
    circ2.setCapacity(3);
    QVERIFY(circ2.capacity() == 3);
    QVERIFY(circ2.size() == 3);
    for (int i = 0; i < circ2.size(); ++i)
        QVERIFY(circ2.at(i).x == i + 17);

    // New capacity > old capacity
    QCircularBuffer<MyComplexType> circ3(20);
    for (int i = 0; i < 36; ++i)
        circ3.append(i + 1);
    circ3.setCapacity(25);
    QVERIFY(circ3.capacity() == 25);
    QVERIFY(circ3.size() == 20);
    for (int i = 0; i < circ3.size(); ++i)
        QVERIFY(circ3.at(i).x == i + 17);
}

void tst_QCircularBuffer::setCapacityMovable()
{
    // New capacity < old capacity & src region wraps
    QCircularBuffer<int> circ(20);
    for (int i = 0; i < 36; ++i)
        circ.append(i + 1);
    circ.setCapacity(10);
    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 10);
    for (int i = 0; i < circ.size(); ++i)
        QVERIFY(circ.at(i) == i + 17);

    // New capacity < old capacity & src region does not wrap
    QCircularBuffer<int> circ2(20);
    for (int i = 0; i < 36; ++i)
        circ2.append(i + 1);
    circ2.setCapacity(3);
    QVERIFY(circ2.capacity() == 3);
    QVERIFY(circ2.size() == 3);
    for (int i = 0; i < circ2.size(); ++i)
        QVERIFY(circ2.at(i) == i + 17);

    // New capacity > old capacity
    QCircularBuffer<int> circ3(20);
    for (int i = 0; i < 36; ++i)
        circ3.append(i + 1);
    circ3.setCapacity(25);
    QVERIFY(circ3.capacity() == 25);
    QVERIFY(circ3.size() == 20);
    for (int i = 0; i < circ3.size(); ++i)
        QVERIFY(circ3.at(i) == i + 17);

    // We also need to test with a linearised buffer of a movable type
    QCircularBuffer<int> circ4(20);
    for (int i = 0; i < 15; ++i)
        circ4.append(i + 1);
    circ4.setCapacity(10);
    QVERIFY(circ4.capacity() == 10);
    QVERIFY(circ4.size() == 10);
    for (int i = 0; i < circ4.size(); ++i)
        QVERIFY(circ4.at(i) == i + 1);
}

void tst_QCircularBuffer::size()
{
    QCircularBuffer<int> circ(3);
    QVERIFY(circ.capacity() == 3);
    QVERIFY(circ.isEmpty() == true);
    QVERIFY(circ.size() == 0);
    circ.append(1);
    QVERIFY(circ.size() == 1);
    circ.append(2);
    circ.append(3);
    QVERIFY(circ.isFull() == true);
    QVERIFY(circ.size() == 3);
    QVERIFY(circ.size() == circ.capacity());
}

void tst_QCircularBuffer::sizeAvailable()
{
    QCircularBuffer<int> circ(10);
    QVERIFY(circ.capacity() == 10);
    QVERIFY(circ.size() == 0);
    QVERIFY(circ.sizeAvailable() == 10);

    for (int i = 0; i < 15; ++i) {
        circ.append(i + 1);
        QVERIFY(circ.size() == qMin(i + 1, circ.capacity()));
        QVERIFY(circ.sizeAvailable() == qMax(0, circ.capacity() - i - 1));
    }
}

void tst_QCircularBuffer::startsWith()
{
    // Test with a linearised buffer
    QCircularBuffer<int> circ(5);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    circ.append(4);
    circ.append(5);
    QVERIFY(circ.startsWith(1) == true);
    QVERIFY(circ.endsWith(3) == false);

    // Now with a non-linearised buffer
    circ.append(6);
    circ.append(7);
    QVERIFY(circ.startsWith(3) == true);
    QVERIFY(circ.startsWith(1) == false);
}

void tst_QCircularBuffer::toList()
{
    QCircularBuffer<int> circ(10);
    for (int i = 0; i < circ.capacity(); ++i)
        circ.append(i + 1);
    QList<int> list = circ.toList();
    QVERIFY(list.size() == circ.size());
    for (int i = 0; i < list.size(); ++i)
        QVERIFY(list.at(i) == circ.at(i));
}

void tst_QCircularBuffer::toVector()
{
    QCircularBuffer<int> circ(10);
    for (int i = 0; i < circ.capacity(); ++i)
        circ.append(i + 1);
    QVector<int> vector = circ.toVector();
    QVERIFY(vector.size() == circ.size());
    for (int i = 0; i < vector.size(); ++i)
        QVERIFY(vector.at(i) == circ.at(i));
}

void tst_QCircularBuffer::value()
{
    QCircularBuffer<int> circ(3);
    circ.append(1);
    circ.append(2);
    circ.append(3);
    QVERIFY(circ.value(0) == 1);
    QVERIFY(circ.value(1) == 2);
    QVERIFY(circ.value(2) == 3);
    QVERIFY(circ.value(3) == 0); // Out of range, so default value
    QVERIFY(circ.value(3, 7) == 7); // Out of range, so specified default value
}

void tst_QCircularBuffer::operatorEquality()
{
    QCircularBuffer<int> circ1(10);
    QCircularBuffer<int> circ2(10);
    for (int i = 0; i < 10; i++) {
        circ1.append(i);
        circ2.append(i);
    }

    QVERIFY(circ1 == circ2);

    // This test for equality should be fast as it should find
    // that the internal d-pointers are the same and so be able
    // to skip the element by element comparison
    QCircularBuffer<int> circ3(circ1);
    QVERIFY(circ1 == circ3);
}

void tst_QCircularBuffer::operatorInequality()
{
    QCircularBuffer<int> circ1(10);
    QCircularBuffer<int> circ2(11);
    for (int i = 0; i < 10; i++) {
        circ1.append(i);
        circ2.append(i);
    }
    circ2.append(11);

    QVERIFY(circ1 != circ2);

    QCircularBuffer<int> circ3(10);
    QCircularBuffer<int> circ4(10);
    for (int i = 0; i < 10; i++) {
        circ3.append(i);
        if (i == 4)
            circ4.append(100);
        else
            circ4.append(i);
    }

    QVERIFY(circ3 != circ4);
}

void tst_QCircularBuffer::operatorPlus()
{
    QCircularBuffer<int> circ1(20);
    for (int i = 0; i < 10; ++i)
        circ1.append(i + 1);

    QCircularBuffer<int> circ2(5);
    for (int i = 0; i < circ2.capacity(); ++i)
        circ2.append(i + 11);

    QCircularBuffer<int> circ3 = circ1 + circ2;
    QVERIFY(circ3.capacity() == circ1.size() + circ2.size());
    QVERIFY(circ3.size() == circ1.size() + circ2.size());
    for (int i = 0; i < circ3.size(); ++i) {
        if (i < circ1.size())
            QVERIFY(circ3.at(i) == circ1.at(i));
        else
            QVERIFY(circ3.at(i) == circ2.at(i - circ1.size()));
    }
}

void tst_QCircularBuffer::operatorPlusEquals()
{
    QCircularBuffer<int> circ(100);
    for (int i = 0; i < 10; ++i)
        circ.append(i + 1);

    // Add individual elements
    for (int i = 0; i < 10; ++i) {
        circ += i + 11;
        QVERIFY(circ.size() == i + 11);
        QVERIFY(circ.last() == i + 11);
    }

    // Add in an existing QCircularBuffer
    QCircularBuffer<int> circ2(10);
    for (int i = 0; i < 10; ++i)
        circ2.append(i + 21);
    circ += circ2;
    QVERIFY(circ.size() == 30);
    for (int i = 20; i < 30; ++i)
        QVERIFY(circ.at(i) == i + 1);

    // Add in a list
    QList<int> list;
    for (int i = 0; i < 10; ++i)
        list.append(i + 31);
    circ += list;
    QVERIFY(circ.size() == 40);
    for (int i = 30; i < 40; ++i)
        QVERIFY(circ.at(i) == i + 1);

    // Add in a vector
    QVector<int> vector;
    for (int i = 0; i < 10; ++i)
        vector.append(i + 41);
    circ += vector;
    QVERIFY(circ.size() == 50);
    for (int i = 40; i < 50; ++i)
        QVERIFY(circ.at(i) == i + 1);
}

void tst_QCircularBuffer::operatorStream()
{
    QCircularBuffer<int> circ(100);
    for (int i = 0; i < 10; ++i)
        circ.append(i + 1);

    // Add individual elements
    for (int i = 0; i < 10; ++i) {
        circ << i + 11;
        QVERIFY(circ.size() == i + 11);
        QVERIFY(circ.last() == i + 11);
    }

    // Add in an existing QCircularBuffer
    QCircularBuffer<int> circ2(10);
    for (int i = 0; i < 10; ++i)
        circ2.append(i + 21);
    circ << circ2;
    QVERIFY(circ.size() == 30);
    for (int i = 20; i < 30; ++i)
        QVERIFY(circ.at(i) == i + 1);

    // Add in a list
    QList<int> list;
    for (int i = 0; i < 10; ++i)
        list.append(i + 31);
    circ << list;
    QVERIFY(circ.size() == 40);
    for (int i = 30; i < 40; ++i)
        QVERIFY(circ.at(i) == i + 1);

    // Add in a vector
    QVector<int> vector;
    for (int i = 0; i < 10; ++i)
        vector.append(i + 41);
    circ << vector;
    QVERIFY(circ.size() == 50);
    for (int i = 40; i < 50; ++i)
        QVERIFY(circ.at(i) == i + 1);
}

void tst_QCircularBuffer::const_iterator()
{
    // Test basic iteration over a linearised buffer.
    // This tests operators !=, prefix ++, and *
    QCircularBuffer<int> circ(10);
    for (int i = 0; i < circ.capacity(); ++i)
        circ.append(i + 1);

    QCircularBuffer<int>::const_iterator it = circ.constBegin();
    int i = 0;
    while (it != circ.constEnd()) {
        QVERIFY(*it == circ.at(i++));
        ++it;
    }
    QVERIFY(i == circ.size());

    // Test basic iteration over a non-linearised buffer.
    // This tests operators !=, prefix ++, and *
    QCircularBuffer<int> circ2(10);
    for (int i = 0; i < 14; ++i)
        circ2.append(i + 1);

    QCircularBuffer<int>::const_iterator it2 = circ2.constBegin();
    i = 0;
    while (it2 != circ2.constEnd()) {
        QVERIFY(*it2 == circ2.at(i++));
        ++it2;
    }
    QVERIFY(i == circ2.size());

    // Reverse iteration
    while (it2 != circ2.constBegin()) {
        --it2;
        QVERIFY(*it2 == circ2.at(--i));
    }
    QVERIFY(i == 0);

    // Operator ==
    it = circ2.constBegin();
    it2 = circ2.constBegin();
    QVERIFY(it == it2);

    // Operator [int]
    for (int i = 0; i < circ2.size(); ++i)
        QVERIFY(it[ i ] == circ2.at(i));

    // Postfix operator ++
    QCircularBuffer<int>::const_iterator it3 = it2++;
    QVERIFY(it3 == it);
    QVERIFY(*it2 == circ2.at(1));

    // Comparison operators
    QVERIFY(it < it2);
    QVERIFY(it <= it2);
    QVERIFY(it <= it3);
    QVERIFY(it2 > it);
    QVERIFY(it2 >= it);
    QVERIFY(it3 >= it);

    // Postfix operator --
    it3 = it2--;
    QVERIFY(*it3 == circ2.at(1));
    QVERIFY(it2 == it);

    // Operator += and -=
    it2 += 1;
    QVERIFY(it2 == it3);
    it2 -= 1;
    QVERIFY(it2 == it);

    // Operator + (int) and operator - (int)
    QCircularBuffer<int>::const_iterator it4 = it + (circ2.size() - 1);
    QVERIFY(*it4 == circ2.last());
    QCircularBuffer<int>::const_iterator it5 = it4 - (circ2.size() - 1);
    QVERIFY(*it5 == circ2.first());

    // Operator - (const_iterator)
    QVERIFY(it4 - it5 == circ2.size() - 1);

    // Operator ->
    QCircularBuffer<MyComplexType> circ3(10);
    for (int i = 0; i < circ3.capacity(); ++i)
        circ3.append(i + 1);
    QCircularBuffer<MyComplexType>::const_iterator it6 = circ3.constBegin();
    for (int i = 0; i < circ2.capacity(); ++i)
    {
        QVERIFY(it6->x == circ3.at(i).x);
        ++it6;
    }
}

void tst_QCircularBuffer::iterator()
{
    // Test basic iteration over a linearised buffer.
    // This tests operators !=, prefix ++, and *
    QCircularBuffer<int> circ(10);
    for (int i = 0; i < circ.capacity(); ++i)
        circ.append(i + 1);

    QCircularBuffer<int>::iterator it = circ.begin();
    int i = 0;
    while (it != circ.end()) {
        QVERIFY(*it == circ.at(i++));
        ++it;
    }
    QVERIFY(i == circ.size());

    // Test basic iteration over a non-linearised buffer.
    // This tests operators !=, prefix ++, and *
    QCircularBuffer<int> circ2(10);
    for (int i = 0; i < 14; ++i)
        circ2.append(i + 1);

    QCircularBuffer<int>::iterator it2 = circ2.begin();
    i = 0;
    while (it2 != circ2.end()) {
        QVERIFY(*it2 == circ2.at(i++));
        ++it2;
    }
    QVERIFY(i == circ2.size());

    // Reverse iteration
    while (it2 != circ2.begin()) {
        --it2;
        QVERIFY(*it2 == circ2.at(--i));
    }
    QVERIFY(i == 0);

    // Operator ==
    it = circ2.begin();
    it2 = circ2.begin();
    QVERIFY(it == it2);

    // Operator [int]
    for (int i = 0; i < circ2.size(); ++i)
        QVERIFY(it[ i ] == circ2.at(i));

    // Postfix operator ++
    QCircularBuffer<int>::iterator it3 = it2++;
    QVERIFY(it3 == it);
    QVERIFY(*it2 == circ2.at(1));

    // Comparison operators
    QVERIFY(it < it2);
    QVERIFY(it <= it2);
    QVERIFY(it <= it3);
    QVERIFY(it2 > it);
    QVERIFY(it2 >= it);
    QVERIFY(it3 >= it);

    // Postfix operator --
    it3 = it2--;
    QVERIFY(*it3 == circ2.at(1));
    QVERIFY(it2 == it);

    // Operator += and -=
    it2 += 1;
    QVERIFY(it2 == it3);
    it2 -= 1;
    QVERIFY(it2 == it);

    // Operator + (int) and operator - (int)
    QCircularBuffer<int>::iterator it4 = it + (circ2.size() - 1);
    QVERIFY(*it4 == circ2.last());
    QCircularBuffer<int>::iterator it5 = it4 - (circ2.size() - 1);
    QVERIFY(*it5 == circ2.first());

    // Operator - (const_iterator)
    QVERIFY(it4 - it5 == circ2.size() - 1);

    // Operator ->
    QCircularBuffer<MyComplexType> circ3(10);
    for (int i = 0; i < circ3.capacity(); ++i)
        circ3.append(i + 1);
    QCircularBuffer<MyComplexType>::iterator it6 = circ3.begin();
    for (int i = 0; i < circ2.capacity(); ++i) {
        QVERIFY(it6->x == circ3.at(i).x);
        ++it6;
    }
}

void tst_QCircularBuffer::testAppendFifo()
{
    QCircularBuffer< float > c;
    c.setCapacity(5);
    c += QVector<float>(4, 0.0f);
    c.setCapacity(7);
    c += QVector<float>(4, 0.0f);
    c.remove(0, 2);
}

QTEST_APPLESS_MAIN(tst_QCircularBuffer)
#include "tst_qcircularbuffer.moc"
