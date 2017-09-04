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

#include "handler_p.h"
#include <Qt3DAnimation/private/managers_p.h>
#include <Qt3DAnimation/private/loadanimationclipjob_p.h>
#include <Qt3DAnimation/private/findrunningclipanimatorsjob_p.h>
#include <Qt3DAnimation/private/evaluateclipanimatorjob_p.h>
#include <Qt3DAnimation/private/buildblendtreesjob_p.h>
#include <Qt3DAnimation/private/evaluateblendclipanimatorjob_p.h>
#include <Qt3DAnimation/private/animationlogging_p.h>
#include <Qt3DAnimation/private/buildblendtreesjob_p.h>
#include <Qt3DAnimation/private/evaluateblendclipanimatorjob_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

Handler::Handler()
    : m_animationClipLoaderManager(new AnimationClipLoaderManager)
    , m_clipAnimatorManager(new ClipAnimatorManager)
    , m_blendedClipAnimatorManager(new BlendedClipAnimatorManager)
    , m_channelMappingManager(new ChannelMappingManager)
    , m_channelMapperManager(new ChannelMapperManager)
    , m_clipBlendNodeManager(new ClipBlendNodeManager)
    , m_loadAnimationClipJob(new LoadAnimationClipJob)
    , m_findRunningClipAnimatorsJob(new FindRunningClipAnimatorsJob)
    , m_buildBlendTreesJob(new BuildBlendTreesJob)
    , m_simulationTime(0)
{
    m_loadAnimationClipJob->setHandler(this);
    m_findRunningClipAnimatorsJob->setHandler(this);
    m_buildBlendTreesJob->setHandler(this);
}

Handler::~Handler()
{
}

void Handler::setDirty(DirtyFlag flag, Qt3DCore::QNodeId nodeId)
{
    switch (flag) {
    case AnimationClipDirty: {
        const auto handle = m_animationClipLoaderManager->lookupHandle(nodeId);
        m_dirtyAnimationClips.push_back(handle);
        break;
    }

    case ChannelMappingsDirty: {
        const auto handle = m_channelMapperManager->lookupHandle(nodeId);
        m_dirtyChannelMappers.push_back(handle);
        break;
    }

    case ClipAnimatorDirty: {
        const auto handle = m_clipAnimatorManager->lookupHandle(nodeId);
        m_dirtyClipAnimators.push_back(handle);
        break;
    }

    case BlendedClipAnimatorDirty: {
        const HBlendedClipAnimator handle = m_blendedClipAnimatorManager->lookupHandle(nodeId);
        m_dirtyBlendedAnimators.push_back(handle);
        break;
    }
    }
}

void Handler::setClipAnimatorRunning(const HClipAnimator &handle, bool running)
{
    // Add clip to running set if not already present
    if (running && !m_runningClipAnimators.contains(handle)) {
        m_runningClipAnimators.push_back(handle);
        ClipAnimator *clipAnimator = m_clipAnimatorManager->data(handle);
        if (clipAnimator)
            clipAnimator->setStartTime(m_simulationTime);
    }

    // If being marked as not running, remove from set of running clips
    if (!running) {
        const auto it = std::find_if(m_runningClipAnimators.begin(),
                                     m_runningClipAnimators.end(),
                                     [handle](const HClipAnimator &h) { return h == handle; });
        if (it != m_runningClipAnimators.end())
            m_runningClipAnimators.erase(it);
    }
}

void Handler::setBlendedClipAnimatorRunning(const HBlendedClipAnimator &handle, bool running)
{
    // Add clip to running set if not already present
    if (running && !m_runningBlendedClipAnimators.contains(handle)) {
        m_runningBlendedClipAnimators.push_back(handle);
        BlendedClipAnimator *blendedClipAnimator = m_blendedClipAnimatorManager->data(handle);
        if (blendedClipAnimator)
            blendedClipAnimator->setStartTime(m_simulationTime);
    }

    // If being marked as not running, remove from set of running clips
    if (!running) {
        const auto it = std::find_if(m_runningBlendedClipAnimators.begin(),
                                     m_runningBlendedClipAnimators.end(),
                                     [handle](const HBlendedClipAnimator &h) { return h == handle; });
        if (it != m_runningBlendedClipAnimators.end())
            m_runningBlendedClipAnimators.erase(it);
    }
}

QVector<Qt3DCore::QAspectJobPtr> Handler::jobsToExecute(qint64 time)
{
    // Store the simulation time so we can mark the start time of
    // animators which will allow us to calculate the local time of
    // animation clips.
    m_simulationTime = time;

    QVector<Qt3DCore::QAspectJobPtr> jobs;

    // If there are any dirty animation clips that need loading,
    // queue up a job for them
    if (!m_dirtyAnimationClips.isEmpty()) {
        qCDebug(HandlerLogic) << "Added LoadAnimationClipJob";
        m_loadAnimationClipJob->addDirtyAnimationClips(m_dirtyAnimationClips);
        jobs.push_back(m_loadAnimationClipJob);
        m_dirtyAnimationClips.clear();
    }

    // If there are dirty clip animators, find the set that are able to
    // run. I.e. are marked as running and have animation clips and
    // channel mappings
    if (!m_dirtyClipAnimators.isEmpty()) {
        qCDebug(HandlerLogic) << "Added FindRunningClipAnimatorsJob";
        m_findRunningClipAnimatorsJob->removeDependency(QWeakPointer<Qt3DCore::QAspectJob>());
        m_findRunningClipAnimatorsJob->setDirtyClipAnimators(m_dirtyClipAnimators);
        jobs.push_back(m_findRunningClipAnimatorsJob);
        if (jobs.contains(m_loadAnimationClipJob))
            m_findRunningClipAnimatorsJob->addDependency(m_loadAnimationClipJob);
        m_dirtyClipAnimators.clear();
    }

    // Rebuild blending trees if a blend tree is dirty
    if (!m_dirtyBlendedAnimators.isEmpty()) {
        const QVector<HBlendedClipAnimator> dirtyBlendedAnimators = std::move(m_dirtyBlendedAnimators);
        m_buildBlendTreesJob->setBlendedClipAnimators(dirtyBlendedAnimators);
        jobs.push_back(m_buildBlendTreesJob);
    }

    // TODO: Parallelise the animator evaluation and property building at a finer level

    // If there are any running ClipAnimators, evaluate them for the current
    // time and send property changes
    if (!m_runningClipAnimators.isEmpty()) {
        qCDebug(HandlerLogic) << "Added EvaluateClipAnimatorJobs";

        // Ensure we have a job per clip animator
        const int oldSize = m_evaluateClipAnimatorJobs.size();
        const int newSize = m_runningClipAnimators.size();
        if (oldSize < newSize) {
            m_evaluateClipAnimatorJobs.resize(newSize);
            for (int i = oldSize; i < newSize; ++i) {
                m_evaluateClipAnimatorJobs[i].reset(new EvaluateClipAnimatorJob());
                m_evaluateClipAnimatorJobs[i]->setHandler(this);
            }
        }

        // Set each job up with an animator to process and set dependencies
        for (int i = 0; i < newSize; ++i) {
            m_evaluateClipAnimatorJobs[i]->setClipAnimator(m_runningClipAnimators[i]);
            m_evaluateClipAnimatorJobs[i]->removeDependency(QWeakPointer<Qt3DCore::QAspectJob>());
            if (jobs.contains(m_loadAnimationClipJob))
                m_evaluateClipAnimatorJobs[i]->addDependency(m_loadAnimationClipJob);
            if (jobs.contains(m_findRunningClipAnimatorsJob))
                m_evaluateClipAnimatorJobs[i]->addDependency(m_findRunningClipAnimatorsJob);
            jobs.push_back(m_evaluateClipAnimatorJobs[i]);
        }
    }

    // BlendClipAnimator execution
    if (!m_runningBlendedClipAnimators.isEmpty()) {
        // Ensure we have a job per clip animator
        const int oldSize = m_evaluateBlendClipAnimatorJobs.size();
        const int newSize = m_runningBlendedClipAnimators.size();
        if (oldSize < newSize) {
            m_evaluateBlendClipAnimatorJobs.resize(newSize);
            for (int i = oldSize; i < newSize; ++i) {
                m_evaluateBlendClipAnimatorJobs[i].reset(new EvaluateBlendClipAnimatorJob());
                m_evaluateBlendClipAnimatorJobs[i]->setHandler(this);
            }
        }

        // Set each job up with an animator to process and set dependencies
        for (int i = 0; i < newSize; ++i) {
            m_evaluateBlendClipAnimatorJobs[i]->setBlendClipAnimator(m_runningBlendedClipAnimators[i]);
            m_evaluateBlendClipAnimatorJobs[i]->removeDependency(QWeakPointer<Qt3DCore::QAspectJob>());
            if (jobs.contains(m_loadAnimationClipJob))
                m_evaluateBlendClipAnimatorJobs[i]->addDependency(m_loadAnimationClipJob);
            if (jobs.contains(m_buildBlendTreesJob))
                m_evaluateBlendClipAnimatorJobs[i]->addDependency(m_buildBlendTreesJob);
            jobs.push_back(m_evaluateBlendClipAnimatorJobs[i]);
        }
    }

    return jobs;
}

} // namespace Animation
} // namespace Qt3DAnimation

QT_END_NAMESPACE
