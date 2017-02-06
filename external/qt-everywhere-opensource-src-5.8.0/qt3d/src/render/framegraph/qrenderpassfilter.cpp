/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qrenderpassfilter.h"
#include "qrenderpassfilter_p.h"

#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

/*!
    \class Qt3DRender::QRenderPassFilter
    \inmodule Qt3DRender
    \since 5.7
    \brief Provides storage for vectors of Filter Keys and Parameters
*/

/*!
  The constructor creates an instance with the specified \a parent.
 */
QRenderPassFilter::QRenderPassFilter(QNode *parent)
    : QFrameGraphNode(*new QRenderPassFilterPrivate, parent)
{}

/*! \internal */
QRenderPassFilter::~QRenderPassFilter()
{
}

/*! \internal */
QRenderPassFilter::QRenderPassFilter(QRenderPassFilterPrivate &dd, QNode *parent)
    : QFrameGraphNode(dd, parent)
{
}

/*!
  Returns a vector of the current keys for the filter.
 */
QVector<QFilterKey *> QRenderPassFilter::matchAny() const
{
    Q_D(const QRenderPassFilter);
    return d->m_matchList;
}

/*!
  Add the filter key \a filterKey to the match vector.
 */
void QRenderPassFilter::addMatch(QFilterKey *filterKey)
{
    Q_ASSERT(filterKey);
    Q_D(QRenderPassFilter);
    if (!d->m_matchList.contains(filterKey)) {
        d->m_matchList.append(filterKey);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(filterKey, &QRenderPassFilter::removeMatch, d->m_matchList);

        // We need to add it as a child of the current node if it has been declared inline
        // Or not previously added as a child of the current node so that
        // 1) The backend gets notified about it's creation
        // 2) When the current node is destroyed, it gets destroyed as well
        if (!filterKey->parent())
            filterKey->setParent(this);

        if (d->m_changeArbiter != nullptr) {
            const auto change = QPropertyNodeAddedChangePtr::create(id(), filterKey);
            change->setPropertyName("match");
            d->notifyObservers(change);
        }
    }
}

/*!
  Remove the filter key \a filterKey from the match vector.
 */
void QRenderPassFilter::removeMatch(QFilterKey *filterKey)
{
    Q_ASSERT(filterKey);
    Q_D(QRenderPassFilter);

    if (d->m_changeArbiter != nullptr) {
        const auto change = QPropertyNodeRemovedChangePtr::create(id(), filterKey);
        change->setPropertyName("match");
        d->notifyObservers(change);
    }
    d->m_matchList.removeOne(filterKey);
    // Remove bookkeeping connection
    d->unregisterDestructionHelper(filterKey);
}

/*!
  Add the parameter \a parameter to the parameter vector.
 */
void QRenderPassFilter::addParameter(QParameter *parameter)
{
    Q_ASSERT(parameter);
    Q_D(QRenderPassFilter);
    if (!d->m_parameters.contains(parameter)) {
        d->m_parameters.append(parameter);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(parameter, &QRenderPassFilter::removeParameter, d->m_parameters);

        // We need to add it as a child of the current node if it has been declared inline
        // Or not previously added as a child of the current node so that
        // 1) The backend gets notified about it's creation
        // 2) When the current node is destroyed, the child parameters get destroyed as well
        if (!parameter->parent())
            parameter->setParent(this);

        if (d->m_changeArbiter != nullptr) {
            const auto change = QPropertyNodeAddedChangePtr::create(id(), parameter);
            change->setPropertyName("parameter");
            d->notifyObservers(change);
        }
    }
}

/*!
  Remove the parameter \a parameter from the parameter vector.
 */
void QRenderPassFilter::removeParameter(QParameter *parameter)
{
    Q_ASSERT(parameter);
    Q_D(QRenderPassFilter);

    if (d->m_changeArbiter != nullptr) {
        const auto change = QPropertyNodeRemovedChangePtr::create(id(), parameter);
        change->setPropertyName("parameter");
        d->notifyObservers(change);
    }
    d->m_parameters.removeOne(parameter);
    // Remove bookkeeping connection
    d->unregisterDestructionHelper(parameter);
}

/*!
  Returns the current vector of parameters.
 */
QVector<QParameter *> QRenderPassFilter::parameters() const
{
    Q_D(const QRenderPassFilter);
    return d->m_parameters;
}

Qt3DCore::QNodeCreatedChangeBasePtr QRenderPassFilter::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QRenderPassFilterData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QRenderPassFilter);
    data.matchIds = qIdsForNodes(d->m_matchList);
    data.parameterIds = qIdsForNodes(d->m_parameters);
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
