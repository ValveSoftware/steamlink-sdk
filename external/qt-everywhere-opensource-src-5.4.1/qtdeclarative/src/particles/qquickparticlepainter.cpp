/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickparticlepainter_p.h"
#include <QQuickWindow>
#include <QDebug>
QT_BEGIN_NAMESPACE
/*!
    \qmltype ParticlePainter
    \instantiates QQuickParticlePainter
    \inqmlmodule QtQuick.Particles
    \inherits Item
    \brief For specifying how to paint particles
    \ingroup qtquick-particles

    The default implementation paints nothing. See the subclasses if you want to
    paint something visible.

*/
/*!
    \qmlproperty ParticleSystem QtQuick.Particles::ParticlePainter::system
    This is the system whose particles can be painted by the element.
    If the ParticlePainter is a direct child of a ParticleSystem, it will automatically be associated with it.
*/
/*!
    \qmlproperty list<string> QtQuick.Particles::ParticlePainter::groups
    Which logical particle groups will be painted.

    If empty, it will paint the default particle group ("").
*/
QQuickParticlePainter::QQuickParticlePainter(QQuickItem *parent) :
    QQuickItem(parent),
    m_system(0), m_count(0), m_pleaseReset(true), m_window(0)
{
}

void QQuickParticlePainter::itemChange(ItemChange change, const ItemChangeData &data)
{
    if (change == QQuickItem::ItemSceneChange) {
        if (m_window)
            disconnect(m_window, SIGNAL(sceneGraphInvalidated()), this, SLOT(sceneGraphInvalidated()));
        m_window = data.window;
        if (m_window)
            connect(m_window, SIGNAL(sceneGraphInvalidated()), this, SLOT(sceneGraphInvalidated()), Qt::DirectConnection);
    }
    QQuickItem::itemChange(change, data);
}

void QQuickParticlePainter::componentComplete()
{
    if (!m_system && qobject_cast<QQuickParticleSystem*>(parentItem()))
        setSystem(qobject_cast<QQuickParticleSystem*>(parentItem()));
    QQuickItem::componentComplete();
}


void QQuickParticlePainter::setSystem(QQuickParticleSystem *arg)
{
    if (m_system != arg) {
        m_system = arg;
        if (m_system){
            m_system->registerParticlePainter(this);
            reset();
        }
        emit systemChanged(arg);
    }
}

void QQuickParticlePainter::load(QQuickParticleData* d)
{
    initialize(d->group, d->index);
    if (m_pleaseReset)
        return;
    m_pendingCommits << qMakePair<int, int>(d->group, d->index);
}

void QQuickParticlePainter::reload(QQuickParticleData* d)
{
    if (m_pleaseReset)
        return;
    m_pendingCommits << qMakePair<int, int>(d->group, d->index);
}

void QQuickParticlePainter::reset()
{
    m_pendingCommits.clear();
    m_pleaseReset = true;
}

void QQuickParticlePainter::setCount(int c)//### TODO: some resizeing so that particles can reallocate on size change instead of recreate
{
    Q_ASSERT(c >= 0); //XXX
    if (c == m_count)
        return;
    m_count = c;
    emit countChanged();
    reset();
}

int QQuickParticlePainter::count()
{
    return m_count;
}

void QQuickParticlePainter::calcSystemOffset(bool resetPending)
{
    if (!m_system || !parentItem())
        return;
    QPointF lastOffset = m_systemOffset;
    m_systemOffset = -1 * this->mapFromItem(m_system, QPointF(0.0, 0.0));
    if (lastOffset != m_systemOffset && !resetPending){
        //Reload all particles//TODO: Necessary?
        foreach (const QString &g, m_groups){
            int gId = m_system->groupIds[g];
            foreach (QQuickParticleData* d, m_system->groupData[gId]->data)
                reload(d);
        }
    }
}
typedef QPair<int,int> intPair;
void QQuickParticlePainter::performPendingCommits()
{
    calcSystemOffset();
    foreach (intPair p, m_pendingCommits)
        commit(p.first, p.second);
    m_pendingCommits.clear();
}

QT_END_NAMESPACE
