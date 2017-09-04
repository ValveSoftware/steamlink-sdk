/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include <QtTest/qtest.h>

#include <QtCore/QScopedPointer>
#include <QtCore/private/qfactoryloader_p.h>

#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qgeometry.h>

#include <Qt3DRender/private/qgeometryloaderfactory_p.h>
#include <Qt3DRender/private/qgeometryloaderinterface_p.h>

#include "../../../../src/plugins/geometryloaders/qtgeometryloaders-config.h"

using namespace Qt3DRender;

Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, geometryLoader,
    (QGeometryLoaderFactory_iid, QLatin1String("/geometryloaders"), Qt::CaseInsensitive))

class tst_geometryloaders : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testOBJLoader();
    void testPLYLoader();
    void testSTLLoader();
    void testGLTFLoader();
#ifdef QT_3DGEOMETRYLOADERS_FBX
    void testFBXLoader();
#endif
};

void tst_geometryloaders::testOBJLoader()
{
    QScopedPointer<QGeometryLoaderInterface> loader;
    loader.reset(qLoadPlugin<QGeometryLoaderInterface, QGeometryLoaderFactory>(geometryLoader(), QStringLiteral("obj")));
    QVERIFY(loader);
    if (!loader)
        return;

    QFile file(QStringLiteral(":/cube.obj"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug("Could not open test file for reading");
        return;
    }

    bool loaded = loader->load(&file, QStringLiteral("Cube"));
    QVERIFY(loaded);
    if (!loaded)
        return;

    QGeometry *geometry = loader->geometry();
    QVERIFY(geometry);
    if (!geometry)
        return;

    QCOMPARE(geometry->attributes().count(), 3);
    for (QAttribute *attr : geometry->attributes()) {
        switch (attr->attributeType()) {
        case QAttribute::IndexAttribute:
            QCOMPARE(attr->count(), 36u);
            break;
        case QAttribute::VertexAttribute:
            QCOMPARE(attr->count(), 24u);
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    file.close();
}

void tst_geometryloaders::testPLYLoader()
{
    QScopedPointer<QGeometryLoaderInterface> loader;
    loader.reset(qLoadPlugin<QGeometryLoaderInterface, QGeometryLoaderFactory>(geometryLoader(), QStringLiteral("ply")));
    QVERIFY(loader);
    if (!loader)
        return;

    QFile file(QStringLiteral(":/cube.ply"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug("Could not open test file for reading");
        return;
    }

    bool loaded = loader->load(&file, QStringLiteral("Cube"));
    QVERIFY(loaded);
    if (!loaded)
        return;

    QGeometry *geometry = loader->geometry();
    QVERIFY(geometry);
    if (!geometry)
        return;

    QCOMPARE(geometry->attributes().count(), 3);
    for (QAttribute *attr : geometry->attributes()) {
        switch (attr->attributeType()) {
        case QAttribute::IndexAttribute:
            QCOMPARE(attr->count(), 36u);
            break;
        case QAttribute::VertexAttribute:
            QCOMPARE(attr->count(), 24u);
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    file.close();
}

void tst_geometryloaders::testSTLLoader()
{
    QScopedPointer<QGeometryLoaderInterface> loader;
    loader.reset(qLoadPlugin<QGeometryLoaderInterface, QGeometryLoaderFactory>(geometryLoader(), QStringLiteral("stl")));
    QVERIFY(loader);
    if (!loader)
        return;

    QFile file(QStringLiteral(":/cube.stl"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug("Could not open test file for reading");
        return;
    }

    bool loaded = loader->load(&file, QStringLiteral("Cube"));
    QVERIFY(loaded);
    if (!loaded)
        return;

    QGeometry *geometry = loader->geometry();
    QVERIFY(geometry);
    if (!geometry)
        return;

    QCOMPARE(geometry->attributes().count(), 3);
    for (QAttribute *attr : geometry->attributes()) {
        QCOMPARE(attr->count(), 36u);
    }

    file.close();
}

void tst_geometryloaders::testGLTFLoader()
{
    QScopedPointer<QGeometryLoaderInterface> loader;
    loader.reset(qLoadPlugin<QGeometryLoaderInterface, QGeometryLoaderFactory>(geometryLoader(), QStringLiteral("gltf")));
    QVERIFY(loader);
    if (!loader)
        return;

    QFile file(QStringLiteral(":/cube.gltf"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug("Could not open test file for reading");
        return;
    }

    bool loaded = loader->load(&file, QStringLiteral("Cube"));
    QVERIFY(loaded);
    if (!loaded)
        return;

    QGeometry *geometry = loader->geometry();
    QVERIFY(geometry);
    if (!geometry)
        return;

    QCOMPARE(geometry->attributes().count(), 3);
    for (QAttribute *attr : geometry->attributes()) {
        switch (attr->attributeType()) {
        case QAttribute::IndexAttribute:
            QCOMPARE(attr->count(), 36u);
            break;
        case QAttribute::VertexAttribute:
            QCOMPARE(attr->count(), 24u);
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
    }

    file.close();
}

#ifdef QT_3DGEOMETRYLOADERS_FBX
void tst_geometryloaders::testFBXLoader()
{
    QScopedPointer<QGeometryLoaderInterface> loader;
    loader.reset(qLoadPlugin<QGeometryLoaderInterface, QGeometryLoaderFactory>(geometryLoader(), QStringLiteral("fbx")));
    QVERIFY(loader);
    if (!loader)
        return;

    QFile file(QStringLiteral(":/cube.fbx"));
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug("Could not open test file for reading");
        return;
    }

    bool loaded = loader->load(&file, QStringLiteral("Cube"));
    QVERIFY(loaded);
    if (!loaded)
        return;

    QGeometry *geometry = loader->geometry();
    QVERIFY(geometry);
    if (!geometry)
        return;

    QCOMPARE(geometry->attributes().count(), 3);
    for (QAttribute *attr : geometry->attributes()) {
        switch (attr->attributeType()) {
        case QAttribute::IndexAttribute:
            QCOMPARE(attr->count(), 36u);
            break;
        case QAttribute::VertexAttribute:
            QCOMPARE(attr->count(), 24u);
            break;
        }
    }

    file.close();
}
#endif

QTEST_MAIN(tst_geometryloaders)

#include "tst_geometryloaders.moc"
