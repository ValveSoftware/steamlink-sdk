/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
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


#include <QtTest/QTest>
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/private/qfilterkey_p.h>
#include <Qt3DRender/private/filterkey_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "qbackendnodetester.h"
#include "testrenderer.h"

class tst_FilterKey : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::FilterKey backendFilterKey;

        // THEN
        QCOMPARE(backendFilterKey.isEnabled(), false);
        QVERIFY(backendFilterKey.peerId().isNull());
        QCOMPARE(backendFilterKey.value(), QVariant());
        QCOMPARE(backendFilterKey.name(), QString());
    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DRender::Render::FilterKey backendFilterKey;

        // WHEN
        backendFilterKey.setEnabled(true);

        {
            Qt3DRender::QFilterKey filterKey;
            filterKey.setName(QStringLiteral("Tim"));
            filterKey.setValue(QVariant(QStringLiteral("McGraw")));
            simulateInitialization(&filterKey, &backendFilterKey);
        }

        backendFilterKey.cleanup();

        // THEN
        QCOMPARE(backendFilterKey.isEnabled(), false);
        QCOMPARE(backendFilterKey.value(), QVariant());
        QCOMPARE(backendFilterKey.name(), QString());
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DRender::QFilterKey filterKey;
        filterKey.setName(QStringLiteral("Dallas"));
        filterKey.setValue(QVariant(QStringLiteral("Smith")));

        {
            // WHEN
            Qt3DRender::Render::FilterKey backendFilterKey;
            simulateInitialization(&filterKey, &backendFilterKey);

            // THEN
            QCOMPARE(backendFilterKey.isEnabled(), true);
            QCOMPARE(backendFilterKey.peerId(), filterKey.id());
            QCOMPARE(backendFilterKey.value(), QVariant(QStringLiteral("Smith")));
            QCOMPARE(backendFilterKey.name(), QStringLiteral("Dallas"));
        }
        {
            // WHEN
            Qt3DRender::Render::FilterKey backendFilterKey;
            filterKey.setEnabled(false);
            simulateInitialization(&filterKey, &backendFilterKey);

            // THEN
            QCOMPARE(backendFilterKey.peerId(), filterKey.id());
            QCOMPARE(backendFilterKey.isEnabled(), false);
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DRender::Render::FilterKey backendFilterKey;
        TestRenderer renderer;
        backendFilterKey.setRenderer(&renderer);

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendFilterKey.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendFilterKey.isEnabled(), newValue);
        }
        {
            // WHEN
            const QVariant newValue(383.0f);
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("value");
            change->setValue(QVariant::fromValue(newValue));
            backendFilterKey.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendFilterKey.value(), newValue);
        }
        {
            // WHEN
            const QString newValue = QStringLiteral("Alan");
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("name");
            change->setValue(QVariant::fromValue(newValue));
            backendFilterKey.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendFilterKey.name(), newValue);
        }
    }

    void checkComparison()
    {
        // GIVEN
        Qt3DRender::Render::FilterKey backendFilterKey1;
        Qt3DRender::Render::FilterKey backendFilterKey2;
        Qt3DRender::Render::FilterKey backendFilterKey3;
        Qt3DRender::Render::FilterKey backendFilterKey4;

        // WHEN
        {
            Qt3DRender::QFilterKey filterKey1;
            filterKey1.setName(QStringLiteral("Dallas"));
            filterKey1.setValue(QVariant(QStringLiteral("Smith")));

            simulateInitialization(&filterKey1, &backendFilterKey1);
            simulateInitialization(&filterKey1, &backendFilterKey4);

            Qt3DRender::QFilterKey filterKey2;
            filterKey2.setName(QStringLiteral("Tim"));
            filterKey2.setValue(QVariant(QStringLiteral("Smith")));

            simulateInitialization(&filterKey2, &backendFilterKey2);

            Qt3DRender::QFilterKey filterKey3;
            filterKey3.setName(QStringLiteral("Dallas"));
            filterKey3.setValue(QVariant(QStringLiteral("McGraw")));

            simulateInitialization(&filterKey3, &backendFilterKey3);
        }

        // THEN
        QVERIFY(backendFilterKey1 == backendFilterKey4);
        QVERIFY(backendFilterKey4 == backendFilterKey1);
        QVERIFY(backendFilterKey1 != backendFilterKey2);
        QVERIFY(backendFilterKey2 != backendFilterKey1);
        QVERIFY(backendFilterKey1 != backendFilterKey3);
        QVERIFY(backendFilterKey3 != backendFilterKey2);
    }

};

QTEST_MAIN(tst_FilterKey)

#include "tst_filterkey.moc"
