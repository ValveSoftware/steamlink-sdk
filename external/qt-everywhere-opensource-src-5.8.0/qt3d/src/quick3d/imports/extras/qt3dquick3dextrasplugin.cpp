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
#include <Qt3DExtras/qcuboidmesh.h>
#include <Qt3DExtras/qconemesh.h>
#include <Qt3DExtras/qcylindermesh.h>
#include <Qt3DExtras/qplanemesh.h>
#include <Qt3DExtras/qspheremesh.h>
#include <Qt3DExtras/qtorusmesh.h>
#include <Qt3DExtras/qtorusgeometry.h>
#include <Qt3DExtras/qspheregeometry.h>
#include <Qt3DExtras/qcuboidgeometry.h>
#include <Qt3DExtras/qplanegeometry.h>
#include <Qt3DExtras/qconegeometry.h>
#include <Qt3DExtras/qcylindergeometry.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

static const struct {
    const char *type;
    int major, minor;
} qmldir [] = {
    // Materials
    { "PhongMaterial", 2, 0 },
    { "PhongAlphaMaterial", 2, 0 },
    { "DiffuseMapMaterial", 2, 0 },
    { "DiffuseSpecularMapMaterial", 2, 0 },
    { "NormalDiffuseMapAlphaMaterial", 2, 0 },
    { "NormalDiffuseMapMaterial", 2, 0 },
    { "NormalDiffuseSpecularMapMaterial", 2, 0 },
    { "PerVertexColorMaterial", 2, 0 },
    { "GoochMaterial", 2, 0 },
    { "TextureMaterial", 2, 0 },
    // Effects
    { "DefaultEffect", 2, 0 },
    { "DefaultAlphaEffect", 2, 0 },
    { "NormalDiffuseMapAlphaEffect", 2, 0 },
    // FrameGraphs
    { "ForwardRenderer", 2, 0 },
    // Entities
    { "SkyboxEntity", 2, 0 },
    // Camera Controllers
    { "OrbitCameraController", 2, 0 },
    { "FirstPersonCameraController", 2, 0 },
};

void Qt3DQuick3DExtrasPlugin::registerTypes(const char *uri)
{
    // Meshes
    qmlRegisterType<Qt3DExtras::QConeMesh>(uri, 2, 0, "ConeMesh");
    qmlRegisterType<Qt3DExtras::QConeGeometry>(uri, 2, 0, "ConeGeometry");
    qmlRegisterType<Qt3DExtras::QCuboidMesh>(uri, 2, 0, "CuboidMesh");
    qmlRegisterType<Qt3DExtras::QCuboidGeometry>(uri, 2, 0, "CuboidGeometry");
    qmlRegisterType<Qt3DExtras::QCylinderMesh>(uri, 2, 0, "CylinderMesh");
    qmlRegisterType<Qt3DExtras::QCylinderGeometry>(uri, 2, 0, "CylinderGeometry");
    qmlRegisterType<Qt3DExtras::QPlaneMesh>(uri, 2, 0, "PlaneMesh");
    qmlRegisterType<Qt3DExtras::QPlaneGeometry>(uri, 2, 0, "PlaneGeometry");
    qmlRegisterType<Qt3DExtras::QTorusMesh>(uri, 2, 0, "TorusMesh");
    qmlRegisterType<Qt3DExtras::QTorusGeometry>(uri, 2, 0, "TorusGeometry");
    qmlRegisterType<Qt3DExtras::QSphereMesh>(uri, 2, 0, "SphereMesh");
    qmlRegisterType<Qt3DExtras::QSphereGeometry>(uri, 2, 0, "SphereGeometry");


    // Register types provided as QML files compiled into the plugin
    for (int i = 0; i < int(sizeof(qmldir) / sizeof(qmldir[0])); i++) {
        auto path = QLatin1String("qrc:/qt-project.org/imports/Qt3D/Extras/defaults/qml/");
        qmlRegisterType(QUrl(path + qmldir[i].type + QLatin1String(".qml")),
                        uri,
                        qmldir[i].major, qmldir[i].minor,
                        qmldir[i].type);
    }
}


QT_END_NAMESPACE
