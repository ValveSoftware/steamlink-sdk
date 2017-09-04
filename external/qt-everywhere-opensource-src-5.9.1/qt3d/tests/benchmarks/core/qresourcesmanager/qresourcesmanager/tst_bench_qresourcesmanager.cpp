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
#include <QMatrix4x4>
#include <Qt3DCore/private/qhandle_p.h>
#include <Qt3DCore/private/qresourcemanager_p.h>
#include <ctime>
#include <cstdlib>

class tst_QResourceManager : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void benchmarkAllocateSmallResources();
    void benchmarkAccessSmallResources();
    void benchmarkLookupSmallResources();
    void benchmarRandomAccessSmallResources();
    void benchmarkRandomLookupSmallResources();
    void benchmarkReleaseSmallResources();
    void benchmarkAllocateBigResources();
    void benchmarkAccessBigResources();
    void benchmarRandomAccessBigResources();
    void benchmarkLookupBigResources();
    void benchmarkRandomLookupBigResources();
    void benchmarkReleaseBigResources();
};

class tst_SmallArrayResource
{
public:
    tst_SmallArrayResource() : m_value(0)
    {}

    int m_value;
};

class tst_BigArrayResource
{
public:
    QMatrix4x4 m_matrix;
};

template<typename Resource>
void benchmarkAllocateResources()
{
    Qt3DCore::QResourceManager<Resource, int, 16> manager;

    volatile Resource *c;
    QBENCHMARK_ONCE {
        const int max = (1 << 16) - 1;
        for (int i = 0; i < max; i++)
            c = manager.getOrCreateResource(i);
    }
    Q_UNUSED(c);
}

template<typename Resource>
void benchmarkAccessResources()
{
    Qt3DCore::QResourceManager<Resource, int, 16> manager;
    const int max = (1 << 16) - 1;
    QVector<Qt3DCore::QHandle<Resource> > handles(max);
    for (int i = 0; i < max; i++)
        handles[i] = manager.acquire();

    volatile Resource *c;
    QBENCHMARK {
        for (int i = 0; i < max; i++)
            c = manager.data(handles[i]);
    }
    Q_UNUSED(c);
}

template<typename Resource>
void benchmarkRandomAccessResource() {
    Qt3DCore::QResourceManager<Resource, int, 16> manager;
    const int max = (1 << 16) - 1;
    QVector<Qt3DCore::QHandle<Resource> > handles(max);
    for (int i = 0; i < max; i++)
        handles[i] = manager.acquire();

    std::srand(std::time(0));
    std::random_shuffle(handles.begin(), handles.end());
    volatile Resource *c;
    QBENCHMARK {
        for (int i = 0; i < max; i++)
            c = manager.data(handles[i]);
    }
    Q_UNUSED(c);
}

template<typename Resource>
void benchmarkLookupResources()
{
    Qt3DCore::QResourceManager<Resource, int, 16> manager;
    const int max = (1 << 16) - 1;
    for (int i = 0; i < max; i++)
        manager.getOrCreateResource(i);

    volatile Resource *c;
    QBENCHMARK {
        for (int i = 0; i < max; i++)
            c = manager.lookupResource(i);
    }
    Q_UNUSED(c);
}

template<typename Resource>
void benchmarkRandomLookupResources()
{
    Qt3DCore::QResourceManager<Resource, int, 16> manager;
    const int max = (1 << 16) - 1;
    QVector<int> resourcesIndices(max);
    for (int i = 0; i < max; i++) {
        manager.getOrCreateResource(i);
        resourcesIndices[i] = i;
    }
    std::srand(std::time(0));
    std::random_shuffle(resourcesIndices.begin(), resourcesIndices.end());
    volatile Resource *c;
    QBENCHMARK {
        for (int i = 0; i < max; i++)
            c = manager.lookupResource(resourcesIndices[i]);
    }
    Q_UNUSED(c);
}

template<typename Resource>
void benchmarkReleaseResources()
{
    Qt3DCore::QResourceManager<Resource, int, 16> manager;
    const int max = (1 << 16) - 1;
    QVector<Qt3DCore::QHandle<Resource> > handles(max);
    for (int i = 0; i < max; i++)
        handles[i] = manager.acquire();

    QBENCHMARK_ONCE {
        manager.reset();
    }
}

void tst_QResourceManager::benchmarkAllocateSmallResources()
{
    benchmarkAllocateResources<tst_SmallArrayResource>();
}

void tst_QResourceManager::benchmarkAccessSmallResources()
{
    benchmarkAccessResources<tst_SmallArrayResource>();
}

void tst_QResourceManager::benchmarkLookupSmallResources()
{
    benchmarkLookupResources<tst_SmallArrayResource>();
}

void tst_QResourceManager::benchmarRandomAccessSmallResources()
{
    benchmarkRandomAccessResource<tst_SmallArrayResource>();
}

void tst_QResourceManager::benchmarkRandomLookupSmallResources()
{
    benchmarkRandomLookupResources<tst_SmallArrayResource>();
}

void tst_QResourceManager::benchmarkReleaseSmallResources()
{
    benchmarkReleaseResources<tst_SmallArrayResource>();
}

void tst_QResourceManager::benchmarkAllocateBigResources()
{
    benchmarkAllocateResources<tst_BigArrayResource>();
}

void tst_QResourceManager::benchmarkAccessBigResources()
{
    benchmarkAccessResources<tst_BigArrayResource>();
}

void tst_QResourceManager::benchmarRandomAccessBigResources()
{
    benchmarkRandomAccessResource<tst_BigArrayResource>();
}

void tst_QResourceManager::benchmarkLookupBigResources()
{
    benchmarkLookupResources<tst_BigArrayResource>();
}

void tst_QResourceManager::benchmarkRandomLookupBigResources()
{
    benchmarkRandomLookupResources<tst_BigArrayResource>();
}

void tst_QResourceManager::benchmarkReleaseBigResources()
{
    benchmarkReleaseResources<tst_BigArrayResource>();
}

QTEST_APPLESS_MAIN(tst_QResourceManager)

#include "tst_bench_qresourcesmanager.moc"
