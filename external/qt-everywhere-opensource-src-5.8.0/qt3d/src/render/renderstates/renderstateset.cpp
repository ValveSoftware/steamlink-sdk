/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "renderstateset_p.h"

#include <bitset>

#include <QDebug>
#include <QOpenGLContext>

#include <Qt3DRender/private/graphicscontext_p.h>
#include <Qt3DRender/private/renderstates_p.h>
#include <Qt3DRender/private/qrenderstate_p.h>

#include <Qt3DRender/qalphacoverage.h>
#include <Qt3DRender/qalphatest.h>
#include <Qt3DRender/private/qalphatest_p.h>
#include <Qt3DRender/qblendequation.h>
#include <Qt3DRender/private/qblendequation_p.h>
#include <Qt3DRender/qblendequationarguments.h>
#include <Qt3DRender/private/qblendequationarguments_p.h>
#include <Qt3DRender/qcolormask.h>
#include <Qt3DRender/private/qcolormask_p.h>
#include <Qt3DRender/qcullface.h>
#include <Qt3DRender/private/qcullface_p.h>
#include <Qt3DRender/qnodepthmask.h>
#include <Qt3DRender/qdepthtest.h>
#include <Qt3DRender/private/qdepthtest_p.h>
#include <Qt3DRender/qdithering.h>
#include <Qt3DRender/qfrontface.h>
#include <Qt3DRender/private/qfrontface_p.h>
#include <Qt3DRender/qpointsize.h>
#include <Qt3DRender/private/qpointsize_p.h>
#include <Qt3DRender/qpolygonoffset.h>
#include <Qt3DRender/private/qpolygonoffset_p.h>
#include <Qt3DRender/qscissortest.h>
#include <Qt3DRender/private/qscissortest_p.h>
#include <Qt3DRender/qstenciltest.h>
#include <Qt3DRender/private/qstenciltest_p.h>
#include <Qt3DRender/qstenciltestarguments.h>
#include <Qt3DRender/private/qstenciltestarguments_p.h>
#include <Qt3DRender/qclipplane.h>
#include <Qt3DRender/private/qclipplane_p.h>
#include <Qt3DRender/qmultisampleantialiasing.h>
#include <Qt3DRender/qseamlesscubemap.h>
#include <Qt3DRender/qstenciloperation.h>
#include <Qt3DRender/private/qstenciloperation_p.h>
#include <Qt3DRender/qstenciloperationarguments.h>
#include <Qt3DRender/private/qstenciloperationarguments_p.h>
#include <Qt3DRender/qstencilmask.h>
#include <Qt3DRender/private/qstencilmask_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

RenderStateSet::RenderStateSet()
    : m_stateMask(0)
{
}

RenderStateSet::~RenderStateSet()
{
}

template<>
void RenderStateSet::addState<StateVariant>(const StateVariant &ds)
{
    m_states.push_back(ds);
    m_stateMask |= ds.type;
}

int RenderStateSet::changeCost(RenderStateSet *previousState)
{
    if (previousState == this)
        return 0;

    int cost = 0;

    // first, find cost of any resets
    StateMaskSet invOurState = ~stateMask();
    StateMaskSet stateToReset = previousState->stateMask() & invOurState;

    std::bitset<64> bs(stateToReset);
    cost += int(bs.count());

    // now, find out how many states we're changing
    for (const StateVariant &ds : qAsConst(m_states)) {
        // if the other state contains matching, then doesn't
        // contribute to cost at all
        if (previousState->contains(ds))
            continue;

        // flat cost for now; could be replaced with a cost() method on
        // RenderState
        cost += 2;
    }

    return cost;
}

void RenderStateSet::apply(GraphicsContext *gc)
{
    RenderStateSet* previousStates = gc->currentStateSet();

    const StateMaskSet invOurState = ~stateMask();
    // generate a mask for each set bit in previous, where we do not have
    // the corresponding bit set.

    StateMaskSet stateToReset = 0;
    if (previousStates) {
        stateToReset = previousStates->stateMask() & invOurState;
        qCDebug(RenderStates) << "previous states " << QString::number(previousStates->stateMask(), 2);
    }
    qCDebug(RenderStates) << " current states " << QString::number(stateMask(), 2)  << "inverse " << QString::number(invOurState, 2) << " -> states to change:  " << QString::number(stateToReset, 2);

    // Reset states that aren't active in the current state set
    resetMasked(stateToReset, gc);

    // Apply states that weren't in the previous state or that have
    // different values
    for (const StateVariant &ds : qAsConst(m_states)) {
        if (previousStates && previousStates->contains(ds))
            continue;
        ds.apply(gc);
    }
}

StateMaskSet RenderStateSet::stateMask() const
{
    return m_stateMask;
}

void RenderStateSet::merge(RenderStateSet *other)
{
    m_stateMask |= other->stateMask();
}

void RenderStateSet::resetMasked(StateMaskSet maskOfStatesToReset, GraphicsContext *gc)
{
    // TO DO -> Call gcHelper methods instead of raw GL
    // QOpenGLFunctions shouldn't be used here directly
    QOpenGLFunctions *funcs = gc->openGLContext()->functions();

    if (maskOfStatesToReset & ScissorStateMask)
        funcs->glDisable(GL_SCISSOR_TEST);

    if (maskOfStatesToReset & BlendStateMask)
        funcs->glDisable(GL_BLEND);

    if (maskOfStatesToReset & StencilWriteStateMask)
        funcs->glStencilMask(0);

    if (maskOfStatesToReset & StencilTestStateMask)
        funcs->glDisable(GL_STENCIL_TEST);

    if (maskOfStatesToReset & DepthTestStateMask)
        funcs->glDisable(GL_DEPTH_TEST);

    if (maskOfStatesToReset & DepthWriteStateMask)
        funcs->glDepthMask(GL_TRUE); // reset to default

    if (maskOfStatesToReset & FrontFaceStateMask)
        funcs->glFrontFace(GL_CCW); // reset to default

    if (maskOfStatesToReset & CullFaceStateMask)
        funcs->glDisable(GL_CULL_FACE);

    if (maskOfStatesToReset & DitheringStateMask)
        funcs->glDisable(GL_DITHER);

    if (maskOfStatesToReset & AlphaCoverageStateMask)
        gc->setAlphaCoverageEnabled(false);

    if (maskOfStatesToReset & PointSizeMask)
        gc->pointSize(false, 1.0f);    // reset to default

    if (maskOfStatesToReset & PolygonOffsetStateMask)
        funcs->glDisable(GL_POLYGON_OFFSET_FILL);

    if (maskOfStatesToReset & ColorStateMask)
        funcs->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (maskOfStatesToReset & ClipPlaneMask) {
        GLint max = gc->maxClipPlaneCount();
        for (GLint i = 0; i < max; ++i)
            gc->disableClipPlane(i);
    }

    if (maskOfStatesToReset & SeamlessCubemapMask)
        gc->setSeamlessCubemap(false);

    if (maskOfStatesToReset & StencilOpMask)
        funcs->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

bool RenderStateSet::contains(const StateVariant &ds) const
{
    // trivial reject using the state mask bits
    if (!(ds.type & stateMask()))
        return false;

    for (const StateVariant &rs : m_states) {
        if (rs == ds)
            return true;
    }
    return false;
}

StateVariant RenderStateSet::initializeStateFromPeer(const Qt3DRender::QRenderStateCreatedChangeBasePtr change)
{
    switch (change->renderStateType()) {
    case AlphaCoverageStateMask: {
        return RenderStateSet::createState<AlphaCoverage>(change->isNodeEnabled());
    }

    case AlphaTestMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QAlphaTestData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<AlphaFunc>(data.alphaFunction, data.referenceValue);
    }

    case BlendStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QBlendEquationData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<BlendEquation>(data.blendFunction);
    }

    case BlendEquationArgumentsMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QBlendEquationArgumentsData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<BlendEquationArguments>(
                    data.sourceRgb, data.destinationRgb,
                    data.sourceAlpha, data.destinationAlpha,
                    change->isNodeEnabled(),
                    data.bufferIndex);
    }

    case MSAAEnabledStateMask: {
        return RenderStateSet::createState<MSAAEnabled>(change->isNodeEnabled());
    }

    case CullFaceStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QCullFaceData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<CullFace>(data.mode);
    }

    case DepthWriteStateMask: {
        return RenderStateSet::createState<NoDepthMask>(false);
    }

    case DepthTestStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QDepthTestData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<DepthTest>(data.depthFunction);
    }

    case FrontFaceStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QFrontFaceData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<FrontFace>(data.direction);
    }

    case ScissorStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QScissorTestData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<ScissorTest>(data.left, data.bottom,
                                                        data.width, data.height);
    }

    case StencilTestStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QStencilTestData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<StencilTest>(data.front.stencilFunction,
                                                        data.front.referenceValue,
                                                        data.front.comparisonMask,
                                                        data.back.stencilFunction,
                                                        data.back.referenceValue,
                                                        data.back.comparisonMask);
    }

    case PointSizeMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QPointSizeData>>(change);
        const auto &data = typedChange->data;
        const bool isProgrammable = (data.sizeMode == QPointSize::Programmable);
        return RenderStateSet::createState<PointSize>(isProgrammable, data.value);
    }

    case PolygonOffsetStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QPolygonOffsetData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<PolygonOffset>(data.scaleFactor, data.depthSteps);
    }

    case ColorStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QColorMaskData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<ColorMask>(data.redMasked, data.greenMasked,
                                                      data.blueMasked, data.alphaMasked);
    }

    case ClipPlaneMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QClipPlaneData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<ClipPlane>(data.planeIndex,
                                                      data.normal,
                                                      data.distance);
    }

    case SeamlessCubemapMask: {
        return RenderStateSet::createState<SeamlessCubemap>(change->isNodeEnabled());
    }

    case StencilOpMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QStencilOperationData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<StencilOp>(data.front.stencilTestFailureOperation,
                                                      data.front.depthTestFailureOperation,
                                                      data.front.allTestsPassOperation,
                                                      data.back.stencilTestFailureOperation,
                                                      data.back.depthTestFailureOperation,
                                                      data.back.allTestsPassOperation);
    }

    case StencilWriteStateMask: {
        const auto typedChange = qSharedPointerCast<Qt3DRender::QRenderStateCreatedChange<QStencilMaskData>>(change);
        const auto &data = typedChange->data;
        return RenderStateSet::createState<StencilMask>(data.frontOutputMask,
                                                        data.backOutputMask);
    }

        // TODO: Fix Dithering state
    case DitheringStateMask:
    default:
        Q_UNREACHABLE();
        return StateVariant();
    }
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
