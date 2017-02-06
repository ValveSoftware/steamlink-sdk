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
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/private/qtexture_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QTextureLoader : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QTextureLoader textureLoader;

        // THEN
        QCOMPARE(textureLoader.source(), QUrl());
        QCOMPARE(textureLoader.isMirrored(), true);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QTextureLoader textureLoader;

        {
            // WHEN
            QSignalSpy spy(&textureLoader, SIGNAL(sourceChanged(QUrl)));
            const QUrl newValue(QStringLiteral("http://msn.com"));
            textureLoader.setSource(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(textureLoader.source(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            textureLoader.setSource(newValue);

            // THEN
            QCOMPARE(textureLoader.source(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&textureLoader, SIGNAL(mirroredChanged(bool)));
            const bool newValue = false;
            textureLoader.setMirrored(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(textureLoader.isMirrored(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            textureLoader.setMirrored(newValue);

            // THEN
            QCOMPARE(textureLoader.isMirrored(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QTextureLoader textureLoader;

        textureLoader.setSource(QUrl(QStringLiteral("SomeUrl")));
        textureLoader.setMirrored(false);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&textureLoader);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QAbstractTextureData>>(creationChanges.first());
            const Qt3DRender::QAbstractTextureData cloneData = creationChangeData->data;

            QCOMPARE(textureLoader.id(), creationChangeData->subjectId());
            QCOMPARE(textureLoader.isEnabled(), true);
            QCOMPARE(textureLoader.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(textureLoader.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        textureLoader.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&textureLoader);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QAbstractTextureData>>(creationChanges.first());

            QCOMPARE(textureLoader.id(), creationChangeData->subjectId());
            QCOMPARE(textureLoader.isEnabled(), false);
            QCOMPARE(textureLoader.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(textureLoader.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkSourceUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QTextureLoader textureLoader;
        arbiter.setArbiterOnNode(&textureLoader);

        {
            // WHEN
            textureLoader.setSource(QUrl(QStringLiteral("Gary")));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            const auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "generator");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            const auto generator = qSharedPointerCast<Qt3DRender::QTextureFromSourceGenerator>(change->value().value<Qt3DRender::QTextureGeneratorPtr>());
            QVERIFY(generator);
            QCOMPARE(generator->url(), QUrl(QStringLiteral("Gary")));


            arbiter.events.clear();
        }

        {
            // WHEN
            textureLoader.setSource(QUrl(QStringLiteral("Gary")));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkMirroredUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QTextureLoader textureLoader;
        arbiter.setArbiterOnNode(&textureLoader);

        {
            // WHEN
            textureLoader.setMirrored(false);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            const auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "generator");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            const auto generator = qSharedPointerCast<Qt3DRender::QTextureFromSourceGenerator>(change->value().value<Qt3DRender::QTextureGeneratorPtr>());
            QVERIFY(generator);
            QCOMPARE(generator->isMirrored(), false);

            arbiter.events.clear();
        }

        {
            // WHEN
            textureLoader.setMirrored(false);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QTextureLoader)

#include "tst_qtextureloader.moc"
