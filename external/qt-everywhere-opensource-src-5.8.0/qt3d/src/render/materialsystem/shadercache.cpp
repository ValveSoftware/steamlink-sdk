/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "shadercache_p.h"

#include <QtCore/QMutexLocker>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

/*!
 * \internal
 *
 * Destroys the ShaderCache and deletes all cached QOpenGLShaderPrograms
 */
ShaderCache::~ShaderCache()
{
    qDeleteAll(m_programHash);
}

/*!
 * \internal
 *
 * Looks up the QOpenGLShaderProgram corresponding to \a dna. Also checks to see if the cache
 * contains a reference to this shader program from the Shader with peerId of \a shaderPeerId.
 * If there is no existing reference for \a shaderPeerId, one is recorded.
 *
 * \return A pointer to the shader program if it is cached, nullptr otherwise
 */
QOpenGLShaderProgram *ShaderCache::getShaderProgramAndAddRef(ProgramDNA dna, Qt3DCore::QNodeId shaderPeerId)
{
    auto shaderProgram = m_programHash.value(dna, nullptr);
    if (shaderProgram) {
        // Ensure we store the fact that shaderPeerId references this shader
        QMutexLocker lock(&m_refsMutex);
        QVector<Qt3DCore::QNodeId> &programRefs = m_programRefs[dna];
        auto it = std::lower_bound(programRefs.begin(), programRefs.end(), shaderPeerId);
        if (*it != shaderPeerId)
            programRefs.insert(it, shaderPeerId);
    }
    return shaderProgram;
}

/*!
 * \internal
 *
 * Inserts the \a program in the cache indexed by \a dna and adds a reference to it from
 * \a shaderPeerId. The cache takes ownership of the \a program.
 */
void ShaderCache::insert(ProgramDNA dna, Qt3DCore::QNodeId shaderPeerId, QOpenGLShaderProgram *program)
{
    Q_ASSERT(!m_programHash.contains(dna));
    m_programHash.insert(dna, program);
    QMutexLocker lock(&m_refsMutex);
    Q_ASSERT(!m_programRefs.contains(dna));
    QVector<Qt3DCore::QNodeId> programRefs;
    programRefs.push_back(shaderPeerId);
    m_programRefs.insert(dna, programRefs);
}

/*!
 * \internal
 *
 * Removes a reference to the shader program indexed by \a dna from the Shader with peerId of
 * \a shaderPeerId. If this was the last reference, the dna and corresponding shader are added
 * to a list of items to be removed by the next call to purge.
 */
void ShaderCache::removeRef(ProgramDNA dna, Qt3DCore::QNodeId shaderPeerId)
{
    QMutexLocker lock(&m_refsMutex);
    auto it = m_programRefs.find(dna);
    if (it != m_programRefs.end()) {
        QVector<Qt3DCore::QNodeId> &programRefs = it.value();
        programRefs.removeOne(shaderPeerId);
        if (programRefs.isEmpty())
            m_pendingRemoval.append(dna);
    }
}

/*!
 * \internal
 *
 * Iterates through a list of program dna's and checks to see if they still have no references
 * from Shaders. If so, the dna and corresponding QOpenGLShaderProgram are removed from the cache.
 */
void ShaderCache::purge()
{
    QMutexLocker lock(&m_refsMutex);
    for (const ProgramDNA &dna : qAsConst(m_pendingRemoval)) {
        QVector<Qt3DCore::QNodeId> &programRefs = m_programRefs[dna];
        if (programRefs.isEmpty()) {
            delete m_programHash.take(dna);
            m_programRefs.remove(dna);
        }
    }

    m_pendingRemoval.clear();
}

/*!
 * \internal
 *
 * Deletes all cached shader programs and removes any references
 */
void ShaderCache::clear()
{
    QMutexLocker lock(&m_refsMutex);
    qDeleteAll(m_programHash);
    m_programHash.clear();
    m_programRefs.clear();
    m_pendingRemoval.clear();
}

QOpenGLShaderProgram *ShaderCache::getShaderProgramForDNA(ProgramDNA dna) const
{
    return m_programHash.value(dna, nullptr);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
