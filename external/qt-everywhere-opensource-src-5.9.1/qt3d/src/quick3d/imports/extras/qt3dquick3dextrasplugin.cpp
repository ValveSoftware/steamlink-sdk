/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qt3dquick3dextrasplugin.h"

#include <Qt3DExtras/qconegeometry.h>
#include <Qt3DExtras/qconemesh.h>
#include <Qt3DExtras/qcuboidgeometry.h>
#include <Qt3DExtras/qcuboidmesh.h>
#include <Qt3DExtras/qcylindergeometry.h>
#include <Qt3DExtras/qcylindermesh.h>
#include <Qt3DExtras/qdiffusemapmaterial.h>
#include <Qt3DExtras/qdiffusespecularmapmaterial.h>
#include <Qt3DExtras/qextrudedtextgeometry.h>
#include <Qt3DExtras/qextrudedtextmesh.h>
#include <Qt3DExtras/qfirstpersoncameracontroller.h>
#include <Qt3DExtras/qforwardrenderer.h>
#include <Qt3DExtras/qgoochmaterial.h>
#include <Qt3DExtras/qmetalroughmaterial.h>
#include <Qt3DExtras/qmorphphongmaterial.h>
#include <Qt3DExtras/qnormaldiffusemapalphamaterial.h>
#include <Qt3DExtras/qnormaldiffusemapmaterial.h>
#include <Qt3DExtras/qnormaldiffusespecularmapmaterial.h>
#include <Qt3DExtras/qorbitcameracontroller.h>
#include <Qt3DExtras/qpervertexcolormaterial.h>
#include <Qt3DExtras/qphongalphamaterial.h>
#include <Qt3DExtras/qphongmaterial.h>
#include <Qt3DExtras/qplanegeometry.h>
#include <Qt3DExtras/qplanemesh.h>
#include <Qt3DExtras/qskyboxentity.h>
#include <Qt3DExtras/qspheregeometry.h>
#include <Qt3DExtras/qspheremesh.h>
#include <Qt3DExtras/qtext2dentity.h>
#include <Qt3DExtras/qtexturedmetalroughmaterial.h>
#include <Qt3DExtras/qtexturematerial.h>
#include <Qt3DExtras/qtorusgeometry.h>
#include <Qt3DExtras/qtorusmesh.h>

#include <Qt3DQuickExtras/private/quick3dlevelofdetailloader_p.h>

#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

void Qt3DQuick3DExtrasPlugin::registerTypes(const char *uri)
{
    // Framegraphs
    qmlRegisterType<Qt3DExtras::QForwardRenderer>(uri, 2, 0, "ForwardRenderer");
    qmlRegisterRevision<Qt3DExtras::QForwardRenderer, 9>(uri, 2, 9);

    // Entities
    qmlRegisterType<Qt3DExtras::QSkyboxEntity>(uri, 2, 0, "SkyboxEntity");
    qmlRegisterRevision<Qt3DExtras::QSkyboxEntity, 9>(uri, 2, 9);
    qmlRegisterType<Qt3DExtras::Extras::Quick::Quick3DLevelOfDetailLoader>(uri, 2, 9, "LevelOfDetailLoader");

    // Camera Controllers
    qmlRegisterType<Qt3DExtras::QFirstPersonCameraController>(uri, 2, 0, "FirstPersonCameraController");
    qmlRegisterType<Qt3DExtras::QOrbitCameraController>(uri, 2, 0, "OrbitCameraController");

    // Materials
    qmlRegisterType<Qt3DExtras::QPhongMaterial>(uri, 2, 0, "PhongMaterial");
    qmlRegisterType<Qt3DExtras::QPhongAlphaMaterial>(uri, 2, 0, "PhongAlphaMaterial");
    qmlRegisterType<Qt3DExtras::QDiffuseMapMaterial>(uri, 2, 0, "DiffuseMapMaterial");
    qmlRegisterType<Qt3DExtras::QDiffuseSpecularMapMaterial>(uri, 2, 0, "DiffuseSpecularMapMaterial");
    qmlRegisterType<Qt3DExtras::QNormalDiffuseMapAlphaMaterial>(uri, 2, 0, "NormalDiffuseMapAlphaMaterial");
    qmlRegisterType<Qt3DExtras::QNormalDiffuseMapMaterial>(uri, 2, 0, "NormalDiffuseMapMaterial");
    qmlRegisterType<Qt3DExtras::QNormalDiffuseSpecularMapMaterial>(uri, 2, 0, "NormalDiffuseSpecularMapMaterial");
    qmlRegisterType<Qt3DExtras::QPerVertexColorMaterial>(uri, 2, 0, "PerVertexColorMaterial");
    qmlRegisterType<Qt3DExtras::QGoochMaterial>(uri, 2, 0, "GoochMaterial");
    qmlRegisterType<Qt3DExtras::QTextureMaterial>(uri, 2, 0, "TextureMaterial");
    qmlRegisterType<Qt3DExtras::QMetalRoughMaterial>(uri, 2, 9, "MetalRoughMaterial");
    qmlRegisterType<Qt3DExtras::QTexturedMetalRoughMaterial>(uri, 2, 9, "TexturedMetalRoughMaterial");
    qmlRegisterType<Qt3DExtras::QMorphPhongMaterial>(uri, 2, 9, "MorphPhongMaterial");

    // Meshes
    qmlRegisterType<Qt3DExtras::QConeMesh>(uri, 2, 0, "ConeMesh");
    qmlRegisterType<Qt3DExtras::QConeGeometry>(uri, 2, 0, "ConeGeometry");
    qmlRegisterType<Qt3DExtras::QCuboidMesh>(uri, 2, 0, "CuboidMesh");
    qmlRegisterType<Qt3DExtras::QCuboidGeometry>(uri, 2, 0, "CuboidGeometry");
    qmlRegisterType<Qt3DExtras::QCylinderMesh>(uri, 2, 0, "CylinderMesh");
    qmlRegisterType<Qt3DExtras::QCylinderGeometry>(uri, 2, 0, "CylinderGeometry");
    qmlRegisterType<Qt3DExtras::QPlaneMesh>(uri, 2, 0, "PlaneMesh");
    qmlRegisterRevision<Qt3DExtras::QPlaneMesh, 9>(uri, 2, 9);
    qmlRegisterType<Qt3DExtras::QPlaneGeometry>(uri, 2, 0, "PlaneGeometry");
    qmlRegisterRevision<Qt3DExtras::QPlaneGeometry, 9>(uri, 2, 9);
    qmlRegisterType<Qt3DExtras::QTorusMesh>(uri, 2, 0, "TorusMesh");
    qmlRegisterType<Qt3DExtras::QTorusGeometry>(uri, 2, 0, "TorusGeometry");
    qmlRegisterType<Qt3DExtras::QSphereMesh>(uri, 2, 0, "SphereMesh");
    qmlRegisterType<Qt3DExtras::QSphereGeometry>(uri, 2, 0, "SphereGeometry");

    // 3D Text
    qmlRegisterType<Qt3DExtras::QExtrudedTextGeometry>(uri, 2, 9, "ExtrudedTextGeometry");
    qmlRegisterType<Qt3DExtras::QExtrudedTextMesh>(uri, 2, 9, "ExtrudedTextMesh");

    qmlRegisterType<Qt3DExtras::QText2DEntity>(uri, 2, 9, "Text2DEntity");
}


QT_END_NAMESPACE
