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
#include <Qt3DCore/private/qhandlemanager_p.h>
#include <Qt3DCore/private/qhandle_p.h>

class tst_HandleManager : public QObject
{
    Q_OBJECT
public:
    tst_HandleManager() {}
    ~tst_HandleManager() {}

private slots:
    void construction();
    void correctPointer();
    void correctPointers();
    void correctConstPointer();
    void nullForRemovedEntry();
    void validHandleForReplacementEntry();
    void updateChangesValue();
    void resetRemovesAllEntries();
    void maximumEntries();
    void checkNoCounterOverflow();
};

class SimpleResource
{
public:
    SimpleResource()
        : m_value(0)
    {}

    int m_value;
};

typedef Qt3DCore::QHandle<SimpleResource> Handle;

void tst_HandleManager::construction()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;

    // THEN
    QVERIFY(manager.activeEntries() == 0);
}

void tst_HandleManager::correctPointer()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;
    SimpleResource *p1 = (SimpleResource *)(quintptr)0xdeadbeef;

    // WHEN
    const Handle h = manager.acquire(p1);

    bool ok = false;
    SimpleResource *p2 = manager.data(h, &ok);

    // THEN
    QVERIFY(ok == true);
    QVERIFY(p1 == p2);
}

void tst_HandleManager::correctPointers()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;
    SimpleResource *p[3];
    p[0] = (SimpleResource *)(quintptr)0xdeadbeef;
    p[1] = (SimpleResource *)(quintptr)0x11111111;
    p[2] = (SimpleResource *)(quintptr)0x22222222;

    // WHEN
    for (int i = 0; i < 3; ++i) {
        // WHEN
        const Handle h = manager.acquire(p[i]);

        bool ok = false;
        SimpleResource *q = manager.data(h, &ok);

        // THEN
        QVERIFY(ok == true);
        QVERIFY(p[i] == q);
    }

    // THEN
    QVERIFY(manager.activeEntries() == 3);
}

void tst_HandleManager::correctConstPointer()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;
    QSharedPointer<SimpleResource> p1(new SimpleResource);
    const Handle h = manager.acquire(p1.data());

    // WHEN
    bool ok = false;
    const SimpleResource *p2 = manager.constData(h, &ok);

    // THEN
    QVERIFY(ok == true);
    QVERIFY(p1.data() == p2);
}

void tst_HandleManager::nullForRemovedEntry()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;
    QSharedPointer<SimpleResource> p1(new SimpleResource);
    const Handle h = manager.acquire(p1.data());

    // WHEN
    manager.release(h);

    // THEN
    bool ok = false;
    SimpleResource *p2 = manager.data(h, &ok);
    QVERIFY(ok == false);
    QVERIFY(p2 == nullptr);
}

void tst_HandleManager::validHandleForReplacementEntry()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;
    QSharedPointer<SimpleResource> p1(new SimpleResource);
    const Handle h = manager.acquire(p1.data());

    // THEN
    QVERIFY(manager.activeEntries() == 1);

    // WHEN
    manager.release(h);

    // THEN
    QVERIFY(manager.activeEntries() == 0);

    // WHEN
    QSharedPointer<SimpleResource> p2(new SimpleResource);
    const Handle h2 = manager.acquire(p2.data());

    // THEN
    QVERIFY(h2.isNull() == false);
    QVERIFY(h2.counter() == 2);
    QVERIFY(manager.activeEntries() == 1);

    // WHEN
    bool ok = false;
    SimpleResource *p3 = manager.data(h2, &ok);

    // THEN
    QVERIFY(ok == true);
    QVERIFY(p3 == p2);
}

void tst_HandleManager::updateChangesValue()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;
    QSharedPointer<SimpleResource> p1(new SimpleResource);
    const Handle h = manager.acquire(p1.data());

    // WHEN
    QSharedPointer<SimpleResource> p2(new SimpleResource);
    manager.update(h, p2.data());

    // THEN
    QVERIFY(manager.activeEntries() == 1);

    // WHEN
    bool ok = false;
    SimpleResource *p3 = manager.data(h, &ok);

    // THEN
    QVERIFY(ok == true);
    QVERIFY(p3 == p2);
}

void tst_HandleManager::resetRemovesAllEntries()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;

    // WHEN
    for (int i = 0; i < 100; ++i) {
        SimpleResource *p = (SimpleResource *)(quintptr)(0xdead0000 + i);
        const Handle h = manager.acquire(p);

        bool ok = false;
        SimpleResource *q = manager.data(h, &ok);
        QVERIFY(ok == true);
        QVERIFY(p == q);
    }

    // THEN
    QVERIFY(manager.activeEntries() == 100);

    // WHEN
    manager.reset();

    // THEN
    QVERIFY(manager.activeEntries() == 0);
}

void tst_HandleManager::maximumEntries()
{
    // GIVEN
    Qt3DCore::QHandleManager<SimpleResource> manager;

    // THEN
    QCOMPARE(Handle::maxIndex(), (uint)((1 << 16) - 1));

    // WHEN
    for (int i = 0; i < (int)Handle::maxIndex(); ++i) {
        SimpleResource *p = (SimpleResource *)(quintptr)(0xdead0000 + i);
        const Handle h = manager.acquire(p);

        bool ok = false;
        SimpleResource *q = manager.data(h, &ok);
        QVERIFY(ok == true);
        QVERIFY(p == q);
    }

    // THEN
    QVERIFY(manager.activeEntries() == Handle::maxIndex());\

    // WHEN
    manager.reset();

    // THEN
    QVERIFY(manager.activeEntries() == 0);
}

void tst_HandleManager::checkNoCounterOverflow()
{
    // GIVEN
    const int indexBits = 16;
    Qt3DCore::QHandleManager<SimpleResource, indexBits> manager;
    SimpleResource *p = (SimpleResource *)(quintptr)0xdead0000;
    Qt3DCore::QHandle<SimpleResource, indexBits> h = manager.acquire(p);

    // THEN
    QCOMPARE(h.maxCounter(), (quint32)((1 << (32 - indexBits - 2)) - 1));
    QCOMPARE(h.counter(), (quint32)1);

    // WHEN
    const quint32 maxIterations = h.maxCounter() - 2;
    const quint32 handleIndex = h.index();

    qDebug() << maxIterations << handleIndex;

    // Acquire and release maxIteration time to increase counter
    for (quint32 i = 0; i < maxIterations; ++i) {
        // WHEN
        manager.release(h);
        h = manager.acquire(p);

        // THEN
        QCOMPARE(h.index(), handleIndex);
        QCOMPARE(h.counter(), i + 2);
    }

    // THEN
    QCOMPARE(h.counter(), h.maxCounter() - 1);

    // WHEN
    manager.release(h);
    h = manager.acquire(p);

    // THEN
    QCOMPARE(h.counter(), (quint32)1);
}



QTEST_APPLESS_MAIN(tst_HandleManager)

#include "tst_handlemanager.moc"
