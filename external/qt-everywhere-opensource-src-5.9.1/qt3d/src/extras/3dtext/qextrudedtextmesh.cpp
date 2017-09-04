/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qextrudedtextmesh.h"
#include "qextrudedtextgeometry.h"

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

/*!
 * \qmltype ExtrudedTextMesh
 * \instantiates Qt3DExtras::QExtrudedTextMesh
 * \inqmlmodule Qt3D.Extras
 * \brief A 3D extruded Text mesh.
 */

/*!
 * \qmlproperty QString ExtrudedTextMesh::text
 *
 * Holds the text used for the mesh.
 */

/*!
 * \qmlproperty QFont ExtrudedTextMesh::font
 *
 * Holds the font of the text.
 */

/*!
 * \qmlproperty float ExtrudedTextMesh::depth
 *
 * Holds the extrusion depth of the text.
 */

/*!
 * \class Qt3DExtras::QExtrudedTextMesh
 * \inheaderfile Qt3DExtras/QExtrudedTextMesh
 * \inmodule Qt3DExtras
 *
 * \inherits Qt3DRender::QGeometryRenderer
 *
 * \brief A 3D extruded Text mesh.
 */

/*!
 * Constructs a new QText3DMesh with \a parent.
 */
QExtrudedTextMesh::QExtrudedTextMesh(Qt3DCore::QNode *parent)
    : QGeometryRenderer(parent)
{
    QExtrudedTextGeometry *geometry = new QExtrudedTextGeometry();
    QObject::connect(geometry, &QExtrudedTextGeometry::depthChanged, this, &QExtrudedTextMesh::depthChanged);
    QObject::connect(geometry, &QExtrudedTextGeometry::textChanged,  this, &QExtrudedTextMesh::textChanged);
    QObject::connect(geometry, &QExtrudedTextGeometry::fontChanged,  this, &QExtrudedTextMesh::fontChanged);
    QGeometryRenderer::setGeometry(geometry);
}

/*! \internal */
QExtrudedTextMesh::~QExtrudedTextMesh()
{}

void QExtrudedTextMesh::setText(const QString &text)
{
    static_cast<QExtrudedTextGeometry*>(geometry())->setText(text);
}

void QExtrudedTextMesh::setFont(const QFont &font)
{
    static_cast<QExtrudedTextGeometry*>(geometry())->setFont(font);
}

void QExtrudedTextMesh::setDepth(float depth)
{
    static_cast<QExtrudedTextGeometry*>(geometry())->setDepth(depth);
}

/*!
 * \property QString QExtrudedTextMesh::text
 *
 * Holds the text used for the mesh.
 */
QString QExtrudedTextMesh::text() const
{
    return static_cast<QExtrudedTextGeometry*>(geometry())->text();
}

/*!
 * \property QFont QExtrudedTextMesh::font
 *
 * Holds the font of the text.
 */
QFont QExtrudedTextMesh::font() const
{
    return static_cast<QExtrudedTextGeometry*>(geometry())->font();
}

/*!
 * \property float QExtrudedTextMesh::depth
 *
 * Holds the extrusion depth of the text.
 */
float QExtrudedTextMesh::depth() const
{
    return static_cast<QExtrudedTextGeometry*>(geometry())->extrusionLength();
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
