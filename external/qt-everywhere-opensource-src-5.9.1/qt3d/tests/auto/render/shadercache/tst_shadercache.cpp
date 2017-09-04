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
#include <Qt3DRender/private/shadercache_p.h>
#include <Qt3DCore/qnodeid.h>
#include <QtGui/qopenglshaderprogram.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

class tst_ShaderCache : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void insert();
    void value();
    void removeRef();
    void purge();
    void destruction();
};

void tst_ShaderCache::insert()
{
    // GIVEN
    ShaderCache cache;

    // THEN
    QCOMPARE(cache.m_programHash.isEmpty(), true);
    QCOMPARE(cache.m_programRefs.isEmpty(), true);
    QCOMPARE(cache.m_pendingRemoval.isEmpty(), true);

    // WHEN
    auto dna = ProgramDNA(12345);
    auto nodeId = QNodeId::createId();
    auto shaderProgram = new QOpenGLShaderProgram;
    cache.insert(dna, nodeId, shaderProgram);

    // THEN
    QCOMPARE(cache.m_programHash.size(), 1);
    QCOMPARE(cache.m_programHash.keys().first(), dna);
    QCOMPARE(cache.m_programHash.values().first(), shaderProgram);

    QCOMPARE(cache.m_programRefs.size(), 1);
    QCOMPARE(cache.m_programRefs.keys().first(), dna);
    QCOMPARE(cache.m_programRefs.values().first().size(), 1);
    QCOMPARE(cache.m_programRefs.values().first().first(), nodeId);

    QCOMPARE(cache.m_pendingRemoval.isEmpty(), true);
}

void tst_ShaderCache::value()
{
    // GIVEN
    ShaderCache cache;

    // WHEN
    auto dnaA = ProgramDNA(12345);
    auto nodeIdA = QNodeId::createId();
    auto shaderProgramA = new QOpenGLShaderProgram;
    cache.insert(dnaA, nodeIdA, shaderProgramA);
    auto cachedProgramA = cache.getShaderProgramAndAddRef(dnaA, nodeIdA);

    // THEN
    QCOMPARE(shaderProgramA, cachedProgramA);

    // WHEN
    auto nodeIdA2 = QNodeId::createId();
    auto cachedProgramA2 = cache.getShaderProgramAndAddRef(dnaA, nodeIdA2);

    // THEN
    QCOMPARE(shaderProgramA, cachedProgramA2);
    QCOMPARE(cache.m_programHash.size(), 1);
    QCOMPARE(cache.m_programHash.keys().first(), dnaA);
    QCOMPARE(cache.m_programHash.values().first(), shaderProgramA);

    QCOMPARE(cache.m_programRefs.size(), 1);
    QCOMPARE(cache.m_programRefs.keys().first(), dnaA);
    const QVector<Qt3DCore::QNodeId> refsA = cache.m_programRefs.values().first();
    QCOMPARE(refsA.size(), 2);
    QCOMPARE(refsA.at(0), nodeIdA);
    QCOMPARE(refsA.at(1), nodeIdA2);

    // WHEN
    auto dnaB = ProgramDNA(67890);
    auto nodeIdB = QNodeId::createId();
    auto shaderProgramB = new QOpenGLShaderProgram;
    cache.insert(dnaB, nodeIdB, shaderProgramB);

    // THEN
    QCOMPARE(cache.m_programHash.size(), 2);
    QCOMPARE(cache.m_programRefs.size(), 2);

    // WHEN
    auto cachedProgramB = cache.getShaderProgramAndAddRef(dnaB, nodeIdB);
    QCOMPARE(shaderProgramB, cachedProgramB);

    // WHEN
    auto dnaC = ProgramDNA(54321);
    auto uncachedProgram = cache.getShaderProgramAndAddRef(dnaC, nodeIdB);
    QVERIFY(uncachedProgram == nullptr);
}

void tst_ShaderCache::removeRef()
{
    // GIVEN
    ShaderCache cache;

    // WHEN we add 2 references and remove one
    auto dnaA = ProgramDNA(12345);
    auto nodeIdA = QNodeId::createId();
    auto shaderProgramA = new QOpenGLShaderProgram;
    cache.insert(dnaA, nodeIdA, shaderProgramA);
    auto cachedProgramA = cache.getShaderProgramAndAddRef(dnaA, nodeIdA);

    auto nodeIdA2 = QNodeId::createId();
    auto cachedProgramA2 = cache.getShaderProgramAndAddRef(dnaA, nodeIdA2);

    cache.removeRef(dnaA, nodeIdA);

    // THEN
    QCOMPARE(cachedProgramA, shaderProgramA);
    QCOMPARE(cachedProgramA2, shaderProgramA);
    QCOMPARE(cache.m_programHash.size(), 1);
    QCOMPARE(cache.m_programRefs.size(), 1);
    const auto refs = cache.m_programRefs.value(dnaA);
    QCOMPARE(refs.size(), 1);
    QCOMPARE(refs.first(), nodeIdA2);
    QCOMPARE(cache.m_pendingRemoval.size(), 0);

    // WHEN we remove same ref again
    cache.removeRef(dnaA, nodeIdA);

    // THEN no change
    QCOMPARE(cache.m_programHash.size(), 1);
    QCOMPARE(cache.m_programRefs.size(), 1);
    const auto refs2 = cache.m_programRefs.value(dnaA);
    QCOMPARE(refs2.size(), 1);
    QCOMPARE(refs.first(), nodeIdA2);

    // WHEN we remove other reference
    cache.removeRef(dnaA, nodeIdA2);

    // THEN
    QCOMPARE(cache.m_programHash.size(), 1);
    QCOMPARE(cache.m_programRefs.size(), 1);
    const auto refs3 = cache.m_programRefs.value(dnaA);
    QCOMPARE(refs3.size(), 0);
    QCOMPARE(cache.m_pendingRemoval.size(), 1);
    QCOMPARE(cache.m_pendingRemoval.first(), dnaA);
}

void tst_ShaderCache::purge()
{
    // GIVEN
    ShaderCache cache;

    // WHEN we add 2 references and remove one and purge
    auto dnaA = ProgramDNA(12345);
    auto nodeIdA = QNodeId::createId();
    auto shaderProgramA = new QOpenGLShaderProgram;
    QPointer<QOpenGLShaderProgram> progPointer(shaderProgramA);
    cache.insert(dnaA, nodeIdA, shaderProgramA);
    auto cachedProgramA = cache.getShaderProgramAndAddRef(dnaA, nodeIdA);

    auto nodeIdA2 = QNodeId::createId();
    auto cachedProgramA2 = cache.getShaderProgramAndAddRef(dnaA, nodeIdA2);

    cache.removeRef(dnaA, nodeIdA);
    cache.purge();

    // THEN no removal
    QCOMPARE(cachedProgramA, shaderProgramA);
    QCOMPARE(cachedProgramA2, shaderProgramA);
    QCOMPARE(cache.m_programHash.size(), 1);
    QCOMPARE(cache.m_programRefs.size(), 1);
    QCOMPARE(cache.m_pendingRemoval.isEmpty(), true);

    // WHEN we remove final ref and purge
    cache.removeRef(dnaA, nodeIdA2);
    cache.purge();

    // THEN shader program is removed from cache and deleted
    QCOMPARE(cache.m_programHash.isEmpty(), true);
    QCOMPARE(cache.m_programRefs.isEmpty(), true);
    QCOMPARE(progPointer.isNull(), true);
}

void tst_ShaderCache::destruction()
{
    // GIVEN
    auto cache = new ShaderCache;

    // WHEN
    auto dnaA = ProgramDNA(12345);
    auto nodeIdA = QNodeId::createId();
    auto shaderProgramA = new QOpenGLShaderProgram;
    QPointer<QOpenGLShaderProgram> progPointerA(shaderProgramA);

    auto dnaB = ProgramDNA(67890);
    auto nodeIdB = QNodeId::createId();
    auto shaderProgramB = new QOpenGLShaderProgram;
    QPointer<QOpenGLShaderProgram> progPointerB(shaderProgramB);

    cache->insert(dnaA, nodeIdA, shaderProgramA);
    cache->insert(dnaB, nodeIdB, shaderProgramB);
    delete cache;

    // THEN
    QCOMPARE(progPointerA.isNull(), true);
    QCOMPARE(progPointerB.isNull(), true);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

QTEST_APPLESS_MAIN(Qt3DRender::Render::tst_ShaderCache)

#include "tst_shadercache.moc"
