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

#include "animationutils_p.h"
#include <Qt3DAnimation/private/handler_p.h>
#include <Qt3DAnimation/private/managers_p.h>
#include <Qt3DAnimation/private/clipblendnode_p.h>
#include <Qt3DAnimation/private/clipblendnodevisitor_p.h>
#include <Qt3DAnimation/private/clipblendvalue_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qpropertyupdatedchangebase_p.h>
#include <QtGui/qvector2d.h>
#include <QtGui/qvector3d.h>
#include <QtGui/qvector4d.h>
#include <QtGui/qquaternion.h>
#include <QtGui/qcolor.h>
#include <QtCore/qvariant.h>
#include <Qt3DAnimation/private/animationlogging_p.h>

#include <numeric>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

int componentsForType(int type)
{
    int componentCount = 1;
    switch (type) {
    case QMetaType::Float:
    case QVariant::Double:
        componentCount = 1;
        break;

    case QVariant::Vector2D:
        componentCount = 2;
        break;

    case QVariant::Vector3D:
    case QVariant::Color:
        componentCount = 3;
        break;

    case QVariant::Vector4D:
    case QVariant::Quaternion:
        componentCount = 4;
        break;

    default:
        qWarning() << "Unhandled animation type";
    }

    return componentCount;
}

ClipEvaluationData evaluationDataForClip(AnimationClip *clip,
                                         const AnimatorEvaluationData &animatorData)
{
    // global time values expected in seconds
    ClipEvaluationData result;
    result.localTime = localTimeFromGlobalTime(animatorData.globalTime, animatorData.startTime,
                                               animatorData.playbackRate, clip->duration(),
                                               animatorData.loopCount, result.currentLoop);
    result.isFinalFrame = isFinalFrame(result.localTime, clip->duration(),
                                       result.currentLoop, animatorData.loopCount);
    return result;
}

double localTimeFromGlobalTime(double t_global,
                               double t_start_global,
                               double playbackRate,
                               double duration,
                               int loopCount,
                               int &currentLoop)
{
    double t_local = playbackRate * (t_global - t_start_global);
    double loopNumber = 0;
    if (loopCount == 1) {
        t_local = qBound(0.0, t_local, duration);
    } else if (loopCount < 0) {
        // Loops forever
        (void) std::modf(t_local / duration, &loopNumber);
        t_local = std::fmod(t_local, duration);
    } else {
        // N loops
        t_local = qBound(0.0, t_local, double(loopCount) * duration);
        (void) std::modf(t_local / duration, &loopNumber);
        t_local = std::fmod(t_local, duration);

        // Ensure we clamp to end of final loop
        if (loopNumber == loopCount) {
            loopNumber = loopCount - 1;
            t_local = duration;
        }
    }

    qCDebug(Jobs) << "t_global - t_start =" << t_global - t_start_global
                  << "current loop =" << loopNumber
                  << "t =" << t_local
                  << "duration =" << duration;

    currentLoop = loopNumber;

    return t_local;
}

double phaseFromGlobalTime(double t_global, double t_start_global,
                           double playbackRate, double duration,
                           int loopCount, int &currentLoop)
{
    const double t_local = localTimeFromGlobalTime(t_global, t_start_global, playbackRate,
                                                   duration, loopCount, currentLoop);
    return t_local / duration;
}

ComponentIndices channelComponentsToIndices(const Channel &channel, int dataType, int offset)
{
#if defined Q_COMPILER_UNIFORM_INIT
    static const QVector<char> standardSuffixes = { 'X', 'Y', 'Z', 'W' };
    static const QVector<char> quaternionSuffixes = { 'W', 'X', 'Y', 'Z' };
    static const QVector<char> colorSuffixes = { 'R', 'G', 'B' };
#else
    static const QVector<char> standardSuffixes = (QVector<char>() << 'X' << 'Y' << 'Z' << 'W');
    static const QVector<char> quaternionSuffixes = (QVector<char>() << 'W' << 'X' << 'Y' << 'Z');
    static const QVector<char> colorSuffixes = (QVector<char>() << 'R' << 'G' << 'B');
#endif

    switch (dataType) {
    case QVariant::Quaternion:
        return channelComponentsToIndicesHelper(channel, dataType, offset, quaternionSuffixes);
    case QVariant::Color:
        return channelComponentsToIndicesHelper(channel, dataType, offset, colorSuffixes);
    default:
        return channelComponentsToIndicesHelper(channel, dataType, offset, standardSuffixes);
    }
}

ComponentIndices channelComponentsToIndicesHelper(const Channel &channel,
                                              int dataType,
                                              int offset,
                                              const QVector<char> &suffixes)
{
    const int expectedComponentCount = componentsForType(dataType);
    const int actualComponentCount = channel.channelComponents.size();
    if (actualComponentCount != expectedComponentCount) {
        qWarning() << "Data type expects" << expectedComponentCount
                   << "but found" << actualComponentCount << "components in the animation clip";
    }

    ComponentIndices indices(expectedComponentCount);
    for (int i = 0; i < expectedComponentCount; ++i) {
        const QString &componentName = channel.channelComponents[i].name;
        char suffix = componentName.at(componentName.length() - 1).toLatin1();
        int index = suffixes.indexOf(suffix);
        if (index != -1)
            indices[i] = index + offset;
        else
            indices[i] = -1;
    }
    return indices;
}

ClipResults evaluateClipAtLocalTime(AnimationClip *clip, float localTime)
{
    QVector<float> channelResults;
    Q_ASSERT(clip);

    // Ensure we have enough storage to hold the evaluations
    channelResults.resize(clip->channelCount());

    // Iterate over channels and evaluate the fcurves
    const QVector<Channel> &channels = clip->channels();
    int i = 0;
    for (const Channel &channel : channels) {
        for (const auto &channelComponent : qAsConst(channel.channelComponents))
            channelResults[i++] = channelComponent.fcurve.evaluateAtTime(localTime);
    }
    return channelResults;
}

ClipResults evaluateClipAtPhase(AnimationClip *clip, float phase)
{
    // Calculate the clip local time from the phase and clip duration
    const double localTime = phase * clip->duration();
    return evaluateClipAtLocalTime(clip, localTime);
}

QVector<Qt3DCore::QSceneChangePtr> preparePropertyChanges(Qt3DCore::QNodeId animatorId,
                                                          const QVector<MappingData> &mappingDataVec,
                                                          const QVector<float> &channelResults,
                                                          bool finalFrame)
{
    QVector<Qt3DCore::QSceneChangePtr> changes;
    // Iterate over the mappings
    for (const MappingData &mappingData : mappingDataVec) {
        // Construct a property update change, set target, property and delivery options
        auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(mappingData.targetId);
        e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
        e->setPropertyName(mappingData.propertyName);

        // Handle intermediate updates vs final flag properly
        Qt3DCore::QPropertyUpdatedChangeBasePrivate::get(e.data())->m_isIntermediate = !finalFrame;

        // Build the new value from the channel/fcurve evaluation results
        QVariant v;
        switch (mappingData.type) {
        case QMetaType::Float:
        case QVariant::Double: {
            v = QVariant::fromValue(channelResults[mappingData.channelIndices[0]]);
            break;
        }

        case QVariant::Vector2D: {
            const QVector2D vector(channelResults[mappingData.channelIndices[0]],
                    channelResults[mappingData.channelIndices[1]]);
            v = QVariant::fromValue(vector);
            break;
        }

        case QVariant::Vector3D: {
            const QVector3D vector(channelResults[mappingData.channelIndices[0]],
                    channelResults[mappingData.channelIndices[1]],
                    channelResults[mappingData.channelIndices[2]]);
            v = QVariant::fromValue(vector);
            break;
        }

        case QVariant::Vector4D: {
            const QVector4D vector(channelResults[mappingData.channelIndices[0]],
                    channelResults[mappingData.channelIndices[1]],
                    channelResults[mappingData.channelIndices[2]],
                    channelResults[mappingData.channelIndices[3]]);
            v = QVariant::fromValue(vector);
            break;
        }

        case QVariant::Quaternion: {
            QQuaternion q(channelResults[mappingData.channelIndices[0]],
                    channelResults[mappingData.channelIndices[1]],
                    channelResults[mappingData.channelIndices[2]],
                    channelResults[mappingData.channelIndices[3]]);
            q.normalize();
            v = QVariant::fromValue(q);
            break;
        }

        case QVariant::Color: {
            const QColor color = QColor::fromRgbF(channelResults[mappingData.channelIndices[0]],
                    channelResults[mappingData.channelIndices[1]],
                    channelResults[mappingData.channelIndices[2]]);
            v = QVariant::fromValue(color);
            break;
        }

        default:
            qWarning() << "Unhandled animation type";
            continue;
        }

        // Assign new value and send
        e->setValue(v);
        changes.push_back(e);
    }


    // If it's the final frame, notify the frontend that we've stopped
    if (finalFrame) {
        auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(animatorId);
        e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
        e->setPropertyName("running");
        e->setValue(false);
        changes.push_back(e);
    }
    return changes;
}

//TODO: Remove this and use new implementation below for both the unblended
//      and blended animation cases.
QVector<MappingData> buildPropertyMappings(Handler *handler,
                                           const AnimationClip *clip,
                                           const ChannelMapper *mapper)
{
    QVector<MappingData> mappingDataVec;
    ChannelMappingManager *mappingManager = handler->channelMappingManager();
    const QVector<Channel> &channels = clip->channels();

    // Iterate over the mappings in the mapper object
    const auto mappingIds = mapper->mappingIds();
    for (const Qt3DCore::QNodeId mappingId : mappingIds) {
        // Get the mapping object
        ChannelMapping *mapping = mappingManager->lookupResource(mappingId);
        Q_ASSERT(mapping);

        // Populate the data we need, easy stuff first
        MappingData mappingData;
        mappingData.targetId = mapping->targetId();
        mappingData.propertyName = mapping->propertyName();
        mappingData.type = mapping->type();

        if (mappingData.type == static_cast<int>(QVariant::Invalid)) {
            qWarning() << "Unknown type for node id =" << mappingData.targetId
                       << "and property =" << mapping->property();
            continue;
        }

        // Now the tricky part. Mapping the channel indices onto the property type.
        // Try to find a ChannelGroup with matching name
        const QString channelName = mapping->channelName();
        int channelGroupIndex = 0;
        bool foundMatch = false;
        for (const Channel &channel : channels) {
            if (channel.name == channelName) {
                foundMatch = true;
                const int channelBaseIndex = clip->channelComponentBaseIndex(channelGroupIndex);

                // Within this group, match channel names with index ordering
                mappingData.channelIndices = channelComponentsToIndices(channel, mappingData.type, channelBaseIndex);

                // Store the mapping data
                mappingDataVec.push_back(mappingData);

                if (foundMatch)
                    break;
            }

            ++channelGroupIndex;
        }
    }

    return mappingDataVec;
}

QVector<MappingData> buildPropertyMappings(const QVector<ChannelMapping*> &channelMappings,
                                           const QVector<ChannelNameAndType> &channelNamesAndTypes,
                                           const QVector<ComponentIndices> &channelComponentIndices)
{
    QVector<MappingData> mappingDataVec;
    mappingDataVec.reserve(channelMappings.size());

    // Iterate over the mappings
    for (const auto mapping : channelMappings) {
        // Populate the data we need, easy stuff first
        MappingData mappingData;
        mappingData.targetId = mapping->targetId();
        mappingData.propertyName = mapping->propertyName();
        mappingData.type = mapping->type();

        if (mappingData.type == static_cast<int>(QVariant::Invalid)) {
            qWarning() << "Unknown type for node id =" << mappingData.targetId
                       << "and property =" << mapping->property();
            continue;
        }

        // Try to find matching channel name and type
        const ChannelNameAndType nameAndType = { mapping->channelName(), mapping->type() };
        const int index = channelNamesAndTypes.indexOf(nameAndType);
        if (index != -1) {
            // We got one!
            mappingData.channelIndices = channelComponentIndices[index];
            mappingDataVec.push_back(mappingData);
        }
    }

    return mappingDataVec;
}

QVector<ChannelNameAndType> buildRequiredChannelsAndTypes(Handler *handler,
                                                          const ChannelMapper *mapper)
{
    ChannelMappingManager *mappingManager = handler->channelMappingManager();
    const QVector<Qt3DCore::QNodeId> mappingIds = mapper->mappingIds();

    // Reserve enough storage assuming each mapping is for a different channel.
    // May be overkill but avoids potential for multiple allocations
    QVector<ChannelNameAndType> namesAndTypes;
    namesAndTypes.reserve(mappingIds.size());

    // Iterate through the mappings and add ones not already used by an earlier mapping.
    // We could add them all then sort and remove duplicates. However, our approach has the
    // advantage of keeping the blend tree format more consistent with the mapping
    // orderings which will have better cache locality when generating events.
    for (const Qt3DCore::QNodeId mappingId : mappingIds) {
        // Get the mapping object
        ChannelMapping *mapping = mappingManager->lookupResource(mappingId);
        Q_ASSERT(mapping);

        // Get the name and type
        const ChannelNameAndType nameAndType{ mapping->channelName(), mapping->type() };

        // Add if not already contained
        if (!namesAndTypes.contains(nameAndType))
            namesAndTypes.push_back(nameAndType);
    }

    return namesAndTypes;
}

QVector<ComponentIndices> assignChannelComponentIndices(const QVector<ChannelNameAndType> &namesAndTypes)
{
    QVector<ComponentIndices> channelComponentIndices;
    channelComponentIndices.reserve(namesAndTypes.size());

    int baseIndex = 0;
    for (const auto &entry : namesAndTypes) {
        // Populate indices in order
        const int componentCount = componentsForType(entry.type);
        ComponentIndices indices(componentCount);
        std::iota(indices.begin(), indices.end(), baseIndex);

        // Append to the results
        channelComponentIndices.push_back(indices);

        // Increment baseIndex
        baseIndex += componentCount;
    }

    return channelComponentIndices;
}

QVector<Qt3DCore::QNodeId> gatherValueNodesToEvaluate(Handler *handler,
                                                      Qt3DCore::QNodeId blendTreeRootId)
{
    Q_ASSERT(handler);
    Q_ASSERT(blendTreeRootId.isNull() == false);

    // We need the ClipBlendNodeManager to be able to lookup nodes from their Ids
    ClipBlendNodeManager *nodeManager = handler->clipBlendNodeManager();

    // Visit the tree in a pre-order manner and collect the dependencies
    QVector<Qt3DCore::QNodeId> clipIds;
    ClipBlendNodeVisitor visitor(nodeManager,
                                 ClipBlendNodeVisitor::PreOrder,
                                 ClipBlendNodeVisitor::VisitOnlyDependencies);

    auto func = [&clipIds, nodeManager] (ClipBlendNode *blendNode) {
        const auto dependencyIds = blendNode->currentDependencyIds();
        for (const auto dependencyId : dependencyIds) {
            // Look up the blend node and if it's a value type (clip),
            // add it to the set of value node ids that need to be evaluated
            ClipBlendNode *node = nodeManager->lookupNode(dependencyId);
            if (node && node->blendType() == ClipBlendNode::ValueType)
                clipIds.append(dependencyId);
        }
    };
    visitor.traverse(blendTreeRootId, func);

    // Sort and remove duplicates
    std::sort(clipIds.begin(), clipIds.end());
    auto last = std::unique(clipIds.begin(), clipIds.end());
    clipIds.erase(last, clipIds.end());
    return clipIds;
}

ComponentIndices generateClipFormatIndices(const QVector<ChannelNameAndType> &targetChannels,
                                           const QVector<ComponentIndices> &targetIndices,
                                           const AnimationClip *clip)
{
    Q_ASSERT(targetChannels.size() == targetIndices.size());

    // Reserve enough storage for all the format indices
    int indexCount = 0;
    for (const auto targetIndexVec : qAsConst(targetIndices))
        indexCount += targetIndexVec.size();
    ComponentIndices format;
    format.resize(indexCount);


    // Iterate through the target channels
    const int channelCount = targetChannels.size();
    auto formatIt = format.begin();
    for (int i = 0; i < channelCount; ++i) {
        // Find the index of the channel from the clip
        const ChannelNameAndType &targetChannel = targetChannels[i];
        const int clipChannelIndex = clip->channelIndex(targetChannel.name);

        // TODO: Ensure channel in the clip has enough components to map to the type.
        //       Requires some improvements to the clip data structure first.
        // TODO: I don't think we need the targetIndices, only the number of components
        //       for each target channel. Check once blend tree is complete.
        const int componentCount = targetIndices[i].size();

        if (clipChannelIndex != -1) {
            // Found a matching channel in the clip. Get the base channel
            // component index and populate the format indices for this channel.
            const int baseIndex = clip->channelComponentBaseIndex(clipChannelIndex);
            std::iota(formatIt, formatIt + componentCount, baseIndex);

        } else {
            // No such channel in this clip. We'll use default values when
            // mapping from the clip to the formatted clip results.
            std::fill(formatIt, formatIt + componentCount, -1);
        }

        formatIt += componentCount;
    }

    return format;
}

ClipResults formatClipResults(const ClipResults &rawClipResults,
                              const ComponentIndices &format)
{
    // Resize the output to match the number of indices
    const int elementCount = format.size();
    ClipResults formattedClipResults(elementCount);

    // Perform a gather operation to format the data
    // TODO: For large numbers of components do this in parallel with
    // for e.g. a parallel_for() like construct
    for (int i = 0; i < elementCount; ++i) {
        const float value = format[i] != -1 ? rawClipResults[format[i]] : 0.0f;
        formattedClipResults[i] = value;
    }

    return formattedClipResults;
}

ClipResults evaluateBlendTree(Handler *handler,
                              BlendedClipAnimator *animator,
                              Qt3DCore::QNodeId blendTreeRootId)
{
    Q_ASSERT(handler);
    Q_ASSERT(blendTreeRootId.isNull() == false);
    const Qt3DCore::QNodeId animatorId = animator->peerId();

    // We need the ClipBlendNodeManager to be able to lookup nodes from their Ids
    ClipBlendNodeManager *nodeManager = handler->clipBlendNodeManager();

    // Visit the tree in a post-order manner and for each interior node call
    // blending function. We only need to visit the nodes that affect the blend
    // tree at this time.
    ClipBlendNodeVisitor visitor(nodeManager,
                                 ClipBlendNodeVisitor::PostOrder,
                                 ClipBlendNodeVisitor::VisitOnlyDependencies);

    // TODO: When jobs can spawn other jobs we could evaluate subtrees of
    // the blend tree in parallel. Since it's just a dependency tree, it maps
    // simply onto the dependencies between jobs.
    auto func = [animatorId] (ClipBlendNode *blendNode) {
        // Look up the blend node and if it's an interior node, perform
        // the blend operation
        if (blendNode->blendType() != ClipBlendNode::ValueType)
            blendNode->blend(animatorId);
    };
    visitor.traverse(blendTreeRootId, func);

    // The clip results stored in the root node for this animator
    // now represent the result of the blend tree evaluation
    ClipBlendNode *blendTreeRootNode = nodeManager->lookupNode(blendTreeRootId);
    Q_ASSERT(blendTreeRootNode);
    return blendTreeRootNode->clipResults(animatorId);
}

} // Animation
} // Qt3DAnimation

QT_END_NAMESPACE
