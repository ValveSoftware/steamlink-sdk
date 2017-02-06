/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qrendersettings.h"
#include "qrendersettings_p.h"
#include "qframegraphnode.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
   \class Qt3DRender::QRenderSettings
   \brief The QRenderSettings class holds settings related to rendering process and host the active
   \l{Qt 3D Render Framegraph}{FrameGraph}.
   \since 5.7
   \inmodule Qt3DRender
   \inherits Qt3DCore::QComponent

   The QRenderSettings component must be set as a component of the scene root entity. It specifies
   render policy and picking settings, as well as hosts the active
   \l{Qt 3D Render Framegraph}{FrameGraph}.
 */

/*!
   \qmltype RenderSettings
   \brief The RenderSettings type holds settings related to rendering process and host the active
   \l{Qt 3D Render Framegraph}{FrameGraph}.
   \since 5.7
   \inqmlmodule Qt3D.Render
   \instantiates Qt3DRender::QRenderSettings

   The RenderSettings component must be set as a component of the scene root entity. It specifies
   render policy and picking settings, as well as hosts the active
   \l{Qt 3D Render Framegraph}{FrameGraph}.
 */

/*!
    \enum QRenderSettings::RenderPolicy

    This enum type describes types of render policies available.
    \value Always Always try to render (default)
    \value OnDemand Only render when something changes
*/

/*! \internal */
QRenderSettingsPrivate::QRenderSettingsPrivate()
    : Qt3DCore::QComponentPrivate()
    , m_activeFrameGraph(nullptr)
    , m_renderPolicy(QRenderSettings::Always)
{
}

/*! \internal */
void QRenderSettingsPrivate::init()
{
    Q_Q(QRenderSettings);
    QObject::connect(&m_pickingSettings, SIGNAL(pickMethodChanged(QPickingSettings::PickMethod)),
                     q, SLOT(_q_onPickingMethodChanged(QPickingSettings::PickMethod)));
    QObject::connect(&m_pickingSettings, SIGNAL(pickResultModeChanged(QPickingSettings::PickResultMode)),
                     q, SLOT(_q_onPickResultModeChanged(QPickingSettings::PickResultMode)));
    QObject::connect(&m_pickingSettings, SIGNAL(faceOrientationPickingModeChanged(QPickingSettings::FaceOrientationPickingMode)),
                     q, SLOT(_q_onFaceOrientationPickingModeChanged(QPickingSettings::FaceOrientationPickingMode)));
}

/*! \internal */
void QRenderSettingsPrivate::_q_onPickingMethodChanged(QPickingSettings::PickMethod pickMethod)
{
    notifyPropertyChange("pickMethod", pickMethod);
}

/*! \internal */
void QRenderSettingsPrivate::_q_onPickResultModeChanged(QPickingSettings::PickResultMode pickResultMode)
{
    notifyPropertyChange("pickResultMode", pickResultMode);
}

/*! \internal */
void QRenderSettingsPrivate::_q_onFaceOrientationPickingModeChanged(QPickingSettings::FaceOrientationPickingMode faceOrientationPickingMode)
{
    notifyPropertyChange("faceOrientationPickingMode", faceOrientationPickingMode);
}

QRenderSettings::QRenderSettings(Qt3DCore::QNode *parent)
    : QRenderSettings(*new QRenderSettingsPrivate, parent) {}

/*! \internal */
QRenderSettings::QRenderSettings(QRenderSettingsPrivate &dd, Qt3DCore::QNode *parent)
    : Qt3DCore::QComponent(dd, parent)
{
    Q_D(QRenderSettings);
    d->init();
}

/*! \internal */
QRenderSettings::~QRenderSettings()
{
}

/*!
    \qmlproperty PickingSettings RenderSettings::pickingSettings

    Holds the current pick settings for the \l{Qt 3D Render Framegraph}{FrameGraph}.

    \readonly
*/
/*!
    \property QRenderSettings::pickingSettings

    Holds the current pick settings for the \l{Qt 3D Render Framegraph}{FrameGraph}.

    \readonly
*/
QPickingSettings *QRenderSettings::pickingSettings()
{
    Q_D(QRenderSettings);
    return &(d->m_pickingSettings);
}

/*!
    \qmlproperty FrameGraphNode RenderSettings::activeFrameGraph

    Holds the currently active \l{Qt 3D Render Framegraph}{FrameGraph}.
*/
/*!
    \property QRenderSettings::activeFrameGraph

    Holds the currently active \l{Qt 3D Render Framegraph}{FrameGraph}.
*/
QFrameGraphNode *QRenderSettings::activeFrameGraph() const
{
    Q_D(const QRenderSettings);
    return d->m_activeFrameGraph;
}


/*!
    \enum QRenderSettings::RenderPolicy

    The render policy.

    \value OnDemand The \l{Qt 3D Render Framegraph}{FrameGraph} is rendered only when something
    changes.
    \value Always The \l{Qt 3D Render Framegraph}{FrameGraph} is rendered continuously, even if
    nothing has changed.
*/

/*!
    \qmlproperty enumeration RenderSettings::renderPolicy

    Holds the current render policy.

    \list
        \li RenderSettings.OnDemand
        \li RenderSettings.Always
    \endlist

    \sa Qt3DRender::QRenderSettings::RenderPolicy
*/
/*!
    \property QRenderSettings::renderPolicy

    Holds the current render policy.
*/
QRenderSettings::RenderPolicy QRenderSettings::renderPolicy() const
{
    Q_D(const QRenderSettings);
    return d->m_renderPolicy;
}

void QRenderSettings::setActiveFrameGraph(QFrameGraphNode *activeFrameGraph)
{
    Q_D(QRenderSettings);
    if (d->m_activeFrameGraph == activeFrameGraph)
        return;

    if (d->m_activeFrameGraph)
        d->unregisterDestructionHelper(d->m_activeFrameGraph);

    if (activeFrameGraph != nullptr && !activeFrameGraph->parent())
        activeFrameGraph->setParent(this);

    d->m_activeFrameGraph = activeFrameGraph;

    // Ensures proper bookkeeping
    if (d->m_activeFrameGraph)
        d->registerDestructionHelper(d->m_activeFrameGraph, &QRenderSettings::setActiveFrameGraph, d->m_activeFrameGraph);

    emit activeFrameGraphChanged(activeFrameGraph);
}

void QRenderSettings::setRenderPolicy(QRenderSettings::RenderPolicy renderPolicy)
{
    Q_D(QRenderSettings);
    if (d->m_renderPolicy == renderPolicy)
        return;

    d->m_renderPolicy = renderPolicy;
    emit renderPolicyChanged(renderPolicy);
}

Qt3DCore::QNodeCreatedChangeBasePtr QRenderSettings::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QRenderSettingsData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QRenderSettings);
    data.activeFrameGraphId = qIdForNode(d->m_activeFrameGraph);
    data.renderPolicy = d->m_renderPolicy;
    data.pickMethod = d->m_pickingSettings.pickMethod();
    data.pickResultMode = d->m_pickingSettings.pickResultMode();
    data.faceOrientationPickingMode = d->m_pickingSettings.faceOrientationPickingMode();
    return creationChange;
}

} // namespace Qt3Drender

QT_END_NAMESPACE

#include "moc_qrendersettings.cpp"
