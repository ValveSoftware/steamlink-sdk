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

#include "qquickitemparticle_p.h"
#include <QtQuick/qsgnode.h>
#include <QTimer>
#include <QQmlComponent>
#include <QDebug>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ItemParticle
    \instantiates QQuickItemParticle
    \inqmlmodule QtQuick.Particles
    \inherits ParticlePainter
    \brief For specifying a delegate to paint particles
    \ingroup qtquick-particles

*/


/*!
    \qmlmethod QtQuick.Particles::ItemParticle::freeze(Item item)

    Suspends the flow of time for the logical particle which item represents, allowing you to control its movement.
*/

/*!
    \qmlmethod QtQuick.Particles::ItemParticle::unfreeze(Item item)

    Restarts the flow of time for the logical particle which item represents, allowing it to be moved by the particle system again.
*/

/*!
    \qmlmethod QtQuick.Particles::ItemParticle::take(Item item, bool prioritize)

    Asks the ItemParticle to take over control of item positioning temporarily.
    It will follow the movement of a logical particle when one is available.

    By default items form a queue when waiting for a logical particle, but if prioritize is true then it will go immediately to the
    head of the queue.

    ItemParticle does not take ownership of the item, and will relinquish
    control when the logical particle expires. Commonly at this point you will
    want to put it back in the queue, you can do this with the below line in
    the delegate definition:
    \code
    ItemParticle.onDetached: itemParticleInstance.take(delegateRootItem);
    \endcode
    or delete it, such as with the below line in the delegate definition:
    \code
    ItemParticle.onDetached: delegateRootItem.destroy();
    \endcode
*/

/*!
    \qmlmethod QtQuick.Particles::ItemParticle::give(Item item)

    Orders the ItemParticle to give you control of the item. It will cease controlling it and the item will lose its association to the logical particle.
*/

/*!
    \qmlproperty bool QtQuick.Particles::ItemParticle::fade

    If true, the item will automatically be faded in and out
    at the ends of its lifetime. If false, you will have to
    implement any entry effect yourself.

    Default is true.
*/
/*!
    \qmlproperty Component QtQuick.Particles::ItemParticle::delegate

    An instance of the delegate will be created for every logical particle, and
    moved along with it. As an alternative to using delegate, you can create
    Item instances yourself and hand them to the ItemParticle to move using the
    \l take method.

    Any delegate instances created by ItemParticle will be destroyed when
    the logical particle expires.
*/

QQuickItemParticle::QQuickItemParticle(QQuickItem *parent) :
    QQuickParticlePainter(parent), m_fade(true), m_lastT(0), m_activeCount(0), m_delegate(0)
{
    setFlag(QQuickItem::ItemHasContents);
    clock = new Clock(this);
    clock->start();
}

QQuickItemParticle::~QQuickItemParticle()
{
    delete clock;
}

void QQuickItemParticle::freeze(QQuickItem* item)
{
    m_stasis << item;
}


void QQuickItemParticle::unfreeze(QQuickItem* item)
{
    m_stasis.remove(item);
}

void QQuickItemParticle::take(QQuickItem *item, bool prioritize)
{
    if (prioritize)
        m_pendingItems.push_front(item);
    else
        m_pendingItems.push_back(item);
}

void QQuickItemParticle::give(QQuickItem *item)
{
    //TODO: This
    Q_UNUSED(item);
}

void QQuickItemParticle::initialize(int gIdx, int pIdx)
{
    m_loadables << m_system->groupData[gIdx]->data[pIdx];//defer to other thread
}

void QQuickItemParticle::commit(int, int)
{
}

void QQuickItemParticle::processDeletables()
{
    foreach (QQuickItem* item, m_deletables){
        if (m_fade)
            item->setOpacity(0.);
        item->setVisible(false);
        QQuickItemParticleAttached* mpa;
        if ((mpa = qobject_cast<QQuickItemParticleAttached*>(qmlAttachedPropertiesObject<QQuickItemParticle>(item))))
            mpa->detach();//reparent as well?
        int idx = -1;
        if ((idx = m_managed.indexOf(item)) != -1) {
            m_managed.takeAt(idx);
            delete item;
        }
        m_activeCount--;
    }
    m_deletables.clear();
}

void QQuickItemParticle::tick(int time)
{
    Q_UNUSED(time);//only needed because QTickAnimationProxy expects one
    processDeletables();

    foreach (QQuickParticleData* d, m_loadables){
        Q_ASSERT(d);
        if (m_stasis.contains(d->delegate))
            qWarning() << "Current model particles prefers overwrite:false";
        //remove old item from the particle that is dying to make room for this one
        if (d->delegate) {
            m_deletables << d->delegate;
            d->delegate = 0;
        }
        if (!m_pendingItems.isEmpty()){
            d->delegate = m_pendingItems.front();
            m_pendingItems.pop_front();
        }else if (m_delegate){
            d->delegate = qobject_cast<QQuickItem*>(m_delegate->create(qmlContext(this)));
            if (d->delegate)
                m_managed << d->delegate;
        }
        if (d && d->delegate){//###Data can be zero if creating an item leads to a reset - this screws things up.
            d->delegate->setX(d->curX(m_system) - d->delegate->width() / 2); //TODO: adjust for system?
            d->delegate->setY(d->curY(m_system) - d->delegate->height() / 2);
            QQuickItemParticleAttached* mpa = qobject_cast<QQuickItemParticleAttached*>(qmlAttachedPropertiesObject<QQuickItemParticle>(d->delegate));
            if (mpa){
                mpa->m_mp = this;
                mpa->attach();
            }
            d->delegate->setParentItem(this);
            if (m_fade)
                d->delegate->setOpacity(0.);
            d->delegate->setVisible(false);//Will be set to true when we prepare the next frame
            m_activeCount++;
        }
    }
    m_loadables.clear();
}

void QQuickItemParticle::reset()
{
    QQuickParticlePainter::reset();
    m_loadables.clear();

    // delete all managed items which had their logical particles cleared
    // but leave it alone if the logical particle is maintained
    QSet<QQuickItem*> lost = QSet<QQuickItem*>::fromList(m_managed);
    for (auto groupId : groupIds()) {
        for (QQuickParticleData* d : qAsConst(m_system->groupData[groupId]->data)) {
            lost.remove(d->delegate);
        }
    }
    m_deletables.append(lost.toList());
    //TODO: This doesn't yet handle calling detach on taken particles in the system reset case
    processDeletables();
}


QSGNode* QQuickItemParticle::updatePaintNode(QSGNode* n, UpdatePaintNodeData* d)
{
    //Dummy update just to get painting tick
    if (m_pleaseReset){
        m_pleaseReset = false;
        //Refill loadables, delayed here so as to only happen once per frame max
        //### Constant resetting might lead to m_loadables never being populated when tick() occurs
        for (auto groupId : groupIds()) {
            for (QQuickParticleData* d : qAsConst(m_system->groupData[groupId]->data)) {
                if (!d->delegate && d->t != -1  && d->stillAlive(m_system)) {
                    m_loadables << d;
                }
            }
        }
    }
    prepareNextFrame();

    update();//Get called again
    if (n)
        n->markDirty(QSGNode::DirtyMaterial);
    return QQuickItem::updatePaintNode(n,d);
}

void QQuickItemParticle::prepareNextFrame()
{
    if (!m_system)
        return;
    qint64 timeStamp = m_system->systemSync(this);
    qreal curT = timeStamp/1000.0;
    qreal dt = curT - m_lastT;
    m_lastT = curT;
    if (!m_activeCount)
        return;

    //TODO: Size, better fade?
    for (auto groupId : groupIds()) {
        for (QQuickParticleData* data : qAsConst(m_system->groupData[groupId]->data)) {
            QQuickItem* item = data->delegate;
            if (!item)
                continue;
            float t = ((timeStamp / 1000.0f) - data->t) / data->lifeSpan;
            if (m_stasis.contains(item)) {
                data->t += dt;//Stasis effect
                continue;
            }
            if (t >= 1.0f){//Usually happens from load
                m_deletables << item;
                data->delegate = 0;
            }else{//Fade
                data->delegate->setVisible(true);
                if (m_fade){
                    float o = 1.f;
                    if (t <0.2f)
                        o = t * 5;
                    if (t > 0.8f)
                        o = (1-t)*5;
                    item->setOpacity(o);
                }
            }
            item->setX(data->curX(m_system) - item->width() / 2 - m_systemOffset.x());
            item->setY(data->curY(m_system) - item->height() / 2 - m_systemOffset.y());
        }
    }
}

QQuickItemParticleAttached *QQuickItemParticle::qmlAttachedProperties(QObject *object)
{
    return new QQuickItemParticleAttached(object);
}

QT_END_NAMESPACE
