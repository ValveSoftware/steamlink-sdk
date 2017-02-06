/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtDataVisualization API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef DECLARATIVERENDERNODE_P_H
#define DECLARATIVERENDERNODE_P_H

#include "datavisualizationglobal_p.h"

#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGTextureMaterial>
#include <QtQuick/QQuickWindow>
#include <QtCore/QMutex>
#include <QtCore/QSharedPointer>
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

class Abstract3DController;
class AbstractDeclarative;

class DeclarativeRenderNode : public QObject, public QSGGeometryNode
{
    Q_OBJECT
public:
    DeclarativeRenderNode(AbstractDeclarative *declarative,
                          const QSharedPointer<QMutex> &nodeMutex);
    ~DeclarativeRenderNode();

    void setSize(const QSize &size);
    QSize size() const { return m_size; }

    void update();
    void updateFBO();

    void setController(Abstract3DController *controller);
    void setQuickWindow(QQuickWindow *window);
    void setSamples(int samples);

    void preprocess();

public Q_SLOTS:
    void handleControllerDestroyed();

private:
    QSGTextureMaterial m_material;
    QSGOpaqueTextureMaterial m_materialO;
    QSGGeometry m_geometry;
    QSGTexture *m_texture;
    QSize m_size;

    AbstractDeclarative *m_declarative;
    Abstract3DController *m_controller;
    QOpenGLFramebufferObject *m_fbo;
    QOpenGLFramebufferObject *m_multisampledFBO;
    QQuickWindow *m_window;
    int m_samples;

    bool m_dirtyFBO;

    QSharedPointer<QMutex> m_nodeMutex;

};

QT_END_NAMESPACE_DATAVISUALIZATION

#endif
