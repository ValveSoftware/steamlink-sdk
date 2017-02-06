/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QSGD3D12RENDERER_P_H
#define QSGD3D12RENDERER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qsgrenderer_p.h>
#include <QtGui/private/qdatabuffer_p.h>
#include <QtCore/qbitarray.h>
#include "qsgd3d12engine_p.h"
#include "qsgd3d12material_p.h"

QT_BEGIN_NAMESPACE

class QSGRenderNode;

class QSGD3D12Renderer : public QSGRenderer
{
public:
    QSGD3D12Renderer(QSGRenderContext *context);
    ~QSGD3D12Renderer();

    void renderScene(GLuint fboId) override;
    void render() override;
    void nodeChanged(QSGNode *node, QSGNode::DirtyState state) override;

    void turnToLayerRenderer() { m_layerRenderer = true; }

private:
    void updateMatrices(QSGNode *node, QSGTransformNode *xform);
    void updateOpacities(QSGNode *node, float inheritedOpacity);
    void buildRenderList(QSGNode *node, QSGClipNode *clip);
    void renderElements();
    void renderElement(int elementIndex);
    void setInputLayout(const QSGGeometry *g, QSGD3D12PipelineState *pipelineState);
    void setupClipping(const QSGClipNode *clip, int elementIndex);
    void setScissor(const QRect &r);
    void renderStencilClip(const QSGClipNode *clip, int elementIndex, const QMatrix4x4 &m, quint32 &stencilValue);
    void renderRenderNode(QSGRenderNode *node, int elementIndex);

    struct Element {
        QSGNode *node = nullptr;
        qint32 vboOffset = -1;
        qint32 iboOffset = -1;
        quint32 iboStride = 0;
        qint32 cboOffset = -1;
        quint32 cboSize = 0;
        bool cboPrepared = false;
    };

    void queueDrawCall(const QSGGeometry *g, const Element &e);

    bool m_layerRenderer = false;
    QSet<QSGNode *> m_dirtyTransformNodes;
    QSet<QSGNode *> m_dirtyOpacityNodes;
    QBitArray m_opaqueElements;
    bool m_rebuild = true;
    bool m_dirtyOpaqueElements = true;
    QDataBuffer<quint8> m_vboData;
    QDataBuffer<quint8> m_iboData;
    QDataBuffer<quint8> m_cboData;
    QDataBuffer<Element> m_renderList;
    uint m_vertexBuf = 0;
    uint m_indexBuf = 0;
    uint m_constantBuf = 0;
    QSGD3D12Engine *m_engine = nullptr;

    QSGMaterialType *m_lastMaterialType = nullptr;
    QSGD3D12PipelineState m_pipelineState;
    QSGD3D12PipelineState m_freshPipelineState;

    typedef QHash<QSGNode *, QSGD3D12MaterialRenderState::DirtyStates> NodeDirtyMap;
    NodeDirtyMap m_nodeDirtyMap;

    QRect m_activeScissorRect;
    QRect m_lastDeviceRect;
    bool m_projectionChangedDueToDeviceSize;

    uint m_renderTarget = 0;
    quint32 m_currentStencilValue;
    enum ClipType {
        ClipScissor = 0x1,
        ClipStencil = 0x2
    };
    int m_currentClipTypes;
};

QT_END_NAMESPACE

#endif // QSGD3D12RENDERER_P_H
