/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QFileDialog>
#include <QApplication>

#include <Qt3DRender/QCamera>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QAspectEngine>
#include <Qt3DInput/QInputAspect>
#include <Qt3DRender/QSceneLoader>
#include <Qt3DRender/QRenderAspect>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/qt3dwindow.h>
#include <Qt3DExtras/qfirstpersoncameracontroller.h>

class SceneWalker : public QObject
{
public:
    SceneWalker(Qt3DRender::QSceneLoader *loader) : m_loader(loader) { }

    void onStatusChanged();

private:
    void walkEntity(Qt3DCore::QEntity *e, int depth = 0);

    Qt3DRender::QSceneLoader *m_loader;
};

void SceneWalker::onStatusChanged()
{
    qDebug() << "Status changed:" << m_loader->status();
    if (m_loader->status() != Qt3DRender::QSceneLoader::Ready)
        return;

    // The QSceneLoader instance is a component of an entity. The loaded scene
    // tree is added under this entity.
    QVector<Qt3DCore::QEntity *> entities = m_loader->entities();

    // Technically there could be multiple entities referencing the scene loader
    // but sharing is discouraged, and in our case there will be one anyhow.
    if (entities.isEmpty())
        return;
    Qt3DCore::QEntity *root = entities[0];
    // Print the tree.
    walkEntity(root);

    // To access a given node (like a named mesh in the scene), use QObject::findChild().
    // The scene structure and names always depend on the asset.
    Qt3DCore::QEntity *e = root->findChild<Qt3DCore::QEntity *>(QStringLiteral("PlanePropeller_mesh")); // toyplane.obj
    if (e)
        qDebug() << "Found propeller node" << e << "with components" << e->components();
}

void SceneWalker::walkEntity(Qt3DCore::QEntity *e, int depth)
{
    Qt3DCore::QNodeVector nodes = e->childNodes();
    for (int i = 0; i < nodes.count(); ++i) {
        Qt3DCore::QNode *node = nodes[i];
        Qt3DCore::QEntity *entity = qobject_cast<Qt3DCore::QEntity *>(node);
        if (entity) {
            QString indent;
            indent.fill(' ', depth * 2);
            qDebug().noquote() << indent << "Entity:" << entity << "Components:" << entity->components();
            walkEntity(entity, depth + 1);
        }
    }
}

int main(int ac, char **av)
{
    QApplication app(ac, av);
    Qt3DExtras::Qt3DWindow view;
    view.defaultFrameGraph()->setClearColor(Qt::black);

    // Root entity
    Qt3DCore::QEntity *sceneRoot = new Qt3DCore::QEntity();

    // Scene Camera
    Qt3DRender::QCamera *camera = view.camera();
    camera->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
    camera->setViewCenter(QVector3D(0.0f, 3.5f, 0.0f));
    camera->setPosition(QVector3D(0.0f, 3.5f, 25.0f));
    camera->setNearPlane(0.001f);
    camera->setFarPlane(10000.0f);

    // For camera controls
    Qt3DExtras::QFirstPersonCameraController *camController = new Qt3DExtras::QFirstPersonCameraController(sceneRoot);
    camController->setCamera(camera);

    // Scene loader
    Qt3DCore::QEntity *sceneLoaderEntity = new Qt3DCore::QEntity(sceneRoot);
    Qt3DRender::QSceneLoader *sceneLoader = new Qt3DRender::QSceneLoader(sceneLoaderEntity);
    SceneWalker sceneWalker(sceneLoader);
    QObject::connect(sceneLoader, &Qt3DRender::QSceneLoader::statusChanged, &sceneWalker, &SceneWalker::onStatusChanged);
    sceneLoaderEntity->addComponent(sceneLoader);

    QStringList args = QCoreApplication::arguments();
    QUrl sourceFileName;
    if (args.count() <= 1) {
        QWidget *container = new QWidget();
        QFileDialog dialog;
        dialog.setFileMode(QFileDialog::AnyFile);
        sourceFileName = dialog.getOpenFileUrl(container, QStringLiteral("Open a scene file"));
    } else {
        sourceFileName = QUrl::fromLocalFile(args[1]);
    }

    if (sourceFileName.isEmpty())
        return 0;

    sceneLoader->setSource(sourceFileName);

    view.setRootEntity(sceneRoot);
    view.show();

    return app.exec();
}
