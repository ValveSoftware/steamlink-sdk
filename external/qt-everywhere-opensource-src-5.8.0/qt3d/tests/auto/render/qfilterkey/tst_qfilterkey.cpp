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
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QFilterKey : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QFilterKey filterKey;

        // THEN
        QCOMPARE(filterKey.value(), QVariant());
        QCOMPARE(filterKey.name(), QString());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QFilterKey filterKey;

        {
            // WHEN
            QSignalSpy spy(&filterKey, SIGNAL(valueChanged(QVariant)));
            const QVariant newValue = QVariant("Tim");
            filterKey.setValue(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(filterKey.value(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            filterKey.setValue(newValue);

            // THEN
            QCOMPARE(filterKey.value(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&filterKey, SIGNAL(nameChanged(QString)));
            const QString newValue = QStringLiteral("Darius");
            filterKey.setName(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(filterKey.name(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            filterKey.setName(newValue);

            // THEN
            QCOMPARE(filterKey.name(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QFilterKey filterKey;

        filterKey.setValue(QVariant(QStringLiteral("Taylor")));
        filterKey.setName(QStringLiteral("Craig"));

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&filterKey);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QFilterKeyData>>(creationChanges.first());
            const Qt3DRender::QFilterKeyData cloneData = creationChangeData->data;

            QCOMPARE(filterKey.value(), cloneData.value);
            QCOMPARE(filterKey.name(), cloneData.name);
            QCOMPARE(filterKey.id(), creationChangeData->subjectId());
            QCOMPARE(filterKey.isEnabled(), true);
            QCOMPARE(filterKey.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(filterKey.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        filterKey.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&filterKey);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QFilterKeyData>>(creationChanges.first());
            const Qt3DRender::QFilterKeyData cloneData = creationChangeData->data;

            QCOMPARE(filterKey.value(), cloneData.value);
            QCOMPARE(filterKey.name(), cloneData.name);
            QCOMPARE(filterKey.id(), creationChangeData->subjectId());
            QCOMPARE(filterKey.isEnabled(), false);
            QCOMPARE(filterKey.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(filterKey.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkValueUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QFilterKey filterKey;
        arbiter.setArbiterOnNode(&filterKey);

        {
            // WHEN
            filterKey.setValue(QVariant(427));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "value");
            QCOMPARE(change->value().value<QVariant>(), filterKey.value());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            filterKey.setValue(QVariant(427));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkNameUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QFilterKey filterKey;
        arbiter.setArbiterOnNode(&filterKey);

        {
            // WHEN
            filterKey.setName(QStringLiteral("Easton"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "name");
            QCOMPARE(change->value().value<QString>(), filterKey.name());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            filterKey.setName(QStringLiteral("Easton"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QFilterKey)

#include "tst_qfilterkey.moc"
