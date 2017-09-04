/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "evaluateblendclipanimatorjob_p.h"
#include <Qt3DAnimation/private/handler_p.h>
#include <Qt3DAnimation/private/managers_p.h>
#include <Qt3DAnimation/private/animationlogging_p.h>
#include <Qt3DAnimation/private/animationutils_p.h>
#include <Qt3DAnimation/private/clipblendvalue_p.h>
#include <Qt3DAnimation/private/lerpclipblend_p.h>
#include <Qt3DAnimation/private/clipblendnodevisitor_p.h>
#include <Qt3DAnimation/private/job_common_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

EvaluateBlendClipAnimatorJob::EvaluateBlendClipAnimatorJob()
    : Qt3DCore::QAspectJob()
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::EvaluateBlendClipAnimator, 0);
}

void EvaluateBlendClipAnimatorJob::run()
{
    // Find the set of clips that need to be evaluated by querying each node
    // in the blend tree.
    // TODO: We should be able to cache this for each blend animator and only
    // update when a node indicates its dependencies have changed as a result
    // of blend factors changing
    BlendedClipAnimator *blendedClipAnimator = m_handler->blendedClipAnimatorManager()->data(m_blendClipAnimatorHandle);
    Qt3DCore::QNodeId blendTreeRootId = blendedClipAnimator->blendTreeRootId();
    const QVector<Qt3DCore::QNodeId> valueNodeIdsToEvaluate = gatherValueNodesToEvaluate(m_handler, blendTreeRootId);

    // Calculate the resulting duration of the blend tree based upon its current state
    ClipBlendNodeManager *blendNodeManager = m_handler->clipBlendNodeManager();
    ClipBlendNode *blendTreeRootNode = blendNodeManager->lookupNode(blendTreeRootId);
    Q_ASSERT(blendTreeRootNode);
    const double duration = blendTreeRootNode->duration();

    // Calculate the phase given the blend tree duration and global time
    const qint64 globalTime = m_handler->simulationTime();
    const AnimatorEvaluationData animatorData = evaluationDataForAnimator(blendedClipAnimator, globalTime);
    int currentLoop = 0;
    const double phase = phaseFromGlobalTime(animatorData.globalTime,
                                             animatorData.startTime,
                                             animatorData.playbackRate,
                                             duration,
                                             animatorData.loopCount,
                                             currentLoop);

    // Iterate over the value nodes of the blend tree, evaluate the
    // contained animation clips at the current phase and store the results
    // in the animator indexed by node.
    AnimationClipLoaderManager *clipLoaderManager = m_handler->animationClipLoaderManager();
    for (const auto valueNodeId : valueNodeIdsToEvaluate) {
        ClipBlendValue *valueNode = static_cast<ClipBlendValue *>(blendNodeManager->lookupNode(valueNodeId));
        Q_ASSERT(valueNode);
        AnimationClip *clip = clipLoaderManager->lookupResource(valueNode->clipId());
        Q_ASSERT(clip);

        ClipResults rawClipResults = evaluateClipAtPhase(clip, phase);

        // Reformat the clip results into the layout used by this animator/blend tree
        ComponentIndices format = valueNode->formatIndices(blendedClipAnimator->peerId());
        ClipResults formattedClipResults = formatClipResults(rawClipResults, format);
        valueNode->setClipResults(blendedClipAnimator->peerId(), formattedClipResults);
    }

    // Evaluate the blend tree
    ClipResults blendedResults = evaluateBlendTree(m_handler, blendedClipAnimator, blendTreeRootId);

    const double localTime = phase * duration;
    const bool finalFrame = isFinalFrame(localTime, duration, currentLoop, animatorData.loopCount);

    // Prepare the property change events
    const QVector<MappingData> mappingData = blendedClipAnimator->mappingData();
    const QVector<Qt3DCore::QSceneChangePtr> changes = preparePropertyChanges(blendedClipAnimator->peerId(),
                                                                              mappingData,
                                                                              blendedResults,
                                                                              finalFrame);
    // Send the property changes
    blendedClipAnimator->sendPropertyChanges(changes);
}

} // Animation
} // Qt3DAnimation

QT_END_NAMESPACE
