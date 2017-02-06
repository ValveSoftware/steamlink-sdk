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

#include <QtTest/QtTest>
#include <private/qframeallocator_p.h>
#include <private/qframeallocator_p_p.h>

class tst_QFrameAllocator : public QObject
{
    Q_OBJECT
public:
    tst_QFrameAllocator() : QObject() {}
    ~tst_QFrameAllocator() {}

    struct small
    {
        char c;
    };

    struct medium
    {
        int a, b, c;
    };

    struct big
    {
        int t[8];
    };

    struct huge
    {
        int t[64];
    };

    struct composed
    {
        QVector<int> vec;
        QList<int> list;
        int t;
    };

    struct subclass : public composed
    {
        float toto;
        // add some more data to force the subclass into a different bucket
        char data[32];
    };

private slots:

    void initQFrameChunk();
    void singleAllocationQFrameChunk();
    void qvectorAllocationQFrameChunk();
    void multipleAllocationQFrameChunk();
    void overflowAllocationQFrameChunk();
    void singleDeallocationQFrameChunk();
    void multipleDeallocationQFrameChunk();
    void containsCheckQFrameChunk();
    void clearQFrameChunk();
    void initQFixedFrameAllocator();
    void allocateQFixedFrameAllocator();
    void deallocateQFixedFrameAllocator();
    void checkFragmentationQFixedFrameAllocator();
    void clearQFixedFrameAllocator();
    void initQFrameAllocator();
    void allocateWithQFrameAllocator();
    void deallocateWithQFrameAllocator();
    void testAllocationComposedValues();
    void allocateSubclass();
    void deallocateSubclass();
    void clearQFrameAllocator();
    void isEmptyQFrameAllocator();
    void subclassPointerClearing();
    void allocateWithRawMemoryDeallocate();
    void allocateDeallocateRawMemory();
};

void tst_QFrameAllocator::initQFrameChunk()
{
    Qt3DCore::QFrameChunk c;

    c.init(16, 12);

    QVERIFY(c.m_blocksAvailable == c.m_maxBlocksAvailable);
    QCOMPARE(c.m_blocksAvailable, (uchar)12);
    QCOMPARE(c.m_firstAvailableBlock, (uchar)0);
    QVERIFY(c.m_data != nullptr);
    QVERIFY(c.isEmpty());

    for (int i = 0; i < 12; i++) {
        QCOMPARE(c.m_data[i * 16], (uchar)(i + 1));
    }
}

void tst_QFrameAllocator::singleAllocationQFrameChunk()
{
    Qt3DCore::QFrameChunk c;

    c.init(16, 12);

    void *ptr = c.allocate(16);

    QVERIFY(c.contains(ptr, 16));
    QVERIFY(!c.isEmpty());
    QCOMPARE(c.m_blocksAvailable, (uchar)(c.m_maxBlocksAvailable - 1));
    QVERIFY(ptr != nullptr);
    QCOMPARE(c.m_firstAvailableBlock, (uchar)1);
    QCOMPARE(*((uchar*)ptr), (uchar)1);
}

void tst_QFrameAllocator::qvectorAllocationQFrameChunk()
{
    Qt3DCore::QFrameChunk c;

    c.init(16, 12);

    QVector<int> *v = nullptr;

    QVERIFY(sizeof(v) < 16);
    v = static_cast<QVector<int> *>(c.allocate(16));
    QVERIFY(v != nullptr);
    new (v) QVector<int>();
    for (int i = 0; i < 1024; i++)
        v->append(i);
}

void tst_QFrameAllocator::multipleAllocationQFrameChunk()
{
    Qt3DCore::QFrameChunk c;

    c.init(16, 12);

    QList<void *> ptrs;
    for (int i = 0; i < 6; i++) {
        ptrs << c.allocate(16);
    }

    QCOMPARE(c.m_maxBlocksAvailable, (uchar)12);
    QCOMPARE(c.m_blocksAvailable, (uchar)6);
    QCOMPARE(c.m_firstAvailableBlock, (uchar)6);
    QVERIFY(!c.isEmpty());
    for (int i = 0; i < 6; i++) {
        QVERIFY(ptrs.at(i) != nullptr);
        QCOMPARE(*((uchar*)ptrs.at(i)), (uchar)(i + 1));
    }
}

void tst_QFrameAllocator::overflowAllocationQFrameChunk()
{
    Qt3DCore::QFrameChunk c;

    c.init(16, 12);

    QList<void *> ptrs;
    for (int i = 0; i < 15; i++) {
        ptrs << c.allocate(16);
    }

    QCOMPARE(c.m_maxBlocksAvailable, (uchar)12);
    QCOMPARE(c.m_blocksAvailable,(uchar) 0);
    QCOMPARE(c.m_firstAvailableBlock, (uchar)12);
    QVERIFY(!c.isEmpty());
    for (int i = 0; i < 15; i++) {
        if (i < 12) {
            QVERIFY(ptrs.at(i) != nullptr);
            QCOMPARE(*((uchar*)ptrs.at(i)), (uchar)(i + 1));
        }
        else {
            QVERIFY(ptrs.at(i) == nullptr);
        }
    }
}

void tst_QFrameAllocator::singleDeallocationQFrameChunk()
{
    Qt3DCore::QFrameChunk c;

    c.init(16, 12);

    void *ptr = c.allocate(16);

    c.deallocate(ptr, 16);

    QVERIFY(c.m_blocksAvailable == c.m_maxBlocksAvailable);
    QCOMPARE(c.m_blocksAvailable, (uchar)12);
    QCOMPARE(c.m_firstAvailableBlock, (uchar)0);
    QVERIFY(c.m_data != nullptr);
    QVERIFY(c.isEmpty());

    for (int i = 0; i < 12; i++) {
        QCOMPARE(c.m_data[i * 16], (uchar)(i + 1));
    }
}

void tst_QFrameAllocator::multipleDeallocationQFrameChunk()
{
    Qt3DCore::QFrameChunk c;

    c.init(16, 12);

    QList<void *> ptrs;
    for (int i = 0; i < 6; i++) {
        ptrs << c.allocate(16);
    }

    // Free half

    for (int i = 0; i < 6; i++) {
        c.deallocate(ptrs.at(i), 16);
        QCOMPARE(c.m_firstAvailableBlock, (uchar)(i));
    }

    QVERIFY(c.m_blocksAvailable == c.m_maxBlocksAvailable);
    QCOMPARE(c.m_blocksAvailable, (uchar)12);
    QVERIFY(c.m_data != nullptr);
    QVERIFY(c.isEmpty());

    // Refill all
    ptrs.clear();
    for (int i = 0; i < 12; i++) {
        void *ptr = c.allocate(16);
        ptrs << ptr;
        QVERIFY(ptr != nullptr);
    }
    QVERIFY(!c.isEmpty());

    // Free all
    for (int i = 0; i < 12; i++) {
        c.deallocate(ptrs.at(i), 16);
    }
    QVERIFY(c.isEmpty());

    // Refill all
    ptrs.clear();

    for (int i = 0; i < 12; i++) {
        void *ptr = c.allocate(16);
        ptrs << ptr;
        QVERIFY(ptr != nullptr);
    }

    // Free all in non linear behavior
    for (int i = 0; i < 6; i++) {
        c.deallocate(ptrs.takeLast(), 16);
        c.deallocate(ptrs.takeFirst(), 16);
    }
    QVERIFY(c.isEmpty());

    // Refill all
    ptrs.clear();
    for (int i = 0; i < 12; i++) {
        void *ptr = c.allocate(16);
        ptrs << ptr;
        QVERIFY(ptr != nullptr);
    }
    QVERIFY(!c.isEmpty());
}

void tst_QFrameAllocator::containsCheckQFrameChunk()
{
    Qt3DCore::QFrameChunk c;
    Qt3DCore::QFrameChunk c2;

    c.init(16, 12);
    c2.init(16, 12);

    QList<void *> ptrs;
    for (int i = 0; i < 6; i++) {
        ptrs << c.allocate(16);
    }

    QVERIFY(!c.contains((void *)(quintptr)0xffffffff, 16));
    QVERIFY(!c2.contains((void *)(quintptr)0xffffffff, 16));
    QVERIFY(c.contains(ptrs.first(), 16));
    QVERIFY(!c2.contains(ptrs.first(), 16));
    QVERIFY(c.contains(ptrs.last(), 16));
    QVERIFY(!c2.contains(ptrs.last(), 16));

    for (int i = 0; i < 6; i++) {
        ptrs << c2.allocate(16);
    }

    QVERIFY(!c.contains((void *)(quintptr)0xffffffff, 16));
    QVERIFY(!c.contains(ptrs.last(), 16));
    QVERIFY(c.contains(ptrs.first(), 16));
    QVERIFY(c2.contains(ptrs.last(), 16));
    QVERIFY(!c2.contains(ptrs.first(), 16));
}

void tst_QFrameAllocator::clearQFrameChunk()
{
    Qt3DCore::QFrameChunk c;

    c.init(16, 12);
    QList<void *> ptrs;
    for (int i = 0; i < 6; i++) {
        ptrs << c.allocate(16);
    }
    QVERIFY(!c.isEmpty());
    c.clear(16, 12);
    QVERIFY(c.isEmpty());
    QCOMPARE(c.m_blocksAvailable, c.m_maxBlocksAvailable);
    QCOMPARE(c.m_firstAvailableBlock, (uchar)0);

    for (int i = 0; i < 12; i++) {
        QCOMPARE(c.m_data[i * 16], (uchar)(i + 1));
    }
}

void tst_QFrameAllocator::initQFixedFrameAllocator()
{
    Qt3DCore::QFixedFrameAllocator f;

    f.init(4);

    QCOMPARE(f.chunkCount(), 0);
    QCOMPARE(f.pageSize(), (uchar)128);
}

void tst_QFrameAllocator::allocateQFixedFrameAllocator()
{
    Qt3DCore::QFixedFrameAllocator f;

    f.init(4, 128);

    QList<void *> ptrs;

    // Fill 1 chunck
    for (int i = 0; i < 128; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    QCOMPARE(f.chunkCount(), 1);
    // Fill half of chunk 2
    for (int i = 0; i < 64; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    // Free chunk 2
    QCOMPARE(f.chunkCount(), 2);
    for (int i = 0; i < 64; i++) {
        f.deallocate(ptrs.takeLast());
    }
    // Allocate 2 chunks worth of data which should create a single new chunk
    // as chunk 2 is empty
    QCOMPARE(f.chunkCount(), 2);

    for (int i = 0; i < 256; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    QCOMPARE(f.chunkCount(), 3);
}

void tst_QFrameAllocator::deallocateQFixedFrameAllocator()
{
    Qt3DCore::QFixedFrameAllocator f;

    f.init(4);

    QList<void *> ptrs;

    for (int i = 0; i < 128; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    QCOMPARE(f.chunkCount(), 1);
    for (int i = 0; i < 64; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    QCOMPARE(f.chunkCount(), 2);
    for (int i = 0; i < 64; i++) {
        f.deallocate(ptrs.takeLast());
    }
    QCOMPARE(f.chunkCount(), 2);
    f.trim();
    QCOMPARE(f.chunkCount(), 1);

    for (int i = 0; i < 64; i++) {
        f.deallocate(ptrs.takeFirst());
    }
    QCOMPARE(f.chunkCount(), 1);
    for (int i = 0; i < 36; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    QCOMPARE(f.chunkCount(), 1);
}

void tst_QFrameAllocator::checkFragmentationQFixedFrameAllocator()
{
    Qt3DCore::QFixedFrameAllocator f;

    f.init(4);

    QList<void *> ptrs;

    for (int i = 0; i < 256; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }

    QCOMPARE(f.chunkCount(), 2);

    for (int i = 0; i < 64; i++) {
        f.deallocate(ptrs.takeFirst());
    }

    QCOMPARE(f.chunkCount(), 2);

    for (int i = 0; i < 64; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    QCOMPARE(f.chunkCount(), 2);
}

void tst_QFrameAllocator::clearQFixedFrameAllocator()
{
    Qt3DCore::QFixedFrameAllocator f;

    f.init(16);

    QList<void *> ptrs;

    for (int i = 0; i < 256; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    QCOMPARE(f.chunkCount(), 2);

    f.clear();
    QCOMPARE(f.chunkCount(), 2);

    for (int i = 0; i < 256; i++) {
        void *ptr = f.allocate();
        QVERIFY(ptr != nullptr);
        ptrs << ptr;
    }
    QCOMPARE(f.chunkCount(), 2);
}

void tst_QFrameAllocator::initQFrameAllocator()
{
    Qt3DCore::QFrameAllocator f(4, 4);
    QCOMPARE(f.allocatorPoolSize(), 1);


    Qt3DCore::QFrameAllocator f2(32, 4);
    QCOMPARE(f2.allocatorPoolSize(), 8);
}

void tst_QFrameAllocator::allocateWithQFrameAllocator()
{
    Qt3DCore::QFrameAllocator f2(256, 4);
    QCOMPARE(f2.allocatorPoolSize(), 256 / 4);

    QVERIFY(f2.allocate<small>());
    QVERIFY(f2.allocate<medium>());
    QVERIFY(f2.allocate<big>());
    QVERIFY(f2.allocate<huge>());

    QList<medium *> mediums;
    for (int i = 0; i < 800; i++) {
        medium *m = f2.allocate<medium>();
        QVERIFY(m);
        m->a = i;
        m->b = 800;
        m->c = m->b - m->a;
        mediums.append(m);
    }
    QCOMPARE(mediums.size(), 800);
    for (int i = 0; i < 800; i++) {
        medium *m = mediums.at(i);
        QCOMPARE(m->a, i);
        QCOMPARE(m->b, 800);
        QCOMPARE(m->c, m->b - m->a);
    }

    QList<huge *> huges;
    for (int i = 0; i < 250; i++) {
        huge *m = f2.allocate<huge>();
        QCOMPARE((int)(sizeof(m->t) / sizeof(int)), 64);
        for (int j = 0; j < 64; j++)
            m->t[j] = i * j;
        huges.append(m);
    }
    for (int i = 0; i < 250; i++) {
        huge *m = huges.at(i);
        for (int j = 0; j < 64; j++)
            QCOMPARE(m->t[j], i * j);
    }
}

void tst_QFrameAllocator::deallocateWithQFrameAllocator()
{
    Qt3DCore::QFrameAllocator f2(256, 4);
    QCOMPARE(f2.allocatorPoolSize(), 256 / 4);

    small *s = f2.allocate<small>();
    QVERIFY(s);
    medium *m = f2.allocate<medium>();
    QVERIFY(m);
    big *b = f2.allocate<big>();
    QVERIFY(b);
    huge *h = f2.allocate<huge>();
    QVERIFY(h);

    f2.deallocate(s);
    f2.deallocate(m);
    f2.deallocate(b);
    f2.deallocate(h);
}

void tst_QFrameAllocator::testAllocationComposedValues()
{
    Qt3DCore::QFrameAllocator f(128, 32);

    composed t;

    QVector<int> *v = f.allocate<QVector<int> >();

    QVERIFY(v);

    for (int i = 0; i < 10; i++)
        v->append(2);


    for (int i = 0; i < 100; i++) {
        t.vec.append(i);
        t.list.append(i);
    }

    QList<composed *> composedValues;

    for (int i = 0; i < 30; i++) {
        composed *v = f.allocate<composed>();
        QVERIFY(v);
        for (int j = 0; j < 1; j++) {
            v->vec.append(j);
            v->list.append(j);
            v->t = 100;
        }
    }

    Q_FOREACH (composed *c, composedValues) {
        QCOMPARE(c->vec.size(), 100);
        QCOMPARE(c->list.size(), 100);
        QCOMPARE(c->t, 100);

        for (int j = 0; j < 100; j++) {
            QCOMPARE(c->vec.at(j), j);
            QCOMPARE(c->list.at(j), j);
        }
    }
}

void tst_QFrameAllocator::allocateSubclass()
{
    Qt3DCore::QFrameAllocator f(128, 32);

    QList<composed *> composeds;

    for (int i = 0; i < 256; i++) {
        // Allocate a composed object of size subclass
        // c is actually a subclass
        composed *c = static_cast<composed*>(f.allocateRawMemory(sizeof(subclass)));
        composeds << c;
    }

    QCOMPARE(composeds.count(), 256);

    Q_FOREACH (composed *c, composeds) {
        subclass *s = static_cast<subclass *>(c);
        s->toto = 2586.0f;
    }

    for (int i = 0; i < 256; i++) {
        subclass *s = static_cast<subclass *>(composeds.takeLast());
        QVERIFY(s->toto == 2586.0f);
    }
}

void tst_QFrameAllocator::deallocateSubclass()
{
    Qt3DCore::QFrameAllocator f(128, 32);


    const int NUM_ITEMS = 256;
    QVector<void *> allocated(NUM_ITEMS);
    uint chunkCount = 0;

    for (int l = 0; l < 6; l++) {

        for (int i = 0; i < NUM_ITEMS; i++) {
            // Allocate a composed object of size subclass
            // c is actually a subclass
            allocated[i] = f.allocateRawMemory(sizeof(subclass));
        }

        if (!chunkCount)
            chunkCount = f.totalChunkCount();
        else
            QCOMPARE(chunkCount, f.totalChunkCount());

        for (int i = 0; i < NUM_ITEMS; i++) {
            f.deallocateRawMemory(allocated[i], sizeof(subclass));
        }
    }

}

void tst_QFrameAllocator::clearQFrameAllocator()
{
    Qt3DCore::QFrameAllocator f(128, 32);

    for (int i = 0; i < 256; i++) {
        f.allocate<composed>();
    }
    uint chunkCount = f.totalChunkCount();
    f.clear();
    QCOMPARE(chunkCount, f.totalChunkCount());

    for (int i = 0; i < 256; i++) {
        QList<composed *> composeds;
        for (int j = 0; j < 256; j++) {
            composed *c = f.allocate<composed>();
            c ->t = i * j;
            composeds << c;
        }
        QCOMPARE(chunkCount, f.totalChunkCount());
        for (int j = 0; j < 256; j++) {
            QCOMPARE(composeds.at(j)->t, i * j);
        }
        f.clear();
        QCOMPARE(chunkCount, f.totalChunkCount());
    }
}

void tst_QFrameAllocator::isEmptyQFrameAllocator()
{
    // GIVEN
    Qt3DCore::QFrameAllocator f(128, 32);


    // WHEN
    for (int i = 0; i < 256; ++i)
        f.allocate<composed>();
    // THEN
    QVERIFY(!f.isEmpty());

    // WHEN
    f.clear();
    // THEN
    QVERIFY(f.isEmpty());

    for (int i = 0; i < 256; ++i) {

        // GIVEN
        QVector<composed *> composeds;
        for (int j = 0; j < 256; ++j) {
            composed *c = f.allocate<composed>();
            c ->t = i * j;
            composeds << c;
        }
        // THEN
        QVERIFY(!f.isEmpty());

        // WHEN
        for (int j = 0; j < 256; ++j)
            f.deallocate(composeds.takeAt(0));
        // THEN
        QVERIFY(f.isEmpty());
    }
}

void tst_QFrameAllocator::subclassPointerClearing()
{
    // GIVEN
    Qt3DCore::QFrameAllocator f(128, 32);
    QVector<composed *> data;

    // WHEN
    for (int i = 0; i < 32; ++i) {
        composed *c = (i % 2 == 0) ? f.allocate<composed>() : f.allocate<subclass>();
        data.append(c);
    }
    // THEN
    QVERIFY(!f.isEmpty());

    // WHEN
    for (int j = 0; j < 32; ++j) {
        if ((j % 2 == 0))
            f.deallocate<composed>(data.takeAt(0));
        else
            f.deallocate<subclass>(static_cast<subclass *>(data.takeAt(0)));
    }

    // THEN
    QVERIFY(f.isEmpty());
}

void tst_QFrameAllocator::allocateWithRawMemoryDeallocate()
{
    // GIVEN
    Qt3DCore::QFrameAllocator f(128, 32);
    subclass *s = static_cast<subclass *>(f.allocateRawMemory(sizeof(subclass)));
    new (s) subclass();

    // WHEN
    f.deallocate(s);

    // THEN
    QVERIFY(f.isEmpty());
}

void tst_QFrameAllocator::allocateDeallocateRawMemory()
{
    // GIVEN
    Qt3DCore::QFrameAllocator f(128, 32);
    subclass *s = f.allocate<subclass>();

    // WHEN
    f.deallocateRawMemory(s, sizeof(subclass));

    // THEN
    QVERIFY(f.isEmpty());
}

QTEST_APPLESS_MAIN(tst_QFrameAllocator)

#include "tst_qframeallocator.moc"
