/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "finaleffect.h"

#include <Qt3DRender/QGraphicsApiFilter>
#include <Qt3DRender/QShaderProgram>
#include <QUrl>

QT_BEGIN_NAMESPACE

FinalEffect::FinalEffect(Qt3DCore::QNode *parent)
    : Qt3DRender::QEffect(parent)
    , m_gl3Technique(new Qt3DRender::QTechnique())
    , m_gl2Technique(new Qt3DRender::QTechnique())
    , m_gl2Pass(new Qt3DRender::QRenderPass())
    , m_gl3Pass(new Qt3DRender::QRenderPass())
    , m_passCriterion(new Qt3DRender::QFilterKey(this))
{
    m_gl3Technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    m_gl3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_gl3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_gl3Technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

    m_gl2Technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    m_gl2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_gl2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_gl2Technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::NoProfile);

    m_passCriterion->setName(QStringLiteral("pass"));
    m_passCriterion->setValue(QStringLiteral("final"));

    Qt3DRender::QShaderProgram *gl3Shader = new Qt3DRender::QShaderProgram();
    gl3Shader->setVertexShaderCode(gl3Shader->loadSource(QUrl(QStringLiteral("qrc:/final_gl3.vert"))));
    gl3Shader->setFragmentShaderCode(gl3Shader->loadSource(QUrl(QStringLiteral("qrc:/final_gl3.frag"))));

    m_gl3Pass->addFilterKey(m_passCriterion);
    m_gl3Pass->setShaderProgram(gl3Shader);
    m_gl3Technique->addRenderPass(m_gl3Pass);

    Qt3DRender::QShaderProgram *gl2Shader = new Qt3DRender::QShaderProgram();
    gl2Shader->setVertexShaderCode(gl2Shader->loadSource(QUrl(QStringLiteral("qrc:/final_gl2.vert"))));
    gl2Shader->setFragmentShaderCode(gl2Shader->loadSource(QUrl(QStringLiteral("qrc:/final_gl2.frag"))));

    m_gl2Pass->addFilterKey(m_passCriterion);
    m_gl2Pass->setShaderProgram(gl2Shader);
    m_gl2Technique->addRenderPass(m_gl2Pass);

    addTechnique(m_gl3Technique);
    addTechnique(m_gl2Technique);
}

QList<Qt3DRender::QFilterKey *> FinalEffect::passCriteria() const
{
    return QList<Qt3DRender::QFilterKey *>() << m_passCriterion;
}

QT_END_NAMESPACE
