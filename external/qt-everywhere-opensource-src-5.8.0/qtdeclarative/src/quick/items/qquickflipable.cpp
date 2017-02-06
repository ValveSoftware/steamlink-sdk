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

#include "qquickflipable_p.h"
#include "qquickitem_p.h"


#include <QtQml/qqmlinfo.h>

QT_BEGIN_NAMESPACE

// XXX todo - i think this needs work and a bit of a re-think

class QQuickLocalTransform : public QQuickTransform
{
    Q_OBJECT
public:
    QQuickLocalTransform(QObject *parent) : QQuickTransform(parent) {}

    void setTransform(const QTransform &t) {
        transform = t;
        update();
    }
    void applyTo(QMatrix4x4 *matrix) const Q_DECL_OVERRIDE {
        *matrix *= transform;
    }
private:
    QTransform transform;
};

class QQuickFlipablePrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QQuickFlipable)
public:
    QQuickFlipablePrivate() : current(QQuickFlipable::Front), front(0), back(0), sideDirty(false) {}

    void transformChanged() Q_DECL_OVERRIDE;
    void updateSide();
    void setBackTransform();

    QQuickFlipable::Side current;
    QPointer<QQuickLocalTransform> backTransform;
    QPointer<QQuickItem> front;
    QPointer<QQuickItem> back;

    bool sideDirty;
    bool wantBackXFlipped;
    bool wantBackYFlipped;
};

/*!
    \qmltype Flipable
    \instantiates QQuickFlipable
    \inqmlmodule QtQuick
    \inherits Item
    \ingroup qtquick-input
    \ingroup qtquick-containers
    \brief Provides a surface that can be flipped

    Flipable is an item that can be visibly "flipped" between its front and
    back sides, like a card. It may used together with \l Rotation, \l State
    and \l Transition types to produce a flipping effect.

    The \l front and \l back properties are used to hold the items that are
    shown respectively on the front and back sides of the flipable item.

    \section1 Example Usage

    The following example shows a Flipable item that flips whenever it is
    clicked, rotating about the y-axis.

    This flipable item has a \c flipped boolean property that is toggled
    whenever the MouseArea within the flipable is clicked. When
    \c flipped is true, the item changes to the "back" state; in this
    state, the \c angle of the \l Rotation item is changed to 180
    degrees to produce the flipping effect. When \c flipped is false, the
    item reverts to the default state, in which the \c angle value is 0.

    \snippet qml/flipable/flipable.qml 0

    \image flipable.gif

    The \l Transition creates the animation that changes the angle over
    four seconds. When the item changes between its "back" and
    default states, the NumberAnimation animates the angle between
    its old and new values.

    See \l {Qt Quick States} for details on state changes and the default
    state, and \l {Animation and Transitions in Qt Quick} for more information on how
    animations work within transitions.

    \sa {customitems/flipable}{UI Components: Flipable Example}
*/
QQuickFlipable::QQuickFlipable(QQuickItem *parent)
: QQuickItem(*(new QQuickFlipablePrivate), parent)
{
}

QQuickFlipable::~QQuickFlipable()
{
}

/*!
  \qmlproperty Item QtQuick::Flipable::front
  \qmlproperty Item QtQuick::Flipable::back

  The front and back sides of the flipable.
*/

QQuickItem *QQuickFlipable::front()
{
    Q_D(const QQuickFlipable);
    return d->front;
}

void QQuickFlipable::setFront(QQuickItem *front)
{
    Q_D(QQuickFlipable);
    if (d->front) {
        qmlInfo(this) << tr("front is a write-once property");
        return;
    }
    d->front = front;
    d->front->setParentItem(this);
    if (Back == d->current) {
        d->front->setOpacity(0.);
        d->front->setEnabled(false);
    }
    emit frontChanged();
}

QQuickItem *QQuickFlipable::back()
{
    Q_D(const QQuickFlipable);
    return d->back;
}

void QQuickFlipable::setBack(QQuickItem *back)
{
    Q_D(QQuickFlipable);
    if (d->back) {
        qmlInfo(this) << tr("back is a write-once property");
        return;
    }
    if (back == 0)
        return;
    d->back = back;
    d->back->setParentItem(this);

    d->backTransform = new QQuickLocalTransform(d->back);
    d->backTransform->prependToItem(d->back);

    if (Front == d->current) {
        d->back->setOpacity(0.);
        d->back->setEnabled(false);
    }

    connect(back, SIGNAL(widthChanged()),
            this, SLOT(retransformBack()));
    connect(back, SIGNAL(heightChanged()),
            this, SLOT(retransformBack()));
    emit backChanged();
}

void QQuickFlipable::retransformBack()
{
    Q_D(QQuickFlipable);
    if (d->current == QQuickFlipable::Back && d->back)
        d->setBackTransform();
}

/*!
  \qmlproperty enumeration QtQuick::Flipable::side

  The side of the Flipable currently visible. Possible values are \c
  Flipable.Front and \c Flipable.Back.
*/
QQuickFlipable::Side QQuickFlipable::side() const
{
    Q_D(const QQuickFlipable);

    const_cast<QQuickFlipablePrivate *>(d)->updateSide();
    return d->current;
}

void QQuickFlipablePrivate::transformChanged()
{
    Q_Q(QQuickFlipable);

    if (!sideDirty) {
        sideDirty = true;
        q->polish();
    }

    QQuickItemPrivate::transformChanged();
}

void QQuickFlipable::updatePolish()
{
    Q_D(QQuickFlipable);
    d->updateSide();
}

// determination on the currently visible side of the flipable
// has to be done on the complete scene transform to give
// correct results.
void QQuickFlipablePrivate::updateSide()
{
    Q_Q(QQuickFlipable);

    if (!sideDirty)
        return;

    sideDirty = false;

    QTransform sceneTransform;
    itemToParentTransform(sceneTransform);

    QPointF p1(0, 0);
    QPointF p2(1, 0);
    QPointF p3(1, 1);

    QPointF scenep1 = sceneTransform.map(p1);
    QPointF scenep2 = sceneTransform.map(p2);
    QPointF scenep3 = sceneTransform.map(p3);
#if 0
    p1 = q->mapToParent(p1);
    p2 = q->mapToParent(p2);
    p3 = q->mapToParent(p3);
#endif

    qreal cross = (scenep1.x() - scenep2.x()) * (scenep3.y() - scenep2.y()) -
                  (scenep1.y() - scenep2.y()) * (scenep3.x() - scenep2.x());

    wantBackYFlipped = scenep1.x() >= scenep2.x();
    wantBackXFlipped = scenep2.y() >= scenep3.y();

    QQuickFlipable::Side newSide;
    if (cross > 0) {
        newSide = QQuickFlipable::Back;
    } else {
        newSide = QQuickFlipable::Front;
    }

    if (newSide != current) {
        current = newSide;
        if (current == QQuickFlipable::Back && back)
            setBackTransform();
        if (front) {
            front->setOpacity((current==QQuickFlipable::Front)?1.:0.);
            front->setEnabled((current==QQuickFlipable::Front)?true:false);
        }
        if (back) {
            back->setOpacity((current==QQuickFlipable::Back)?1.:0.);
            back->setEnabled((current==QQuickFlipable::Back)?true:false);
        }
        emit q->sideChanged();
    }
}

/* Depends on the width/height of the back item, and so needs reevaulating
   if those change.
*/
void QQuickFlipablePrivate::setBackTransform()
{
    QTransform mat;
    mat.translate(back->width()/2,back->height()/2);
    if (back->width() && wantBackYFlipped)
        mat.rotate(180, Qt::YAxis);
    if (back->height() && wantBackXFlipped)
        mat.rotate(180, Qt::XAxis);
    mat.translate(-back->width()/2,-back->height()/2);

    if (backTransform)
        backTransform->setTransform(mat);
}

QT_END_NAMESPACE

#include "qquickflipable.moc"
