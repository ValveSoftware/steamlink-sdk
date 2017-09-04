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

#ifndef QSGD3D12SHADEREFFECTNODE_P_H
#define QSGD3D12SHADEREFFECTNODE_P_H

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

#include <private/qsgadaptationlayer_p.h>
#include "qsgd3d12material_p.h"

QT_BEGIN_NAMESPACE

class QSGD3D12RenderContext;
class QSGD3D12GuiThreadShaderEffectManager;
class QSGD3D12ShaderEffectNode;
class QSGD3D12Texture;
class QFileSelector;

class QSGD3D12ShaderLinker
{
public:
    void reset(const QByteArray &vertBlob, const QByteArray &fragBlob);

    void feedVertexInput(const QSGShaderEffectNode::ShaderData &shader);
    void feedConstants(const QSGShaderEffectNode::ShaderData &shader, const QSet<int> *dirtyIndices = nullptr);
    void feedSamplers(const QSGShaderEffectNode::ShaderData &shader);
    void feedTextures(const QSGShaderEffectNode::ShaderData &shader, const QSet<int> *dirtyIndices = nullptr);
    void linkTextureSubRects();

    void dump();

    struct Constant {
        uint size;
        QSGShaderEffectNode::VariableData::SpecialType specialType;
        QVariant value;
        bool operator==(const Constant &other) const {
            return size == other.size && specialType == other.specialType
                    && (specialType == QSGShaderEffectNode::VariableData::None ? value == other.value : true);
        }
    };

    bool error;
    QByteArray vs;
    QByteArray fs;
    uint constantBufferSize;
    QHash<uint, Constant> constants; // offset -> Constant
    QSet<int> samplers; // bindpoint
    QHash<int, QVariant> textures; // bindpoint -> value (source ref)
    QHash<QByteArray, int> textureNameMap; // name -> bindpoint
};

QDebug operator<<(QDebug debug, const QSGD3D12ShaderLinker::Constant &c);

class QSGD3D12ShaderEffectMaterial : public QSGD3D12Material
{
public:
    QSGD3D12ShaderEffectMaterial(QSGD3D12ShaderEffectNode *node);
    ~QSGD3D12ShaderEffectMaterial();

    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;

    int constantBufferSize() const override;
    void preparePipeline(QSGD3D12PipelineState *pipelineState) override;
    UpdateResults updatePipeline(const QSGD3D12MaterialRenderState &state,
                                 QSGD3D12PipelineState *pipelineState,
                                 ExtraState *extraState,
                                 quint8 *constantBuffer) override;

    void updateTextureProviders(bool layoutChange);

    QSGD3D12ShaderEffectNode *node;
    bool valid = false;
    QSGShaderEffectNode::CullMode cullMode = QSGShaderEffectNode::NoCulling;
    bool hasCustomVertexShader = false;
    bool hasCustomFragmentShader = false;
    QSGD3D12ShaderLinker linker;
    QSGMaterialType *mtype = nullptr;
    QVector<QSGTextureProvider *> textureProviders;
    QSGD3D12Texture *dummy = nullptr;
    bool geometryUsesTextureSubRect = false;
};

class QSGD3D12ShaderEffectNode : public QObject, public QSGShaderEffectNode
{
    Q_OBJECT

public:
    QSGD3D12ShaderEffectNode(QSGD3D12RenderContext *rc, QSGD3D12GuiThreadShaderEffectManager *mgr);

    QRectF updateNormalizedTextureSubRect(bool supportsAtlasTextures) override;
    void syncMaterial(SyncData *syncData) override;

    static void cleanupMaterialTypeCache();

    void preprocess() override;

    QSGD3D12RenderContext *renderContext() { return m_rc; }

private Q_SLOTS:
    void handleTextureChange();
    void handleTextureProviderDestroyed(QObject *object);

private:
    QSGD3D12RenderContext *m_rc;
    QSGD3D12GuiThreadShaderEffectManager *m_mgr;
    QSGD3D12ShaderEffectMaterial m_material;
};

class QSGD3D12GuiThreadShaderEffectManager : public QSGGuiThreadShaderEffectManager
{
public:
    bool hasSeparateSamplerAndTextureObjects() const override;

    QString log() const override;
    Status status() const override;

    void prepareShaderCode(ShaderInfo::Type typeHint, const QByteArray &src, ShaderInfo *result) override;

private:
    bool reflect(ShaderInfo *result);
    QString m_log;
    Status m_status = Uncompiled;
    QFileSelector *m_fileSelector = nullptr;

    friend class QSGD3D12ShaderCompileTask;
};

QT_END_NAMESPACE

#endif // QSGD3D12SHADEREFFECTNODE_P_H
