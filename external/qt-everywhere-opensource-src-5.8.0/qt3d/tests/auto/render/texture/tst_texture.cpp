/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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
#include <qbackendnodetester.h>
#include <Qt3DCore/qdynamicpropertyupdatedchange.h>
#include <Qt3DRender/private/texture_p.h>

#include "testpostmanarbiter.h"
#include "testrenderer.h"

class DummyTexture : public Qt3DRender::QAbstractTexture
{
    Q_OBJECT

public:
    explicit DummyTexture(Qt3DCore::QNode *parent = nullptr)
        : QAbstractTexture(TargetAutomatic, parent)
    {
    }
};

class tst_RenderTexture : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private:
    template <typename FrontendTextureType, Qt3DRender::QAbstractTexture::Target Target>
    void checkPropertyMirroring();

private slots:
    void checkFrontendPropertyNotifications();
    void checkPropertyMirroring();
    void checkPropertyChanges();
};

void tst_RenderTexture::checkFrontendPropertyNotifications()
{
    // GIVEN
    TestArbiter arbiter;
    DummyTexture texture;
    arbiter.setArbiterOnNode(&texture);

    // WHEN
    texture.setWidth(512);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 1);
    Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
    QCOMPARE(change->propertyName(), "width");
    QCOMPARE(change->value().value<int>(), 512);
    QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

    arbiter.events.clear();

    // WHEN
    texture.setWidth(512);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 0);

    // WHEN
    texture.setHeight(256);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 1);
    change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
    QCOMPARE(change->propertyName(), "height");
    QCOMPARE(change->value().value<int>(), 256);
    QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

    arbiter.events.clear();

    // WHEN
    texture.setHeight(256);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 0);

    // WHEN
    texture.setDepth(128);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 1);
    change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
    QCOMPARE(change->propertyName(), "depth");
    QCOMPARE(change->value().value<int>(), 128);
    QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

    arbiter.events.clear();

    // WHEN
    texture.setDepth(128);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 0);

    // WHEN
    texture.setLayers(16);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 1);
    change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
    QCOMPARE(change->propertyName(), "layers");
    QCOMPARE(change->value().value<int>(), 16);
    QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

    arbiter.events.clear();

    // WHEN
    texture.setLayers(16);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 0);

    // WHEN
    texture.setSamples(32);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 1);
    change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
    QCOMPARE(change->propertyName(), "samples");
    QCOMPARE(change->value().value<int>(), 32);
    QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

    arbiter.events.clear();

    // WHEN
    texture.setSamples(32);
    QCoreApplication::processEvents();

    // THEN
    QCOMPARE(arbiter.events.size(), 0);
}

template <typename FrontendTextureType, Qt3DRender::QAbstractTexture::Target Target>
void tst_RenderTexture::checkPropertyMirroring()
{
    // GIVEN
    Qt3DRender::Render::Texture backend;

    FrontendTextureType frontend;
    frontend.setWidth(256);
    frontend.setHeight(128);
    frontend.setDepth(16);
    frontend.setLayers(8);
    frontend.setSamples(32);

    // WHEN
    simulateInitialization(&frontend, &backend);

    // THEN
    QCOMPARE(backend.peerId(), frontend.id());
    QCOMPARE(backend.properties().target, Target);
    QCOMPARE(backend.properties().width, frontend.width());
    QCOMPARE(backend.properties().height, frontend.height());
    QCOMPARE(backend.properties().depth, frontend.depth());
    QCOMPARE(backend.properties().layers, frontend.layers());
    QCOMPARE(backend.properties().samples, frontend.samples());
}

void tst_RenderTexture::checkPropertyMirroring()
{
    checkPropertyMirroring<Qt3DRender::QTexture1D, Qt3DRender::QAbstractTexture::Target1D>();
    checkPropertyMirroring<Qt3DRender::QTexture1DArray, Qt3DRender::QAbstractTexture::Target1DArray>();
    checkPropertyMirroring<Qt3DRender::QTexture2D, Qt3DRender::QAbstractTexture::Target2D>();
    checkPropertyMirroring<Qt3DRender::QTexture2DArray, Qt3DRender::QAbstractTexture::Target2DArray>();
    checkPropertyMirroring<Qt3DRender::QTexture3D, Qt3DRender::QAbstractTexture::Target3D>();
    checkPropertyMirroring<Qt3DRender::QTextureCubeMap, Qt3DRender::QAbstractTexture::TargetCubeMap>();
    checkPropertyMirroring<Qt3DRender::QTextureCubeMapArray, Qt3DRender::QAbstractTexture::TargetCubeMapArray>();
    checkPropertyMirroring<Qt3DRender::QTexture2DMultisample, Qt3DRender::QAbstractTexture::Target2DMultisample>();
    checkPropertyMirroring<Qt3DRender::QTexture2DMultisampleArray, Qt3DRender::QAbstractTexture::Target2DMultisampleArray>();
    checkPropertyMirroring<Qt3DRender::QTextureRectangle, Qt3DRender::QAbstractTexture::TargetRectangle>();
    checkPropertyMirroring<Qt3DRender::QTextureBuffer, Qt3DRender::QAbstractTexture::TargetBuffer>();
}

void tst_RenderTexture::checkPropertyChanges()
{
    // GIVEN
    TestRenderer renderer;
    Qt3DRender::Render::Texture backend;
    backend.setRenderer(&renderer);

    // WHEN
    Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
    updateChange->setValue(256);
    updateChange->setPropertyName("width");
    backend.sceneChangeEvent(updateChange);

    // THEN
    QCOMPARE(backend.properties().width, 256);

    // WHEN
    updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
    updateChange->setValue(128);
    updateChange->setPropertyName("height");
    backend.sceneChangeEvent(updateChange);

    // THEN
    QCOMPARE(backend.properties().height, 128);

    // WHEN
    updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
    updateChange->setValue(16);
    updateChange->setPropertyName("depth");
    backend.sceneChangeEvent(updateChange);

    // THEN
    QCOMPARE(backend.properties().depth, 16);

    // WHEN
    updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
    updateChange->setValue(32);
    updateChange->setPropertyName("layers");
    backend.sceneChangeEvent(updateChange);

    // THEN
    QCOMPARE(backend.properties().layers, 32);

    // WHEN
    updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
    updateChange->setValue(64);
    updateChange->setPropertyName("samples");
    backend.sceneChangeEvent(updateChange);

    // THEN
    QCOMPARE(backend.properties().samples, 64);
}

QTEST_APPLESS_MAIN(tst_RenderTexture)

#include "tst_texture.moc"
