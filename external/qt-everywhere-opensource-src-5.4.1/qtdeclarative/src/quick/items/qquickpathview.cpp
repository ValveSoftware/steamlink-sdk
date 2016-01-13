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

#include "qquickpathview_p.h"
#include "qquickpathview_p_p.h"
#include "qquickwindow.h"
#include "qquickflickablebehavior_p.h" //Contains flicking behavior defines

#include <QtQuick/private/qquickstate_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmlopenmetaobject_p.h>
#include <private/qqmlchangeset_p.h>

#include <QtQml/qqmlinfo.h>
#include <QtGui/qevent.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qstylehints.h>
#include <QtCore/qmath.h>
#include <math.h>


QT_BEGIN_NAMESPACE

const qreal MinimumFlickVelocity = 75.0;

inline qreal qmlMod(qreal x, qreal y)
{
#ifdef QT_USE_MATH_H_FLOATS
    if (sizeof(qreal) == sizeof(float))
        return fmodf(float(x), float(y));
    else
#endif
        return fmod(x, y);
}

static QQmlOpenMetaObjectType *qPathViewAttachedType = 0;

QQuickPathViewAttached::QQuickPathViewAttached(QObject *parent)
: QObject(parent), m_percent(-1), m_view(0), m_onPath(false), m_isCurrent(false)
{
    if (qPathViewAttachedType) {
        m_metaobject = new QQmlOpenMetaObject(this, qPathViewAttachedType);
        m_metaobject->setCached(true);
    } else {
        m_metaobject = new QQmlOpenMetaObject(this);
    }
}

QQuickPathViewAttached::~QQuickPathViewAttached()
{
}

QVariant QQuickPathViewAttached::value(const QByteArray &name) const
{
    return m_metaobject->value(name);
}
void QQuickPathViewAttached::setValue(const QByteArray &name, const QVariant &val)
{
    m_metaobject->setValue(name, val);
}

QQuickPathViewPrivate::QQuickPathViewPrivate()
  : path(0), currentIndex(0), currentItemOffset(0.0), startPc(0)
    , offset(0.0), offsetAdj(0.0), mappedRange(1.0), mappedCache(0.0)
    , stealMouse(false), ownModel(false), interactive(true), haveHighlightRange(true)
    , autoHighlight(true), highlightUp(false), layoutScheduled(false)
    , moving(false), flicking(false), dragging(false), inRequest(false), delegateValidated(false)
    , inRefill(false)
    , dragMargin(0), deceleration(100), maximumFlickVelocity(QML_FLICK_DEFAULTMAXVELOCITY)
    , moveOffset(this, &QQuickPathViewPrivate::setAdjustedOffset), flickDuration(0)
    , firstIndex(-1), pathItems(-1), requestedIndex(-1), cacheSize(0), requestedZ(0)
    , moveReason(Other), moveDirection(Shortest), attType(0), highlightComponent(0), highlightItem(0)
    , moveHighlight(this, &QQuickPathViewPrivate::setHighlightPosition)
    , highlightPosition(0)
    , highlightRangeStart(0), highlightRangeEnd(0)
    , highlightRangeMode(QQuickPathView::StrictlyEnforceRange)
    , highlightMoveDuration(300), modelCount(0), snapMode(QQuickPathView::NoSnap)
{
}

void QQuickPathViewPrivate::init()
{
    Q_Q(QQuickPathView);
    offset = 0;
    q->setAcceptedMouseButtons(Qt::LeftButton);
    q->setFlag(QQuickItem::ItemIsFocusScope);
    q->setFiltersChildMouseEvents(true);
    qmlobject_connect(&tl, QQuickTimeLine, SIGNAL(updated()),
                      q, QQuickPathView, SLOT(ticked()))
    timer.invalidate();
    qmlobject_connect(&tl, QQuickTimeLine, SIGNAL(completed()),
                      q, QQuickPathView, SLOT(movementEnding()))
}

QQuickItem *QQuickPathViewPrivate::getItem(int modelIndex, qreal z, bool async)
{
    Q_Q(QQuickPathView);
    requestedIndex = modelIndex;
    requestedZ = z;
    inRequest = true;
    QObject *object = model->object(modelIndex, async);
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (!item) {
        if (object) {
            model->release(object);
            if (!delegateValidated) {
                delegateValidated = true;
                QObject* delegate = q->delegate();
                qmlInfo(delegate ? delegate : q) << QQuickPathView::tr("Delegate must be of Item type");
            }
        }
    } else {
        item->setParentItem(q);
        requestedIndex = -1;
        QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
        itemPrivate->addItemChangeListener(this, QQuickItemPrivate::Geometry);
    }
    inRequest = false;
    return item;
}

void QQuickPathView::createdItem(int index, QObject *object)
{
    Q_D(QQuickPathView);
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (d->requestedIndex != index) {
        qPathViewAttachedType = d->attachedType();
        QQuickPathViewAttached *att = static_cast<QQuickPathViewAttached *>(qmlAttachedPropertiesObject<QQuickPathView>(item));
        qPathViewAttachedType = 0;
        if (att) {
            att->m_view = this;
            att->setOnPath(false);
        }
        item->setParentItem(this);
        d->updateItem(item, 1.0);
    } else {
        d->requestedIndex = -1;
        if (!d->inRequest)
            refill();
    }
}

void QQuickPathView::initItem(int index, QObject *object)
{
    Q_D(QQuickPathView);
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (item && d->requestedIndex == index) {
        QQuickItemPrivate::get(item)->setCulled(true);
        item->setParentItem(this);
        qPathViewAttachedType = d->attachedType();
        QQuickPathViewAttached *att = static_cast<QQuickPathViewAttached *>(qmlAttachedPropertiesObject<QQuickPathView>(item));
        qPathViewAttachedType = 0;
        if (att) {
            att->m_view = this;
            qreal percent = d->positionOfIndex(index);
            if (percent < 1.0 && d->path) {
                foreach (const QString &attr, d->path->attributes())
                    att->setValue(attr.toUtf8(), d->path->attributeAt(attr, percent));
                item->setZ(d->requestedZ);
            }
            att->setOnPath(percent < 1.0);
        }
    }
}

void QQuickPathViewPrivate::releaseItem(QQuickItem *item)
{
    if (!item || !model)
        return;
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
    itemPrivate->removeItemChangeListener(this, QQuickItemPrivate::Geometry);
    QQmlInstanceModel::ReleaseFlags flags = model->release(item);
    if (!flags) {
        // item was not destroyed, and we no longer reference it.
        if (QQuickPathViewAttached *att = attached(item))
            att->setOnPath(false);
    } else if (flags & QQmlInstanceModel::Destroyed) {
        // but we still reference it
        item->setParentItem(0);
    }
}

QQuickPathViewAttached *QQuickPathViewPrivate::attached(QQuickItem *item)
{
    return static_cast<QQuickPathViewAttached *>(qmlAttachedPropertiesObject<QQuickPathView>(item, false));
}

QQmlOpenMetaObjectType *QQuickPathViewPrivate::attachedType()
{
    Q_Q(QQuickPathView);
    if (!attType) {
        // pre-create one metatype to share with all attached objects
        attType = new QQmlOpenMetaObjectType(&QQuickPathViewAttached::staticMetaObject, qmlEngine(q));
        if (path) {
            foreach (const QString &attr, path->attributes())
                attType->createProperty(attr.toUtf8());
        }
    }

    return attType;
}

void QQuickPathViewPrivate::clear()
{
    if (currentItem) {
        releaseItem(currentItem);
        currentItem = 0;
    }
    for (int i=0; i<items.count(); i++){
        QQuickItem *p = items[i];
        releaseItem(p);
    }
    if (requestedIndex >= 0) {
        if (model)
            model->cancel(requestedIndex);
        requestedIndex = -1;
    }

    items.clear();
    tl.clear();
}

void QQuickPathViewPrivate::updateMappedRange()
{
    if (model && pathItems != -1 && pathItems < modelCount) {
        mappedRange = qreal(modelCount)/pathItems;
        mappedCache = qreal(cacheSize)/pathItems/2; // Half of cache at each end
    } else {
        mappedRange = 1.0;
        mappedCache = 0.0;
    }
}

qreal QQuickPathViewPrivate::positionOfIndex(qreal index) const
{
    qreal pos = -1.0;

    if (model && index >= 0 && index < modelCount) {
        qreal start = 0.0;
        if (haveHighlightRange && (highlightRangeMode != QQuickPathView::NoHighlightRange
                                   || snapMode != QQuickPathView::NoSnap))
            start = highlightRangeStart;
        qreal globalPos = index + offset;
        globalPos = qmlMod(globalPos, qreal(modelCount)) / modelCount;
        if (pathItems != -1 && pathItems < modelCount) {
            globalPos += start / mappedRange;
            globalPos = qmlMod(globalPos, 1.0);
            pos = globalPos * mappedRange;
        } else {
            pos = qmlMod(globalPos + start, 1.0);
        }
    }

    return pos;
}

// returns true if position is between lower and upper, taking into
// account the circular space.
bool QQuickPathViewPrivate::isInBound(qreal position, qreal lower, qreal upper) const
{
    if (lower > upper) {
        if (position > upper && position > lower)
            position -= mappedRange;
        lower -= mappedRange;
    }
    return position >= lower && position < upper;
}

void QQuickPathViewPrivate::createHighlight()
{
    Q_Q(QQuickPathView);
    if (!q->isComponentComplete())
        return;

    bool changed = false;
    if (highlightItem) {
        highlightItem->setParentItem(0);
        highlightItem->deleteLater();
        highlightItem = 0;
        changed = true;
    }

    QQuickItem *item = 0;
    if (highlightComponent) {
        QQmlContext *creationContext = highlightComponent->creationContext();
        QQmlContext *highlightContext = new QQmlContext(
                creationContext ? creationContext : qmlContext(q));
        QObject *nobj = highlightComponent->create(highlightContext);
        if (nobj) {
            QQml_setParent_noEvent(highlightContext, nobj);
            item = qobject_cast<QQuickItem *>(nobj);
            if (!item)
                delete nobj;
        } else {
            delete highlightContext;
        }
    } else {
        item = new QQuickItem;
    }
    if (item) {
        QQml_setParent_noEvent(item, q);
        item->setParentItem(q);
        highlightItem = item;
        changed = true;
    }
    if (changed)
        emit q->highlightItemChanged();
}

void QQuickPathViewPrivate::updateHighlight()
{
    Q_Q(QQuickPathView);
    if (!q->isComponentComplete() || !isValid())
        return;
    if (highlightItem) {
        if (haveHighlightRange && highlightRangeMode == QQuickPathView::StrictlyEnforceRange) {
            updateItem(highlightItem, highlightRangeStart);
        } else {
            qreal target = currentIndex;

            offsetAdj = 0.0;
            tl.reset(moveHighlight);
            moveHighlight.setValue(highlightPosition);

            const int duration = highlightMoveDuration;

            if (target - highlightPosition > modelCount/2) {
                highlightUp = false;
                qreal distance = modelCount - target + highlightPosition;
                tl.move(moveHighlight, 0.0, QEasingCurve(QEasingCurve::InQuad), int(duration * highlightPosition / distance));
                tl.set(moveHighlight, modelCount-0.01);
                tl.move(moveHighlight, target, QEasingCurve(QEasingCurve::OutQuad), int(duration * (modelCount-target) / distance));
            } else if (target - highlightPosition <= -modelCount/2) {
                highlightUp = true;
                qreal distance = modelCount - highlightPosition + target;
                tl.move(moveHighlight, modelCount-0.01, QEasingCurve(QEasingCurve::InQuad), int(duration * (modelCount-highlightPosition) / distance));
                tl.set(moveHighlight, 0.0);
                tl.move(moveHighlight, target, QEasingCurve(QEasingCurve::OutQuad), int(duration * target / distance));
            } else {
                highlightUp = highlightPosition - target < 0;
                tl.move(moveHighlight, target, QEasingCurve(QEasingCurve::InOutQuad), duration);
            }
        }
    }
}

void QQuickPathViewPrivate::setHighlightPosition(qreal pos)
{
    if (pos != highlightPosition) {
        qreal start = 0.0;
        qreal end = 1.0;
        if (haveHighlightRange && highlightRangeMode != QQuickPathView::NoHighlightRange) {
            start = highlightRangeStart;
            end = highlightRangeEnd;
        }

        qreal range = qreal(modelCount);
        // calc normalized position of highlight relative to offset
        qreal relativeHighlight = qmlMod(pos + offset, range) / range;

        if (!highlightUp && relativeHighlight > end / mappedRange) {
            qreal diff = 1.0 - relativeHighlight;
            setOffset(offset + diff * range);
        } else if (highlightUp && relativeHighlight >= (end - start) / mappedRange) {
            qreal diff = relativeHighlight - (end - start) / mappedRange;
            setOffset(offset - diff * range - 0.00001);
        }

        highlightPosition = pos;
        qreal pathPos = positionOfIndex(pos);
        updateItem(highlightItem, pathPos);
        if (QQuickPathViewAttached *att = attached(highlightItem))
            att->setOnPath(pathPos < 1.0);
    }
}

void QQuickPathView::pathUpdated()
{
    Q_D(QQuickPathView);
    QList<QQuickItem*>::iterator it = d->items.begin();
    while (it != d->items.end()) {
        QQuickItem *item = *it;
        if (QQuickPathViewAttached *att = d->attached(item))
            att->m_percent = -1;
        ++it;
    }
    refill();
}

void QQuickPathViewPrivate::updateItem(QQuickItem *item, qreal percent)
{
    if (!path)
        return;
    if (QQuickPathViewAttached *att = attached(item)) {
        if (qFuzzyCompare(att->m_percent, percent))
            return;
        att->m_percent = percent;
        foreach (const QString &attr, path->attributes())
            att->setValue(attr.toUtf8(), path->attributeAt(attr, percent));
        att->setOnPath(percent < 1.0);
    }
    QQuickItemPrivate::get(item)->setCulled(percent >= 1.0);
    QPointF pf = path->pointAt(qMin(percent, qreal(1.0)));
    item->setX(pf.x() - item->width()/2);
    item->setY(pf.y() - item->height()/2);
}

void QQuickPathViewPrivate::regenerate()
{
    Q_Q(QQuickPathView);
    if (!q->isComponentComplete())
        return;

    clear();

    if (!isValid())
        return;

    firstIndex = -1;
    updateMappedRange();
    q->refill();
}

void QQuickPathViewPrivate::setDragging(bool d)
{
    Q_Q(QQuickPathView);
    if (dragging == d)
        return;

    dragging = d;
    if (dragging)
        emit q->dragStarted();
    else
        emit q->dragEnded();

    emit q->draggingChanged();
}

/*!
    \qmltype PathView
    \instantiates QQuickPathView
    \inqmlmodule QtQuick
    \ingroup qtquick-paths
    \ingroup qtquick-views
    \inherits Item
    \brief Lays out model-provided items on a path

    A PathView displays data from models created from built-in QML types like ListModel
    and XmlListModel, or custom model classes defined in C++ that inherit from
    QAbstractListModel.

    The view has a \l model, which defines the data to be displayed, and
    a \l delegate, which defines how the data should be displayed.
    The \l delegate is instantiated for each item on the \l path.
    The items may be flicked to move them along the path.

    For example, if there is a simple list model defined in a file \c ContactModel.qml like this:

    \snippet qml/pathview/ContactModel.qml 0

    This data can be represented as a PathView, like this:

    \snippet qml/pathview/pathview.qml 0

    \image pathview.gif

    (Note the above example uses PathAttribute to scale and modify the
    opacity of the items as they rotate. This additional code can be seen in the
    PathAttribute documentation.)

    PathView does not automatically handle keyboard navigation.  This is because
    the keys to use for navigation will depend upon the shape of the path.  Navigation
    can be added quite simply by setting \c focus to \c true and calling
    \l decrementCurrentIndex() or \l incrementCurrentIndex(), for example to navigate
    using the left and right arrow keys:

    \qml
    PathView {
        // ...
        focus: true
        Keys.onLeftPressed: decrementCurrentIndex()
        Keys.onRightPressed: incrementCurrentIndex()
    }
    \endqml

    The path view itself is a focus scope (see \l{Keyboard Focus in Qt Quick} for more details).

    Delegates are instantiated as needed and may be destroyed at any time.
    State should \e never be stored in a delegate.

    PathView attaches a number of properties to the root item of the delegate, for example
    \c {PathView.isCurrentItem}.  In the following example, the root delegate item can access
    this attached property directly as \c PathView.isCurrentItem, while the child
    \c nameText object must refer to this property as \c wrapper.PathView.isCurrentItem.

    \snippet qml/pathview/pathview.qml 1

    \b Note that views do not enable \e clip automatically.  If the view
    is not clipped by another item or the screen, it will be necessary
    to set \e {clip: true} in order to have the out of view items clipped
    nicely.

    \sa Path, {QML Data Models}, ListView, GridView, {Qt Quick Examples - Views}
*/

QQuickPathView::QQuickPathView(QQuickItem *parent)
  : QQuickItem(*(new QQuickPathViewPrivate), parent)
{
    Q_D(QQuickPathView);
    d->init();
}

QQuickPathView::~QQuickPathView()
{
    Q_D(QQuickPathView);
    d->clear();
    if (d->attType)
        d->attType->release();
    if (d->ownModel)
        delete d->model;
}

/*!
    \qmlattachedproperty PathView QtQuick::PathView::view
    This attached property holds the view that manages this delegate instance.

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty bool QtQuick::PathView::onPath
    This attached property holds whether the item is currently on the path.

    If a pathItemCount has been set, it is possible that some items may
    be instantiated, but not considered to be currently on the path.
    Usually, these items would be set invisible, for example:

    \qml
    Component {
        Rectangle {
            visible: PathView.onPath
            // ...
        }
    }
    \endqml

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty bool QtQuick::PathView::isCurrentItem
    This attached property is true if this delegate is the current item; otherwise false.

    It is attached to each instance of the delegate.

    This property may be used to adjust the appearance of the current item.

    \snippet qml/pathview/pathview.qml 1
*/

/*!
    \qmlproperty model QtQuick::PathView::model
    This property holds the model providing data for the view.

    The model provides a set of data that is used to create the items for the view.
    For large or dynamic datasets the model is usually provided by a C++ model object.
    Models can also be created directly in QML, using the ListModel type.

    \note changing the model will reset the offset and currentIndex to 0.

    \sa {qml-data-models}{Data Models}
*/
QVariant QQuickPathView::model() const
{
    Q_D(const QQuickPathView);
    return d->modelVariant;
}

void QQuickPathView::setModel(const QVariant &m)
{
    Q_D(QQuickPathView);
    QVariant model = m;
    if (model.userType() == qMetaTypeId<QJSValue>())
        model = model.value<QJSValue>().toVariant();

    if (d->modelVariant == model)
        return;

    if (d->model) {
        qmlobject_disconnect(d->model, QQmlInstanceModel, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                             this, QQuickPathView, SLOT(modelUpdated(QQmlChangeSet,bool)));
        qmlobject_disconnect(d->model, QQmlInstanceModel, SIGNAL(createdItem(int,QObject*)),
                             this, QQuickPathView, SLOT(createdItem(int,QObject*)));
        qmlobject_disconnect(d->model, QQmlInstanceModel, SIGNAL(initItem(int,QObject*)),
                             this, QQuickPathView, SLOT(initItem(int,QObject*)));
        d->clear();
    }

    d->modelVariant = model;
    QObject *object = qvariant_cast<QObject*>(model);
    QQmlInstanceModel *vim = 0;
    if (object && (vim = qobject_cast<QQmlInstanceModel *>(object))) {
        if (d->ownModel) {
            delete d->model;
            d->ownModel = false;
        }
        d->model = vim;
    } else {
        if (!d->ownModel) {
            d->model = new QQmlDelegateModel(qmlContext(this));
            d->ownModel = true;
            if (isComponentComplete())
                static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
        }
        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model))
            dataModel->setModel(model);
    }
    int oldModelCount = d->modelCount;
    d->modelCount = 0;
    if (d->model) {
        qmlobject_connect(d->model, QQmlInstanceModel, SIGNAL(modelUpdated(QQmlChangeSet,bool)),
                          this, QQuickPathView, SLOT(modelUpdated(QQmlChangeSet,bool)));
        qmlobject_connect(d->model, QQmlInstanceModel, SIGNAL(createdItem(int,QObject*)),
                          this, QQuickPathView, SLOT(createdItem(int,QObject*)));
        qmlobject_connect(d->model, QQmlInstanceModel, SIGNAL(initItem(int,QObject*)),
                          this, QQuickPathView, SLOT(initItem(int,QObject*)));
        d->modelCount = d->model->count();
    }
    if (isComponentComplete()) {
        if (d->currentIndex != 0) {
            d->currentIndex = 0;
            emit currentIndexChanged();
        }
        if (d->offset != 0.0) {
            d->offset = 0;
            emit offsetChanged();
        }
    }
    d->regenerate();
    if (d->modelCount != oldModelCount)
        emit countChanged();
    emit modelChanged();
}

/*!
    \qmlproperty int QtQuick::PathView::count
    This property holds the number of items in the model.
*/
int QQuickPathView::count() const
{
    Q_D(const QQuickPathView);
    return d->model ? d->modelCount : 0;
}

/*!
    \qmlproperty Path QtQuick::PathView::path
    This property holds the path used to lay out the items.
    For more information see the \l Path documentation.
*/
QQuickPath *QQuickPathView::path() const
{
    Q_D(const QQuickPathView);
    return d->path;
}

void QQuickPathView::setPath(QQuickPath *path)
{
    Q_D(QQuickPathView);
    if (d->path == path)
        return;
    if (d->path)
        qmlobject_disconnect(d->path, QQuickPath, SIGNAL(changed()),
                             this, QQuickPathView, SLOT(pathUpdated()));
    d->path = path;
    qmlobject_connect(d->path, QQuickPath, SIGNAL(changed()),
                      this, QQuickPathView, SLOT(pathUpdated()));
    if (d->isValid() && isComponentComplete()) {
        d->clear();
        if (d->attType) {
            d->attType->release();
            d->attType = 0;
        }
        d->regenerate();
    }
    emit pathChanged();
}

/*!
    \qmlproperty int QtQuick::PathView::currentIndex
    This property holds the index of the current item.
*/
int QQuickPathView::currentIndex() const
{
    Q_D(const QQuickPathView);
    return d->currentIndex;
}

void QQuickPathView::setCurrentIndex(int idx)
{
    Q_D(QQuickPathView);
    if (!isComponentComplete()) {
        if (idx != d->currentIndex) {
            d->currentIndex = idx;
            emit currentIndexChanged();
        }
        return;
    }

    idx = d->modelCount
        ? ((idx % d->modelCount) + d->modelCount) % d->modelCount
        : 0;
    if (d->model && (idx != d->currentIndex || !d->currentItem)) {
        if (d->currentItem) {
            if (QQuickPathViewAttached *att = d->attached(d->currentItem))
                att->setIsCurrentItem(false);
            d->releaseItem(d->currentItem);
        }
        int oldCurrentIdx = d->currentIndex;
        QQuickItem *oldCurrentItem = d->currentItem;
        d->currentItem = 0;
        d->moveReason = QQuickPathViewPrivate::SetIndex;
        d->currentIndex = idx;
        if (d->modelCount) {
            d->createCurrentItem();
            if (d->haveHighlightRange && d->highlightRangeMode == QQuickPathView::StrictlyEnforceRange)
                d->snapToIndex(d->currentIndex);
            d->currentItemOffset = d->positionOfIndex(d->currentIndex);
            d->updateHighlight();
        }
        if (oldCurrentIdx != d->currentIndex)
            emit currentIndexChanged();
        if (oldCurrentItem != d->currentItem)
            emit currentItemChanged();
    }
}

/*!
    \qmlproperty Item QtQuick::PathView::currentItem
    This property holds the current item in the view.
*/
QQuickItem *QQuickPathView::currentItem() const
{
    Q_D(const QQuickPathView);
    return d->currentItem;
}

/*!
    \qmlmethod QtQuick::PathView::incrementCurrentIndex()

    Increments the current index.

    \b Note: methods should only be called after the Component has completed.
*/
void QQuickPathView::incrementCurrentIndex()
{
    Q_D(QQuickPathView);
    d->moveDirection = QQuickPathViewPrivate::Positive;
    setCurrentIndex(currentIndex()+1);
}

/*!
    \qmlmethod QtQuick::PathView::decrementCurrentIndex()

    Decrements the current index.

    \b Note: methods should only be called after the Component has completed.
*/
void QQuickPathView::decrementCurrentIndex()
{
    Q_D(QQuickPathView);
    d->moveDirection = QQuickPathViewPrivate::Negative;
    setCurrentIndex(currentIndex()-1);
}

/*!
    \qmlproperty real QtQuick::PathView::offset

    The offset specifies how far along the path the items are from their initial positions.
    This is a real number that ranges from 0.0 to the count of items in the model.
*/
qreal QQuickPathView::offset() const
{
    Q_D(const QQuickPathView);
    return d->offset;
}

void QQuickPathView::setOffset(qreal offset)
{
    Q_D(QQuickPathView);
    d->moveReason = QQuickPathViewPrivate::Other;
    d->setOffset(offset);
    d->updateCurrent();
}

void QQuickPathViewPrivate::setOffset(qreal o)
{
    Q_Q(QQuickPathView);
    if (offset != o) {
        if (isValid() && q->isComponentComplete()) {
            offset = qmlMod(o, qreal(modelCount));
            if (offset < 0)
                offset += qreal(modelCount);
            q->refill();
        } else {
            offset = o;
        }
        emit q->offsetChanged();
    }
}

void QQuickPathViewPrivate::setAdjustedOffset(qreal o)
{
    setOffset(o+offsetAdj);
}

/*!
    \qmlproperty Component QtQuick::PathView::highlight
    This property holds the component to use as the highlight.

    An instance of the highlight component will be created for each view.
    The geometry of the resultant component instance will be managed by the view
    so as to stay with the current item.

    The below example demonstrates how to make a simple highlight.  Note the use
    of the \l{PathView::onPath}{PathView.onPath} attached property to ensure that
    the highlight is hidden when flicked away from the path.

    \qml
    Component {
        Rectangle {
            visible: PathView.onPath
            // ...
        }
    }
    \endqml

    \sa highlightItem, highlightRangeMode
*/

QQmlComponent *QQuickPathView::highlight() const
{
    Q_D(const QQuickPathView);
    return d->highlightComponent;
}

void QQuickPathView::setHighlight(QQmlComponent *highlight)
{
    Q_D(QQuickPathView);
    if (highlight != d->highlightComponent) {
        d->highlightComponent = highlight;
        d->createHighlight();
        d->updateHighlight();
        emit highlightChanged();
    }
}

/*!
  \qmlproperty Item QtQuick::PathView::highlightItem

  \c highlightItem holds the highlight item, which was created
  from the \l highlight component.

  \sa highlight
*/
QQuickItem *QQuickPathView::highlightItem()
{
    Q_D(const QQuickPathView);
    return d->highlightItem;
}
/*!
    \qmlproperty real QtQuick::PathView::preferredHighlightBegin
    \qmlproperty real QtQuick::PathView::preferredHighlightEnd
    \qmlproperty enumeration QtQuick::PathView::highlightRangeMode

    These properties set the preferred range of the highlight (current item)
    within the view. The preferred values must be in the range 0.0-1.0.

    Valid values for \c highlightRangeMode are:

    \list
        \li \e PathView.NoHighlightRange - no range is applied and the
            highlight will move freely within the view.
        \li \e PathView.ApplyRange - the view will attempt to maintain
            the highlight within the range, however the highlight can
            move outside of the range at the ends of the path or due to
            a mouse interaction.
        \li \e PathView.StrictlyEnforceRange - the highlight will never
            move outside of the range. This means that the current item
            will change if a keyboard or mouse action would cause the
            highlight to move outside of the range.
    \endlist

    The default value is \e PathView.StrictlyEnforceRange.

    Defining a highlight range is the correct way to influence where the
    current item ends up when the view moves. For example, if you want the
    currently selected item to be in the middle of the path, then set the
    highlight range to be 0.5,0.5 and highlightRangeMode to \e PathView.StrictlyEnforceRange.
    Then, when the path scrolls,
    the currently selected item will be the item at that position. This also applies to
    when the currently selected item changes - it will scroll to within the preferred
    highlight range. Furthermore, the behaviour of the current item index will occur
    whether or not a highlight exists.

    \note A valid range requires \c preferredHighlightEnd to be greater
    than or equal to \c preferredHighlightBegin.
*/
qreal QQuickPathView::preferredHighlightBegin() const
{
    Q_D(const QQuickPathView);
    return d->highlightRangeStart;
}

void QQuickPathView::setPreferredHighlightBegin(qreal start)
{
    Q_D(QQuickPathView);
    if (d->highlightRangeStart == start || start < 0 || start > 1.0)
        return;
    d->highlightRangeStart = start;
    d->haveHighlightRange = d->highlightRangeStart <= d->highlightRangeEnd;
    refill();
    emit preferredHighlightBeginChanged();
}

qreal QQuickPathView::preferredHighlightEnd() const
{
    Q_D(const QQuickPathView);
    return d->highlightRangeEnd;
}

void QQuickPathView::setPreferredHighlightEnd(qreal end)
{
    Q_D(QQuickPathView);
    if (d->highlightRangeEnd == end || end < 0 || end > 1.0)
        return;
    d->highlightRangeEnd = end;
    d->haveHighlightRange = d->highlightRangeStart <= d->highlightRangeEnd;
    refill();
    emit preferredHighlightEndChanged();
}

QQuickPathView::HighlightRangeMode QQuickPathView::highlightRangeMode() const
{
    Q_D(const QQuickPathView);
    return d->highlightRangeMode;
}

void QQuickPathView::setHighlightRangeMode(HighlightRangeMode mode)
{
    Q_D(QQuickPathView);
    if (d->highlightRangeMode == mode)
        return;
    d->highlightRangeMode = mode;
    d->haveHighlightRange = d->highlightRangeStart <= d->highlightRangeEnd;
    if (d->haveHighlightRange) {
        d->regenerate();
        int index = d->highlightRangeMode != NoHighlightRange ? d->currentIndex : d->calcCurrentIndex();
        if (index >= 0)
            d->snapToIndex(index);
    }
    emit highlightRangeModeChanged();
}

/*!
    \qmlproperty int QtQuick::PathView::highlightMoveDuration
    This property holds the move animation duration of the highlight delegate.

    If the highlightRangeMode is StrictlyEnforceRange then this property
    determines the speed that the items move along the path.

    The default value for the duration is 300ms.
*/
int QQuickPathView::highlightMoveDuration() const
{
    Q_D(const QQuickPathView);
    return d->highlightMoveDuration;
}

void QQuickPathView::setHighlightMoveDuration(int duration)
{
    Q_D(QQuickPathView);
    if (d->highlightMoveDuration == duration)
        return;
    d->highlightMoveDuration = duration;
    emit highlightMoveDurationChanged();
}

/*!
    \qmlproperty real QtQuick::PathView::dragMargin
    This property holds the maximum distance from the path that initiate mouse dragging.

    By default the path can only be dragged by clicking on an item.  If
    dragMargin is greater than zero, a drag can be initiated by clicking
    within dragMargin pixels of the path.
*/
qreal QQuickPathView::dragMargin() const
{
    Q_D(const QQuickPathView);
    return d->dragMargin;
}

void QQuickPathView::setDragMargin(qreal dragMargin)
{
    Q_D(QQuickPathView);
    if (d->dragMargin == dragMargin)
        return;
    d->dragMargin = dragMargin;
    emit dragMarginChanged();
}

/*!
    \qmlproperty real QtQuick::PathView::flickDeceleration
    This property holds the rate at which a flick will decelerate.

    The default is 100.
*/
qreal QQuickPathView::flickDeceleration() const
{
    Q_D(const QQuickPathView);
    return d->deceleration;
}

void QQuickPathView::setFlickDeceleration(qreal dec)
{
    Q_D(QQuickPathView);
    if (d->deceleration == dec)
        return;
    d->deceleration = dec;
    emit flickDecelerationChanged();
}

/*!
    \qmlproperty real QtQuick::PathView::maximumFlickVelocity
    This property holds the approximate maximum velocity that the user can flick the view in pixels/second.

    The default value is platform dependent.
*/
qreal QQuickPathView::maximumFlickVelocity() const
{
    Q_D(const QQuickPathView);
    return d->maximumFlickVelocity;
}

void QQuickPathView::setMaximumFlickVelocity(qreal vel)
{
    Q_D(QQuickPathView);
    if (vel == d->maximumFlickVelocity)
        return;
    d->maximumFlickVelocity = vel;
    emit maximumFlickVelocityChanged();
}


/*!
    \qmlproperty bool QtQuick::PathView::interactive

    A user cannot drag or flick a PathView that is not interactive.

    This property is useful for temporarily disabling flicking. This allows
    special interaction with PathView's children.
*/
bool QQuickPathView::isInteractive() const
{
    Q_D(const QQuickPathView);
    return d->interactive;
}

void QQuickPathView::setInteractive(bool interactive)
{
    Q_D(QQuickPathView);
    if (interactive != d->interactive) {
        d->interactive = interactive;
        if (!interactive)
            d->tl.clear();
        emit interactiveChanged();
    }
}

/*!
    \qmlproperty bool QtQuick::PathView::moving

    This property holds whether the view is currently moving
    due to the user either dragging or flicking the view.
*/
bool QQuickPathView::isMoving() const
{
    Q_D(const QQuickPathView);
    return d->moving;
}

/*!
    \qmlproperty bool QtQuick::PathView::flicking

    This property holds whether the view is currently moving
    due to the user flicking the view.
*/
bool QQuickPathView::isFlicking() const
{
    Q_D(const QQuickPathView);
    return d->flicking;
}

/*!
    \qmlproperty bool QtQuick::PathView::dragging

    This property holds whether the view is currently moving
    due to the user dragging the view.
*/
bool QQuickPathView::isDragging() const
{
    Q_D(const QQuickPathView);
    return d->dragging;
}

/*!
    \qmlsignal QtQuick::PathView::movementStarted()

    This signal is emitted when the view begins moving due to user
    interaction.

    The corresponding handler is \c onMovementStarted.
*/

/*!
    \qmlsignal QtQuick::PathView::movementEnded()

    This signal is emitted when the view stops moving due to user
    interaction.  If a flick was generated, this signal will
    be emitted once the flick stops.  If a flick was not
    generated, this signal will be emitted when the
    user stops dragging - i.e. a mouse or touch release.

    The corresponding handler is \c onMovementEnded.
*/

/*!
    \qmlsignal QtQuick::PathView::flickStarted()

    This signal is emitted when the view is flicked.  A flick
    starts from the point that the mouse or touch is released,
    while still in motion.

    The corresponding handler is \c onFlickStarted.
*/

/*!
    \qmlsignal QtQuick::PathView::flickEnded()

    This signal is emitted when the view stops moving due to a flick.

    The corresponding handler is \c onFlickEnded.
*/

/*!
    \qmlsignal QtQuick::PathView::dragStarted()

    This signal is emitted when the view starts to be dragged due to user
    interaction.

    The corresponding handler is \c onDragStarted.
*/

/*!
    \qmlsignal QtQuick::PathView::dragEnded()

    This signal is emitted when the user stops dragging the view.

    If the velocity of the drag is suffient at the time the
    touch/mouse button is released then a flick will start.

    The corresponding handler is \c onDragEnded.
*/

/*!
    \qmlproperty Component QtQuick::PathView::delegate

    The delegate provides a template defining each item instantiated by the view.
    The index is exposed as an accessible \c index property.  Properties of the
    model are also available depending upon the type of \l {qml-data-models}{Data Model}.

    The number of objects and bindings in the delegate has a direct effect on the
    flicking performance of the view when pathItemCount is specified.  If at all possible, place functionality
    that is not needed for the normal display of the delegate in a \l Loader which
    can load additional components when needed.

    Note that the PathView will layout the items based on the size of the root
    item in the delegate.

    Here is an example delegate:
    \snippet qml/pathview/pathview.qml 1
*/
QQmlComponent *QQuickPathView::delegate() const
{
    Q_D(const QQuickPathView);
     if (d->model) {
        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model))
            return dataModel->delegate();
    }

    return 0;
}

void QQuickPathView::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQuickPathView);
    if (delegate == this->delegate())
        return;
    if (!d->ownModel) {
        d->model = new QQmlDelegateModel(qmlContext(this));
        d->ownModel = true;
        if (isComponentComplete())
            static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
    }
    if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model)) {
        int oldCount = dataModel->count();
        dataModel->setDelegate(delegate);
        d->modelCount = dataModel->count();
        d->regenerate();
        if (oldCount != dataModel->count())
            emit countChanged();
        emit delegateChanged();
        d->delegateValidated = false;
    }
}

/*!
  \qmlproperty int QtQuick::PathView::pathItemCount
  This property holds the number of items visible on the path at any one time.

  Setting pathItemCount to undefined will show all items on the path.
*/
int QQuickPathView::pathItemCount() const
{
    Q_D(const QQuickPathView);
    return d->pathItems;
}

void QQuickPathView::setPathItemCount(int i)
{
    Q_D(QQuickPathView);
    if (i == d->pathItems)
        return;
    if (i < 1)
        i = 1;
    d->pathItems = i;
    d->updateMappedRange();
    if (d->isValid() && isComponentComplete()) {
        d->regenerate();
    }
    emit pathItemCountChanged();
}

void QQuickPathView::resetPathItemCount()
{
    Q_D(QQuickPathView);
    if (-1 == d->pathItems)
        return;
    d->pathItems = -1;
    d->updateMappedRange();
    if (d->isValid() && isComponentComplete())
        d->regenerate();
    emit pathItemCountChanged();
}

/*!
    \qmlproperty int QtQuick::PathView::cacheItemCount
    This property holds the maximum number of items to cache off the path.

    For example, a PathView with a model containing 20 items, a pathItemCount
    of 10, and an cacheItemCount of 4 will create up to 14 items, with 10 visible
    on the path and 4 invisible cached items.

    The cached delegates are created asynchronously,
    allowing creation to occur across multiple frames and reducing the
    likelihood of skipping frames.

    Setting this value can improve the smoothness of scrolling behavior at the expense
    of additional memory usage.  It is not a substitute for creating efficient
    delegates; the fewer objects and bindings in a delegate, the faster a view can be
    moved.

    \sa pathItemCount
*/
int QQuickPathView::cacheItemCount() const
{
    Q_D(const QQuickPathView);
    return d->cacheSize;
}

void QQuickPathView::setCacheItemCount(int i)
{
    Q_D(QQuickPathView);
    if (i == d->cacheSize || i < 0)
        return;

    d->cacheSize = i;
    d->updateMappedRange();
    refill();
    emit cacheItemCountChanged();
}

/*!
    \qmlproperty enumeration QtQuick::PathView::snapMode

    This property determines how the items will settle following a drag or flick.
    The possible values are:

    \list
    \li PathView.NoSnap (default) - the items stop anywhere along the path.
    \li PathView.SnapToItem - the items settle with an item aligned with the \l preferredHighlightBegin.
    \li PathView.SnapOneItem - the items settle no more than one item away from the item nearest
        \l preferredHighlightBegin at the time the press is released.  This mode is particularly
        useful for moving one page at a time.
    \endlist

    \c snapMode does not affect the \l currentIndex.  To update the
    \l currentIndex as the view is moved, set \l highlightRangeMode
    to \c PathView.StrictlyEnforceRange (default for PathView).

    \sa highlightRangeMode
*/
QQuickPathView::SnapMode QQuickPathView::snapMode() const
{
    Q_D(const QQuickPathView);
    return d->snapMode;
}

void QQuickPathView::setSnapMode(SnapMode mode)
{
    Q_D(QQuickPathView);
    if (mode == d->snapMode)
        return;
    d->snapMode = mode;
    emit snapModeChanged();
}

/*!
    \qmlmethod QtQuick::PathView::positionViewAtIndex(int index, PositionMode mode)

    Positions the view such that the \a index is at the position specified by
    \a mode:

    \list
    \li PathView.Beginning - position item at the beginning of the path.
    \li PathView.Center - position item in the center of the path.
    \li PathView.End - position item at the end of the path.
    \li PathView.Contain - ensure the item is positioned on the path.
    \li PathView.SnapPosition - position the item at \l preferredHighlightBegin.  This mode
    is only valid if \l highlightRangeMode is StrictlyEnforceRange or snapping is enabled
    via \l snapMode.
    \endlist

    \b Note: methods should only be called after the Component has completed.  To position
    the view at startup, this method should be called by Component.onCompleted.  For
    example, to position the view at the end:

    \code
    Component.onCompleted: positionViewAtIndex(count - 1, PathView.End)
    \endcode
*/
void QQuickPathView::positionViewAtIndex(int index, int mode)
{
    Q_D(QQuickPathView);
    if (!d->isValid())
        return;
    if (mode < QQuickPathView::Beginning || mode > QQuickPathView::SnapPosition || mode == 3) // 3 is unused in PathView
        return;

    if (mode == QQuickPathView::Contain && (d->pathItems < 0 || d->modelCount <= d->pathItems))
        return;

    int count = d->pathItems == -1 ? d->modelCount : qMin(d->pathItems, d->modelCount);
    int idx = (index+d->modelCount) % d->modelCount;
    bool snap = d->haveHighlightRange && (d->highlightRangeMode != QQuickPathView::NoHighlightRange
            || d->snapMode != QQuickPathView::NoSnap);

    qreal beginOffset;
    qreal endOffset;
    if (snap) {
        beginOffset = d->modelCount - idx - qFloor(count * d->highlightRangeStart);
        endOffset = beginOffset + count - 1;
    } else {
        beginOffset = d->modelCount - idx;
        // Small offset since the last point coincides with the first and
        // this the only "end" position that gives the expected visual result.
        qreal adj = sizeof(qreal) == sizeof(float) ? 0.00001f : 0.000000000001;
        endOffset = qmlMod(beginOffset + count, d->modelCount) - adj;
    }
    qreal offset = d->offset;
    switch (mode) {
    case Beginning:
        offset = beginOffset;
        break;
    case End:
        offset = endOffset;
        break;
    case Center:
        if (beginOffset < endOffset)
            offset = (beginOffset + endOffset)/2;
        else
            offset = (beginOffset + (endOffset + d->modelCount))/2;
        if (snap)
            offset = qRound(offset);
        break;
    case Contain:
        if ((beginOffset < endOffset && (d->offset < beginOffset || d->offset > endOffset))
                || (d->offset < beginOffset && d->offset > endOffset)) {
            qreal diff1 = qmlMod(beginOffset - d->offset + d->modelCount, d->modelCount);
            qreal diff2 = qmlMod(d->offset - endOffset + d->modelCount, d->modelCount);
            if (diff1 < diff2)
                offset = beginOffset;
            else
                offset = endOffset;
        }
        break;
    case SnapPosition:
        offset = d->modelCount - idx;
        break;
    }

    d->tl.clear();
    setOffset(offset);
}

/*!
    \qmlmethod int QtQuick::PathView::indexAt(int x, int y)

    Returns the index of the item containing the point \a x, \a y in content
    coordinates.  If there is no item at the point specified, -1 is returned.

    \b Note: methods should only be called after the Component has completed.
*/
int QQuickPathView::indexAt(qreal x, qreal y) const
{
    Q_D(const QQuickPathView);
    if (!d->isValid())
        return -1;

    for (int idx = 0; idx < d->items.count(); ++idx) {
        QQuickItem *item = d->items.at(idx);
        QPointF p = item->mapFromItem(this, QPointF(x, y));
        if (item->contains(p))
            return (d->firstIndex + idx) % d->modelCount;
    }

    return -1;
}

/*!
    \qmlmethod Item QtQuick::PathView::itemAt(int x, int y)

    Returns the item containing the point \a x, \a y in content
    coordinates.  If there is no item at the point specified, null is returned.

    \b Note: methods should only be called after the Component has completed.
*/
QQuickItem *QQuickPathView::itemAt(qreal x, qreal y) const
{
    Q_D(const QQuickPathView);
    if (!d->isValid())
        return 0;

    for (int idx = 0; idx < d->items.count(); ++idx) {
        QQuickItem *item = d->items.at(idx);
        QPointF p = item->mapFromItem(this, QPointF(x, y));
        if (item->contains(p))
            return item;
    }

    return 0;
}

QPointF QQuickPathViewPrivate::pointNear(const QPointF &point, qreal *nearPercent) const
{
    qreal samples = qMin(path->path().length()/5, qreal(500.0));
    qreal res = path->path().length()/samples;

    qreal mindist = 1e10; // big number
    QPointF nearPoint = path->pointAt(0);
    qreal nearPc = 0;

    // get rough pos
    for (qreal i=1; i < samples; i++) {
        QPointF pt = path->pointAt(i/samples);
        QPointF diff = pt - point;
        qreal dist = diff.x()*diff.x() + diff.y()*diff.y();
        if (dist < mindist) {
            nearPoint = pt;
            nearPc = i;
            mindist = dist;
        }
    }

    // now refine
    qreal approxPc = nearPc;
    for (qreal i = approxPc-1.0; i < approxPc+1.0; i += 1/(2*res)) {
        QPointF pt = path->pointAt(i/samples);
        QPointF diff = pt - point;
        qreal dist = diff.x()*diff.x() + diff.y()*diff.y();
        if (dist < mindist) {
            nearPoint = pt;
            nearPc = i;
            mindist = dist;
        }
    }

    if (nearPercent)
        *nearPercent = nearPc / samples;

    return nearPoint;
}

void QQuickPathViewPrivate::addVelocitySample(qreal v)
{
    velocityBuffer.append(v);
    if (velocityBuffer.count() > QML_FLICK_SAMPLEBUFFER)
        velocityBuffer.remove(0);
}

qreal QQuickPathViewPrivate::calcVelocity() const
{
    qreal velocity = 0;
    if (velocityBuffer.count() > QML_FLICK_DISCARDSAMPLES) {
        int count = velocityBuffer.count()-QML_FLICK_DISCARDSAMPLES;
        for (int i = 0; i < count; ++i) {
            qreal v = velocityBuffer.at(i);
            velocity += v;
        }
        velocity /= count;
    }
    return velocity;
}

qint64 QQuickPathViewPrivate::computeCurrentTime(QInputEvent *event)
{
    if (0 != event->timestamp())
        return event->timestamp();
    return timer.elapsed();
}

void QQuickPathView::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickPathView);
    if (d->interactive) {
        d->handleMousePressEvent(event);
        event->accept();
    } else {
        QQuickItem::mousePressEvent(event);
    }
}

void QQuickPathViewPrivate::handleMousePressEvent(QMouseEvent *event)
{
    if (!interactive || !items.count() || !model || !modelCount)
        return;
    velocityBuffer.clear();
    int idx = 0;
    for (; idx < items.count(); ++idx) {
        QQuickItem *item = items.at(idx);
        if (item->contains(item->mapFromScene(event->windowPos())))
            break;
    }
    if (idx == items.count() && dragMargin == 0.)  // didn't click on an item
        return;

    startPoint = pointNear(event->localPos(), &startPc);
    startPos = event->localPos();
    if (idx == items.count()) {
        qreal distance = qAbs(event->localPos().x() - startPoint.x()) + qAbs(event->localPos().y() - startPoint.y());
        if (distance > dragMargin)
            return;
    }


    if (tl.isActive() && flicking && flickDuration && qreal(tl.time())/flickDuration < 0.8)
        stealMouse = true; // If we've been flicked then steal the click.
    else
        stealMouse = false;

    timer.start();
    lastPosTime = computeCurrentTime(event);
    tl.clear();
}

void QQuickPathView::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickPathView);
    if (d->interactive) {
        d->handleMouseMoveEvent(event);
        event->accept();
    } else {
        QQuickItem::mouseMoveEvent(event);
    }
}

void QQuickPathViewPrivate::handleMouseMoveEvent(QMouseEvent *event)
{
    Q_Q(QQuickPathView);
    if (!interactive || !timer.isValid() || !model || !modelCount)
        return;

    qint64 currentTimestamp = computeCurrentTime(event);
    qreal newPc;
    QPointF pathPoint = pointNear(event->localPos(), &newPc);
    if (!stealMouse) {
        QPointF posDelta = event->localPos() - startPos;
        if (QQuickWindowPrivate::dragOverThreshold(posDelta.y(), Qt::YAxis, event) || QQuickWindowPrivate::dragOverThreshold(posDelta.x(), Qt::XAxis, event)) {
            // The touch has exceeded the threshold. If the movement along the path is close to the drag threshold
            // then we'll assume that this gesture targets the PathView. This ensures PathView gesture grabbing
            // is in sync with other items.
            QPointF pathDelta = pathPoint - startPoint;
            if (qAbs(pathDelta.x()) > qApp->styleHints()->startDragDistance() * 0.8
                    || qAbs(pathDelta.y()) > qApp->styleHints()->startDragDistance() * 0.8) {
                stealMouse = true;
                q->setKeepMouseGrab(true);
            }
        }
    } else {
        moveReason = QQuickPathViewPrivate::Mouse;
        int count = pathItems == -1 ? modelCount : qMin(pathItems, modelCount);
        qreal diff = (newPc - startPc)*count;
        if (diff) {
            q->setOffset(offset + diff);

            if (diff > modelCount/2)
                diff -= modelCount;
            else if (diff < -modelCount/2)
                diff += modelCount;

            qint64 elapsed = currentTimestamp - lastPosTime;
            if (elapsed > 0)
                addVelocitySample(diff / (qreal(elapsed) / 1000.));
        }
        if (!moving) {
            moving = true;
            emit q->movingChanged();
            emit q->movementStarted();
        }
        setDragging(true);
    }
    startPc = newPc;
    lastPosTime = currentTimestamp;
}

void QQuickPathView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickPathView);
    if (d->interactive) {
        d->handleMouseReleaseEvent(event);
        event->accept();
        ungrabMouse();
    } else {
        QQuickItem::mouseReleaseEvent(event);
    }
}

void QQuickPathViewPrivate::handleMouseReleaseEvent(QMouseEvent *)
{
    Q_Q(QQuickPathView);
    stealMouse = false;
    q->setKeepMouseGrab(false);
    setDragging(false);
    if (!interactive || !timer.isValid() || !model || !modelCount) {
        timer.invalidate();
        if (!tl.isActive())
            q->movementEnding();
        return;
    }

    qreal velocity = calcVelocity();
    qreal count = pathItems == -1 ? modelCount : qMin(pathItems, modelCount);
    qreal pixelVelocity = (path->path().length()/count) * velocity;
    if (qAbs(pixelVelocity) > MinimumFlickVelocity) {
        if (qAbs(pixelVelocity) > maximumFlickVelocity || snapMode == QQuickPathView::SnapOneItem) {
            // limit velocity
            qreal maxVel = velocity < 0 ? -maximumFlickVelocity : maximumFlickVelocity;
            velocity = maxVel / (path->path().length()/count);
        }
        // Calculate the distance to be travelled
        qreal v2 = velocity*velocity;
        qreal accel = deceleration/10;
        qreal dist = 0;
        if (haveHighlightRange && (highlightRangeMode == QQuickPathView::StrictlyEnforceRange
                || snapMode != QQuickPathView::NoSnap)) {
            if (snapMode == QQuickPathView::SnapOneItem) {
                // encourage snapping one item in direction of motion
                if (velocity > 0.)
                    dist = qRound(0.5 + offset) - offset;
                else
                    dist = qRound(0.5 - offset) + offset;
            } else {
                // + 0.25 to encourage moving at least one item in the flick direction
                dist = qMin(qreal(modelCount-1), qreal(v2 / (accel * 2.0) + 0.25));

                // round to nearest item.
                if (velocity > 0.)
                    dist = qRound(dist + offset) - offset;
                else
                    dist = qRound(dist - offset) + offset;
            }
            // Calculate accel required to stop on item boundary
            if (dist <= 0.) {
                dist = 0.;
                accel = 0.;
            } else {
                accel = v2 / (2.0f * qAbs(dist));
            }
        } else {
            dist = qMin(qreal(modelCount-1), qreal(v2 / (accel * 2.0)));
        }
        flickDuration = static_cast<int>(1000 * qAbs(velocity) / accel);
        offsetAdj = 0.0;
        moveOffset.setValue(offset);
        tl.accel(moveOffset, velocity, accel, dist);
        tl.callback(QQuickTimeLineCallback(&moveOffset, fixOffsetCallback, this));
        if (!flicking) {
            flicking = true;
            emit q->flickingChanged();
            emit q->flickStarted();
        }
    } else {
        fixOffset();
    }

    timer.invalidate();
    if (!tl.isActive())
        q->movementEnding();
}

bool QQuickPathView::sendMouseEvent(QMouseEvent *event)
{
    Q_D(QQuickPathView);
    QPointF localPos = mapFromScene(event->windowPos());

    QQuickWindow *c = window();
    QQuickItem *grabber = c ? c->mouseGrabberItem() : 0;
    if (grabber == this && d->stealMouse) {
        // we are already the grabber and we do want the mouse event to ourselves.
        return true;
    }

    bool grabberDisabled = grabber && !grabber->isEnabled();
    bool stealThisEvent = d->stealMouse;
    if ((stealThisEvent || contains(localPos)) && (!grabber || !grabber->keepMouseGrab() || grabberDisabled)) {
        QScopedPointer<QMouseEvent> mouseEvent(QQuickWindowPrivate::cloneMouseEvent(event, &localPos));
        mouseEvent->setAccepted(false);

        switch (mouseEvent->type()) {
        case QEvent::MouseMove:
            d->handleMouseMoveEvent(mouseEvent.data());
            break;
        case QEvent::MouseButtonPress:
            d->handleMousePressEvent(mouseEvent.data());
            stealThisEvent = d->stealMouse;   // Update stealThisEvent in case changed by function call above
            break;
        case QEvent::MouseButtonRelease:
            d->handleMouseReleaseEvent(mouseEvent.data());
            break;
        default:
            break;
        }
        grabber = c->mouseGrabberItem();
        if ((grabber && stealThisEvent && !grabber->keepMouseGrab() && grabber != this) || grabberDisabled) {
            grabMouse();
        }

        const bool filtered = stealThisEvent || grabberDisabled;
        if (filtered) {
            event->setAccepted(false);
        }
        return filtered;
    } else if (d->timer.isValid()) {
        d->timer.invalidate();
        d->fixOffset();
    }
    if (event->type() == QEvent::MouseButtonRelease || (grabber && grabber->keepMouseGrab() && !grabberDisabled)) {
        d->stealMouse = false;
    }
    return false;
}

bool QQuickPathView::childMouseEventFilter(QQuickItem *i, QEvent *e)
{
    Q_D(QQuickPathView);
    if (!isVisible() || !d->interactive)
        return QQuickItem::childMouseEventFilter(i, e);

    switch (e->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
        return sendMouseEvent(static_cast<QMouseEvent *>(e));
    default:
        break;
    }

    return QQuickItem::childMouseEventFilter(i, e);
}

void QQuickPathView::mouseUngrabEvent()
{
    Q_D(QQuickPathView);
    if (d->stealMouse) {
        // if our mouse grab has been removed (probably by a Flickable),
        // fix our state
        d->stealMouse = false;
        setKeepMouseGrab(false);
        d->timer.invalidate();
        d->fixOffset();
        d->setDragging(false);
        if (!d->tl.isActive())
            movementEnding();
    }
}

void QQuickPathView::updatePolish()
{
    QQuickItem::updatePolish();
    refill();
}

void QQuickPathView::componentComplete()
{
    Q_D(QQuickPathView);
    if (d->model && d->ownModel)
        static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();

    QQuickItem::componentComplete();

    if (d->model) {
        d->modelCount = d->model->count();
        if (d->modelCount && d->currentIndex != 0) // an initial value has been provided for currentIndex
            d->offset = qmlMod(d->modelCount - d->currentIndex, d->modelCount);
    }

    d->createHighlight();
    d->regenerate();
    d->updateHighlight();
    d->updateCurrent();

    if (d->modelCount)
        emit countChanged();
}

void QQuickPathView::refill()
{
    Q_D(QQuickPathView);

    if (d->inRefill) {
        d->scheduleLayout();
        return;
    }

    d->layoutScheduled = false;

    if (!d->isValid() || !isComponentComplete())
        return;

    d->inRefill = true;

    bool currentVisible = false;
    int count = d->pathItems == -1 ? d->modelCount : qMin(d->pathItems, d->modelCount);

    // first move existing items and remove items off path
    int idx = d->firstIndex;
    QList<QQuickItem*>::iterator it = d->items.begin();
    while (it != d->items.end()) {
        qreal pos = d->positionOfIndex(idx);
        QQuickItem *item = *it;
        if (pos < 1.0) {
            d->updateItem(item, pos);
            if (idx == d->currentIndex) {
                currentVisible = true;
                d->currentItemOffset = pos;
            }
            ++it;
        } else {
            d->updateItem(item, pos);
            if (QQuickPathViewAttached *att = d->attached(item))
                att->setOnPath(pos < 1.0);
            if (!d->isInBound(pos, d->mappedRange - d->mappedCache, 1.0 + d->mappedCache)) {
//                qDebug() << "release";
                d->releaseItem(item);
                if (it == d->items.begin()) {
                    if (++d->firstIndex >= d->modelCount) {
                        d->firstIndex = 0;
                    }
                }
                it = d->items.erase(it);
            } else {
                ++it;
            }
        }
        ++idx;
        if (idx >= d->modelCount)
            idx = 0;
    }

    if (!d->items.count())
        d->firstIndex = -1;

    bool waiting = false;
    if (d->modelCount) {
        // add items to beginning and end
        if (d->items.count() < count+d->cacheSize) {
            int idx = qRound(d->modelCount - d->offset) % d->modelCount;
            qreal startPos = 0.0;
            if (d->haveHighlightRange && (d->highlightRangeMode != QQuickPathView::NoHighlightRange
                                          || d->snapMode != QQuickPathView::NoSnap))
                startPos = d->highlightRangeStart;
            if (d->firstIndex >= 0) {
                startPos = d->positionOfIndex(d->firstIndex);
                idx = (d->firstIndex + d->items.count()) % d->modelCount;
            }
            qreal pos = d->positionOfIndex(idx);
            while ((d->isInBound(pos, startPos, 1.0 + d->mappedCache) || !d->items.count()) && d->items.count() < count+d->cacheSize) {
//                qDebug() << "append" << idx;
                QQuickItem *item = d->getItem(idx, idx+1, pos >= 1.0);
                if (!item) {
                    waiting = true;
                    break;
                }
                if (d->currentIndex == idx) {
                    currentVisible = true;
                    d->currentItemOffset = pos;
                }
                if (d->items.count() == 0)
                    d->firstIndex = idx;
                d->items.append(item);
                d->updateItem(item, pos);
                ++idx;
                if (idx >= d->modelCount)
                    idx = 0;
                pos = d->positionOfIndex(idx);
            }

            idx = d->firstIndex - 1;
            if (idx < 0)
                idx = d->modelCount - 1;
            pos = d->positionOfIndex(idx);
            while (!waiting && d->isInBound(pos, d->mappedRange - d->mappedCache, startPos) && d->items.count() < count+d->cacheSize) {
//                 qDebug() << "prepend" << idx;
                QQuickItem *item = d->getItem(idx, idx+1, pos >= 1.0);
                if (!item) {
                    waiting = true;
                    break;
                }
                if (d->currentIndex == idx) {
                    currentVisible = true;
                    d->currentItemOffset = pos;
                }
                d->items.prepend(item);
                d->updateItem(item, pos);
                d->firstIndex = idx;
                idx = d->firstIndex - 1;
                if (idx < 0)
                    idx = d->modelCount - 1;
                pos = d->positionOfIndex(idx);
            }
        }
    }

    bool currentChanged = false;
    if (!currentVisible) {
        d->currentItemOffset = 1.0;
        if (d->currentItem) {
            d->updateItem(d->currentItem, 1.0);
        } else if (!waiting && d->currentIndex >= 0 && d->currentIndex < d->modelCount) {
            if ((d->currentItem = d->getItem(d->currentIndex, d->currentIndex))) {
                currentChanged = true;
                d->updateItem(d->currentItem, 1.0);
                if (QQuickPathViewAttached *att = d->attached(d->currentItem))
                    att->setIsCurrentItem(true);
            }
        }
    } else if (!waiting && !d->currentItem) {
        if ((d->currentItem = d->getItem(d->currentIndex, d->currentIndex))) {
            currentChanged = true;
            d->currentItem->setFocus(true);
            if (QQuickPathViewAttached *att = d->attached(d->currentItem))
                att->setIsCurrentItem(true);
        }
    }

    if (d->highlightItem && d->haveHighlightRange && d->highlightRangeMode == QQuickPathView::StrictlyEnforceRange) {
        d->updateItem(d->highlightItem, d->highlightRangeStart);
        if (QQuickPathViewAttached *att = d->attached(d->highlightItem))
            att->setOnPath(true);
    } else if (d->highlightItem && d->moveReason != QQuickPathViewPrivate::SetIndex) {
        d->updateItem(d->highlightItem, d->currentItemOffset);
        if (QQuickPathViewAttached *att = d->attached(d->highlightItem))
            att->setOnPath(currentVisible);
    }
    while (d->itemCache.count())
        d->releaseItem(d->itemCache.takeLast());

    d->inRefill = false;
    if (currentChanged)
        emit currentItemChanged();
}

void QQuickPathView::modelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_D(QQuickPathView);
    if (!d->model || !d->model->isValid() || !d->path || !isComponentComplete())
        return;

    if (reset) {
        d->modelCount = d->model->count();
        d->regenerate();
        emit countChanged();
        return;
    }

    if (changeSet.removes().isEmpty() && changeSet.inserts().isEmpty())
        return;

    const int modelCount = d->modelCount;
    int moveId = -1;
    int moveOffset = 0;
    bool currentChanged = false;
    bool changedOffset = false;
    foreach (const QQmlChangeSet::Change &r, changeSet.removes()) {
        if (moveId == -1 && d->currentIndex >= r.index + r.count) {
            d->currentIndex -= r.count;
            currentChanged = true;
        } else if (moveId == -1 && d->currentIndex >= r.index && d->currentIndex < r.index + r.count) {
            // current item has been removed.
            if (r.isMove()) {
                moveId = r.moveId;
                moveOffset = d->currentIndex - r.index;
            } else if (d->currentItem) {
                if (QQuickPathViewAttached *att = d->attached(d->currentItem))
                    att->setIsCurrentItem(true);
                d->releaseItem(d->currentItem);
                d->currentItem = 0;
            }
            d->currentIndex = qMin(r.index, d->modelCount - r.count - 1);
            currentChanged = true;
        }

        if (r.index > d->currentIndex) {
            changedOffset = true;
            d->offset -= r.count;
            d->offsetAdj -= r.count;
        }
        d->modelCount -= r.count;
    }
    foreach (const QQmlChangeSet::Change &i, changeSet.inserts()) {
        if (d->modelCount) {
            if (moveId == -1 && i.index <= d->currentIndex) {
                d->currentIndex += i.count;
                currentChanged = true;
            } else {
                if (moveId != -1 && moveId == i.moveId) {
                    d->currentIndex = i.index + moveOffset;
                    currentChanged = true;
                }
                if (i.index > d->currentIndex) {
                    d->offset += i.count;
                    d->offsetAdj += i.count;
                    changedOffset = true;
                }
            }
        }
        d->modelCount += i.count;
    }

    d->offset = qmlMod(d->offset, d->modelCount);
    if (d->offset < 0)
        d->offset += d->modelCount;
    if (d->currentIndex == -1)
        d->currentIndex = d->calcCurrentIndex();

    d->itemCache += d->items;
    d->items.clear();

    if (!d->modelCount) {
        while (d->itemCache.count())
            d->releaseItem(d->itemCache.takeLast());
        d->offset = 0;
        changedOffset = true;
        d->tl.reset(d->moveOffset);
    } else {
        if (!d->flicking && !d->moving && d->haveHighlightRange && d->highlightRangeMode == QQuickPathView::StrictlyEnforceRange) {
            d->offset = qmlMod(d->modelCount - d->currentIndex, d->modelCount);
            changedOffset = true;
        }
        d->firstIndex = -1;
        d->updateMappedRange();
        d->scheduleLayout();
    }
    if (changedOffset)
        emit offsetChanged();
    if (currentChanged)
        emit currentIndexChanged();
    if (d->modelCount != modelCount)
        emit countChanged();
}

void QQuickPathView::destroyingItem(QObject *item)
{
    Q_UNUSED(item);
}

void QQuickPathView::ticked()
{
    Q_D(QQuickPathView);
    d->updateCurrent();
}

void QQuickPathView::movementEnding()
{
    Q_D(QQuickPathView);
    if (d->flicking) {
        d->flicking = false;
        emit flickingChanged();
        emit flickEnded();
    }
    if (d->moving && !d->stealMouse) {
        d->moving = false;
        emit movingChanged();
        emit movementEnded();
    }
}

// find the item closest to the snap position
int QQuickPathViewPrivate::calcCurrentIndex()
{
    int current = 0;
    if (modelCount && model && items.count()) {
        offset = qmlMod(offset, modelCount);
        if (offset < 0)
            offset += modelCount;
        current = qRound(qAbs(qmlMod(modelCount - offset, modelCount)));
        current = current % modelCount;
    }

    return current;
}

void QQuickPathViewPrivate::createCurrentItem()
{
    if (requestedIndex != -1)
        return;
    int itemIndex = (currentIndex - firstIndex + modelCount) % modelCount;
    if (itemIndex < items.count()) {
        if ((currentItem = getItem(currentIndex, currentIndex))) {
            currentItem->setFocus(true);
            if (QQuickPathViewAttached *att = attached(currentItem))
                att->setIsCurrentItem(true);
        }
    } else if (currentIndex >= 0 && currentIndex < modelCount) {
        if ((currentItem = getItem(currentIndex, currentIndex))) {
            updateItem(currentItem, 1.0);
            if (QQuickPathViewAttached *att = attached(currentItem))
                att->setIsCurrentItem(true);
        }
    }
}

void QQuickPathViewPrivate::updateCurrent()
{
    Q_Q(QQuickPathView);
    if (moveReason == SetIndex)
        return;
    if (!modelCount || !haveHighlightRange || highlightRangeMode != QQuickPathView::StrictlyEnforceRange)
        return;

    int idx = calcCurrentIndex();
    if (model && (idx != currentIndex || !currentItem)) {
        if (currentItem) {
            if (QQuickPathViewAttached *att = attached(currentItem))
                att->setIsCurrentItem(false);
            releaseItem(currentItem);
        }
        int oldCurrentIndex = currentIndex;
        currentIndex = idx;
        currentItem = 0;
        createCurrentItem();
        if (oldCurrentIndex != currentIndex)
            emit q->currentIndexChanged();
        emit q->currentItemChanged();
    }
}

void QQuickPathViewPrivate::fixOffsetCallback(void *d)
{
    ((QQuickPathViewPrivate *)d)->fixOffset();
}

void QQuickPathViewPrivate::fixOffset()
{
    Q_Q(QQuickPathView);
    if (model && items.count()) {
        if (haveHighlightRange && (highlightRangeMode == QQuickPathView::StrictlyEnforceRange
                || snapMode != QQuickPathView::NoSnap)) {
            int curr = calcCurrentIndex();
            if (curr != currentIndex && highlightRangeMode == QQuickPathView::StrictlyEnforceRange)
                q->setCurrentIndex(curr);
            else
                snapToIndex(curr);
        }
    }
}

void QQuickPathViewPrivate::snapToIndex(int index)
{
    if (!model || modelCount <= 0)
        return;

    qreal targetOffset = qmlMod(modelCount - index, modelCount);

    if (offset == targetOffset)
        return;

    moveReason = Other;
    offsetAdj = 0.0;
    tl.reset(moveOffset);
    moveOffset.setValue(offset);

    const int duration = highlightMoveDuration;

    if (!duration) {
        tl.set(moveOffset, targetOffset);
    } else if (moveDirection == Positive || (moveDirection == Shortest && targetOffset - offset > modelCount/2.0)) {
        qreal distance = modelCount - targetOffset + offset;
        if (targetOffset > moveOffset) {
            tl.move(moveOffset, 0.0, QEasingCurve(QEasingCurve::InQuad), int(duration * offset / distance));
            tl.set(moveOffset, modelCount);
            tl.move(moveOffset, targetOffset, QEasingCurve(offset == 0.0 ? QEasingCurve::InOutQuad : QEasingCurve::OutQuad), int(duration * (modelCount-targetOffset) / distance));
        } else {
            tl.move(moveOffset, targetOffset, QEasingCurve(QEasingCurve::InOutQuad), duration);
        }
    } else if (moveDirection == Negative || targetOffset - offset <= -modelCount/2.0) {
        qreal distance = modelCount - offset + targetOffset;
        if (targetOffset < moveOffset) {
            tl.move(moveOffset, modelCount, QEasingCurve(targetOffset == 0 ? QEasingCurve::InOutQuad : QEasingCurve::InQuad), int(duration * (modelCount-offset) / distance));
            tl.set(moveOffset, 0.0);
            tl.move(moveOffset, targetOffset, QEasingCurve(QEasingCurve::OutQuad), int(duration * targetOffset / distance));
        } else {
            tl.move(moveOffset, targetOffset, QEasingCurve(QEasingCurve::InOutQuad), duration);
        }
    } else {
        tl.move(moveOffset, targetOffset, QEasingCurve(QEasingCurve::InOutQuad), duration);
    }
    moveDirection = Shortest;
}

QQuickPathViewAttached *QQuickPathView::qmlAttachedProperties(QObject *obj)
{
    return new QQuickPathViewAttached(obj);
}

QT_END_NAMESPACE

