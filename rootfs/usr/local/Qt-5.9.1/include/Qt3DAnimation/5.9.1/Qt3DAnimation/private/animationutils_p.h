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

#ifndef QT3DANIMATION_ANIMATION_ANIMATIONUTILS_P_H
#define QT3DANIMATION_ANIMATION_ANIMATIONUTILS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <Qt3DAnimation/private/qt3danimation_global_p.h>
#include <Qt3DCore/qnodeid.h>
#include <Qt3DCore/qscenechange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

struct Channel;
class BlendedClipAnimator;
class Handler;
class AnimationClip;
class ChannelMapper;
class ChannelMapping;

typedef QVector<int> ComponentIndices;

struct MappingData
{
    Qt3DCore::QNodeId targetId;
    const char *propertyName;
    int type;
    ComponentIndices channelIndices;
};

struct AnimatorEvaluationData
{
    double globalTime;
    double startTime;
    int loopCount;
    double playbackRate;
};

struct ClipEvaluationData
{
    int currentLoop;
    double localTime;
    bool isFinalFrame;
};

typedef QVector<float> ClipResults;

struct ChannelNameAndType
{
    QString name;
    int type;

    bool operator==(const ChannelNameAndType &rhs) const
    {
        return name == rhs.name && type == rhs.type;
    }
};

template<typename Animator>
AnimatorEvaluationData evaluationDataForAnimator(Animator animator, qint64 globalTime)
{
    AnimatorEvaluationData data;
    data.loopCount = animator->loops();
    data.playbackRate = 1.0; // should be a property on the animator
    // Convert global time from nsec to sec
    data.startTime = double(animator->startTime()) / 1.0e9;
    data.globalTime = double(globalTime) / 1.0e9;
    return data;
}

inline bool isFinalFrame(double localTime,
                         double duration,
                         int currentLoop,
                         int loopCount)
{
    return (localTime >= duration &&
            loopCount != 0 &&
            currentLoop >= loopCount - 1);
}

Q_AUTOTEST_EXPORT
int componentsForType(int type);

Q_AUTOTEST_EXPORT
ClipEvaluationData evaluationDataForClip(AnimationClip *clip,
                                         const AnimatorEvaluationData &animatorData);

Q_AUTOTEST_EXPORT
ComponentIndices channelComponentsToIndices(const Channel &channel,
                                            int dataType,
                                            int offset = 0);

Q_AUTOTEST_EXPORT
ComponentIndices channelComponentsToIndicesHelper(const Channel &channelGroup,
                                                  int dataType,
                                                  int offset,
                                                  const QVector<char> &suffixes);

Q_AUTOTEST_EXPORT
ClipResults evaluateClipAtLocalTime(AnimationClip *clip,
                                    float localTime);

Q_AUTOTEST_EXPORT
ClipResults evaluateClipAtPhase(AnimationClip *clip,
                                float phase);

Q_AUTOTEST_EXPORT
QVector<Qt3DCore::QSceneChangePtr> preparePropertyChanges(Qt3DCore::QNodeId animatorId,
                                                          const QVector<MappingData> &mappingData,
                                                          const QVector<float> &channelResults,
                                                          bool finalFrame);

Q_AUTOTEST_EXPORT
QVector<MappingData> buildPropertyMappings(Handler *handler,
                                           const AnimationClip *clip,
                                           const ChannelMapper *mapper);

Q_AUTOTEST_EXPORT
QVector<MappingData> buildPropertyMappings(const QVector<ChannelMapping *> &channelMappings,
                                           const QVector<ChannelNameAndType> &channelNamesAndTypes,
                                           const QVector<ComponentIndices> &channelComponentIndices);

Q_AUTOTEST_EXPORT
QVector<ChannelNameAndType> buildRequiredChannelsAndTypes(Handler *handler,
                                                          const ChannelMapper *mapper);

Q_AUTOTEST_EXPORT
QVector<ComponentIndices> assignChannelComponentIndices(const QVector<ChannelNameAndType> &namesAndTypes);

Q_AUTOTEST_EXPORT
double localTimeFromGlobalTime(double t_global, double t_start_global,
                               double playbackRate, double duration,
                               int loopCount, int &currentLoop);

Q_AUTOTEST_EXPORT
double phaseFromGlobalTime(double t_global, double t_start_global,
                           double playbackRate, double duration,
                           int loopCount, int &currentLoop);

Q_AUTOTEST_EXPORT
QVector<Qt3DCore::QNodeId> gatherValueNodesToEvaluate(Handler *handler,
                                                      Qt3DCore::QNodeId blendTreeRootId);

Q_AUTOTEST_EXPORT
ComponentIndices generateClipFormatIndices(const QVector<ChannelNameAndType> &targetChannels,
                                           const QVector<ComponentIndices> &targetIndices,
                                           const AnimationClip *clip);

Q_AUTOTEST_EXPORT
ClipResults formatClipResults(const ClipResults &rawClipResults,
                              const ComponentIndices &format);

Q_AUTOTEST_EXPORT
ClipResults evaluateBlendTree(Handler *handler,
                              BlendedClipAnimator *animator,
                              Qt3DCore::QNodeId blendNodeId);

} // Animation
} // Qt3DAnimation

QT_END_NAMESPACE


#endif // QT3DANIMATION_ANIMATION_ANIMATIONUTILS_P_H
