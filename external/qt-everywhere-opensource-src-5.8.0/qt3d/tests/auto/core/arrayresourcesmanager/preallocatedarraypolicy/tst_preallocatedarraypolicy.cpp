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
#include <QList>
#include <Qt3DCore/private/qresourcemanager_p.h>
#include <Qt3DCore/private/qhandle_p.h>

class tst_PreallocatedArrayPolicy : public QObject
{
    Q_OBJECT
public:
    tst_PreallocatedArrayPolicy() {}
    ~tst_PreallocatedArrayPolicy() {}

private slots:
    void createResourcesManager();
    void acquireResources();
    void getResources();
    void registerResourcesResize();
    void removeResource();
    void resetResource();
    void lookupResource();
    void releaseResource();
    void heavyDutyMultiThreadedAccess();
    void heavyDutyMultiThreadedAccessRelease();
    void maximumNumberOfResources();
};

class tst_ArrayResource
{
public:
    tst_ArrayResource() : m_value(0)
    {}

    QAtomicInt m_value;
};

typedef Qt3DCore::QHandle<tst_ArrayResource> tHandle;
typedef Qt3DCore::QHandle<tst_ArrayResource, 4> tHandle4;
typedef Qt3DCore::QHandle<tst_ArrayResource, 8> tHandle8;
typedef Qt3DCore::QHandle<tst_ArrayResource, 16> tHandle16;

void tst_PreallocatedArrayPolicy::createResourcesManager()
{
    Qt3DCore::QResourceManager<tst_ArrayResource, int, 16, Qt3DCore::ArrayPreallocationPolicy> manager16;
    Qt3DCore::QResourceManager<tst_ArrayResource, int, 4, Qt3DCore::ArrayPreallocationPolicy> manager4;
    Qt3DCore::QResourceManager<tst_ArrayResource, int, 8, Qt3DCore::ArrayPreallocationPolicy> manager8;
    QVERIFY(manager16.maximumSize() == 65535);
    QVERIFY(manager8.maximumSize() == 255);
    QVERIFY(manager4.maximumSize() == 15);
}

/*!
 * Check that the handles returned when a registering resources
 * have a correct index and counter.
 */
void tst_PreallocatedArrayPolicy::acquireResources()
{
    Qt3DCore::QResourceManager<tst_ArrayResource, uint, 4, Qt3DCore::ArrayPreallocationPolicy> manager;

    QList<tHandle4> handles;

    for (int i = 0; i < 5; i++) {
        handles << manager.acquire();
    }

    for (uint i = 0; i < 5; i++) {
        QVERIFY(handles.at(i).index() == i);
        QVERIFY(handles.at(i).counter() == 1);
    }
}

/*!
 * Test that values can be properly retrieved.
 */
void tst_PreallocatedArrayPolicy::getResources()
{

    Qt3DCore::QResourceManager<tst_ArrayResource, int, 8, Qt3DCore::ArrayPreallocationPolicy> manager;
    QList<tst_ArrayResource *> resources;
    QList<tHandle8> handles;

    for (int i = 0; i < 5; i++) {
        handles << manager.acquire();
    }

    for (uint i = 0; i < 5; i++) {
        QVERIFY(handles.at(i).index() == i);
        QVERIFY(handles.at(i).counter() == 1);
        resources << manager.data(handles.at(i));
        QVERIFY(resources.at(i) != nullptr);
        resources.at(i)->m_value = i;
    }

    for (int i = 0; i < 5; i++)
        QVERIFY(manager.data(handles.at(i))->m_value == i);

    // Check that an invalid resource returns NULL
    tHandle8 iHandle;
    QVERIFY(manager.data(iHandle) == nullptr);

}

/*!
 * Test that when a resize of the data vectors in the manager occurs,
 * everything behaves correctly.
 */
void tst_PreallocatedArrayPolicy::registerResourcesResize()
{
    Qt3DCore::QResourceManager<tst_ArrayResource, uint, 16, Qt3DCore::ArrayPreallocationPolicy> manager;
    QList<tHandle16> handles;

    for (uint i = 0; i < 2; i++) {
        handles << manager.acquire();
        manager.data(handles.at(i))->m_value = i + 2;
    }

    for (uint i = 0; i < 5; i++) {
        handles << manager.acquire();
        manager.data(handles.at(i + 2))->m_value = i + 2 + 5;
    }

    for (int i = 0; i < 7; i++) {
        QVERIFY(handles.at(i).index() == static_cast<uint>(i));
        QVERIFY(handles.at(i).counter() == 1);
        if (i < 2)
            QVERIFY(manager.data(handles.at(i))->m_value == i + 2);
        else
            QVERIFY(manager.data(handles.at(i))->m_value == i + 5);
    }
}

/*!
 * Checks for the removal of resources.
 */
void tst_PreallocatedArrayPolicy::removeResource()
{
    Qt3DCore::QResourceManager<tst_ArrayResource, int, 16, Qt3DCore::ArrayPreallocationPolicy> manager;

    QList<tst_ArrayResource *> resources;
    QList<tHandle> handles;

    for (int i = 0; i < 32; i++) {
        handles << manager.acquire();
        resources << manager.data(handles.at(i));
    }

    manager.release(handles.at(2));
    QVERIFY(manager.data(handles.at(2)) == nullptr);
    // Triggers QASSERT so commented
    //    manager.release(handles.at(2));

    tHandle nHandle = manager.acquire();
    QVERIFY(manager.data(nHandle) != nullptr);
}

/*!
 * Checks that reset behaves correctly.
 */
void tst_PreallocatedArrayPolicy::resetResource()
{
    Qt3DCore::QResourceManager<tst_ArrayResource, uint, 16, Qt3DCore::ArrayPreallocationPolicy> manager;

    QList<tst_ArrayResource *> resources;
    QList<tHandle16> handles;

    for (int i = 0; i < 5; i++) {
        handles << manager.acquire();
        resources << manager.data(handles.at(i));
        resources.at(i)->m_value = 4;
    }
    manager.reset();
    for (uint i = 0; i < 5; i++) {
        QVERIFY(manager.data(handles.at(i)) == nullptr);
    }
    handles.clear();
    for (uint i = 0; i < 5; i++)
        handles << manager.acquire();

    for (uint i = 0; i < 5; i++) {
        QVERIFY(handles.at(i).index() == i);
        QVERIFY(handles.at(i).counter() == 1);
        QVERIFY(manager.data(handles.at(i))->m_value != 4);
    }
}

void tst_PreallocatedArrayPolicy::lookupResource()
{
    Qt3DCore::QResourceManager<tst_ArrayResource, uint, 16, Qt3DCore::ArrayPreallocationPolicy> manager;

    QList<tst_ArrayResource *> resources;
    QList<tHandle16> handles;

    for (int i = 0; i < 5; i++) {
        handles << manager.acquire();
        resources << manager.data(handles.at(i));
        resources.at(i)->m_value = 4;
    }

    tHandle16 t = manager.lookupHandle(2);
    QVERIFY(t.handle() == 0);
    QVERIFY(manager.data(t) == nullptr);
    tst_ArrayResource *resource = manager.getOrCreateResource(2);
    QVERIFY(resource != nullptr);
    t = manager.lookupHandle(2);
    QVERIFY(manager.data(t) == manager.lookupResource(2));
    QVERIFY(t == manager.getOrAcquireHandle(2));
    QVERIFY(resource == manager.getOrCreateResource(2));
    QVERIFY(manager.data(t) == resource);
}

void tst_PreallocatedArrayPolicy::releaseResource()
{
    Qt3DCore::QResourceManager<tst_ArrayResource, uint, 16, Qt3DCore::ArrayPreallocationPolicy> manager;
    QList<tst_ArrayResource *> resources;

    for (int i = 0; i < 5; i++) {
        resources << manager.getOrCreateResource(i);
    }

    for (int i = 0; i < 5; i++) {
        QVERIFY(resources.at(i) == manager.lookupResource(i));
    }

    for (int i = 0; i < 5; i++) {
        manager.releaseResource(i);
        QVERIFY(manager.lookupResource(i) == nullptr);
    }
}

class tst_Thread : public QThread
{
    Q_OBJECT
public:

    typedef Qt3DCore::QResourceManager<tst_ArrayResource,
    int,
    16,
    Qt3DCore::ArrayPreallocationPolicy,
    Qt3DCore::ObjectLevelLockingPolicy> Manager;

    tst_Thread()
        : QThread()
    {
    }

    void setManager(Manager *manager)
    {
        m_manager = manager;
    }

    // QThread interface
protected:
    void run()
    {
        int i = 0;
        int max = tHandle::maxIndex();
        while (i < max) {
            tst_ArrayResource *r = m_manager->getOrCreateResource(i);
            i++;
            QVERIFY(r != nullptr);
            Manager::Locker lock(m_manager);
            r->m_value++;
        }
        qDebug() << QThread::currentThread() << "Done";
    }

    Manager *m_manager;
};

void tst_PreallocatedArrayPolicy::heavyDutyMultiThreadedAccess()
{
    tst_Thread::Manager *manager = new tst_Thread::Manager();

    QList<tst_Thread *> threads;

    int iterations = 8;
    int max = tHandle16::maxIndex();

    for (int i = 0; i < iterations; i++) {
        tst_Thread *thread = new tst_Thread();
        thread->setManager(manager);
        threads << thread;
    }

    for (int i = 0; i < iterations; i++) {
        threads[i]->start();
    }

    for (int i = 0; i < iterations; i++) {
        threads[i]->wait();
    }

    for (int i = 0; i < max; i++) {
        QVERIFY(manager->lookupResource(i) != nullptr);
        QVERIFY(manager->lookupResource(i)->m_value = iterations);
    }

    qDeleteAll(threads);
    delete manager;
}

class tst_Thread2 : public QThread
{
    Q_OBJECT
public:

    typedef Qt3DCore::QResourceManager<tst_ArrayResource,
    int,
    16,
    Qt3DCore::ArrayPreallocationPolicy,
    Qt3DCore::ObjectLevelLockingPolicy> Manager;

    tst_Thread2(int releaseAbove = 7)
        : QThread()
        , m_releaseAbove(releaseAbove)
    {
    }

    void setManager(Manager *manager)
    {
        m_manager = manager;
    }

    // QThread interface
protected:
    void run()
    {
        int i = 0;
        int max = tHandle::maxIndex();
        while (i < max) {
            tst_ArrayResource *r = m_manager->getOrCreateResource(i);
            QVERIFY(r != nullptr);
            int oldValue = r->m_value.fetchAndAddOrdered(+1);
            if (oldValue == m_releaseAbove)
                m_manager->releaseResource(i);
            i++;
        }
        qDebug() << QThread::currentThread() << "Done";
    }

    Manager *m_manager;
    int m_releaseAbove;
};

void tst_PreallocatedArrayPolicy::heavyDutyMultiThreadedAccessRelease()
{
    tst_Thread2::Manager *manager = new tst_Thread2::Manager();

    QList<tst_Thread2 *> threads;

    int iterations = 8;
    int max = tHandle16::maxIndex();

    for (int u = 0; u < 2; u++) {

        for (int i = 0; i < iterations; i++) {
            tst_Thread2 *thread = new tst_Thread2();
            thread->setManager(manager);
            threads << thread;
        }

        for (int i = 0; i < iterations; i++) {
            threads[i]->start();
        }

        for (int i = 0; i < iterations; i++) {
            threads[i]->wait();
        }

        for (int i = 0; i < max; i++) {
            QVERIFY(manager->lookupResource(i) == nullptr);
        }

        qDeleteAll(threads);
        threads.clear();
    }

    delete manager;
}

void tst_PreallocatedArrayPolicy::maximumNumberOfResources()
{
    Qt3DCore::QResourceManager<tst_ArrayResource, uint, 16, Qt3DCore::ArrayPreallocationPolicy> manager;

    QList<tst_ArrayResource *> resources;
    QList<tHandle16> handles;

    QCOMPARE(tHandle16::maxIndex(), (uint)manager.maximumSize());

    for (int i = 0; i < manager.maximumSize(); i++) {
        handles << manager.acquire();
        resources << manager.data(handles.at(i));
        resources.at(i)->m_value = 4;
    }
}

QTEST_APPLESS_MAIN(tst_PreallocatedArrayPolicy)

#include "tst_preallocatedarraypolicy.moc"
