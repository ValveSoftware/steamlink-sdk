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

#include <Qt3DCore/private/qframeallocator_p.h>

using namespace Qt3DCore;

class tst_QFrameAllocator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void benchmarkAllocateSmall();
    void benchmarkAllocateBig();
    void benchmarkDeallocateSmall();
    void benchmarkDeallocateBig();
    void benchmarkAllocateRaw();
    void benchmarkDeallocateRaw();
};

namespace {
template<uint Size>
struct Object {
    char data[Size];
};

template<uint Size>
void benchmarkAlloc()
{
    QFrameAllocator allocator(128, 16, 128);
    QVector<Object<Size>*> items(10000);
    QBENCHMARK_ONCE {
        for (int i = 0; i < items.size(); ++i) {
            items[i] = allocator.allocate< Object<Size> >();
        }
    }
    foreach (Object<Size>* item, items) {
        allocator.deallocate(item);
    }
}

template<uint Size>
void benchmarkDealloc()
{
    QFrameAllocator allocator(128, 16, 128);
    QVector<Object<Size>*> items(10000);
    for (int i = 0; i < items.size(); ++i) {
        items[i] = allocator.allocate< Object<Size> >();
    }
    QBENCHMARK_ONCE {
        foreach (Object<Size>* item, items) {
            allocator.deallocate(item);
        }
    }
}
}

void tst_QFrameAllocator::benchmarkAllocateSmall()
{
    benchmarkAlloc<16>();
}

void tst_QFrameAllocator::benchmarkAllocateBig()
{
    benchmarkAlloc<128>();
}

void tst_QFrameAllocator::benchmarkDeallocateSmall()
{
    benchmarkDealloc<16>();
}

void tst_QFrameAllocator::benchmarkDeallocateBig()
{
    benchmarkDealloc<128>();
}

void tst_QFrameAllocator::benchmarkAllocateRaw()
{
    QFrameAllocator allocator(128, 16, 128);
    QVector<void*> items(10000);
    QBENCHMARK_ONCE {
        for (int i = 0; i < items.size(); ++i) {
            items[i] = allocator.allocateRawMemory(i % 128 + 1);
        }
    }
    for (int i = 0; i < items.size(); ++i) {
        allocator.deallocateRawMemory(items[i], i % 128 + 1);
    }
}

void tst_QFrameAllocator::benchmarkDeallocateRaw()
{
    QFrameAllocator allocator(128, 16, 128);
    QVector<void*> items(10000);
    for (int i = 0; i < items.size(); ++i) {
        items[i] = allocator.allocateRawMemory(i % 128 + 1);
    }
    QBENCHMARK_ONCE {
        for (int i = 0; i < items.size(); ++i) {
            allocator.deallocateRawMemory(items[i], i % 128 + 1);
        }
    }
}

QTEST_APPLESS_MAIN(tst_QFrameAllocator)
#include "tst_bench_qframeallocator.moc"
