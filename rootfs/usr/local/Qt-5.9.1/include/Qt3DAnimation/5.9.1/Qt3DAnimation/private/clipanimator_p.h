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

#ifndef QT3DANIMATION_ANIMATION_CLIPANIMATOR_P_H
#define QT3DANIMATION_ANIMATION_CLIPANIMATOR_P_H

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

#include <Qt3DAnimation/private/backendnode_p.h>
#include <Qt3DAnimation/private/animationutils_p.h>
#include <Qt3DCore/qnodeid.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

struct Channel;
class Handler;

class Q_AUTOTEST_EXPORT ClipAnimator : public BackendNode
{
public:
    ClipAnimator();

    void cleanup();
    void setClipId(Qt3DCore::QNodeId clipId);
    Qt3DCore::QNodeId clipId() const { return m_clipId; }
    void setMapperId(Qt3DCore::QNodeId mapperId);
    Qt3DCore::QNodeId mapperId() const { return m_mapperId; }

    void setRunning(bool running);
    bool isRunning() const { return m_running; }
    void setLoops(int loops) { m_loops = loops; }
    int loops() const { return m_loops; }

    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e) Q_DECL_OVERRIDE;
    void setHandler(Handler *handler) { m_handler = handler; }

    // Called by jobs
    bool canRun() const { return !m_clipId.isNull() && !m_mapperId.isNull() && m_running; }
    void setMappingData(const QVector<MappingData> &mappingData) { m_mappingData = mappingData; }
    QVector<MappingData> mappingData() const { return m_mappingData; }

    void setStartTime(qint64 globalTime) { m_startGlobalTime = globalTime; }
    qint64 startTime() const { return m_startGlobalTime; }

    int currentLoop() const { return m_currentLoop; }
    void setCurrentLoop(int currentLoop) { m_currentLoop = currentLoop; }

    void sendPropertyChanges(const QVector<Qt3DCore::QSceneChangePtr> &changes);

private:
    void initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change) Q_DECL_FINAL;

    Qt3DCore::QNodeId m_clipId;
    Qt3DCore::QNodeId m_mapperId;
    bool m_running;
    int m_loops;

    // Working state
    qint64 m_startGlobalTime;
    QVector<MappingData> m_mappingData;

    int m_currentLoop;
};

} // namespace Animation
} // namespace Qt3DAnimation


QT_END_NAMESPACE

#endif // QT3DANIMATION_ANIMATION_CLIPANIMATOR_P_H
