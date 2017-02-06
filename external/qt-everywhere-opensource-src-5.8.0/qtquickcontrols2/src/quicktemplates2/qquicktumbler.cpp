/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
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

#include "qquicktumbler_p.h"

#include <QtQml/qqmlinfo.h>
#include <QtQuick/private/qquickflickable_p.h>
#include <QtQuickTemplates2/private/qquickcontrol_p_p.h>
#include <QtQuickTemplates2/private/qquicktumbler_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Tumbler
    \inherits Control
    \instantiates QQuickTumbler
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-input
    \brief Spinnable wheel of items that can be selected.

    \image qtquickcontrols2-tumbler-wrap.gif

    \code
    Tumbler {
        model: 5
        // ...
    }
    \endcode

    Tumbler allows the user to select an option from a spinnable \e "wheel" of
    items. It is useful for when there are too many options to use, for
    example, a RadioButton, and too few options to require the use of an
    editable SpinBox. It is convenient in that it requires no keyboard usage
    and wraps around at each end when there are a large number of items.

    The API is similar to that of views like \l ListView and \l PathView; a
    \l model and \l delegate can be set, and the \l count and \l currentItem
    properties provide read-only access to information about the view.

    Unlike views like \l PathView and \l ListView, however, there is always a
    current item (when the model isn't empty). This means that when \l count is
    equal to \c 0, \l currentIndex will be \c -1. In all other cases, it will
    be greater than or equal to \c 0.

    By default, Tumbler \l {wrap}{wraps} when it reaches the top and bottom, as
    long as there are more items in the model than there are visible items;
    that is, when \l count is greater than \l visibleItemCount:

    \snippet qtquickcontrols2-tumbler-timePicker.qml tumbler

    \sa {Customizing Tumbler}, {Input Controls}
*/

QQuickTumblerPrivate::QQuickTumblerPrivate() :
    delegate(nullptr),
    visibleItemCount(5),
    wrap(true),
    explicitWrap(false),
    ignoreWrapChanges(false),
    view(nullptr),
    viewContentItem(nullptr),
    viewContentItemType(UnsupportedContentItemType),
    currentIndex(-1),
    pendingCurrentIndex(-1),
    ignoreCurrentIndexChanges(false),
    count(0)
{
}

QQuickTumblerPrivate::~QQuickTumblerPrivate()
{
}

namespace {
    static inline qreal delegateHeight(const QQuickTumbler *tumbler)
    {
        return tumbler->availableHeight() / tumbler->visibleItemCount();
    }
}

/*
    Finds the contentItem of the view that is a child of the control's \a contentItem.
    The type is stored in \a type.
*/
QQuickItem *QQuickTumblerPrivate::determineViewType(QQuickItem *contentItem)
{
    if (contentItem->inherits("QQuickPathView")) {
        view = contentItem;
        viewContentItem = contentItem;
        viewContentItemType = PathViewContentItem;
        return contentItem;
    } else if (contentItem->inherits("QQuickListView")) {
        view = contentItem;
        viewContentItem = qobject_cast<QQuickFlickable*>(contentItem)->contentItem();
        viewContentItemType = ListViewContentItem;
        return contentItem;
    } else {
        const auto childItems = contentItem->childItems();
        for (QQuickItem *childItem : childItems) {
            QQuickItem *item = determineViewType(childItem);
            if (item)
                return item;
        }
    }

    resetViewData();
    return nullptr;
}

void QQuickTumblerPrivate::resetViewData()
{
    view = nullptr;
    viewContentItem = nullptr;
    viewContentItemType = UnsupportedContentItemType;
}

QList<QQuickItem *> QQuickTumblerPrivate::viewContentItemChildItems() const
{
    if (!viewContentItem)
        return QList<QQuickItem *>();

    return viewContentItem->childItems();
}

QQuickTumblerPrivate *QQuickTumblerPrivate::get(QQuickTumbler *tumbler)
{
    return tumbler->d_func();
}

void QQuickTumblerPrivate::_q_updateItemHeights()
{
    // Can't use our own private padding members here, as the padding property might be set,
    // which doesn't affect them, only their getters.
    Q_Q(const QQuickTumbler);
    const qreal itemHeight = delegateHeight(q);
    const auto items = viewContentItemChildItems();
    for (QQuickItem *childItem : items)
        childItem->setHeight(itemHeight);
}

void QQuickTumblerPrivate::_q_updateItemWidths()
{
    Q_Q(const QQuickTumbler);
    const qreal availableWidth = q->availableWidth();
    const auto items = viewContentItemChildItems();
    for (QQuickItem *childItem : items)
        childItem->setWidth(availableWidth);
}

void QQuickTumblerPrivate::_q_onViewCurrentIndexChanged()
{
    Q_Q(QQuickTumbler);
    if (view && !ignoreCurrentIndexChanges) {
        const int oldCurrentIndex = currentIndex;
        currentIndex = view->property("currentIndex").toInt();
        if (oldCurrentIndex != currentIndex)
            emit q->currentIndexChanged();
    }
}

void QQuickTumblerPrivate::_q_onViewCountChanged()
{
    Q_Q(QQuickTumbler);

    setCount(view->property("count").toInt());

    if (count > 0) {
        if (pendingCurrentIndex != -1) {
            // If there was an attempt to set currentIndex at creation, try to finish that attempt now.
            // componentComplete() is too early, because the count might only be known sometime after completion.
            q->setCurrentIndex(pendingCurrentIndex);
            // If we could successfully set the currentIndex, consider it done.
            // Otherwise, we'll try again later in updatePolish().
            if (currentIndex == pendingCurrentIndex)
                pendingCurrentIndex = -1;
            else
                q->polish();
        } else if (currentIndex == -1) {
            // If new items were added and our currentIndex was -1, we must
            // enforce our rule of a non-negative currentIndex when count > 0.
            q->setCurrentIndex(0);
        }
    } else {
        q->setCurrentIndex(-1);
    }
}

void QQuickTumblerPrivate::itemChildAdded(QQuickItem *, QQuickItem *)
{
    _q_updateItemWidths();
    _q_updateItemHeights();
}

void QQuickTumblerPrivate::itemChildRemoved(QQuickItem *, QQuickItem *)
{
    _q_updateItemWidths();
    _q_updateItemHeights();
}

QQuickTumbler::QQuickTumbler(QQuickItem *parent) :
    QQuickControl(*(new QQuickTumblerPrivate), parent)
{
    setActiveFocusOnTab(true);

    connect(this, SIGNAL(leftPaddingChanged()), this, SLOT(_q_updateItemWidths()));
    connect(this, SIGNAL(rightPaddingChanged()), this, SLOT(_q_updateItemWidths()));
    connect(this, SIGNAL(topPaddingChanged()), this, SLOT(_q_updateItemHeights()));
    connect(this, SIGNAL(bottomPaddingChanged()), this, SLOT(_q_updateItemHeights()));
}

QQuickTumbler::~QQuickTumbler()
{
    Q_D(QQuickTumbler);
    // Ensure that the item change listener is removed.
    d->disconnectFromView();
}

/*!
    \qmlproperty variant QtQuick.Controls::Tumbler::model

    This property holds the model that provides data for this tumbler.
*/
QVariant QQuickTumbler::model() const
{
    Q_D(const QQuickTumbler);
    return d->model;
}

void QQuickTumbler::setModel(const QVariant &model)
{
    Q_D(QQuickTumbler);
    if (model == d->model)
        return;

    d->lockWrap();

    d->model = model;
    emit modelChanged();

    d->unlockWrap();

    // Don't try to correct the currentIndex if count() isn't known yet.
    // We can check in setupViewData() instead.
    if (isComponentComplete() && d->view && count() == 0)
        setCurrentIndex(-1);
}

/*!
    \qmlproperty int QtQuick.Controls::Tumbler::count
    \readonly

    This property holds the number of items in the model.
*/
int QQuickTumbler::count() const
{
    Q_D(const QQuickTumbler);
    return d->count;
}

/*!
    \qmlproperty int QtQuick.Controls::Tumbler::currentIndex

    This property holds the index of the current item.

    The value of this property is \c -1 when \l count is equal to \c 0. In all
    other cases, it will be greater than or equal to \c 0.
*/
int QQuickTumbler::currentIndex() const
{
    Q_D(const QQuickTumbler);
    return d->currentIndex;
}

void QQuickTumbler::setCurrentIndex(int currentIndex)
{
    Q_D(QQuickTumbler);
    if (currentIndex == d->currentIndex || currentIndex < -1)
        return;

    if (!isComponentComplete()) {
        // Views can't set currentIndex until they're ready.
        d->pendingCurrentIndex = currentIndex;
        return;
    }

    // -1 doesn't make sense for a non-empty Tumbler, because unlike
    // e.g. ListView, there's always one item selected.
    // Wait until the component has finished before enforcing this rule, though,
    // because the count might not be known yet.
    if ((d->count > 0 && currentIndex == -1) || (currentIndex >= d->count)) {
        return;
    }

    // The view might not have been created yet, as is the case
    // if you create a Tumbler component and pass e.g. { currentIndex: 2 }
    // to createObject().
    if (d->view) {
        // Only actually set our currentIndex if the view was able to set theirs.
        bool couldSet = false;
        if (d->count == 0 && currentIndex == -1) {
            // PathView insists on using 0 as the currentIndex when there are no items.
            couldSet = true;
        } else {
            d->ignoreCurrentIndexChanges = true;
            d->view->setProperty("currentIndex", currentIndex);
            d->ignoreCurrentIndexChanges = false;

            couldSet = d->view->property("currentIndex").toInt() == currentIndex;
        }

        if (couldSet) {
            // The view's currentIndex might not have actually changed, but ours has,
            // and that's what user code sees.
            d->currentIndex = currentIndex;
            emit currentIndexChanged();
        }
    }
}

/*!
    \qmlproperty Item QtQuick.Controls::Tumbler::currentItem
    \readonly

    This property holds the item at the current index.
*/
QQuickItem *QQuickTumbler::currentItem() const
{
    Q_D(const QQuickTumbler);
    return d->view ? d->view->property("currentItem").value<QQuickItem*>() : nullptr;
}

/*!
    \qmlproperty Component QtQuick.Controls::Tumbler::delegate

    This property holds the delegate used to display each item.
*/
QQmlComponent *QQuickTumbler::delegate() const
{
    Q_D(const QQuickTumbler);
    return d->delegate;
}

void QQuickTumbler::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQuickTumbler);
    if (delegate == d->delegate)
        return;

    d->delegate = delegate;
    emit delegateChanged();
}

/*!
    \qmlproperty int QtQuick.Controls::Tumbler::visibleItemCount

    This property holds the number of items visible in the tumbler. It must be
    an odd number, as the current item is always vertically centered.
*/
int QQuickTumbler::visibleItemCount() const
{
    Q_D(const QQuickTumbler);
    return d->visibleItemCount;
}

void QQuickTumbler::setVisibleItemCount(int visibleItemCount)
{
    Q_D(QQuickTumbler);
    if (visibleItemCount == d->visibleItemCount)
        return;

    d->visibleItemCount = visibleItemCount;
    d->_q_updateItemHeights();
    emit visibleItemCountChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::Tumbler::wrap
    \since QtQuick.Controls 2.1

    This property determines whether or not the tumbler wraps around when it
    reaches the top or bottom.

    The default value is \c false when \l count is less than
    \l visibleItemCount, as it is simpler to interact with a non-wrapping Tumbler
    when there are only a few items. To override this behavior, explicitly set
    the value of this property. To return to the default behavior, set this
    property to \c undefined.
*/
bool QQuickTumbler::wrap() const
{
    Q_D(const QQuickTumbler);
    return d->wrap;
}

void QQuickTumbler::setWrap(bool wrap)
{
    Q_D(QQuickTumbler);
    d->setWrap(wrap, true);
}

void QQuickTumbler::resetWrap()
{
    Q_D(QQuickTumbler);
    d->explicitWrap = false;
    d->setWrapBasedOnCount();
}

QQuickTumblerAttached *QQuickTumbler::qmlAttachedProperties(QObject *object)
{
    return new QQuickTumblerAttached(object);
}

void QQuickTumbler::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickTumbler);

    QQuickControl::geometryChanged(newGeometry, oldGeometry);

    d->_q_updateItemHeights();

    if (newGeometry.width() != oldGeometry.width())
        d->_q_updateItemWidths();
}

void QQuickTumbler::componentComplete()
{
    Q_D(QQuickTumbler);
    QQuickControl::componentComplete();
    d->_q_updateItemHeights();
    d->_q_updateItemWidths();

    if (!d->view) {
        // Force the view to be created.
        emit wrapChanged();
        // Determine the type of view for attached properties, etc.
        d->setupViewData(d->contentItem);
    }
}

void QQuickTumbler::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_D(QQuickTumbler);

    QQuickControl::contentItemChange(newItem, oldItem);

    if (oldItem)
        d->disconnectFromView();

    if (newItem) {
        // We wait until wrap is set to that we know which type of view to create.
        // If we try to set up the view too early, we'll issue warnings about it not existing.
        if (isComponentComplete()) {
            // Make sure we use the new content item and not the current one, as that won't
            // be changed until after contentItemChange() has finished.
            d->setupViewData(newItem);
        }
    }
}

void QQuickTumblerPrivate::disconnectFromView()
{
    Q_Q(QQuickTumbler);
    if (!view) {
        // If a custom content item is declared, it can happen that
        // the original contentItem exists without the view etc. having been
        // determined yet, and then this is called when the custom content item
        // is eventually set.
        return;
    }

    QObject::disconnect(view, SIGNAL(currentIndexChanged()), q, SLOT(_q_onViewCurrentIndexChanged()));
    QObject::disconnect(view, SIGNAL(currentItemChanged()), q, SIGNAL(currentItemChanged()));
    QObject::disconnect(view, SIGNAL(countChanged()), q, SLOT(_q_onViewCountChanged()));

    QQuickItemPrivate *oldViewContentItemPrivate = QQuickItemPrivate::get(viewContentItem);
    oldViewContentItemPrivate->removeItemChangeListener(this, QQuickItemPrivate::Children);

    resetViewData();
}

void QQuickTumblerPrivate::setupViewData(QQuickItem *newControlContentItem)
{
    // Don't do anything if we've already set up.
    if (view)
        return;

    determineViewType(newControlContentItem);

    if (viewContentItemType == QQuickTumblerPrivate::UnsupportedContentItemType) {
        qWarning() << "Tumbler: contentItem must contain either a PathView or a ListView";
        return;
    }

    Q_Q(QQuickTumbler);
    QObject::connect(view, SIGNAL(currentIndexChanged()), q, SLOT(_q_onViewCurrentIndexChanged()));
    QObject::connect(view, SIGNAL(currentItemChanged()), q, SIGNAL(currentItemChanged()));
    QObject::connect(view, SIGNAL(countChanged()), q, SLOT(_q_onViewCountChanged()));

    QQuickItemPrivate *viewContentItemPrivate = QQuickItemPrivate::get(viewContentItem);
    viewContentItemPrivate->addItemChangeListener(this, QQuickItemPrivate::Children);

    // Sync the view's currentIndex with ours.
    syncCurrentIndex();
}

void QQuickTumblerPrivate::syncCurrentIndex()
{
    const int actualViewIndex = view->property("currentIndex").toInt();
    Q_Q(QQuickTumbler);

    // Nothing to do.
    if (actualViewIndex == currentIndex)
        return;

    // PathView likes to use 0 as currentIndex for empty models, but we use -1 for that.
    if (q->count() == 0 && actualViewIndex == 0)
        return;

    ignoreCurrentIndexChanges = true;
    view->setProperty("currentIndex", currentIndex);
    ignoreCurrentIndexChanges = false;
}

void QQuickTumblerPrivate::setCount(int newCount)
{
    if (newCount == count)
        return;

    count = newCount;

    Q_Q(QQuickTumbler);
    setWrapBasedOnCount();

    emit q->countChanged();
}

void QQuickTumblerPrivate::setWrapBasedOnCount()
{
    if (count == 0 || explicitWrap || ignoreWrapChanges)
        return;

    setWrap(count >= visibleItemCount, false);
}

void QQuickTumblerPrivate::setWrap(bool shouldWrap, bool isExplicit)
{
    if (isExplicit)
        explicitWrap = true;

    Q_Q(QQuickTumbler);
    if (q->isComponentComplete() && shouldWrap == wrap)
        return;

    // Since we use the currentIndex of the contentItem directly, we must
    // ensure that we keep track of the currentIndex so it doesn't get lost
    // between view changes.
    const int oldCurrentIndex = currentIndex;

    disconnectFromView();

    wrap = shouldWrap;

    // New views will set their currentIndex upon creation, which we'd otherwise
    // take as the correct one, so we must ignore them.
    ignoreCurrentIndexChanges = true;

    // This will cause the view to be created if our contentItem is a TumblerView.
    emit q->wrapChanged();

    ignoreCurrentIndexChanges = false;

    // The view should have been created now, so we can start determining its type, etc.
    // If the delegates use attached properties, this will have already been called,
    // in which case it will return early. If the delegate doesn't use attached properties,
    // we need to call it here.
    setupViewData(contentItem);

    q->setCurrentIndex(oldCurrentIndex);
}

void QQuickTumblerPrivate::lockWrap()
{
    ignoreWrapChanges = true;
}

void QQuickTumblerPrivate::unlockWrap()
{
    ignoreWrapChanges = false;
    setWrapBasedOnCount();
}

void QQuickTumbler::keyPressEvent(QKeyEvent *event)
{
    QQuickControl::keyPressEvent(event);

    Q_D(QQuickTumbler);
    if (event->isAutoRepeat() || !d->view)
        return;

    if (event->key() == Qt::Key_Up) {
        QMetaObject::invokeMethod(d->view, "decrementCurrentIndex");
    } else if (event->key() == Qt::Key_Down) {
        QMetaObject::invokeMethod(d->view, "incrementCurrentIndex");
    }
}

void QQuickTumbler::updatePolish()
{
    Q_D(QQuickTumbler);
    if (d->pendingCurrentIndex != -1) {
        // If the count is still 0, it's not going to happen.
        if (d->count == 0) {
            d->pendingCurrentIndex = -1;
            return;
        }

        // If there is a pending currentIndex at this stage, it means that
        // the view wouldn't set our currentIndex in _q_onViewCountChanged
        // because it wasn't ready. Try one last time here.
        setCurrentIndex(d->pendingCurrentIndex);

        if (d->currentIndex != d->pendingCurrentIndex && d->currentIndex == -1) {
            // If we *still* couldn't set it, it's probably invalid.
            // See if we can at least enforce our rule of "non-negative currentIndex when count > 0" instead.
            setCurrentIndex(0);
        }

        d->pendingCurrentIndex = -1;
    }
}

class QQuickTumblerAttachedPrivate : public QObjectPrivate, public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickTumblerAttached)
public:
    QQuickTumblerAttachedPrivate() :
        tumbler(nullptr),
        index(-1),
        displacement(0)
    {
    }

    void init(QQuickItem *delegateItem)
    {
        if (!delegateItem->parentItem()) {
            qWarning() << "Tumbler: attached properties must be accessed through a delegate item that has a parent";
            return;
        }

        QVariant indexContextProperty = qmlContext(delegateItem)->contextProperty(QStringLiteral("index"));
        if (!indexContextProperty.isValid()) {
            qWarning() << "Tumbler: attempting to access attached property on item without an \"index\" property";
            return;
        }

        index = indexContextProperty.toInt();

        QQuickItem *parentItem = delegateItem;
        while ((parentItem = parentItem->parentItem())) {
            if ((tumbler = qobject_cast<QQuickTumbler*>(parentItem)))
                break;
        }
    }

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &diff) override;
    void itemChildAdded(QQuickItem *, QQuickItem *) override;
    void itemChildRemoved(QQuickItem *, QQuickItem *) override;

    void _q_calculateDisplacement();
    void emitIfDisplacementChanged(qreal oldDisplacement, qreal newDisplacement);

    // The Tumbler that contains the delegate. Required to calculated the displacement.
    QPointer<QQuickTumbler> tumbler;
    // The index of the delegate. Used to calculate the displacement.
    int index;
    // The displacement for our delegate.
    qreal displacement;
};

void QQuickTumblerAttachedPrivate::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    _q_calculateDisplacement();
}

void QQuickTumblerAttachedPrivate::itemChildAdded(QQuickItem *, QQuickItem *)
{
    _q_calculateDisplacement();
}

void QQuickTumblerAttachedPrivate::itemChildRemoved(QQuickItem *item, QQuickItem *child)
{
    _q_calculateDisplacement();

    if (parent == child) {
        // The child that was removed from the contentItem was the delegate
        // that our properties are attached to. If we don't remove the change
        // listener, the contentItem will attempt to notify a destroyed
        // listener, causing a crash.

        // item is the "actual content item" of Tumbler's contentItem, i.e. a PathView or ListView.contentItem
        QQuickItemPrivate *p = QQuickItemPrivate::get(item);
        p->removeItemChangeListener(this, QQuickItemPrivate::Geometry | QQuickItemPrivate::Children);
    }
}

void QQuickTumblerAttachedPrivate::_q_calculateDisplacement()
{
    const int previousDisplacement = displacement;
    displacement = 0;

    // Can happen if the attached properties are accessed on the wrong type of item or the tumbler was destroyed.
    if (!tumbler) {
        emitIfDisplacementChanged(previousDisplacement, displacement);
        return;
    }

    // Can happen if there is no ListView or PathView within the contentItem.
    QQuickTumblerPrivate *tumblerPrivate = QQuickTumblerPrivate::get(tumbler);
    if (!tumblerPrivate->viewContentItem) {
        emitIfDisplacementChanged(previousDisplacement, displacement);
        return;
    }

    // The attached property gets created before our count is updated, so just cheat here
    // to avoid having to listen to count changes.
    const int count = tumblerPrivate->view->property("count").toInt();
    // This can happen in tests, so it may happen in normal usage too.
    if (count == 0) {
        emitIfDisplacementChanged(previousDisplacement, displacement);
        return;
    }

    if (tumblerPrivate->viewContentItemType == QQuickTumblerPrivate::PathViewContentItem) {
        const qreal offset = tumblerPrivate->view->property("offset").toReal();

        displacement = count > 1 ? count - index - offset : 0;
        // Don't add 1 if count <= visibleItemCount
        const int visibleItems = tumbler->visibleItemCount();
        const int halfVisibleItems = visibleItems / 2 + (visibleItems < count ? 1 : 0);
        if (displacement > halfVisibleItems)
            displacement -= count;
        else if (displacement < -halfVisibleItems)
            displacement += count;
    } else {
        const qreal contentY = tumblerPrivate->view->property("contentY").toReal();
        const qreal delegateH = delegateHeight(tumbler);
        const qreal preferredHighlightBegin = tumblerPrivate->view->property("preferredHighlightBegin").toReal();
        // Tumbler's displacement goes from negative at the top to positive towards the bottom, so we must switch this around.
        const qreal reverseDisplacement = (contentY + preferredHighlightBegin) / delegateH;
        displacement = reverseDisplacement - index;
    }

    emitIfDisplacementChanged(previousDisplacement, displacement);
}

void QQuickTumblerAttachedPrivate::emitIfDisplacementChanged(qreal oldDisplacement, qreal newDisplacement)
{
    Q_Q(QQuickTumblerAttached);
    if (newDisplacement != oldDisplacement)
        emit q->displacementChanged();
}

QQuickTumblerAttached::QQuickTumblerAttached(QObject *parent) :
    QObject(*(new QQuickTumblerAttachedPrivate), parent)
{
    Q_D(QQuickTumblerAttached);
    QQuickItem *delegateItem = qobject_cast<QQuickItem *>(parent);
    if (delegateItem)
        d->init(delegateItem);
    else if (parent)
        qmlInfo(parent) << "Tumbler: attached properties of Tumbler must be accessed through a delegate item";

    if (d->tumbler) {
        // When the Tumbler is completed, wrapChanged() is emitted to let QQuickTumblerView
        // know that it can create the view. The view itself might instantiate delegates
        // that use attached properties. At this point, setupViewData() hasn't been called yet
        // (it's called on the next line in componentComplete()), so we call it here so that
        // we have access to the view.
        QQuickTumblerPrivate *tumblerPrivate = QQuickTumblerPrivate::get(d->tumbler);
        tumblerPrivate->setupViewData(tumblerPrivate->contentItem);

        if (!tumblerPrivate->viewContentItem)
            return;

        QQuickItemPrivate *p = QQuickItemPrivate::get(tumblerPrivate->viewContentItem);
        p->addItemChangeListener(d, QQuickItemPrivate::Geometry | QQuickItemPrivate::Children);

        const char *contentItemSignal = tumblerPrivate->viewContentItemType == QQuickTumblerPrivate::PathViewContentItem
            ? SIGNAL(offsetChanged()) : SIGNAL(contentYChanged());
        connect(tumblerPrivate->view, contentItemSignal, this, SLOT(_q_calculateDisplacement()));

        d->_q_calculateDisplacement();
    }
}

QQuickTumblerAttached::~QQuickTumblerAttached()
{
    Q_D(QQuickTumblerAttached);
    if (!d->tumbler)
        return;

    QQuickTumblerPrivate *tumblerPrivate = QQuickTumblerPrivate::get(d->tumbler);
    if (!tumblerPrivate->viewContentItem)
        return;

    QQuickItemPrivate *viewContentItemPrivate = QQuickItemPrivate::get(tumblerPrivate->viewContentItem);
    viewContentItemPrivate->removeItemChangeListener(d, QQuickItemPrivate::Geometry | QQuickItemPrivate::Children);
}

/*!
    \qmlattachedproperty Tumbler QtQuick.Controls::Tumbler::tumbler
    \readonly

    This attached property holds the tumbler. The property can be attached to
    a tumbler delegate. The value is \c null if the item is not a tumbler delegate.
*/
QQuickTumbler *QQuickTumblerAttached::tumbler() const
{
    Q_D(const QQuickTumblerAttached);
    return d->tumbler;
}

/*!
    \qmlattachedproperty real QtQuick.Controls::Tumbler::displacement
    \readonly

    This attached property holds a value from \c {-visibleItemCount / 2} to
    \c {visibleItemCount / 2}, which represents how far away this item is from
    being the current item, with \c 0 being completely current.

    For example, the item below will be 40% opaque when it is not the current item,
    and transition to 100% opacity when it becomes the current item:

    \code
    delegate: Text {
        text: modelData
        opacity: 0.4 + Math.max(0, 1 - Math.abs(Tumbler.displacement)) * 0.6
    }
    \endcode
*/
qreal QQuickTumblerAttached::displacement() const
{
    Q_D(const QQuickTumblerAttached);
    return d->displacement;
}

QT_END_NAMESPACE

#include "moc_qquicktumbler_p.cpp"
