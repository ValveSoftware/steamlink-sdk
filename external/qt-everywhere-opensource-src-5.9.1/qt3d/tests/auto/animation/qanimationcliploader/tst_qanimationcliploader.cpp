/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DAnimation/qanimationcliploader.h>
#include <Qt3DAnimation/private/qanimationcliploader_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <QObject>
#include <QSignalSpy>
#include <testpostmanarbiter.h>

class tst_QAnimationClipLoader : public Qt3DAnimation::QAnimationClipLoader
{
    Q_OBJECT

private Q_SLOTS:
    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DAnimation::QAnimationClipLoader clip;

        // THEN
        QCOMPARE(clip.source(), QUrl());
        QCOMPARE(clip.duration(), 0.0f);
        QCOMPARE(clip.status(), Qt3DAnimation::QAnimationClipLoader::NotReady);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::QAnimationClipLoader clip;

        {
            // WHEN
            QSignalSpy spy(&clip, SIGNAL(sourceChanged(QUrl)));
            const QUrl newValue(QStringLiteral("qrc:/walk.qlip"));
            clip.setSource(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(clip.source(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            clip.setSource(newValue);

            // THEN
            QCOMPARE(clip.source(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DAnimation::QAnimationClipLoader clip;

        clip.setSource(QUrl(QStringLiteral("http://someRemoteURL.com")));

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&clip);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DAnimation::QAnimationClipLoaderData>>(creationChanges.first());
            const Qt3DAnimation::QAnimationClipLoaderData data = creationChangeData->data;

            QCOMPARE(clip.id(), creationChangeData->subjectId());
            QCOMPARE(clip.isEnabled(), true);
            QCOMPARE(clip.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(clip.metaObject(), creationChangeData->metaObject());
            QCOMPARE(clip.source(), data.source);
        }

        // WHEN
        clip.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&clip);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DAnimation::QAnimationClipLoaderData>>(creationChanges.first());

            QCOMPARE(clip.id(), creationChangeData->subjectId());
            QCOMPARE(clip.isEnabled(), false);
            QCOMPARE(clip.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(clip.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkSourceUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QAnimationClipLoader clip;
        arbiter.setArbiterOnNode(&clip);

        {
            // WHEN
            clip.setSource(QUrl(QStringLiteral("qrc:/toyplane.qlip")));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "source");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            clip.setSource(QStringLiteral("qrc:/toyplane.qlip"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkStatusPropertyUpdate()
    {
        // GIVEN
        qRegisterMetaType<Qt3DAnimation::QAnimationClipLoader::Status>("Status");
        TestArbiter arbiter;
        arbiter.setArbiterOnNode(this);
        QSignalSpy spy(this, SIGNAL(statusChanged(Status)));
        const Qt3DAnimation::QAnimationClipLoader::Status newStatus = Qt3DAnimation::QAnimationClipLoader::Error;

        // THEN
        QVERIFY(spy.isValid());

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr valueChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        valueChange->setPropertyName("status");
        valueChange->setValue(QVariant::fromValue(newStatus));
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(spy.count(), 1);
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(status(), newStatus);

        // WHEN
        spy.clear();
        sceneChangeEvent(valueChange);

        // THEN
        QCOMPARE(spy.count(), 0);
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(status(), newStatus);
    }
};

QTEST_MAIN(tst_QAnimationClipLoader)

#include "tst_qanimationcliploader.moc"
