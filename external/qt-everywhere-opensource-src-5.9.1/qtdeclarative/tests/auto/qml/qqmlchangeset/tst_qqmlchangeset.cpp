/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
#include <qtest.h>
#include <private/qqmlchangeset_p.h>

class tst_qqmlchangeset : public QObject
{
    Q_OBJECT
private slots:
    void sequence_data();
    void sequence();

    void apply_data();
    void apply();

    void removeConsecutive_data();
    void removeConsecutive();
    void insertConsecutive_data();
    void insertConsecutive();

    void copy();
    void debug();

    // These create random sequences and verify a list with the reordered changes applied is the
    // same as one with the unordered changes applied.
private:
    void random_data();
    void random();

public:
    enum {
        MoveOp = 0,
        InsertOp = -1,
        RemoveOp = -2
    };

    struct Signal
    {
        int index;
        int count;
        int to;
        int moveId;
        int offset;

        bool isInsert() const { return to == -1; }
        bool isRemove() const { return to == -2; }
        bool isChange() const { return to == -3; }
        bool isMove() const { return to >= 0; }
    };

    static Signal Insert(int index, int count) {
        Signal signal = { index, count, -1, -1, 0 }; return signal; }
    static Signal Insert(int index, int count, int moveId, int moveOffset) {
        Signal signal = { index, count, -1, moveId, moveOffset }; return signal; }
    static Signal Remove(int index, int count) {
        Signal signal = { index, count, -2, -1, 0 }; return signal; }
    static Signal Remove(int index, int count, int moveId, int moveOffset) {
        Signal signal = { index, count, -2, moveId, moveOffset }; return signal; }
    static Signal Move(int from, int to, int count, int moveId) {
        Signal signal = { from, count, to, moveId, 0 }; return signal; }
    static Signal Change(int index, int count) {
        Signal signal = { index, count, -3, -1, 0 }; return signal; }

    typedef QVector<Signal> SignalList;
    typedef QVector<SignalList> SignalListList;

    template<typename T>
    void move(int from, int to, int n, T *items)
    {
        if (from > to) {
            // Only move forwards - flip if backwards moving
            int tfrom = from;
            int tto = to;
            from = tto;
            to = tto+n;
            n = tfrom-tto;
        }

        T replaced;
        int i=0;
        typename T::ConstIterator it=items->begin(); it += from+n;
        for (; i<to-from; ++i,++it)
            replaced.append(*it);
        i=0;
        it=items->begin(); it += from;
        for (; i<n; ++i,++it)
            replaced.append(*it);
        typename T::ConstIterator f=replaced.begin();
        typename T::Iterator t=items->begin(); t += from;
        for (; f != replaced.end(); ++f, ++t)
            *t = *f;
    }

    bool applyChanges(QVector<int> &list, const QVector<QVector<Signal> > &changes)
    {
        foreach (const SignalList &sl, changes) {
            if (!applyChanges(list, sl))
                return false;
        }
        return true;
    }

    bool applyChanges(QVector<int> &list, const QVector<Signal> &changes)
    {
        QHash<QQmlChangeSet::MoveKey, int> removedValues;
        foreach (const Signal &signal, changes) {
            if (signal.isInsert()) {
                if (signal.index < 0 || signal.index > list.count()) {
                    qDebug() << "insert out of range" << signal.index << list.count();
                    return false;
                }
                if (signal.moveId != -1) {
                    QQmlChangeSet::Change insert(signal.index, signal.count, signal.moveId, signal.offset);
                    for (int i = insert.start(); i < insert.end(); ++i)
                        list.insert(i, removedValues.take(insert.moveKey(i)));
                } else {
                    list.insert(signal.index, signal.count, 100);
                }
            } else if (signal.isRemove()) {
                if (signal.index < 0 || signal.index + signal.count > list.count()) {
                    qDebug() << "remove out of range" << signal.index << signal.count << list.count();
                    return false;
                }
                if (signal.moveId != -1) {
                    QQmlChangeSet::Change remove(signal.index, signal.count, signal.moveId, signal.offset);
                    for (int i = remove.start(); i < remove.end(); ++i)
                        removedValues.insert(remove.moveKey(i), list.at(i));
                }
                list.erase(list.begin() + signal.index, list.begin() + signal.index + signal.count);
            } else if (signal.isMove()) {
                if (signal.index < 0
                        || signal.to < 0
                        || signal.index + signal.count > list.count()
                        || signal.to + signal.count > list.count()) {
                    qDebug() << "move out of range" << signal.index << signal.to << signal.count << list.count();
                    return false;
                }
                move(signal.index, signal.to, signal.count, &list);
            } else if (signal.isChange()) {
                for (int i = signal.index; i < signal.index + signal.count; ++i)
                    list[i] = 100;
            }
        }
        return true;
    }

};

bool operator ==(const tst_qqmlchangeset::Signal &left, const tst_qqmlchangeset::Signal &right)
{
    return left.index == right.index
            && left.count == right.count
            && left.to == right.to
            && left.moveId == right.moveId
            && (left.moveId == -1 || left.offset == right.offset);
}

QT_BEGIN_NAMESPACE
bool operator ==(const QQmlChangeSet::Change &left, const QQmlChangeSet::Change &right)
{
    return left.index == right.index && left.count == right.count && left.moveId == right.moveId;
}
QT_END_NAMESPACE

QDebug operator <<(QDebug debug, const tst_qqmlchangeset::Signal &signal)
{
    if (signal.isInsert() && signal.moveId == -1)
        debug.nospace() << "Insert(" << signal.index << "," << signal.count << ")";
    else if (signal.isInsert())
        debug.nospace() << "Insert(" << signal.index << "," << signal.count << "," << signal.moveId << "," << signal.offset << ")";
    else if (signal.isRemove() && signal.moveId == -1)
        debug.nospace() << "Remove(" << signal.index << "," << signal.count << ")";
    else if (signal.isRemove())
        debug.nospace() << "Remove(" << signal.index << "," << signal.count << "," << signal.moveId << "," << signal.offset << ")";
    else if (signal.isMove())
        debug.nospace() << "Move(" << signal.index << "," << signal.to << "," << signal.count << "," << signal.moveId << ")";
    else if (signal.isChange())
        debug.nospace() << "Change(" << signal.index << "," << signal.count << ")";
    return debug;
}

Q_DECLARE_METATYPE(tst_qqmlchangeset::SignalList)
Q_DECLARE_METATYPE(tst_qqmlchangeset::SignalListList)

#if 0
# define VERIFY_EXPECTED_OUTPUT \
    QVector<int> inputList; \
    for (int i = 0; i < 40; ++i) \
        inputList.append(i); \
    QVector<int> outputList = inputList; \
    if (!applyChanges(inputList, input)) { \
        qDebug() << input; \
        qDebug() << output; \
        qDebug() << changes; \
        QVERIFY(false); \
    } else if (!applyChanges(outputList, output)) { \
        qDebug() << input; \
        qDebug() << output; \
        qDebug() << changes; \
        QVERIFY(false); \
    } else if (outputList != inputList /* || changes != output*/) { \
        qDebug() << input; \
        qDebug() << output; \
        qDebug() << changes; \
        qDebug() << inputList; \
        qDebug() << outputList; \
        QVERIFY(false); \
    } else if (changes != output) { \
        qDebug() << output; \
        qDebug() << changes; \
        QCOMPARE(outputList, inputList); \
    }
#else
# define VERIFY_EXPECTED_OUTPUT \
    if (changes != output) { \
        qDebug() << output; \
        qDebug() << changes; \
    }
#endif

void tst_qqmlchangeset::sequence_data()
{
    QTest::addColumn<SignalList>("input");
    QTest::addColumn<SignalList>("output");

    // Insert
    QTest::newRow("i(12,5)")
            << (SignalList() << Insert(12,5))
            << (SignalList() << Insert(12,5));
    QTest::newRow("i(2,3),i(12,5)")
            << (SignalList() << Insert(2,3) << Insert(12,5))
            << (SignalList() << Insert(2,3) << Insert(12,5));
    QTest::newRow("i(12,5),i(2,3)")
            << (SignalList() << Insert(12,5) << Insert(2,3))
            << (SignalList() << Insert(2,3) << Insert(15,5));
    QTest::newRow("i(12,5),i(12,3)")
            << (SignalList() << Insert(12,5) << Insert(12,3))
            << (SignalList() << Insert(12,8));
    QTest::newRow("i(12,5),i(17,3)")
            << (SignalList() << Insert(12,5) << Insert(17,3))
            << (SignalList() << Insert(12,8));
    QTest::newRow("i(12,5),i(15,3)")
            << (SignalList() << Insert(12,5) << Insert(15,3))
            << (SignalList() << Insert(12,8));

    // Remove
    QTest::newRow("r(3,9)")
            << (SignalList() << Remove(3,9))
            << (SignalList() << Remove(3,9));
    QTest::newRow("r(3,4),r(3,2)")
            << (SignalList() << Remove(3,4) << Remove(3,2))
            << (SignalList() << Remove(3,6));
    QTest::newRow("r(4,3),r(14,5)")
            << (SignalList() << Remove(4,3) << Remove(14,5))
            << (SignalList() << Remove(4,3) << Remove(14,5));
    QTest::newRow("r(14,5),r(4,3)")
            << (SignalList() << Remove(14,5) << Remove(4,3))
            << (SignalList() << Remove(4,3) << Remove(11,5));
    QTest::newRow("r(4,3),r(2,9)")
            << (SignalList() << Remove(4,3) << Remove(2,9))
            << (SignalList() << Remove(2,12));

    // Move
    QTest::newRow("m(8-10,2)")
            << (SignalList() << Move(8,10,2,0))
            << (SignalList() << Remove(8,2,0,0) << Insert(10,2,0,0));

    QTest::newRow("m(23-12,6),m(13-15,5)")
            << (SignalList() << Move(23,12,6,0) << Move(13,15,5,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(12,1,0,0) << Insert(15,5,0,1));
    QTest::newRow("m(23-12,6),m(13-15,2)")
            << (SignalList() << Move(23,12,6,0) << Move(13,20,2,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(12,1,0,0) << Insert(13,3,0,3) << Insert(20,2,0,1));
    QTest::newRow("m(23-12,6),m(13-2,2)")
            << (SignalList() << Move(23,12,6,0) << Move(13,2,2,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(2,2,0,1) << Insert(14,1,0,0) << Insert(15,3,0,3));
    QTest::newRow("m(23-12,6),m(12-6,5)")
            << (SignalList() << Move(23,12,6,0) << Move(12,6,5,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(6,5,0,0) << Insert(17,1,0,5));
    QTest::newRow("m(23-12,6),m(10-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(10,5,4,1))
            << (SignalList() << Remove(10,2,1,0) << Remove(21,6,0,0) << Insert(5,2,1,0) << Insert(7,2,0,0) << Insert(14,4,0,2));
    QTest::newRow("m(23-12,6),m(16-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(16,5,4,1))
            << (SignalList() << Remove(12,2,1,2) << Remove(21,6,0,0) << Insert(5,2,0,4) << Insert(7,2,1,2) << Insert(16,4,0,0));
    QTest::newRow("m(23-12,6),m(13-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(13,5,4,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(5,4,0,1) << Insert(16,1,0,0) << Insert(17,1,0,5));
    QTest::newRow("m(23-12,6),m(14-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(14,5,4,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(5,4,0,2) << Insert(16,2,0,0));
    QTest::newRow("m(23-12,6),m(12-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(12,5,4,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(5,4,0,0) << Insert(16,2,0,4));
    QTest::newRow("m(23-12,6),m(11-5,8)")
            << (SignalList() << Move(23,12,6,0) << Move(11,5,8,1))
            << (SignalList() << Remove(11,1,1,0) << Remove(11,1,1,7) << Remove(21,6,0,0) << Insert(5,1,1,0) << Insert(6,6,0,0) << Insert(12,1,1,7));
    QTest::newRow("m(23-12,6),m(8-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(8,5,4,1))
            << (SignalList() << Remove(8,4,1,0) << Remove(19,6,0,0) << Insert(5,4,1,0) << Insert(12,6,0,0));
    QTest::newRow("m(23-12,6),m(2-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(2,5,4,1))
            << (SignalList() << Remove(2,4,1,0) << Remove(19,6,0,0) << Insert(5,4,1,0) << Insert(12,6,0,0));
    QTest::newRow("m(23-12,6),m(18-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(18,5,4,1))
            << (SignalList() << Remove(12,4,1,0) << Remove(19,6,0,0) << Insert(5,4,1,0) << Insert(16,6,0,0));
    QTest::newRow("m(23-12,6),m(20-5,4)")
            << (SignalList() << Move(23,12,6,0) << Move(20,5,4,1))
            << (SignalList() << Remove(14,4,1,0) << Remove(19,6,0,0) << Insert(5,4,1,0) << Insert(16,6,0,0));

    QTest::newRow("m(23-12,6),m(5-13,11)")
            << (SignalList() << Move(23,12,6,0) << Move(5,13,11,1))
            << (SignalList() << Remove(5,7,1,0) << Remove(16,6,0,0) << Insert(5,2,0,4) << Insert(13,7,1,0) << Insert(20,4,0,0));

    QTest::newRow("m(23-12,6),m(12-23,6)")
            << (SignalList() << Move(23,12,6,0) << Move(12,23,6,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(23,6,0,0));  // ### These cancel out.
    QTest::newRow("m(23-12,6),m(10-23,4)")
            << (SignalList() << Move(23,12,6,0) << Move(10,23,4,1))
            << (SignalList() << Remove(10,2,1,0) << Remove(21,6,0,0) << Insert(10,4,0,2) << Insert(23,2,1,0) << Insert(25,2,0,0));
    QTest::newRow("m(23-12,6),m(16-23.4)")
            << (SignalList() << Move(23,12,6,0) << Move(16,23,4,1))
            << (SignalList() << Remove(12,2,1,2) << Remove(21,6,0,0) << Insert(12,4,0,0) << Insert(23,2,0,4) << Insert(25,2,1,2));
    QTest::newRow("m(23-12,6),m(13-23,4)")
            << (SignalList() << Move(23,12,6,0) << Move(13,23,4,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(12,1,0,0) << Insert(13,1,0,5) << Insert(23,4,0,1));
    QTest::newRow("m(23-12,6),m(14-23,)")
            << (SignalList() << Move(23,12,6,0) << Move(14,23,4,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(12,2,0,0) << Insert(23,4,0,2));
    QTest::newRow("m(23-12,6),m(12-23,4)")
            << (SignalList() << Move(23,12,6,0) << Move(12,23,4,1))
            << (SignalList() << Remove(23,6,0,0) << Insert(12,2,0,4) << Insert(23,4,0,0));
    QTest::newRow("m(23-12,6),m(11-23,8)")
            << (SignalList() << Move(23,12,6,0) << Move(11,23,8,1))
            << (SignalList() << Remove(11,1,1,0) << Remove(11,1,1,7) << Remove(21,6,0,0) << Insert(23,1,1,0) << Insert(24,6,0,0) << Insert(30,1,1,7));
    QTest::newRow("m(23-12,6),m(8-23,4)")
            << (SignalList() << Move(23,12,6,0) << Move(8,23,4,1))
            << (SignalList() << Remove(8,4,1,0) << Remove(19,6,0,0) << Insert(8,6,0,0) << Insert(23,4,1,0));
    QTest::newRow("m(23-12,6),m(2-23,4)")
            << (SignalList() << Move(23,12,6,0) << Move(2,23,4,1))
            << (SignalList() << Remove(2,4,1,0) << Remove(19,6,0,0) << Insert(8,6,0,0) << Insert(23,4,1,0));
    QTest::newRow("m(23-12,6),m(18-23,4)")
            << (SignalList() << Move(23,12,6,0) << Move(18,23,4,1))
            << (SignalList() << Remove(12,4,1,0) << Remove(19,6,0,0) << Insert(12,6,0,0) << Insert(23,4,1,0));
    QTest::newRow("m(23-12,6),m(20-23,4)")
            << (SignalList() << Move(23,12,6,0) << Move(20,23,4,1))
            << (SignalList() << Remove(14,4,1,0) << Remove(19,6,0,0) << Insert(12,6,0,0) << Insert(23,4,1,0));

    QTest::newRow("m(23-12,6),m(11-23,10)")
            << (SignalList() << Move(23,12,6,0) << Move(11,23,10,1))
            << (SignalList() << Remove(11,1,1,0) << Remove(11,3,1,7) << Remove(19,6,0,0) << Insert(23,1,1,0) << Insert(24,6,0,0) << Insert(30,3,1,7));

    QTest::newRow("m(3-9,12),m(13-5,12)")
            << (SignalList() << Move(3,9,12,0) << Move(13,15,5,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,4,0,0) << Insert(13,2,0,9) << Insert(15,5,0,4) << Insert(20,1,0,11));
    QTest::newRow("m(3-9,12),m(13-15,20)")
            << (SignalList() << Move(3,9,12,0) << Move(13,15,20,1))
            << (SignalList() << Remove(3,12,0,0) << Remove(9,12,1,8) << Insert(9,4,0,0) << Insert(15,8,0,4) << Insert(23,12,1,8));
    QTest::newRow("m(3-9,12),m(13-15,2)")
            << (SignalList() << Move(3,9,12,0) << Move(13,15,2,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,4,0,0) << Insert(13,2,0,6) << Insert(15,2,0,4) << Insert(17,4,0,8));
    QTest::newRow("m(3-9,12),m(12-5,6)")
            << (SignalList() << Move(3,9,12,0) << Move(12,5,6,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(5,6,0,3) << Insert(15,3,0,0) << Insert(18,3,0,9));
    QTest::newRow("m(3-9,12),m(10-14,5)")
            << (SignalList() << Move(3,9,12,0) << Move(10,14,5,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,1,0,0) << Insert(10,4,0,6) << Insert(14,5,0,1) << Insert(19,2,0,10));
    QTest::newRow("m(3-9,12),m(16-20,5)")
            << (SignalList() << Move(3,9,12,0) << Move(16,20,5,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,7,0,0) << Insert(20,5,0,7));
    QTest::newRow("m(3-9,12),m(13-17,5)")
            << (SignalList() << Move(3,9,12,0) << Move(13,17,5,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,4,0,0) << Insert(13,3,0,9) << Insert(17,5,0,4));
    QTest::newRow("m(3-9,12),m(14-18,5)")
            << (SignalList() << Move(3,9,12,0) << Move(14,18,5,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,5,0,0) << Insert(14,2,0,10) << Insert(18,5,0,5));
    QTest::newRow("m(3-9,12),m(12-16,5)")
            << (SignalList() << Move(3,9,12,0) << Move(12,16,5,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,3,0,0) << Insert(12,4,0,8) << Insert(16,5,0,3));
    QTest::newRow("m(3-9,12),m(11-19,5)")
            << (SignalList() << Move(3,9,12,0) << Move(11,19,5,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,2,0,0) << Insert(11,5,0,7) << Insert(19,5,0,2));
    QTest::newRow("m(3-9,12),m(8-12,5)")
            << (SignalList() << Move(3,9,12,0) << Move(8,12,5,1))
            << (SignalList() << Remove(3,12,0,0) << Remove(8,1,1,0) << Insert(8,4,0,4) << Insert(12,1,1,0) << Insert(13,4,0,0) << Insert(17,4,0,8));
    QTest::newRow("m(3-9,12),m(2-6,5)")
            << (SignalList() << Move(3,9,12,0) << Move(2,6,5,1))
            << (SignalList() <<  Remove(2,1,1,0) << Remove(2,12,0,0) << Remove(2,4,1,1) << Insert(4,2,0,0) << Insert(6,5,1,0) << Insert(11,10,0,2));
    QTest::newRow("m(3-9,12),m(18-22,5)")
            << (SignalList() << Move(3,9,12,0) << Move(18,22,5,1))
            << (SignalList() << Remove(3,12,0,0) << Remove(9,2,1,3) << Insert(9,9,0,0) << Insert(22,3,0,9) << Insert(25,2,1,3));
    QTest::newRow("m(3-9,12),m(20-24,5)")
            << (SignalList() << Move(3,9,12,0) << Move(20,24,5,1))
            << (SignalList() << Remove(3,12,0,0) << Remove(9,4,1,1) << Insert(9,11,0,0) << Insert(24,1,0,11) << Insert(25,4,1,1));

    QTest::newRow("m(3-9,12),m(5-11,8)")
            << (SignalList() << Move(3,9,12,0) << Move(5,11,8,1))
            << (SignalList() << Remove(3,12,0,0) << Remove(5,4,1,0) << Insert(5,6,0,4) << Insert(11,4,1,0) << Insert(15,4,0,0) << Insert(19,2,0,10));

    QTest::newRow("m(3-9,12),m(12-23,6)")
            << (SignalList() << Move(3,9,12,0) << Move(12,23,6,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,3,0,0) << Insert(12,3,0,9) << Insert(23,6,0,3));
    QTest::newRow("m(3-9,12),m(10-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(10,23,4,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,1,0,0) << Insert(10,7,0,5) << Insert(23,4,0,1));
    QTest::newRow("m(3-9,12),m(16-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(16,23,4,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,7,0,0) << Insert(16,1,0,11) << Insert(23,4,0,7));
    QTest::newRow("m(3-9,12),m(13-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(13,23,4,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,4,0,0) << Insert(13,4,0,8) << Insert(23,4,0,4));
    QTest::newRow("m(3-9,12),m(14-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(14,23,4,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,5,0,0) << Insert(14,3,0,9) << Insert(23,4,0,5));
    QTest::newRow("m(3-9,12),m(12-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(12,23,4,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,3,0,0) << Insert(12,5,0,7) << Insert(23,4,0,3));
    QTest::newRow("m(3-9,12),m(11-23,8)")
            << (SignalList() << Move(3,9,12,0) << Move(11,23,8,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,2,0,0) << Insert(11,2,0,10) << Insert(23,8,0,2));
    QTest::newRow("m(3-9,12),m(8-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(8,23,4,1))
            << (SignalList() << Remove(3,12,0,0) << Remove(8,1,1,0) << Insert(8,9,0,3) << Insert(23,1,1,0) << Insert(24,3,0,0));
    QTest::newRow("m(3-9,12),m(2-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(2,23,4,1))
            << (SignalList() << Remove(2,1,1,0) << Remove(2,12,0,0) << Remove(2,3,1,1) << Insert(5,12,0,0) << Insert(23,4,1,0));
    QTest::newRow("m(3-9,12),m(18-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(18,23,4,1))
            << (SignalList() << Remove(3,12,0,0) << Remove(9,1,1,3) << Insert(9,9,0,0) << Insert(23,3,0,9) << Insert(26,1,1,3));
    QTest::newRow("m(3-9,12),m(20-23,4)")
            << (SignalList() << Move(3,9,12,0) << Move(20,23,4,1))
            << (SignalList() << Remove(3,12,0,0) << Remove(9,3,1,1) << Insert(9,11,0,0) << Insert(23,1,0,11) << Insert(24,3,1,1));

    QTest::newRow("m(3-9,12),m(11-23,10)")
            << (SignalList() << Move(3,9,12,0) << Move(11,23,10,1))
            << (SignalList() << Remove(3,12,0,0) << Insert(9,2,0,0) << Insert(23,10,0,2));

    // Change
    QTest::newRow("c(4,5)")
            << (SignalList() << Change(4,5))
            << (SignalList() << Change(4,5));
    QTest::newRow("c(4,5),c(12,2)")
            << (SignalList() << Change(4,5) << Change(12,2))
            << (SignalList() << Change(4,5) << Change(12,2));
    QTest::newRow("c(12,2),c(4,5)")
            << (SignalList() << Change(12,2) << Change(4,5))
            << (SignalList() << Change(4,5) << Change(12,2));
    QTest::newRow("c(4,5),c(2,2)")
            << (SignalList() << Change(4,5) << Change(2,2))
            << (SignalList() << Change(2,7));
    QTest::newRow("c(4,5),c(9,2)")
            << (SignalList() << Change(4,5) << Change(9,2))
            << (SignalList() << Change(4,7));
    QTest::newRow("c(4,5),c(3,2)")
            << (SignalList() << Change(4,5) << Change(3,2))
            << (SignalList() << Change(3,6));
    QTest::newRow("c(4,5),c(8,2)")
            << (SignalList() << Change(4,5) << Change(8,2))
            << (SignalList() << Change(4,6));
    QTest::newRow("c(4,5),c(3,2)")
            << (SignalList() << Change(4,5) << Change(3,2))
            << (SignalList() << Change(3,6));
    QTest::newRow("c(4,5),c(2,9)")
            << (SignalList() << Change(4,5) << Change(2,9))
            << (SignalList() << Change(2,9));
    QTest::newRow("c(4,5),c(12,3),c(8,6)")
            << (SignalList() << Change(4,5) << Change(12,3) << Change(8,6))
            << (SignalList() << Change(4,11));

    // Insert,then remove.
    QTest::newRow("i(12,6),r(12,6)")
            << (SignalList() << Insert(12,6) << Remove(12,6))
            << (SignalList());
    QTest::newRow("i(12,6),r(10,4)")
            << (SignalList() << Insert(12,6) << Remove(10,4))
            << (SignalList() << Remove(10,2) << Insert(10,4));
    QTest::newRow("i(12,6),r(16,4)")
            << (SignalList() << Insert(12,6) << Remove(16,4))
            << (SignalList() << Remove(12,2) << Insert(12,4));
    QTest::newRow("i(12,6),r(13,4)")
            << (SignalList() << Insert(12,6) << Remove(13,4))
            << (SignalList() << Insert(12,2));
    QTest::newRow("i(12,6),r(14,4)")
            << (SignalList() << Insert(12,6) << Remove(14,4))
            << (SignalList() << Insert(12,2));
    QTest::newRow("i(12,6),r(12,4)")
            << (SignalList() << Insert(12,6) << Remove(12,4))
            << (SignalList() << Insert(12,2));
    QTest::newRow("i(12,6),r(11,8)")
            << (SignalList() << Insert(12,6) << Remove(11,8))
            << (SignalList() << Remove(11,2));
    QTest::newRow("i(12,6),r(8,4)")
            << (SignalList() << Insert(12,6) << Remove(8,4))
            << (SignalList() << Remove(8,4) << Insert(8,6));
    QTest::newRow("i(12,6),r(2,4)")
            << (SignalList() << Insert(12,6) << Remove(2,4))
            << (SignalList() << Remove(2,4) << Insert(8,6));
    QTest::newRow("i(12,6),r(18,4)")
            << (SignalList() << Insert(12,6) << Remove(18,4))
            << (SignalList() << Remove(12,4) << Insert(12,6));
    QTest::newRow("i(12,6),r(20,4)")
            << (SignalList() << Insert(12,6) << Remove(20,4))
            << (SignalList() << Remove(14,4) << Insert(12,6));

    // Insert,then change
    QTest::newRow("i(12,6),c(12,6)")
            << (SignalList() << Insert(12,6) << Change(12,6))
            << (SignalList() << Insert(12,6));
    QTest::newRow("i(12,6),c(10,6)")
            << (SignalList() << Insert(12,6) << Change(10,6))
            << (SignalList() << Insert(12,6) << Change(10,2));
    QTest::newRow("i(12,6),c(16,4)")
            << (SignalList() << Insert(12,6) << Change(16,4))
            << (SignalList() << Insert(12,6) << Change(18,2));
    QTest::newRow("i(12,6),c(13,4)")
            << (SignalList() << Insert(12,6) << Change(13,4))
            << (SignalList() << Insert(12,6));
    QTest::newRow("i(12,6),c(14,4)")
            << (SignalList() << Insert(12,6) << Change(14,4))
            << (SignalList() << Insert(12,6));
    QTest::newRow("i(12,6),c(12,4)")
            << (SignalList() << Insert(12,6) << Change(12,4))
            << (SignalList() << Insert(12,6));
    QTest::newRow("i(12,6),c(11,8)")
            << (SignalList() << Insert(12,6) << Change(11,8))
            << (SignalList() << Insert(12,6) << Change(11,1) << Change(18,1));
    QTest::newRow("i(12,6),c(8,4)")
            << (SignalList() << Insert(12,6) << Change(8,4))
            << (SignalList() << Insert(12,6) << Change(8,4));
    QTest::newRow("i(12,6),c(2,4)")
            << (SignalList() << Insert(12,6) << Change(2,4))
            << (SignalList() << Insert(12,6) << Change(2,4));
    QTest::newRow("i(12,6),c(18,4)")
            << (SignalList() << Insert(12,6) << Change(18,4))
            << (SignalList() << Insert(12,6) << Change(18,4));
    QTest::newRow("i(12,6),c(20,4)")
            << (SignalList() << Insert(12,6) << Change(20,4))
            << (SignalList() << Insert(12,6) << Change(20,4));

    // Insert,then move
    QTest::newRow("i(12,6),m(12-5,6)")
            << (SignalList() << Insert(12,6) << Move(12,5,6,0))
            << (SignalList() << Insert(5,6));
    QTest::newRow("i(12,6),m(10-5,4)")
            << (SignalList() << Insert(12,6) << Move(10,5,4,0))
            << (SignalList() << Remove(10,2,0,0) << Insert(5,2,0,0) << Insert(7,2) << Insert(14,4));
    QTest::newRow("i(12,6),m(16-5,4)")
            << (SignalList() << Insert(12,6) << Move(16,5,4,0))
            << (SignalList() << Remove(12,2,0,2) << Insert(5,2) << Insert(7,2,0,2) << Insert(16,4));
    QTest::newRow("i(12,6),m(13-5,4)")
            << (SignalList() << Insert(12,6) << Move(13,5,4,0))
            << (SignalList() << Insert(5,4) << Insert(16,2));
    QTest::newRow("i(12,6),m(14-5,4)")
            << (SignalList() << Insert(12,6) << Move(14,5,4,0))
            << (SignalList() << Insert(5,4) << Insert(16,2));
    QTest::newRow("i(12,6),m(12-5,4)")
            << (SignalList() << Insert(12,6) << Move(12,5,4,0))
            << (SignalList() << Insert(5,4) << Insert(16,2));
    QTest::newRow("i(12,6),m(11-5,8)")
            << (SignalList() << Insert(12,6) << Move(11,5,8,0))
            << (SignalList() << Remove(11,1,0,0) << Remove(11,1,0,7) << Insert(5,1,0,0) << Insert(6,6) << Insert(12,1,0,7));
    QTest::newRow("i(12,6),m(8-5,4)")
            << (SignalList() << Insert(12,6) << Move(8,5,4,0))
            << (SignalList() << Remove(8,4,0,0) << Insert(5,4,0,0) << Insert(12,6));
    QTest::newRow("i(12,6),m(2-5,4)")
            << (SignalList() << Insert(12,6) << Move(2,5,4,0))
            << (SignalList() << Remove(2,4,0,0) << Insert(5,4,0,0) << Insert(12,6));
    QTest::newRow("i(12,6),m(18-5,4)")
            << (SignalList() << Insert(12,6) << Move(18,5,4,0))
            << (SignalList() << Remove(12,4,0,0) << Insert(5,4,0,0) << Insert(16,6));
    QTest::newRow("i(12,6),m(20-5,4)")
            << (SignalList() << Insert(12,6) << Move(20,5,4,0))
            << (SignalList() << Remove(14,4,0,0) << Insert(5,4,0,0) << Insert(16,6));

    QTest::newRow("i(12,6),m(5-13,11)")
            << (SignalList() << Insert(12,6) << Move(5,11,8,0))
            << (SignalList() << Remove(5,7,0,0) << Insert(5,5) << Insert(11,7,0,0) << Insert(18,1));

    QTest::newRow("i(12,6),m(12-23,6)")
            << (SignalList() << Insert(12,6) << Move(12,23,6,0))
            << (SignalList() << Insert(23,6));
    QTest::newRow("i(12,6),m(10-23,4)")
            << (SignalList() << Insert(12,6) << Move(10,23,4,0))
            << (SignalList() << Remove(10,2,0,0) << Insert(10,4) << Insert(23,2,0,0) << Insert(25,2));
    QTest::newRow("i(12,6),m(16-23,4)")
            << (SignalList() << Insert(12,6) << Move(16,23,4,0))
            << (SignalList() << Remove(12,2,0,2) << Insert(12,4) << Insert(23,2) << Insert(25,2,0,2));
    QTest::newRow("i(12,6),m(13-23,4)")
            << (SignalList() << Insert(12,6) << Move(13,23,4,0))
            << (SignalList() << Insert(12,2) << Insert(23,4));
    QTest::newRow("i(12,6),m(14-23,4)")
            << (SignalList() << Insert(12,6) << Move(14,23,4,0))
            << (SignalList() << Insert(12,2) << Insert(23,4));
    QTest::newRow("i(12,6),m(12-23,4)")
            << (SignalList() << Insert(12,6) << Move(12,23,4,0))
            << (SignalList() << Insert(12,2) << Insert(23,4));
    QTest::newRow("i(12,6),m(11-23,8)")
            << (SignalList() << Insert(12,6) << Move(11,23,8,0))
            << (SignalList() << Remove(11,1,0,0) << Remove(11,1,0,7) << Insert(23,1,0,0)<< Insert(24,6) << Insert(30,1,0,7));
    QTest::newRow("i(12,6),m(8-23,4)")
            << (SignalList() << Insert(12,6) << Move(8,23,4,0))
            << (SignalList() << Remove(8,4,0,0) << Insert(8,6) << Insert(23,4,0,0));
    QTest::newRow("i(12,6),m(2-23,4)")
            << (SignalList() << Insert(12,6) << Move(2,23,4,0))
            << (SignalList() << Remove(2,4,0,0) << Insert(8,6) << Insert(23,4,0,0));
    QTest::newRow("i(12,6),m(18-23,4)")
            << (SignalList() << Insert(12,6) << Move(18,23,4,0))
            << (SignalList() << Remove(12,4,0,0) << Insert(12,6) << Insert(23,4,0,0));
    QTest::newRow("i(12,6),m(20-23,4)")
            << (SignalList() << Insert(12,6) << Move(20,23,4,0))
            << (SignalList() << Remove(14,4,0,0) << Insert(12,6) << Insert(23,4,0,0));

    QTest::newRow("i(12,6),m(11-23,10)")
            << (SignalList() << Insert(12,6) << Move(11,23,10,0))
            << (SignalList() << Remove(11,1,0,0) << Remove(11,3,0,7) << Insert(23,1,0,0) << Insert(24,6) << Insert(30,3,0,7));

    // Remove,then insert
    QTest::newRow("r(12,6),i(12,6)")
            << (SignalList() << Remove(12,6) << Insert(12,6))
            << (SignalList() << Remove(12,6) << Insert(12,6));
    QTest::newRow("r(12,6),i(10,4)")
            << (SignalList() << Remove(12,6) << Insert(10,14))
            << (SignalList() << Remove(12,6) << Insert(10,14));
    QTest::newRow("r(12,6),i(16,4)")
            << (SignalList() << Remove(12,6) << Insert(16,4))
            << (SignalList() << Remove(12,6) << Insert(16,4));
    QTest::newRow("r(12,6),i(13,4)")
            << (SignalList() << Remove(12,6) << Insert(13,4))
            << (SignalList() << Remove(12,6) << Insert(13,4));
    QTest::newRow("r(12,6),i(14,4)")
            << (SignalList() << Remove(12,6) << Insert(14,4))
            << (SignalList() << Remove(12,6) << Insert(14,4));
    QTest::newRow("r(12,6),i(12,4)")
            << (SignalList() << Remove(12,6) << Insert(12,4))
            << (SignalList() << Remove(12,6) << Insert(12,4));
    QTest::newRow("r(12,6),i(11,8)")
            << (SignalList() << Remove(12,6) << Insert(11,8))
            << (SignalList() << Remove(12,6) << Insert(11,8));
    QTest::newRow("r(12,6),i(8,4)")
            << (SignalList() << Remove(12,6) << Insert(8,4))
            << (SignalList() << Remove(12,6) << Insert(8,4));
    QTest::newRow("r(12,6),i(2,4)")
            << (SignalList() << Remove(12,6) << Insert(2,4))
            << (SignalList() << Remove(12,6) << Insert(2,4));
    QTest::newRow("r(12,6),i(18,4)")
            << (SignalList() << Remove(12,6) << Insert(18,4))
            << (SignalList() << Remove(12,6) << Insert(18,4));
    QTest::newRow("r(12,6),i(20,4)")
            << (SignalList() << Remove(12,6) << Insert(20,4))
            << (SignalList() << Remove(12,6) << Insert(20,4));

    // Move,then insert
    QTest::newRow("m(12-5,6),i(12,6)")
            << (SignalList() << Move(12,5,6,0) << Insert(12,6))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,6,0,0) << Insert(12,6));
    QTest::newRow("m(12-5,6),i(10,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(10,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,5,0,0) << Insert(10,4) << Insert(14,1,0,5));
    QTest::newRow("m(12-5,6),i(16,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(16,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,6,0,0) << Insert(16,4));
    QTest::newRow("m(12-5,6),i(13,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(13,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,6,0,0) << Insert(13,4));
    QTest::newRow("m(12-5,6),i(14,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(14,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,6,0,0) << Insert(14,4));
    QTest::newRow("m(12-5,6),i(12,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(12,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,6,0,0) << Insert(12,4));
    QTest::newRow("m(12-5,6),i(11,8)")
            << (SignalList() << Move(12,5,6,0) << Insert(11,8))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,6,0,0) << Insert(11,8));
    QTest::newRow("m(12-5,6),i(8,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(8,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,3,0,0) << Insert(8,4) << Insert(12,3,0,3));
    QTest::newRow("m(12-5,6),i(2,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(2,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(2,4) << Insert(9,6,0,0));
    QTest::newRow("m(12-5,6),i(18,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(18,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,6,0,0) << Insert(18,4));
    QTest::newRow("m(12-5,6),i(20,4)")
            << (SignalList() << Move(12,5,6,0) << Insert(20,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,6,0,0) << Insert(20,4));

    QTest::newRow("m(12-23,6),i(12,6)")
            << (SignalList() << Move(12,23,6,0) << Insert(12,6))
            << (SignalList() << Remove(12,6,0,0) << Insert(12,6) << Insert(29,6,0,0));
    QTest::newRow("m(12-23,6),i(10,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(10,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(10,4) << Insert(27,6,0,0));
    QTest::newRow("m(12-23,6),i(16,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(16,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(16,4) << Insert(27,6,0,0));
    QTest::newRow("m(12-23,6),i(13,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(13,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(13,4) << Insert(27,6,0,0));
    QTest::newRow("m(12-23,6),i(14,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(14,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(14,4) << Insert(27,6,0,0));
    QTest::newRow("m(12-23,6),i(12,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(12,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(12,4) << Insert(27,6,0,0));
    QTest::newRow("m(12-23,6),i(11,8)")
            << (SignalList() << Move(12,23,6,0) << Insert(11,8))
            << (SignalList() << Remove(12,6,0,0) << Insert(11,8) << Insert(31,6,0,0));
    QTest::newRow("m(12-23,6),i(8,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(8,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(8,4) << Insert(27,6,0,0));
    QTest::newRow("m(12-23,6),i(2,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(2,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(2,4) << Insert(27,6,0,0));
    QTest::newRow("m(12-23,6),i(18,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(18,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(18,4) << Insert(27,6,0,0));
    QTest::newRow("m(12-23,6),i(20,4)")
            << (SignalList() << Move(12,23,6,0) << Insert(20,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(20,4) << Insert(27,6,0,0));

    // Move,then remove
    QTest::newRow("m(12-5,6),r(12,6)")
            << (SignalList() << Move(12,5,6,0) << Remove(12,6))
            << (SignalList() << Remove(6,6) << Remove(6,6,0,0) << Insert(5,6,0,0));
    QTest::newRow("m(12-5,6),r(10,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(10,4)) // ###
            << (SignalList() << Remove(5,3) << Remove(9,6,0,0) << Insert(5,5,0,0));
    QTest::newRow("m(12-5,6),r(16,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(16,4))
            << (SignalList() << Remove(10,2) << Remove(10,6,0,0) << Remove(10,2) << Insert(5,6,0,0));
    QTest::newRow("m(12-5,6),r(13,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(13,4))
            << (SignalList() << Remove(7,4) << Remove(8,6,0,0) << Insert(5,6,0,0));
    QTest::newRow("m(12-5,6),r(14,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(14,4))
            << (SignalList() << Remove(8,4) << Remove(8,6,0,0) << Insert(5,6,0,0));
    QTest::newRow("m(12-5,6),r(12,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(12,4))
            << (SignalList() << Remove(6,4) << Remove(8,6,0,0) << Insert(5,6,0,0));
    QTest::newRow("m(12-5,6),r(11,8)")
            << (SignalList() << Move(12,5,6,0) << Remove(11,8))
            << (SignalList() << Remove(5,7) << Remove(5,6,0,0) << Remove(5,1) << Insert(5,6,0,0));
    QTest::newRow("m(12-5,6),r(8,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(8,4)) // ###
            << (SignalList() << Remove(5,1) << Remove(11,6,0,0) << Insert(5,3,0,0));
    QTest::newRow("m(12-5,6),r(2,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(2,4))
            << (SignalList() << Remove(2,3) << Remove(9,6,0,0) << Insert(2,5,0,1));
    QTest::newRow("m(12-5,6),r(6,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(6,4))
            << (SignalList() << Remove(12,6,0,0) << Insert(5,1,0,0) << Insert(6,1,0,5));
    QTest::newRow("m(12-5,6),r(18,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(18,4))
            << (SignalList() << Remove(12,6,0,0) << Remove(12,4) << Insert(5,6,0,0));
    QTest::newRow("m(12-5,6),r(20,4)")
            << (SignalList() << Move(12,5,6,0) << Remove(20,4))
            << (SignalList() << Remove(12,6,0,0) << Remove(14,4) << Insert(5,6,0,0));

    QTest::newRow("m(12-23,6),r(12,6)")
            << (SignalList() << Move(12,23,6,0) << Remove(12,6))
            << (SignalList() << Remove(12,6,0,0) << Remove(12,6) << Insert(17,6,0,0));
    QTest::newRow("m(12-23,6),r(10,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(10,4))
            << (SignalList() << Remove(10,2) << Remove(10,6,0,0) << Remove(10,2) << Insert(19,6,0,0));
    QTest::newRow("m(12-23,6),r(16,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(16,4))
            << (SignalList() << Remove(12,6,0,0) << Remove(16,4) << Insert(19,6,0,0));
    QTest::newRow("m(12-23,6),r(13,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(13,4))
            << (SignalList() << Remove(12,6,0,0) << Remove(13,4) << Insert(19,6,0,0));
    QTest::newRow("m(12-23,6),r(14,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(14,4))
            << (SignalList() << Remove(12,6,0,0) << Remove(14,4) << Insert(19,6,0,0));
    QTest::newRow("m(12-23,6),r(12,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(12,4))
            << (SignalList() << Remove(12,6,0,0) << Remove(12,4) << Insert(19,6,0,0));
    QTest::newRow("m(12-23,6),r(11,8)")
            << (SignalList() << Move(12,23,6,0) << Remove(11,8))
            << (SignalList() << Remove(11,1) << Remove(11,6,0,0) << Remove(11,7) << Insert(15,6,0,0));
    QTest::newRow("m(12-23,6),r(8,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(8,4))
            << (SignalList() << Remove(8,4) << Remove(8,6,0,0) << Insert(19,6,0,0));
    QTest::newRow("m(12-23,6),r(2,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(2,4))
            << (SignalList() << Remove(2,4) << Remove(8,6,0,0) << Insert(19,6,0,0));
    QTest::newRow("m(12-23,6),r(18,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(18,4))
            << (SignalList() << Remove(12,6,0,0) << Remove(18,4) << Insert(19,6,0,0));
    QTest::newRow("m(12-23,6),r(20,4)")
            << (SignalList() << Move(12,23,6,0) << Remove(20,4))
            << (SignalList() << Remove(12,6,0,0) << Remove(20,3) << Insert(20,5,0,1));


    // Complex
    QTest::newRow("r(15,1),r(22,1)")
            << (SignalList() << Remove(15,1) << Remove(22,1))
            << (SignalList() << Remove(15,1) << Remove(22,1));
    QTest::newRow("r(15,1),r(22,1),r(25,1)")
            << (SignalList() << Remove(15,1) << Remove(22,1) << Remove(25,1))
            << (SignalList() << Remove(15,1) << Remove(22,1) << Remove(25,1));
    QTest::newRow("r(15,1),r(22,1),r(25,1),r(15,1)")
            << (SignalList() << Remove(15,1) << Remove(22,1) << Remove(25,1) << Remove(15,1))
            << (SignalList() << Remove(15,2) << Remove(21,1) << Remove(24,1));
    QTest::newRow("r(15,1),r(22,1),r(25,1),r(15,1),r(13,1)")
            << (SignalList() << Remove(15,1) << Remove(22,1) << Remove(25,1) << Remove(15,1) << Remove(13,1))
            << (SignalList() << Remove(13,1) << Remove(14,2) << Remove(20,1) << Remove(23,1));
    QTest::newRow("r(15,1),r(22,1),r(25,1),r(15,1),r(13,1),r(13,1)")
            << (SignalList() << Remove(15,1) << Remove(22,1) << Remove(25,1) << Remove(15,1) << Remove(13,1) << Remove(13,1))
            << (SignalList() << Remove(13,4) << Remove(19,1) << Remove(22,1));
    QTest::newRow("r(15,1),r(22,1),r(25,1),r(15,1),r(13,1),r(13,1),m(12,13,1)")
            << (SignalList() << Remove(15,1) << Remove(22,1) << Remove(25,1) << Remove(15,1) << Remove(13,1) << Remove(13,1) << Move(12,13,1,0))
            << (SignalList() << Remove(12,1,0,0) << Remove(12,4) << Remove(18,1) << Remove(21,1) << Insert(13,1,0,0));

    QTest::newRow("r(12,18),r(3,0)")
            << (SignalList() << Remove(12,18) << Remove(3,0))
            << (SignalList() << Remove(12,18));
    QTest::newRow("r(12,18),r(3,0),r(1,2)")
            << (SignalList() << Remove(12,18) << Remove(3,0) << Remove(1,2))
            << (SignalList() << Remove(1,2) << Remove(10,18));
    QTest::newRow("r(12,18),r(3,0),r(1,2),m(4,0,11)")
            << (SignalList() << Remove(12,18) << Remove(3,0) << Remove(1,2) << Move(4,0,11,0))
            << (SignalList() << Remove(1,2) << Remove(4,6,0,0) << Remove(4,18) << Remove(4,5,0,6) << Insert(0,11,0,0));
    QTest::newRow("r(12,18),r(3,0),r(1,2),m(4,0,11),r(14,3)")
            << (SignalList() << Remove(12,18) << Remove(3,0) << Remove(1,2) << Move(4,0,11,0) << Remove(14,3))
            << (SignalList() << Remove(1,2) << Remove(3,1) << Remove(3,6,0,0) << Remove(3,18) << Remove(3,5,0,6) << Remove(3,2) << Insert(0,11,0,0));

    QTest::newRow("m(9,11,14),i(16,1)")
            << (SignalList() << Move(9,11,14,0) << Insert(16,1))
            << (SignalList() << Remove(9,14,0,0) << Insert(11,5,0,0) << Insert(16,1) << Insert(17,9,0,5));
    QTest::newRow("m(9,11,14),i(16,1),m(22,38,3)")
            << (SignalList() << Move(9,11,14,0) << Insert(16,1) << Move(22,38,3,1))
            << (SignalList() << Remove(9,14,0,0) << Insert(11,5,0,0) << Insert(16,1) << Insert(17,5,0,5) << Insert(22,1,0,13) << Insert(38,3,0,10));

    QTest::newRow("i(28,2),m(7,5,22)")
            << (SignalList() << Insert(28,2) << Move(7,5,22,0))
            << (SignalList() << Remove(7,21,0,0) << Insert(5,21,0,0) << Insert(26,1) << Insert(29,1));

    QTest::newRow("i(16,3),m(18,28,15)")
            << (SignalList() << Insert(16,3) << Move(18,28,15,0))
            << (SignalList() << Remove(16,14,0,1) << Insert(16,2) << Insert(28,1) << Insert(29,14,0,1));

    QTest::newRow("m(33,12,6),m(18,20,20)")
            << (SignalList() << Move(22,12,6,0) << Move(18,20,20,1))
            << (SignalList() << Remove(12,10,1,0) << Remove(12,6,0,0) << Remove(12,10,1,10)
                             << Insert(12,6,0,0) << Insert(20,20,1,0));
    QTest::newRow("m(33,12,6),m(18,20,20),m(38,19,1)")
            << (SignalList() << Move(22,12,6,0) << Move(18,20,20,1) << Move(28,19,1,2))
            << (SignalList() << Remove(12,10,1,0) << Remove(12,6,0,0) << Remove(12,10,1,10)
                             << Insert(12,6,0,0) << Insert(19,1,1,8) << Insert(21,8,1,0) << Insert(29,11,1,9));
    QTest::newRow("m(33,12,6),m(18,20,20),m(38,19,1),r(34,4)")
            << (SignalList() << Move(22,12,6,0) << Move(18,20,20,1) << Move(28,19,1,2) << Remove(34,4))
            << (SignalList() << Remove(12,10,1,0) << Remove(12,6,0,0) << Remove(12,10,1,10)
                             << Insert(12,6,0,0) << Insert(19,1,1,8) << Insert(21,8,1,0) << Insert(29,5,1,9) << Insert(34,2,1,18));
    QTest::newRow("m(33,12,6),m(18,20,20),m(38,19,1),r(34,4),m(13,9,15)")
            << (SignalList() << Move(22,12,6,0) << Move(18,20,20,1) << Move(28,19,1,2) << Remove(34,4) << Move(13,9,15,3))
            << (SignalList() << Remove(12,10,1,0) << Remove(12,6,0,0) << Remove(12,10,1,10) << Remove(12,1,3,5) << Remove(12,1,3,7)
                             << Insert(9,5,0,1) << Insert(14,1,3,5) << Insert(15,1,1,8) << Insert(16,1,3,7) << Insert(17,7,1,0) << Insert(27,1,0,0) << Insert(28,1,1,7) << Insert(29,5,1,9) << Insert(34,2,1,18));

    QTest::newRow("i(8,5),m(14,26,14)")
            << (SignalList() << Insert(8,5) << Move(14,26,14,0))
            << (SignalList() << Remove(9,14,0,0) << Insert(8,5) << Insert(26,14,0,0));
    QTest::newRow("i(8,5),m(14,26,14),r(45,0)")
            << (SignalList() << Insert(8,5) << Move(14,26,14,0) << Remove(45,0))
            << (SignalList() << Remove(9,14,0,0) << Insert(8,5) << Insert(26,14,0,0));
    QTest::newRow("i(8,5),m(14,26,14),r(45,0),m(5,8,21)")
            << (SignalList() << Insert(8,5) << Move(14,26,14,0) << Remove(45,0) << Move(5,8,21,1))
            << (SignalList() << Remove(5,3,1,0) << Remove(5,1,1,8) << Remove(5,14,0,0) <<  Remove(5,12,1,9)
                             << Insert(5,3,0,0) << Insert(8,3,1,0) << Insert(11,5) << Insert(16,13,1,8) << Insert(29,11,0,3));

    QTest::newRow("i(35,1),r(5,31)")
            << (SignalList() << Insert(35,1) << Remove(5,31))
            << (SignalList() << Remove(5,30));
    QTest::newRow("i(35,1),r(5,31),m(9,8,1)")
            << (SignalList() << Insert(35,1) << Remove(5,31) << Move(9,8,1,0))
            << (SignalList() << Remove(5,30) << Remove(9,1,0,0) << Insert(8,1,0,0));
    QTest::newRow("i(35,1),r(5,31),m(9,8,1),i(7,2)")
            << (SignalList() << Insert(35,1) << Remove(5,31) << Move(9,8,1,0) << Insert(7,2))
            << (SignalList() << Remove(5,30) << Remove(9,1,0,0) << Insert(7,2) << Insert(10,1,0,0));
    QTest::newRow("i(35,1),r(5,31),m(9,8,1),i(7,2),r(4,3)")
            << (SignalList() << Insert(35,1) << Remove(5,31) << Move(9,8,1,0) << Insert(7,2) << Remove(4,3))
            << (SignalList() << Remove(4,33) << Remove(6,1,0,0) << Insert(4,2) << Insert(7,1,0,0));

    QTest::newRow("r(37,0),r(21,1)")
            << (SignalList() << Remove(37,0) << Remove(21,1))
            << (SignalList() << Remove(21,1));
    QTest::newRow("r(37,0),r(21,1),m(27,35,2)")
            << (SignalList() << Remove(37,0) << Remove(21,1) << Move(27,35,2,0))
            << (SignalList() << Remove(21,1) << Remove(27,2,0,0) << Insert(35,2,0,0));
    QTest::newRow("r(37,0),r(21,1),m(27,35,2),i(31,5)")
            << (SignalList() << Remove(37,0) << Remove(21,1) << Move(27,35,2,0) << Insert(31,5))
            << (SignalList() << Remove(21,1) << Remove(27,2,0,0) << Insert(31,5) << Insert(40,2,0,0));
    QTest::newRow("r(37,0),r(21,1),m(27,35,2),i(31,5),r(10,31)")
            << (SignalList() << Remove(37,0) << Remove(21,1) << Move(27,35,2,0) << Insert(31,5) << Remove(10,31))
            << (SignalList() << Remove(10, 18) << Remove(10,2,0,0) << Remove(10,8) << Insert(10,1,0,1));

    QTest::newRow("m(1,1,39),r(26,10)")
            << (SignalList() << Move(1,1,39,0) << Remove(26,10))
            << (SignalList() << Remove(1,39,0,0) << Insert(1,25,0,0) << Insert(26,4,0,35));
    QTest::newRow("m(1,1,39),r(26,10),i(10,5)")
            << (SignalList() << Move(1,1,39,0) << Remove(26,10) << Insert(10,5))
            << (SignalList() << Remove(1,39,0,0) << Insert(1,9,0,0) << Insert(10,5) << Insert(15,16,0,9) << Insert(31,4,0,35));
    QTest::newRow("m(1,1,39),r(26,10),i(10,5),i(27,3)")
            << (SignalList() << Move(1,1,39,0) << Remove(26,10) << Insert(10,5) << Insert(27,3))
            << (SignalList() << Remove(1,39,0,0)
                             << Insert(1,9,0,0) << Insert(10,5) << Insert(15,12,0,9) << Insert(27,3) << Insert(30,4,0,21) << Insert(34,4,0,35));
    QTest::newRow("m(1,1,39),r(26,10),i(10,5),i(27,3),r(28,5)")
            << (SignalList() << Move(1,1,39,0) << Remove(26,10) << Insert(10,5) << Insert(27,3) << Remove(28,5))
            << (SignalList() << Remove(1,39,0,0)
                             << Insert(1,9,0,0) << Insert(10,5) << Insert(15,12,0,9) << Insert(27,1) << Insert(28,1,0,24) << Insert(29,4,0,35));

    QTest::newRow("i(36,4)m(25,39,5)")
            << (SignalList() << Insert(36,4) << Move(25,39,5,0))
            << (SignalList() << Remove(25,5,0,0) << Insert(31,4) << Insert(39,5,0,0));
    QTest::newRow("i(36,4)m(25,39,5),i(16,5)")
            << (SignalList() << Insert(36,4) << Move(25,39,5,0) << Insert(16,5))
            << (SignalList() << Remove(25,5,0,0) << Insert(16,5) << Insert(36,4) << Insert(44,5,0,0));
    QTest::newRow("i(36,4)m(25,39,5),i(16,5),i(37,5)")
            << (SignalList() << Insert(36,4) << Move(25,39,5,0) << Insert(16,5) << Insert(37,5))
            << (SignalList() << Remove(25,5,0,0) << Insert(16,5) << Insert(36,9) << Insert(49,5,0,0));
    QTest::newRow("i(36,4)m(25,39,5),i(16,5),i(37,5),m(40,21,11)")
            << (SignalList() << Insert(36,4) << Move(25,39,5,0) << Insert(16,5) << Insert(37,5) << Move(40,21,11,1))
            << (SignalList() << Remove(25,5,0,0) << Remove(31,4,1,5)
                             << Insert(16,10) << Insert(26,4,1,5) << Insert(30,2,0,0) << Insert(47,4) << Insert(51,3,0,2));

    QTest::newRow("i(24,1),r(33,4)")
            << (SignalList() << Insert(24,1) << Remove(33,4))
            << (SignalList() << Remove(32,4) << Insert(24,1));
    QTest::newRow("i(24,1),r(33,4),r(15,15)")
            << (SignalList() << Insert(24,1) << Remove(33,4) << Remove(15,15))
            << (SignalList() << Remove(15,14) << Remove(18,4));
    QTest::newRow("i(24,1),r(33,4),r(15,15),m(8,10,2)")
            << (SignalList() << Insert(24,1) << Remove(33,4) << Remove(15,15) << Move(8,10,2,0))
            << (SignalList() << Remove(8,2,0,0) << Remove(13,14) << Remove(16,4) << Insert(10,2,0,0));
    QTest::newRow("i(24,1),r(33,4),r(15,15),m(8,10,2),r(2,19)")
            << (SignalList() << Insert(24,1) << Remove(33,4) << Remove(15,15) << Move(8,10,2,0) << Remove(2,19))
            << (SignalList() << Remove(2,6) << Remove(2,2,0,0) << Remove(2,29));

    QTest::newRow("r(1,35),i(3,4),m(4,2,2)")
            << (SignalList() << Remove(1,35) << Insert(3,4) << Move(4,2,2,0))
            << (SignalList() << Remove(1,35) <<Insert(2,2) << Insert(5,2));
    QTest::newRow("r(1,35),i(3,4),m(4,2,2),r(7,1)")
            << (SignalList() << Remove(1,35) << Insert(3,4) << Move(4,2,2,0) << Remove(7,1))
            << (SignalList() << Remove(1,35) << Remove(3,1) << Insert(2,2) << Insert(5,2));

    QTest::newRow("i(30,4),m(7,28,16),m(6,7,13)")
            << (SignalList() << Insert(30,4) << Move(7,28,16,0))
            << (SignalList() << Remove(7,16,0,0) << Insert(14,4) << Insert(28,16,0,0));
    QTest::newRow("(i(30,4),m(7,28,16),m(6,7,13),m(41,35,2)")
            << (SignalList() << Insert(30,4) << Move(7,28,16,0) << Move(6,7,13,1) << Move(41,35,2,2))
            << (SignalList() << Remove(6,1,1,0) << Remove(6,16,0,0) << Remove(6,7,1,1) << Remove(6,1,1,12)
                             << Insert(7,8,1,0) << Insert(15,4) << Insert(19,1,1,12) << Insert(28,7,0,0) << Insert(35,2,0,13) << Insert(37,6,0,7) << Insert(43,1,0,15));

    QTest::newRow("(i(35,2),r(39,0))(r(25,11))")
            << (SignalList() << Insert(35,2) << Remove(39,0) << Remove(25,11))
            << (SignalList() << Remove(25,10) << Insert(25,1));
    QTest::newRow("(i(35,2),r(39,0))(r(25,11),m(28,8,7))")
            << (SignalList() << Insert(35,2) << Remove(39,0) << Remove(25,11) << Move(24,8,7,0))
            << (SignalList() << Remove(24,1,0,0) << Remove(24,10) << Remove(24,5,0,2)
                << Insert(8,1,0,0) << Insert(9,1) << Insert(10,5,0,2));

    QTest::newRow("i(26,1),i(39,1)")
            << (SignalList() << Insert(26,1) << Insert(39,1))
            << (SignalList() << Insert(26,1) << Insert(39,1));
    QTest::newRow("i(26,1),i(39,1),m(31,34,2)")
            << (SignalList() << Insert(26,1) << Insert(39,1) << Move(31,34,2,0))
            << (SignalList() << Remove(30,2,0,0) << Insert(26,1) << Insert(34,2,0,0) << Insert(39,1));
    QTest::newRow("i(26,1),i(39,1),m(31,34,2),r(15,27)")
            << (SignalList() << Insert(26,1) << Insert(39,1) << Move(31,34,2,0) << Remove(15,27))
            << (SignalList() << Remove(15,15) << Remove(15,2,0,0) << Remove(15,8));

    QTest::newRow("i(19,1),i(2,3)")
            << (SignalList() << Insert(19,1) << Insert(2,3))
            << (SignalList() << Insert(2,3) << Insert(22,1));
    QTest::newRow("i(19,1),i(2,3),r(38,4)")
            << (SignalList() << Insert(19,1) << Insert(2,3) << Remove(38,4))
            << (SignalList() << Remove(34,4) << Insert(2,3) << Insert(22,1));
    QTest::newRow("i(19,1),(2,3),r(38,4),m(2,20,3")
            << (SignalList() << Insert(19,1) << Insert(2,3) << Remove(38,4) << Move(2,20,3,0))
            << (SignalList() << Remove(34,4) << Insert(19,4));

    QTest::newRow("i(4,3),i(19,1)")
            << (SignalList() << Insert(4,3) << Insert(19,1))
            << (SignalList() << Insert(4,3) << Insert(19,1));
    QTest::newRow("i(4,3),i(19,1),i(31,3)")
            << (SignalList() << Insert(4,3) << Insert(19,1) << Insert(31,3))
            << (SignalList() << Insert(4,3) << Insert(19,1) << Insert(31,3));
    QTest::newRow("i(4,3),i(19,1),i(31,3),m(8,10,29)")
            << (SignalList() << Insert(4,3) << Insert(19,1) << Insert(31,3) << Move(8,10,29,0))
            << (SignalList() << Remove(5,11,0,0) << Remove(5,11,0,12) << Remove(5,3,0,26)
                             << Insert(4,3) << Insert(10,11,0,0) << Insert(21,1) << Insert(22,11,0,12) << Insert(33,3) << Insert(36,3,0,26));

    QTest::newRow("m(18,15,16),i(0,1)")
            << (SignalList() << Move(18,15,16,0) << Insert(0,1))
            << (SignalList() << Remove(18,16,0,0) << Insert(0,1) << Insert(16,16,0,0));
    QTest::newRow("m(18,15,16),i(0,1),i(32,2)")
            << (SignalList() << Move(18,15,16,0) << Insert(0,1) << Insert(32,2))
            << (SignalList() << Remove(18,16,0,0) << Insert(0,1) << Insert(16,16,0,0) << Insert(32,2));
    QTest::newRow("m(18,15,16),i(0,1),i(32,2),i(29,2)")
            << (SignalList() << Move(18,15,16,0) << Insert(0,1) << Insert(32,2) << Insert(29,2))
            << (SignalList() << Remove(18,16,0,0) << Insert(0,1) << Insert(16,13,0,0) << Insert(29,2) << Insert(31,3,0,13) << Insert(34,2));

    QTest::newRow("i(38,5),i(12,5)")
            << (SignalList() << Insert(38,5) << Insert(12,5))
            << (SignalList() << Insert(12,5) << Insert(43,5));
    QTest::newRow("i(38,5),i(12,5),i(48,3)")
            << (SignalList() << Insert(38,5) << Insert(12,5) << Insert(48,3))
            << (SignalList() << Insert(12,5) << Insert(43,8));
    QTest::newRow("i(38,5),i(12,5),i(48,3),m(28,6,6)")
            << (SignalList() << Insert(38,5) << Insert(12,5) << Insert(48,3) << Move(28,6,6,0))
            << (SignalList() << Remove(23,6,0,0) << Insert(6,6,0,0) << Insert(18,5) << Insert(43,8));

    QTest::newRow("r(8,9),m(7,10,18)")
            << (SignalList() << Remove(8,9) << Move(7,10,18,0))
            << (SignalList() << Remove(7,1,0,0) << Remove(7,9) << Remove(7,17,0,1) << Insert(10,18,0,0));
    QTest::newRow("r(8,9),m(7,10,18),m(8,10,18)")
            << (SignalList() << Remove(8,9) << Move(7,10,18,0) << Move(8,10,18,1))
            << (SignalList() << Remove(7,1,0,0) << Remove(7,9) << Remove(7,17,0,1) << Remove(8,2,1,0) << Insert(8,2,0,16) << Insert(10,2,1,0) << Insert(12,16,0,0));
    QTest::newRow("r(8,9),m(7,10,18),m(8,10,18),i(17,2)")
            << (SignalList() << Remove(8,9) << Move(7,10,18,0) << Move(8,10,18,1) << Insert(17,2))
            << (SignalList() << Remove(7,1,0,0) << Remove(7,9) << Remove(7,17,0,1) << Remove(8,2,1,0) << Insert(8,2,0,16) << Insert(10,2,1,0) << Insert(12,5,0,0) << Insert(17,2) << Insert(19,11,0,5));

    QTest::newRow("r(39,0),m(18,1,21)")
            << (SignalList() << Remove(39,0) << Move(18,1,21,0))
            << (SignalList() << Remove(18,21,0,0) << Insert(1,21,0,0));
    QTest::newRow("r(39,0),m(18,1,21),m(2,6,31)")
            << (SignalList() << Remove(39,0) << Move(18,1,21,0) << Move(2,6,31,1))
            << (SignalList() << Remove(1,11,1,20) << Remove(7,21,0,0) << Insert(1,1,0,0) << Insert(6,20,0,1) << Insert(26,11,1,20));
    QTest::newRow("r(39,0),m(18,1,21),m(2,6,31),r(9,4)")
            << (SignalList() << Remove(39,0) << Move(18,1,21,0) << Move(2,6,31,1) << Remove(9,4))
            << (SignalList() << Remove(1,11,1,20) << Remove(7,21,0,0) << Insert(1,1,0,0) << Insert(6,3,0,1) << Insert(9,13,0,8) << Insert(22,11,1,20));

    QTest::newRow("i(1,1),m(9,32,3)")
            << (SignalList()
                << Insert(1,1) << Move(9,32,3,0))
            << (SignalList() << Remove(8,3,0,0) << Insert(1,1) << Insert(32,3,0,0));
    QTest::newRow("i(1,1),m(9,32,3),r(22,1)")
            << (SignalList()
                << Insert(1,1) << Move(9,32,3,0) << Remove(22,1))
            << (SignalList() << Remove(8,3,0,0) << Remove(21,1) << Insert(1,1) << Insert(31,3,0,0));
    QTest::newRow("i(1,1),m(9,32,3),r(22,1),i(29,3)")
            << (SignalList()
                << Insert(1,1) << Move(9,32,3,0) << Remove(22,1) << Insert(29,3))
            << (SignalList() << Remove(8,3,0,0) << Remove(21,1) << Insert(1,1) << Insert(29,3) << Insert(34,3,0,0));
    QTest::newRow("i(1,1),m(9,32,3),r(22,1),i(29,3),m(7,15,23)")
            << (SignalList()
                << Insert(1,1) << Move(9,32,3,0) << Remove(22,1) << Insert(29,3) << Move(7,15,23,1))
            << (SignalList() << Remove(6,2,1,0) << Remove(6,3,0,0) << Remove(6,13,1,2) << Remove(6,1) << Remove(6,7,1,15) << Insert(1,1) << Insert(7,2) << Insert(11,3,0,0) << Insert(15,22,1,0) << Insert(37,1));

    QTest::newRow("r(36,4),m(3,2,14)")
            << (SignalList() << Remove(36,4) << Move(3,2,14,0))
            << (SignalList() << Remove(3,14,0,0) << Remove(22,4) << Insert(2,14,0,0));
    QTest::newRow("r(36,4),m(3,2,14),i(36,3)")
            << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3))
            << (SignalList() << Remove(3,14,0,0) << Remove(22,4) << Insert(2,14,0,0) << Insert(36,3));
    QTest::newRow("r(36,4),m(3,2,14),i(36,3),r(30,7)")
            << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7))
            << (SignalList() << Remove(3,14,0,0) << Remove(16,10) << Insert(2,14,0,0) << Insert(30,2));  // ###
    QTest::newRow("r(36,4),m(3,2,14),i(36,3),r(30,7),i(3,5)")
            << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
            << (SignalList() << Remove(3,14,0,0) << Remove(16,10) << Insert(2,1,0,0) << Insert(3,5) << Insert(8,13,0,1) << Insert(35,2));    // ###

    QTest::newRow("3*5 (10)")
            << (SignalList()
                << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5)
                << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2))
            << (SignalList()
                << Remove(2,1,2,12) << Remove(2,14,0,0) << Remove(2,2,2,13) << Remove(2,1)
                << Remove(2,7,2,15) << Remove(5,10) << Insert(1,1) << Insert(3,1,0,0)
                << Insert(4,3) << Insert(7,2) << Insert(11,3,0,1) << Insert(15,2)
                << Insert(17,10,0,4) << Insert(27,10,2,12) << Insert(37,3));
    QTest::newRow("3*5 (11)")
            << (SignalList()
                << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5)
                << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2)
                << Move(38,23,1,3))
            << (SignalList()
                << Remove(2,1,2,12) << Remove(2,14,0,0) << Remove(2,2,2,13)
                << Remove(2,1) << Remove(2,7,2,15) << Remove(5,10) << Insert(1,1)
                << Insert(3,1,0,0) << Insert(4,3) << Insert(7,2) << Insert(11,3,0,1) << Insert(15,2)
                << Insert(17,6,0,4) << Insert(23,1) << Insert(24,4,0,10) << Insert(28,10,2,12)
                << Insert(38,2));
    QTest::newRow("3*5 (12)")
            << (SignalList()
                << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5)
                << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2)
                << Move(38,23,1,3) << Move(38,31,0,4))
            << (SignalList()
                << Remove(2,1,2,12) << Remove(2,14,0,0) << Remove(2,2,2,13) << Remove(2,1)
                << Remove(2,7,2,15) << Remove(5,10) << Insert(1,1) << Insert(3,1,0,0)
                << Insert(4,3) << Insert(7,2) << Insert(11,3,0,1) << Insert(15,2) << Insert(17,6,0,4)
                << Insert(23,1) << Insert(24,4,0,10) << Insert(28,10,2,12) << Insert(38,2));
    QTest::newRow("3*5 (13)")
            << (SignalList()
                << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5)
                << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2)
                << Move(38,23,1,3) << Move(38,31,0,4) << Remove(26,11))
            << (SignalList()
                << Remove(2,1,2,12) << Remove(2,14,0,0) << Remove(2,2,2,13) << Remove(2,1)
                << Remove(2,7,2,15) << Remove(5,10) << Insert(1,1) << Insert(3,1,0,0)
                << Insert(4,3) << Insert(7,2) << Insert(11,3,0,1) << Insert(15,2) << Insert(17,6,0,4)
                << Insert(23,1) << Insert(24,2,0,10) << Insert(26,1,2,21) << Insert(27,2));
    QTest::newRow("3*5 (14)")
            << (SignalList()
                << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5)
                << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2)
                << Move(38,23,1,3) << Move(38,31,0,4) << Remove(26,11) << Move(5,7,18,5))
            << (SignalList()
                << Remove(2,1,2,12) << Remove(2,14,0,0) << Remove(2,2,2,13) << Remove(2,1)
                << Remove(2,7,2,15) << Remove(2,2,5,4) << Remove(2,1,5,9) << Remove(2,10)
                << Insert(1,1) << Insert(3,1,0,0) << Insert(4,1) << Insert(5,1)
                << Insert(6,1,0,10) << Insert(7,4) << Insert(11,2,5,4)
                << Insert(13,3,0,1) << Insert(16,1,5,9) << Insert(17,2) << Insert(19,6,0,4)
                << Insert(25,1,0,11) << Insert(26,1,2,21) << Insert(27,2));
    QTest::newRow("3*5 (15)")
            << (SignalList()
                << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5)
                << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2)
                << Move(38,23,1,3) << Move(38,31,0,4) << Remove(26,11) << Move(5,7,18,5) << Move(19,0,8,6))
            << (SignalList()
                << Remove(2,1,2,12) << Remove(2,14,0,0) << Remove(2,2,2,13) << Remove(2,1)
                << Remove(2,7,2,15) << Remove(2,2,5,4) << Remove(2,1,5,9) << Remove(2,10)
                << Insert(0,6,0,4) << Insert(6,1,0,11) << Insert(7,1,2,21) << Insert(9,1)
                << Insert(11,1,0,0) << Insert(12,1) << Insert(13,1) << Insert(14,1,0,10)
                << Insert(15,4) << Insert(19,2,5,4) << Insert(21,3,0,1)
                << Insert(24,1,5,9) << Insert(25,2) << Insert(27,2));
}

void tst_qqmlchangeset::sequence()
{
    QFETCH(SignalList, input);
    QFETCH(SignalList, output);

    QQmlChangeSet set;

    foreach (const Signal &signal, input) {
        if (signal.isRemove())
            set.remove(signal.index, signal.count);
        else if (signal.isInsert())
            set.insert(signal.index, signal.count);
        else if (signal.isMove())
            set.move(signal.index, signal.to, signal.count, signal.moveId);
        else if (signal.isChange())
            set.change(signal.index, signal.count);
    }

    SignalList changes;
    foreach (const QQmlChangeSet::Change &remove, set.removes())
        changes << Remove(remove.index, remove.count, remove.moveId, remove.offset);
    foreach (const QQmlChangeSet::Change &insert, set.inserts())
        changes << Insert(insert.index, insert.count, insert.moveId, insert.offset);
    foreach (const QQmlChangeSet::Change &change, set.changes())
        changes << Change(change.index, change.count);

    VERIFY_EXPECTED_OUTPUT
    QCOMPARE(changes, output);
}

void tst_qqmlchangeset::apply_data()
{
    QTest::addColumn<SignalListList>("input");

    QTest::newRow("(r(1,35),i(3,4)),(m(4,2,2),r(7,1))")
            << (SignalListList()
                << (SignalList() << Remove(1,35) << Insert(3,4))
                << (SignalList() << Move(4,2,2,0) << Remove(7,1)));

    QTest::newRow("(i(30,4),m(7,28,16))(m(6,7,13),m(41,35,2))")
            << (SignalListList()
                << (SignalList() << Insert(30,4) << Move(7,28,16,0))
                << (SignalList() << Move(6,7,13,1) << Move(41,35,2,2)));

    QTest::newRow("(i(35,2),r(39,0))(r(25,11),m(24,8,7))")
            << (SignalListList()
                << (SignalList() << Insert(35,2) << Remove(39,0))
                << (SignalList() << Remove(25,11) << Move(24,8,7,0)));

    QTest::newRow("i(26,1),i(39,1),m(31,34,2),r(15,27)")
            << (SignalListList()
                << (SignalList() << Insert(26,1) << Insert(39,1))
                << (SignalList() << Move(31,34,2,0) << Remove(15,27)));

    QTest::newRow("i(19,1),(2,3),r(38,4),m(2,20,3)")
            << (SignalListList()
                << (SignalList() << Insert(19,1) << Insert(2,3))
                << (SignalList() << Remove(38,4) << Move(2,20,3,0)));

    QTest::newRow("i(4,3),i(19,1),i(31,3),m(8,10,29)")
            << (SignalListList()
                << (SignalList() << Insert(4,3) << Insert(19,1))
                << (SignalList() << Insert(31,3) << Move(8,10,29,0)));

    QTest::newRow("m(18,15,16),i(0,1),i(32,2),i(29,2)")
            << (SignalListList()
                << (SignalList() << Move(18,15,16,0) << Insert(0,1))
                << (SignalList() << Insert(32,2) << Insert(29,2)));

    QTest::newRow("i(38,5),i(12,5),i(48,3),m(28,6,6)")
            << (SignalListList()
                << (SignalList() << Insert(38,5) << Insert(12,5))
                << (SignalList() << Insert(48,3) << Move(28,6,6,0)));

    QTest::newRow("r(8,9),m(7,10,18),m(8,10,18),i(17,2)")
            << (SignalListList()
                << (SignalList() << Remove(8,9) << Move(7,10,18,0))
                << (SignalList() << Move(8,10,18,1) << Insert(17,2)));

    QTest::newRow("r(39,0),m(18,1,21),m(2,6,31),r(9,4)")
            << (SignalListList()
                << (SignalList() << Remove(39,0) << Move(18,1,21,0))
                << (SignalList() << Move(2,6,31,1) << Remove(9,4)));

    QTest::newRow("3*5 (5)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5)));
    QTest::newRow("3*5 (6)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1)));
    QTest::newRow("3*5 (7)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1)));
    QTest::newRow("3*5 (8)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1) << Remove(22,1)));
    QTest::newRow("3*5 (9)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3)));
    QTest::newRow("3*5 (10)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2)));
    QTest::newRow("3*5 (11)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2))
                << (SignalList() << Move(38,23,1,3)));
    QTest::newRow("3*5 (12)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2))
                << (SignalList() << Move(38,23,1,3) << Move(38,31,0,4)));
    QTest::newRow("3*5 (13)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2))
                << (SignalList() << Move(38,23,1,3) << Move(38,31,0,4) << Remove(26,11)));
    QTest::newRow("3*5 (14)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2))
                << (SignalList() << Move(38,23,1,3) << Move(38,31,0,4) << Remove(26,11) << Move(5,7,18,5)));
    QTest::newRow("3*5 (15)")
            << (SignalListList()
                << (SignalList() << Remove(36,4) << Move(3,2,14,0) << Insert(36,3)  << Remove(30,7) << Insert(3,5))
                << (SignalList() << Insert(1,1) << Move(9,32,3,1) << Remove(22,1) << Insert(29,3) << Move(7,15,23,2))
                << (SignalList() << Move(38,23,1,3) << Move(38,31,0,4) << Remove(26,11) << Move(5,7,18,5) << Move(19,0,8,6)));
}

void tst_qqmlchangeset::apply()
{
    QFETCH(SignalListList, input);

    QQmlChangeSet set;
    QQmlChangeSet linearSet;

    foreach (const SignalList &list, input) {
        QQmlChangeSet intermediateSet;
        foreach (const Signal &signal, list) {
            if (signal.isRemove()) {
                intermediateSet.remove(signal.index, signal.count);
                linearSet.remove(signal.index, signal.count);
            } else if (signal.isInsert()) {
                intermediateSet.insert(signal.index, signal.count);
                linearSet.insert(signal.index, signal.count);
            } else if (signal.isMove()) {
                intermediateSet.move(signal.index, signal.to, signal.count, signal.moveId);
                linearSet.move(signal.index, signal.to, signal.count, signal.moveId);
            }
        }
        set.apply(intermediateSet);
    }

    SignalList changes;
    foreach (const QQmlChangeSet::Change &remove, set.removes())
        changes << Remove(remove.index, remove.count, remove.moveId, remove.offset);
    foreach (const QQmlChangeSet::Change &insert, set.inserts())
        changes << Insert(insert.index, insert.count, insert.moveId, insert.offset);

    SignalList linearChanges;
    foreach (const QQmlChangeSet::Change &remove, linearSet.removes())
        linearChanges << Remove(remove.index, remove.count, remove.moveId, remove.offset);
    foreach (const QQmlChangeSet::Change &insert, linearSet.inserts())
        linearChanges << Insert(insert.index, insert.count, insert.moveId, insert.offset);

    // The output in the failing tests isn't incorrect, merely sub-optimal.
    QEXPECT_FAIL("3*5 (10)", "inserts not joined when dividing space removed", Abort);
    QEXPECT_FAIL("3*5 (11)", "inserts not joined when dividing space removed", Abort);
    QEXPECT_FAIL("3*5 (12)", "inserts not joined when dividing space removed", Abort);
    QEXPECT_FAIL("3*5 (13)", "inserts not joined when dividing space removed", Abort);
    QEXPECT_FAIL("3*5 (14)", "inserts not joined when dividing space removed", Abort);
    QEXPECT_FAIL("3*5 (15)", "inserts not joined when dividing space removed", Abort);
    QCOMPARE(changes, linearChanges);
}

void tst_qqmlchangeset::removeConsecutive_data()
{
    QTest::addColumn<SignalList>("input");
    QTest::addColumn<SignalList>("output");

    QTest::newRow("at start")
            << (SignalList() << Remove(0,2) << Remove(0,1) << Remove(0,5))
            << (SignalList() << Remove(0,8));
    QTest::newRow("offset")
            << (SignalList() << Remove(3,2) << Remove(3,1) << Remove(3,5))
            << (SignalList() << Remove(3,8));
    QTest::newRow("with move")
            << (SignalList() << Remove(0,2) << Remove(0,1,0,0) << Remove(0,5))
            << (SignalList() << Remove(0,2) << Remove(0,1,0,0) << Remove(0,5));
}

void tst_qqmlchangeset::removeConsecutive()
{
    QFETCH(SignalList, input);
    QFETCH(SignalList, output);

    QVector<QQmlChangeSet::Change> removes;
    foreach (const Signal &signal, input) {
        QVERIFY(signal.isRemove());
        removes.append(QQmlChangeSet::Change(signal.index, signal.count, signal.moveId, signal.offset));
    }

    QQmlChangeSet set;
    set.remove(removes);

    SignalList changes;
    foreach (const QQmlChangeSet::Change &remove, set.removes())
        changes << Remove(remove.index, remove.count, remove.moveId, remove.offset);
    QVERIFY(set.inserts().isEmpty());
    QVERIFY(set.changes().isEmpty());

    VERIFY_EXPECTED_OUTPUT
    QCOMPARE(changes, output);
}

void tst_qqmlchangeset::insertConsecutive_data()
{
    QTest::addColumn<SignalList>("input");
    QTest::addColumn<SignalList>("output");

    QTest::newRow("at start")
            << (SignalList() << Insert(0,2) << Insert(2,1) << Insert(3,5))
            << (SignalList() << Insert(0,8));
    QTest::newRow("offset")
            << (SignalList() << Insert(3,2) << Insert(5,1) << Insert(6,5))
            << (SignalList() << Insert(3,8));
    QTest::newRow("with move")
            << (SignalList() << Insert(0,2) << Insert(2,1,0,0) << Insert(3,5))
            << (SignalList() << Insert(0,2) << Insert(2,1,0,0) << Insert(3,5));
}

void tst_qqmlchangeset::insertConsecutive()
{
    QFETCH(SignalList, input);
    QFETCH(SignalList, output);

    QVector<QQmlChangeSet::Change> inserts;
    foreach (const Signal &signal, input) {
        QVERIFY(signal.isInsert());
        inserts.append(QQmlChangeSet::Change(signal.index, signal.count, signal.moveId, signal.offset));
    }

    QQmlChangeSet set;
    set.insert(inserts);

    SignalList changes;
    foreach (const QQmlChangeSet::Change &insert, set.inserts())
        changes << Insert(insert.index, insert.count, insert.moveId, insert.offset);
    QVERIFY(set.removes().isEmpty());
    QVERIFY(set.changes().isEmpty());

    VERIFY_EXPECTED_OUTPUT
    QCOMPARE(changes, output);
}

void tst_qqmlchangeset::copy()
{
    QQmlChangeSet changeSet;
    changeSet.remove(0, 12);
    changeSet.remove(5, 4);
    changeSet.insert(3, 9);
    changeSet.insert(15, 2);
    changeSet.change(24, 8);
    changeSet.move(3, 5, 9, 0);

    QQmlChangeSet copy(changeSet);

    QQmlChangeSet assign;
    assign = changeSet;

    copy.move(4, 2, 5, 1);
    assign.move(4, 2, 5, 1);
    changeSet.move(4, 2, 5, 1);

    QCOMPARE(copy.removes(), changeSet.removes());
    QCOMPARE(copy.inserts(), changeSet.inserts());
    QCOMPARE(copy.changes(), changeSet.changes());
    QCOMPARE(copy.difference(), changeSet.difference());

    QCOMPARE(assign.removes(), changeSet.removes());
    QCOMPARE(assign.inserts(), changeSet.inserts());
    QCOMPARE(assign.changes(), changeSet.changes());
    QCOMPARE(assign.difference(), changeSet.difference());
}

void tst_qqmlchangeset::debug()
{
    QQmlChangeSet changeSet;
    changeSet.remove(0, 12);
    changeSet.remove(5, 4);
    changeSet.insert(3, 9);
    changeSet.insert(15, 2);
    changeSet.change(24, 8);

    QTest::ignoreMessage(QtDebugMsg, "QQmlChangeSet(Change(0,12) Change(5,4) Change(3,9) Change(15,2) Change(24,8) )");
    qDebug() << changeSet;

    changeSet.clear();

    QTest::ignoreMessage(QtDebugMsg, "QQmlChangeSet(Change(12,4) Change(5,4) )");

    changeSet.move(12, 5, 4, 0);
    qDebug() << changeSet;
}

void tst_qqmlchangeset::random_data()
{
    QTest::addColumn<int>("seed");
    QTest::addColumn<int>("combinations");
    QTest::addColumn<int>("depth");
    QTest::newRow("1*5") << 32 << 1 << 5;
    QTest::newRow("2*2") << 32 << 2 << 2;
    QTest::newRow("3*2") << 32 << 3 << 2;
    QTest::newRow("3*5") << 32 << 3 << 5;
}

void tst_qqmlchangeset::random()
{
    QFETCH(int, seed);
    QFETCH(int, combinations);
    QFETCH(int, depth);

    qsrand(seed);

    int failures = 0;
    for (int i = 0; i < 20000; ++i) {
        QQmlChangeSet accumulatedSet;
        SignalList input;

        int modelCount = 40;
        int moveCount = 0;

        for (int j = 0; j < combinations; ++j) {
            QQmlChangeSet set;
            for (int k = 0; k < depth; ++k) {
                switch (-(qrand() % 3)) {
                case InsertOp: {
                    int index = qrand() % (modelCount + 1);
                    int count = qrand() % 5 + 1;
                    set.insert(index, count);
                    input.append(Insert(index, count));
                    modelCount += count;
                    break;
                }
                case RemoveOp: {
                    const int index = qrand() % (modelCount + 1);
                    const int count = qrand() % (modelCount - index + 1);
                    set.remove(index, count);
                    input.append(Remove(index, count));
                    modelCount -= count;
                    break;
                }
                case MoveOp: {
                    const int from = qrand() % (modelCount + 1);
                    const int count = qrand() % (modelCount - from + 1);
                    const int to = qrand() % (modelCount - count + 1);
                    const int moveId = moveCount++;
                    set.move(from, to, count, moveId);
                    input.append(Move(from, to, count, moveId));
                    break;
                }
                default:
                    break;
                }
            }
            accumulatedSet.apply(set);
        }

        SignalList output;
        foreach (const QQmlChangeSet::Change &remove, accumulatedSet.removes())
            output << Remove(remove.index, remove.count, remove.moveId, remove.offset);
        foreach (const QQmlChangeSet::Change &insert, accumulatedSet.inserts())
            output << Insert(insert.index, insert.count, insert.moveId, insert.offset);

        QVector<int> inputList;
        for (int i = 0; i < 40; ++i)
            inputList.append(i);
        QVector<int> outputList = inputList;
        if (!applyChanges(inputList, input)) {
            qDebug() << "Invalid input list";
            qDebug() << input;
            qDebug() << inputList;
            ++failures;
        } else if (!applyChanges(outputList, output)) {
            qDebug() << "Invalid output list";
            qDebug() << input;
            qDebug() << output;
            qDebug() << outputList;
            ++failures;
        } else if (outputList != inputList) {
            qDebug() << "Input/output mismatch";
            qDebug() << input;
            qDebug() << output;
            qDebug() << inputList;
            qDebug() << outputList;
            ++failures;
        }
    }
    QCOMPARE(failures, 0);
}

QTEST_MAIN(tst_qqmlchangeset)

#include "tst_qqmlchangeset.moc"
