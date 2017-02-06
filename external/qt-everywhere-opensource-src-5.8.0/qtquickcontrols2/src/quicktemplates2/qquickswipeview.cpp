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

#include "qquickswipeview_p.h"

#include <QtQml/qqmlinfo.h>
#include <QtQuickTemplates2/private/qquickcontainer_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype SwipeView
    \inherits Container
    \instantiates QQuickSwipeView
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-navigation
    \ingroup qtquickcontrols2-containers
    \brief Enables the user to navigate pages by swiping sideways.

    SwipeView provides a swipe-based navigation model.

    \image qtquickcontrols2-swipeview.gif

    SwipeView is populated with a set of pages. One page is visible at a time.
    The user can navigate between the pages by swiping sideways. Notice that
    SwipeView itself is entirely non-visual. It is recommended to combine it
    with PageIndicator, to give the user a visual clue that there are multiple
    pages.

    \snippet qtquickcontrols2-swipeview-indicator.qml 1

    As shown above, SwipeView is typically populated with a static set of
    pages that are defined inline as children of the view. It is also possible
    to \l {Container::addItem()}{add}, \l {Container::insertItem()}{insert},
    \l {Container::moveItem()}{move}, and \l {Container::removeItem()}{remove}
    pages dynamically at run time.

    It is generally not advisable to add excessive amounts of pages to a
    SwipeView. However, when the amount of pages grows larger, or individual
    pages are relatively complex, it may be desired free up resources by
    unloading pages that are outside the reach. The following example presents
    how to use \l Loader to keep a maximum of three pages simultaneously
    instantiated.

    \code
    SwipeView {
        Repeater {
            model: 6
            Loader {
                active: SwipeView.isCurrentItem || SwipeView.isNextItem || SwipeView.isPreviousItem
                sourceComponent: Text {
                    text: index
                    Component.onCompleted: console.log("created:", index)
                    Component.onDestruction: console.log("destroyed:", index)
                }
            }
        }
    }
    \endcode

    \note SwipeView takes over the geometry management of items added to the
          view. Using anchors on the items is not supported, and any \c width
          or \c height assignment will be overridden by the view. Notice that
          this only applies to the root of the item. Specifying width and height,
          or using anchors for its children works as expected.

    \sa TabBar, PageIndicator, {Customizing SwipeView}, {Navigation Controls}, {Container Controls}
*/

class QQuickSwipeViewPrivate : public QQuickContainerPrivate
{
    Q_DECLARE_PUBLIC(QQuickSwipeView)

public:
    QQuickSwipeViewPrivate() : interactive(true) { }

    void resizeItem(QQuickItem *item);
    void resizeItems();

    static QQuickSwipeViewPrivate *get(QQuickSwipeView *view);

    bool interactive;
};

void QQuickSwipeViewPrivate::resizeItems()
{
    Q_Q(QQuickSwipeView);
    const int count = q->count();
    for (int i = 0; i < count; ++i) {
        QQuickItem *item = itemAt(i);
        if (item) {
            QQuickAnchors *anchors = QQuickItemPrivate::get(item)->_anchors;
            // TODO: expose QQuickAnchorLine so we can test for other conflicting anchors
            if (anchors && (anchors->fill() || anchors->centerIn()) && !item->property("_q_QQuickSwipeView_warned").toBool()) {
                qmlInfo(item) << "SwipeView has detected conflicting anchors. Unable to layout the item.";
                item->setProperty("_q_QQuickSwipeView_warned", true);
            }

            item->setSize(QSizeF(contentItem->width(), contentItem->height()));
        }
    }
}

QQuickSwipeViewPrivate *QQuickSwipeViewPrivate::get(QQuickSwipeView *view)
{
    return view->d_func();
}

QQuickSwipeView::QQuickSwipeView(QQuickItem *parent) :
    QQuickContainer(*(new QQuickSwipeViewPrivate), parent)
{
    setFlag(ItemIsFocusScope);
    setActiveFocusOnTab(true);
}

/*!
    \since QtQuick.Controls 2.1
    \qmlproperty bool QtQuick.Controls::SwipeView::interactive

    This property describes whether the user can interact with the SwipeView.
    The user cannot swipe a view that is not interactive.

    The default value is \c true.
*/
bool QQuickSwipeView::isInteractive() const
{
    Q_D(const QQuickSwipeView);
    return d->interactive;
}

void QQuickSwipeView::setInteractive(bool interactive)
{
    Q_D(QQuickSwipeView);
    if (d->interactive == interactive)
        return;

    d->interactive = interactive;
    emit interactiveChanged();
}

QQuickSwipeViewAttached *QQuickSwipeView::qmlAttachedProperties(QObject *object)
{
    return new QQuickSwipeViewAttached(object);
}

void QQuickSwipeView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickSwipeView);
    QQuickContainer::geometryChanged(newGeometry, oldGeometry);
    d->resizeItems();
}

void QQuickSwipeView::itemAdded(int, QQuickItem *item)
{
    Q_D(QQuickSwipeView);
    QQuickItemPrivate::get(item)->setCulled(true); // QTBUG-51078, QTBUG-51669
    if (isComponentComplete())
        item->setSize(QSizeF(d->contentItem->width(), d->contentItem->height()));
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickSwipeView::accessibleRole() const
{
    return QAccessible::PageTabList;
}
#endif

/*!
    \qmlattachedproperty int QtQuick.Controls::SwipeView::index
    \readonly

    This attached property holds the index of each child item in the SwipeView.

    It is attached to each child item of the SwipeView.
*/

/*!
    \qmlattachedproperty bool QtQuick.Controls::SwipeView::isCurrentItem
    \readonly

    This attached property is \c true if this child is the current item.

    It is attached to each child item of the SwipeView.
*/

/*!
    \qmlattachedproperty bool QtQuick.Controls::SwipeView::isNextItem
    \since QtQuick.Controls 2.1
    \readonly

    This attached property is \c true if this child is the next item.

    It is attached to each child item of the SwipeView.
*/

/*!
    \qmlattachedproperty bool QtQuick.Controls::SwipeView::isPreviousItem
    \since QtQuick.Controls 2.1
    \readonly

    This attached property is \c true if this child is the previous item.

    It is attached to each child item of the SwipeView.
*/

/*!
    \qmlattachedproperty SwipeView QtQuick.Controls::SwipeView::view
    \readonly

    This attached property holds the view that manages this child item.

    It is attached to each child item of the SwipeView.
*/

class QQuickSwipeViewAttachedPrivate : public QObjectPrivate, public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickSwipeViewAttached)
public:
    QQuickSwipeViewAttachedPrivate() :
        item(nullptr),
        swipeView(nullptr),
        index(-1),
        currentIndex(-1)
    {
    }

    ~QQuickSwipeViewAttachedPrivate() {
    }

    void updateView(QQuickItem *parent);

    void itemChildAdded(QQuickItem *, QQuickItem *) override;
    void itemChildRemoved(QQuickItem *, QQuickItem *) override;
    void itemParentChanged(QQuickItem *, QQuickItem *) override;
    void itemDestroyed(QQuickItem *) override;

    void updateIndex();
    void updateCurrentIndex();

    void setView(QQuickSwipeView *view);
    void setIndex(int i);
    void setCurrentIndex(int i);

    QQuickItem *item;
    QQuickSwipeView *swipeView;
    int index;
    int currentIndex;
};

void QQuickSwipeViewAttachedPrivate::updateIndex()
{
    setIndex(swipeView ? QQuickSwipeViewPrivate::get(swipeView)->contentModel->indexOf(item, nullptr) : -1);
}

void QQuickSwipeViewAttachedPrivate::updateCurrentIndex()
{
    setCurrentIndex(swipeView ? swipeView->currentIndex() : -1);
}

void QQuickSwipeViewAttachedPrivate::setView(QQuickSwipeView *view)
{
    if (view == swipeView)
        return;

    if (swipeView) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(swipeView);
        p->removeItemChangeListener(this, QQuickItemPrivate::Children);

        disconnect(swipeView, &QQuickSwipeView::currentIndexChanged,
            this, &QQuickSwipeViewAttachedPrivate::updateCurrentIndex);
        disconnect(swipeView, &QQuickSwipeView::contentChildrenChanged,
            this, &QQuickSwipeViewAttachedPrivate::updateIndex);
    }

    swipeView = view;

    if (swipeView) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(swipeView);
        p->addItemChangeListener(this, QQuickItemPrivate::Children);

        connect(swipeView, &QQuickSwipeView::currentIndexChanged,
            this, &QQuickSwipeViewAttachedPrivate::updateCurrentIndex);
        connect(swipeView, &QQuickSwipeView::contentChildrenChanged,
            this, &QQuickSwipeViewAttachedPrivate::updateIndex);
    }

    Q_Q(QQuickSwipeViewAttached);
    emit q->viewChanged();

    updateIndex();
    updateCurrentIndex();
}

void QQuickSwipeViewAttachedPrivate::setCurrentIndex(int i)
{
    if (i == currentIndex)
        return;

    Q_Q(QQuickSwipeViewAttached);
    const bool wasCurrent = q->isCurrentItem();
    const bool wasNext = q->isNextItem();
    const bool wasPrevious = q->isPreviousItem();

    currentIndex = i;
    if (wasCurrent != q->isCurrentItem())
        emit q->isCurrentItemChanged();
    if (wasNext != q->isNextItem())
        emit q->isNextItemChanged();
    if (wasPrevious != q->isPreviousItem())
        emit q->isPreviousItemChanged();
}

void QQuickSwipeViewAttachedPrivate::setIndex(int i)
{
    if (i == index)
        return;

    index = i;
    Q_Q(QQuickSwipeViewAttached);
    emit q->indexChanged();
}

void QQuickSwipeViewAttachedPrivate::updateView(QQuickItem *parent)
{
    // parent can be, e.g.:
    // - The contentItem of a ListView (typically the case)
    // - A non-visual or weird type like TestCase, when child items are created from components
    //   wherein the attached properties are used
    // - null, when the item was removed with removeItem()
    QQuickSwipeView *view = nullptr;
    if (parent) {
        view = qobject_cast<QQuickSwipeView*>(parent);
        if (!view) {
            if (parent->parentItem() && parent->parentItem()->property("contentItem").isValid()) {
                // The parent is the contentItem of some kind of view.
                view = qobject_cast<QQuickSwipeView*>(parent->parentItem()->parentItem());
            }
        }
    }

    setView(view);
}

void QQuickSwipeViewAttachedPrivate::itemChildAdded(QQuickItem *, QQuickItem *)
{
    updateIndex();
}

void QQuickSwipeViewAttachedPrivate::itemChildRemoved(QQuickItem *, QQuickItem *)
{
    updateIndex();
}

void QQuickSwipeViewAttachedPrivate::itemParentChanged(QQuickItem *, QQuickItem *parent)
{
    updateView(parent);
}

void QQuickSwipeViewAttachedPrivate::itemDestroyed(QQuickItem *item)
{
    QQuickItemPrivate::get(item)->removeItemChangeListener(this, QQuickItemPrivate::Parent | QQuickItemPrivate::Destroyed);
}

QQuickSwipeViewAttached::QQuickSwipeViewAttached(QObject *parent) :
    QObject(*(new QQuickSwipeViewAttachedPrivate), parent)
{
    Q_D(QQuickSwipeViewAttached);
    d->item = qobject_cast<QQuickItem *>(parent);
    if (d->item) {
        if (d->item->parentItem())
            d->updateView(d->item->parentItem());

        QQuickItemPrivate *p = QQuickItemPrivate::get(d->item);
        p->addItemChangeListener(d, QQuickItemPrivate::Parent | QQuickItemPrivate::Destroyed);
    } else if (parent) {
        qmlInfo(parent) << "SwipeView: attached properties must be accessed from within a child item";
    }
}

QQuickSwipeViewAttached::~QQuickSwipeViewAttached()
{
    Q_D(QQuickSwipeViewAttached);
    QQuickItem *item = qobject_cast<QQuickItem *>(parent());
    if (item)
        QQuickItemPrivate::get(item)->removeItemChangeListener(d, QQuickItemPrivate::Parent | QQuickItemPrivate::Destroyed);
}

QQuickSwipeView *QQuickSwipeViewAttached::view() const
{
    Q_D(const QQuickSwipeViewAttached);
    return d->swipeView;
}

int QQuickSwipeViewAttached::index() const
{
    Q_D(const QQuickSwipeViewAttached);
    return d->index;
}

bool QQuickSwipeViewAttached::isCurrentItem() const
{
    Q_D(const QQuickSwipeViewAttached);
    return d->index != -1 && d->currentIndex != -1 && d->index == d->currentIndex;
}

bool QQuickSwipeViewAttached::isNextItem() const
{
    Q_D(const QQuickSwipeViewAttached);
    return d->index != -1 && d->currentIndex != -1 && d->index == d->currentIndex + 1;
}

bool QQuickSwipeViewAttached::isPreviousItem() const
{
    Q_D(const QQuickSwipeViewAttached);
    return d->index != -1 && d->currentIndex != -1 && d->index == d->currentIndex - 1;
}

QT_END_NAMESPACE
