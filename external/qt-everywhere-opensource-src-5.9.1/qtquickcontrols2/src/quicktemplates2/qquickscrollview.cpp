/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qquickscrollview_p.h"
#include "qquickcontrol_p_p.h"
#include "qquickscrollbar_p_p.h"

#include <QtQuick/private/qquickflickable_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ScrollView
    \inherits Control
    \instantiates QQuickScrollView
    \inqmlmodule QtQuick.Controls
    \since 5.9
    \ingroup qtquickcontrols2-containers
    \brief Scrollable view.

    ScrollView provides scrolling for user-defined content. It can be used to
    either replace a \l Flickable, or to decorate an existing one.

    \image qtquickcontrols2-scrollview.png

    The first example demonstrates the simplest usage of ScrollView.

    \snippet qtquickcontrols2-scrollview.qml file

    \note ScrollView does not automatically clip its contents. If it is not used as
    a full-screen item, you should consider setting the \l {Item::}{clip} property
    to \c true, as shown above.

    The second example illustrates using an existing \l Flickable, that is,
    a \l ListView.

    \snippet qtquickcontrols2-scrollview-listview.qml file

    \section2 Scroll Bars

    The horizontal and vertical scroll bars can be accessed and customized using
    the \l {ScrollBar::horizontal}{ScrollBar.horizontal} and \l {ScrollBar::vertical}
    {ScrollBar.vertical} attached properties. The following example adjusts the scroll
    bar policies so that the horizontal scroll bar is always off, and the vertical
    scroll bar is always on.

    \snippet qtquickcontrols2-scrollview-policy.qml file

    \section2 Touch vs. Mouse Interaction

    On touch, ScrollView enables flicking and makes the scroll bars non-interactive.

    \image qtquickcontrols2-scrollindicator.gif

    When interacted with a mouse device, flicking is disabled and the scroll bars
    are interactive.

    \image qtquickcontrols2-scrollbar.gif

    Scroll bars can be made interactive on touch, or non-interactive when interacted
    with a mouse device, by setting the \l {ScrollBar::}{interactive} property explicitly
    to \c true or \c false, respectively.

    \snippet qtquickcontrols2-scrollview-interactive.qml file

    \sa ScrollBar, ScrollIndicator, {Customizing ScrollView}, {Container Controls},
*/

class QQuickScrollViewPrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickScrollView)

public:
    QQuickScrollViewPrivate();

    QQuickItem *getContentItem() override;

    QQuickFlickable *ensureFlickable(bool content);
    bool setFlickable(QQuickFlickable *flickable, bool content);

    void updateContentWidth();
    void updateContentHeight();

    QQuickScrollBar *verticalScrollBar() const;
    QQuickScrollBar *horizontalScrollBar() const;

    void setScrollBarsInteractive(bool interactive);

    static void contentData_append(QQmlListProperty<QObject> *prop, QObject *obj);
    static int contentData_count(QQmlListProperty<QObject> *prop);
    static QObject *contentData_at(QQmlListProperty<QObject> *prop, int index);
    static void contentData_clear(QQmlListProperty<QObject> *prop);

    static void contentChildren_append(QQmlListProperty<QQuickItem> *prop, QQuickItem *obj);
    static int contentChildren_count(QQmlListProperty<QQuickItem> *prop);
    static QQuickItem *contentChildren_at(QQmlListProperty<QQuickItem> *prop, int index);
    static void contentChildren_clear(QQmlListProperty<QQuickItem> *prop);

    bool wasTouched;
    qreal contentWidth;
    qreal contentHeight;
    QQuickFlickable *flickable;
};

QQuickScrollViewPrivate::QQuickScrollViewPrivate()
    : wasTouched(false),
      contentWidth(-1),
      contentHeight(-1),
      flickable(nullptr)
{
    wheelEnabled = true;
}

QQuickItem *QQuickScrollViewPrivate::getContentItem()
{
    return ensureFlickable(false);
}

QQuickFlickable *QQuickScrollViewPrivate::ensureFlickable(bool content)
{
    Q_Q(QQuickScrollView);
    if (!flickable)
        setFlickable(new QQuickFlickable(q), content);
    return flickable;
}

bool QQuickScrollViewPrivate::setFlickable(QQuickFlickable *item, bool content)
{
    Q_Q(QQuickScrollView);
    if (item == flickable)
        return false;

    QQuickScrollBarAttached *attached = qobject_cast<QQuickScrollBarAttached *>(qmlAttachedPropertiesObject<QQuickScrollBar>(q, false));

    if (flickable) {
        flickable->removeEventFilter(q);

        if (attached)
            QQuickScrollBarAttachedPrivate::get(attached)->setFlickable(nullptr);

        QObject::disconnect(flickable->contentItem(), &QQuickItem::childrenChanged, q, &QQuickScrollView::contentChildrenChanged);
        QObjectPrivate::disconnect(flickable, &QQuickFlickable::contentWidthChanged, this, &QQuickScrollViewPrivate::updateContentWidth);
        QObjectPrivate::disconnect(flickable, &QQuickFlickable::contentHeightChanged, this, &QQuickScrollViewPrivate::updateContentHeight);
    }

    flickable = item;
    if (content)
        q->setContentItem(flickable);

    if (flickable) {
        flickable->installEventFilter(q);
        if (contentWidth > 0)
            item->setContentWidth(contentWidth);
        else
            updateContentWidth();
        if (contentHeight > 0)
            item->setContentHeight(contentHeight);
        else
            updateContentHeight();

        if (attached)
            QQuickScrollBarAttachedPrivate::get(attached)->setFlickable(flickable);

        QObject::connect(flickable->contentItem(), &QQuickItem::childrenChanged, q, &QQuickScrollView::contentChildrenChanged);
        QObjectPrivate::connect(flickable, &QQuickFlickable::contentWidthChanged, this, &QQuickScrollViewPrivate::updateContentWidth);
        QObjectPrivate::connect(flickable, &QQuickFlickable::contentHeightChanged, this, &QQuickScrollViewPrivate::updateContentHeight);
    }

    return true;
}

void QQuickScrollViewPrivate::updateContentWidth()
{
    Q_Q(QQuickScrollView);
    if (!flickable)
        return;

    const qreal cw = flickable->contentWidth();
    if (qFuzzyCompare(cw, contentWidth))
        return;

    contentWidth = cw;
    emit q->contentWidthChanged();
}

void QQuickScrollViewPrivate::updateContentHeight()
{
    Q_Q(QQuickScrollView);
    if (!flickable)
        return;

    const qreal ch = flickable->contentHeight();
    if (qFuzzyCompare(ch, contentHeight))
        return;

    contentHeight = ch;
    emit q->contentHeightChanged();
}

QQuickScrollBar *QQuickScrollViewPrivate::verticalScrollBar() const
{
    Q_Q(const QQuickScrollView);
    QQuickScrollBarAttached *attached = qobject_cast<QQuickScrollBarAttached *>(qmlAttachedPropertiesObject<QQuickScrollBar>(q, false));
    if (!attached)
        return nullptr;
    return attached->vertical();
}

QQuickScrollBar *QQuickScrollViewPrivate::horizontalScrollBar() const
{
    Q_Q(const QQuickScrollView);
    QQuickScrollBarAttached *attached = qobject_cast<QQuickScrollBarAttached *>(qmlAttachedPropertiesObject<QQuickScrollBar>(q, false));
    if (!attached)
        return nullptr;
    return attached->horizontal();
}

void QQuickScrollViewPrivate::setScrollBarsInteractive(bool interactive)
{
    QQuickScrollBar *hbar = horizontalScrollBar();
    if (hbar) {
        QQuickScrollBarPrivate *p = QQuickScrollBarPrivate::get(hbar);
        if (!p->explicitInteractive)
            p->setInteractive(interactive);
    }

    QQuickScrollBar *vbar = verticalScrollBar();
    if (vbar) {
        QQuickScrollBarPrivate *p = QQuickScrollBarPrivate::get(vbar);
        if (!p->explicitInteractive)
            p->setInteractive(interactive);
    }
}

void QQuickScrollViewPrivate::contentData_append(QQmlListProperty<QObject> *prop, QObject *obj)
{
    QQuickScrollViewPrivate *p = static_cast<QQuickScrollViewPrivate *>(prop->data);
    if (!p->flickable && p->setFlickable(qobject_cast<QQuickFlickable *>(obj), true))
        return;

    QQuickFlickable *flickable = p->ensureFlickable(true);
    Q_ASSERT(flickable);
    QQmlListProperty<QObject> data = flickable->flickableData();
    data.append(&data, obj);
}

int QQuickScrollViewPrivate::contentData_count(QQmlListProperty<QObject> *prop)
{
    QQuickScrollViewPrivate *p = static_cast<QQuickScrollViewPrivate *>(prop->data);
    if (!p->flickable)
        return 0;

    QQmlListProperty<QObject> data = p->flickable->flickableData();
    return data.count(&data);
}

QObject *QQuickScrollViewPrivate::contentData_at(QQmlListProperty<QObject> *prop, int index)
{
    QQuickScrollViewPrivate *p = static_cast<QQuickScrollViewPrivate *>(prop->data);
    if (!p->flickable)
        return nullptr;

    QQmlListProperty<QObject> data = p->flickable->flickableData();
    return data.at(&data, index);
}

void QQuickScrollViewPrivate::contentData_clear(QQmlListProperty<QObject> *prop)
{
    QQuickScrollViewPrivate *p = static_cast<QQuickScrollViewPrivate *>(prop->data);
    if (!p->flickable)
        return;

    QQmlListProperty<QObject> data = p->flickable->flickableData();
    return data.clear(&data);
}

void QQuickScrollViewPrivate::contentChildren_append(QQmlListProperty<QQuickItem> *prop, QQuickItem *item)
{
    QQuickScrollViewPrivate *p = static_cast<QQuickScrollViewPrivate *>(prop->data);
    if (!p->flickable)
        p->setFlickable(qobject_cast<QQuickFlickable *>(item), true);

    QQuickFlickable *flickable = p->ensureFlickable(true);
    Q_ASSERT(flickable);
    QQmlListProperty<QQuickItem> children = flickable->flickableChildren();
    children.append(&children, item);
}

int QQuickScrollViewPrivate::contentChildren_count(QQmlListProperty<QQuickItem> *prop)
{
    QQuickScrollViewPrivate *p = static_cast<QQuickScrollViewPrivate *>(prop->data);
    if (!p->flickable)
        return 0;

    QQmlListProperty<QQuickItem> children = p->flickable->flickableChildren();
    return children.count(&children);
}

QQuickItem *QQuickScrollViewPrivate::contentChildren_at(QQmlListProperty<QQuickItem> *prop, int index)
{
    QQuickScrollViewPrivate *p = static_cast<QQuickScrollViewPrivate *>(prop->data);
    if (!p->flickable)
        return nullptr;

    QQmlListProperty<QQuickItem> children = p->flickable->flickableChildren();
    return children.at(&children, index);
}

void QQuickScrollViewPrivate::contentChildren_clear(QQmlListProperty<QQuickItem> *prop)
{
    QQuickScrollViewPrivate *p = static_cast<QQuickScrollViewPrivate *>(prop->data);
    if (!p->flickable)
        return;

    QQmlListProperty<QQuickItem> children = p->flickable->flickableChildren();
    children.clear(&children);
}

QQuickScrollView::QQuickScrollView(QQuickItem *parent)
    : QQuickControl(*(new QQuickScrollViewPrivate), parent)
{
    setFlag(ItemIsFocusScope);
    setActiveFocusOnTab(true);
    setFiltersChildMouseEvents(true);
}

/*!
    \qmlproperty real QtQuick.Controls::ScrollView::contentWidth

    This property holds the width of the scrollable content.

    If only a single item is used within a ScrollView, the content size is
    automatically calculated based on the implicit size of its contained item.

    \sa contentHeight
*/
qreal QQuickScrollView::contentWidth() const
{
    Q_D(const QQuickScrollView);
    return d->contentWidth;
}

void QQuickScrollView::setContentWidth(qreal width)
{
    Q_D(QQuickScrollView);
    if (qFuzzyCompare(d->contentWidth, width))
        return;

    if (d->flickable) {
        d->flickable->setContentWidth(width);
    } else {
        d->contentWidth = width;
        emit contentWidthChanged();
    }
}

/*!
    \qmlproperty real QtQuick.Controls::ScrollView::contentHeight

    This property holds the height of the scrollable content.

    If only a single item is used within a ScrollView, the content size is
    automatically calculated based on the implicit size of its contained item.

    \sa contentWidth
*/
qreal QQuickScrollView::contentHeight() const
{
    Q_D(const QQuickScrollView);
    return d->contentHeight;
}

void QQuickScrollView::setContentHeight(qreal height)
{
    Q_D(QQuickScrollView);
    if (qFuzzyCompare(d->contentHeight, height))
        return;

    if (d->flickable) {
        d->flickable->setContentHeight(height);
    } else {
        d->contentHeight = height;
        emit contentHeightChanged();
    }
}

/*!
    \qmlproperty list<Object> QtQuick.Controls::ScrollView::contentData
    \default

    This property holds the list of content data.

    The list contains all objects that have been declared in QML as children of the view.

    \note Unlike \c contentChildren, \c contentData does include non-visual QML objects.

    \sa Item::data, contentChildren
*/
QQmlListProperty<QObject> QQuickScrollView::contentData()
{
    Q_D(QQuickScrollView);
    return QQmlListProperty<QObject>(this, d,
                                     QQuickScrollViewPrivate::contentData_append,
                                     QQuickScrollViewPrivate::contentData_count,
                                     QQuickScrollViewPrivate::contentData_at,
                                     QQuickScrollViewPrivate::contentData_clear);
}

/*!
    \qmlproperty list<Item> QtQuick.Controls::ScrollView::contentChildren

    This property holds the list of content children.

    The list contains all items that have been declared in QML as children of the view.

    \note Unlike \c contentData, \c contentChildren does not include non-visual QML objects.

    \sa Item::children, contentData
*/
QQmlListProperty<QQuickItem> QQuickScrollView::contentChildren()
{
    Q_D(QQuickScrollView);
    return QQmlListProperty<QQuickItem>(this, d,
                                        QQuickScrollViewPrivate::contentChildren_append,
                                        QQuickScrollViewPrivate::contentChildren_count,
                                        QQuickScrollViewPrivate::contentChildren_at,
                                        QQuickScrollViewPrivate::contentChildren_clear);
}

bool QQuickScrollView::childMouseEventFilter(QQuickItem *item, QEvent *event)
{
    Q_D(QQuickScrollView);
    switch (event->type()) {
    case QEvent::TouchBegin:
        d->wasTouched = true;
        d->setScrollBarsInteractive(false);
        return false;

    case QEvent::TouchEnd:
        d->wasTouched = false;
        return false;

    case QEvent::MouseButtonPress:
        // NOTE: Flickable does not handle touch events, only synthesized mouse events
        if (static_cast<QMouseEvent *>(event)->source() == Qt::MouseEventNotSynthesized) {
            d->wasTouched = false;
            d->setScrollBarsInteractive(true);
            return false;
        }
        return !d->wasTouched && item == d->flickable;

    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
        if (static_cast<QMouseEvent *>(event)->source() == Qt::MouseEventNotSynthesized)
            return item == d->flickable;
        break;

    case QEvent::HoverEnter:
    case QEvent::HoverMove:
        if (d->wasTouched && (item == d->verticalScrollBar() || item == d->horizontalScrollBar()))
            d->setScrollBarsInteractive(true);
        break;

    default:
        break;
    }

    return false;
}

bool QQuickScrollView::eventFilter(QObject *object, QEvent *event)
{
    Q_D(QQuickScrollView);
    if (event->type() == QEvent::Wheel) {
        d->setScrollBarsInteractive(true);
        if (!d->wheelEnabled)
            return true;
    }
    return QQuickControl::eventFilter(object, event);
}

void QQuickScrollView::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickScrollView);
    QQuickControl::keyPressEvent(event);
    switch (event->key()) {
    case Qt::Key_Up:
        if (QQuickScrollBar *vbar = d->verticalScrollBar()) {
            vbar->decrease();
            event->accept();
        }
        break;
    case Qt::Key_Down:
        if (QQuickScrollBar *vbar = d->verticalScrollBar()) {
            vbar->increase();
            event->accept();
        }
        break;
    case Qt::Key_Left:
        if (QQuickScrollBar *hbar = d->horizontalScrollBar()) {
            hbar->decrease();
            event->accept();
        }
        break;
    case Qt::Key_Right:
        if (QQuickScrollBar *hbar = d->horizontalScrollBar()) {
            hbar->increase();
            event->accept();
        }
        break;
    default:
        event->ignore();
        break;
    }
}

void QQuickScrollView::componentComplete()
{
    Q_D(QQuickScrollView);
    QQuickControl::componentComplete();
    if (!d->contentItem)
        d->ensureFlickable(true);
}

void QQuickScrollView::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_D(QQuickScrollView);
    QQuickControl::contentItemChange(newItem, oldItem);
    d->setFlickable(qobject_cast<QQuickFlickable *>(newItem), false);
}

#if QT_CONFIG(accessibility)
QAccessible::Role QQuickScrollView::accessibleRole() const
{
    return QAccessible::Pane;
}
#endif

QT_END_NAMESPACE
