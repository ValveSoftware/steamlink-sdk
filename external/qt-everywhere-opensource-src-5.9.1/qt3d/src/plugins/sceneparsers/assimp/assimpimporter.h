/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QT3D_ASSIMPIMPORTER_H
#define QT3D_ASSIMPIMPORTER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

// ASSIMP LIBRARY INCLUDE
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>

#include <QtCore/QMap>
#include <QtCore/QDir>
#include <QtCore/QLoggingCategory>
#include <QtCore/QVector>

#include "assimphelpers.h"

#include <Qt3DRender/private/qsceneimporter_p.h>

QT_BEGIN_NAMESPACE

class QFile;

namespace Qt3DCore {
class QCamera;
}

namespace Qt3DAnimation {
class QKeyframeAnimation;
class QMorphingAnimation;
}

namespace Qt3DRender {

class QMaterial;
class QShaderProgram;
class QEffect;
class QAbstractTexture;
class QMesh;
class QGeometryRenderer;

Q_DECLARE_LOGGING_CATEGORY(AssimpImporterLog)

class AssimpImporter : public QSceneImporter
{
    Q_OBJECT

public:
    AssimpImporter();
    ~AssimpImporter();

    // SceneParserInterface interface
    void setSource(const QUrl& source) Q_DECL_OVERRIDE;
    bool isFileTypeSupported(const QUrl &source) const Q_DECL_OVERRIDE;
    Qt3DCore::QEntity *scene(const QString &id = QString()) Q_DECL_OVERRIDE;
    Qt3DCore::QEntity *node(const QString &id) Q_DECL_OVERRIDE;

private:
    static bool isAssimpPath(const QString &path);
    static QStringList assimpSupportedFormats();

    Qt3DCore::QEntity *node(aiNode *node);

    void readSceneFile(const QString &file);

    void cleanup();
    void parse();

    void loadMaterial(uint materialIndex);
    void loadMesh(uint meshIndex);
    void loadEmbeddedTexture(uint textureIndex);
    void loadLight(uint lightIndex);
    void loadCamera(uint cameraIndex);
    void loadAnimation(uint animationIndex);

    void copyMaterialName(QMaterial *material, aiMaterial *assimpMaterial);
    void copyMaterialColorProperties(QMaterial *material, aiMaterial *assimpMaterial);
    void copyMaterialFloatProperties(QMaterial *material, aiMaterial *assimpMaterial);
    void copyMaterialBoolProperties(QMaterial *material, aiMaterial *assimpMaterial);
    void copyMaterialShadingModel(QMaterial *material, aiMaterial *assimpMaterial);
    void copyMaterialBlendingFunction(QMaterial *material, aiMaterial *assimpMaterial);
    void copyMaterialTextures(QMaterial *material, aiMaterial *assimpMaterial);

    class SceneImporter {
    public :

        SceneImporter();
        ~SceneImporter();

        Assimp::Importer *m_importer;
        mutable const aiScene *m_aiScene;

        QMap<uint, QGeometryRenderer *> m_meshes;
        QMap<uint, QMaterial*> m_materials;
        QMap<uint, QEffect *> m_effects;
        QMap<uint, QAbstractTexture *> m_embeddedTextures;
        QMap<aiNode*, Qt3DCore::QEntity*> m_cameras;
        QHash<aiTextureType, QString> m_textureToParameterName;
        QVector<Qt3DAnimation::QKeyframeAnimation *> m_animations;
        QVector<Qt3DAnimation::QMorphingAnimation *> m_morphAnimations;
//    QMap<aiNode*, Light*> m_lights;
    };

    QDir     m_sceneDir;
    bool     m_sceneParsed;
    AssimpImporter::SceneImporter *m_scene;
    static QStringList assimpSupportedFormatsList;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3D_ASSIMPIMPORTER_H
