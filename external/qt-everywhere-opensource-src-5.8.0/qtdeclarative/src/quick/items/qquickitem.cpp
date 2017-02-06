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

#include "qquickitem.h"

#include "qquickwindow.h"
#include "qquickrendercontrol.h"
#include <QtQml/qjsengine.h>
#include "qquickwindow_p.h"

#include "qquickevents_p_p.h"
#include "qquickscreen_p.h"

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlinfo.h>
#include <QtGui/qpen.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qstylehints.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qinputmethod.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/private/qnumeric_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtCore/qloggingcategory.h>

#include <private/qqmlglobal_p.h>
#include <private/qqmlengine_p.h>
#include <QtQuick/private/qquickstategroup_p.h>
#include <private/qqmlopenmetaobject_p.h>
#include <QtQuick/private/qquickstate_p.h>
#include <private/qquickitem_p.h>
#include <QtQuick/private/qquickaccessibleattached_p.h>

#include <private/qv4engine_p.h>
#include <private/qv4object_p.h>
#include <private/qv4qobjectwrapper_p.h>
#include <private/qdebug_p.h>

#ifndef QT_NO_CURSOR
# include <QtGui/qcursor.h>
#endif

#include <algorithm>
#include <limits>

// XXX todo Check that elements that create items handle memory correctly after visual ownership change

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(DBG_MOUSE_TARGET)
Q_DECLARE_LOGGING_CATEGORY(DBG_HOVER_TRACE)

void debugFocusTree(QQuickItem *item, QQuickItem *scope = 0, int depth = 1)
{
    if (DBG_FOCUS().isEnabled(QtDebugMsg)) {
        qCDebug(DBG_FOCUS)
                << QByteArray(depth, '\t').constData()
                << (scope && QQuickItemPrivate::get(scope)->subFocusItem == item ? '*' : ' ')
                << item->hasFocus()
                << item->hasActiveFocus()
                << item->isFocusScope()
                << item;
        const auto childItems = item->childItems();
        for (QQuickItem *child : childItems) {
            debugFocusTree(
                    child,
                    item->isFocusScope() || !scope ? item : scope,
                    item->isFocusScope() || !scope ? depth + 1 : depth);
        }
    }
}

/*!
    \qmltype Transform
    \instantiates QQuickTransform
    \inqmlmodule QtQuick
    \ingroup qtquick-visual-transforms
    \brief For specifying advanced transformations on Items

    The Transform type is a base type which cannot be instantiated directly.
    The following concrete Transform types are available:

    \list
    \li \l Rotation
    \li \l Scale
    \li \l Translate
    \endlist

    The Transform types let you create and control advanced transformations that can be configured
    independently using specialized properties.

    You can assign any number of Transforms to an \l Item. Each Transform is applied in order,
    one at a time.
*/
QQuickTransformPrivate::QQuickTransformPrivate()
{
}

QQuickTransform::QQuickTransform(QObject *parent)
: QObject(*(new QQuickTransformPrivate), parent)
{
}

QQuickTransform::QQuickTransform(QQuickTransformPrivate &dd, QObject *parent)
: QObject(dd, parent)
{
}

QQuickTransform::~QQuickTransform()
{
    Q_D(QQuickTransform);
    for (QQuickItem *item : qAsConst(d->items)) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(item);
        p->transforms.removeOne(this);
        p->dirty(QQuickItemPrivate::Transform);
    }
}

void QQuickTransform::update()
{
    Q_D(QQuickTransform);
    for (QQuickItem *item : qAsConst(d->items)) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(item);
        p->dirty(QQuickItemPrivate::Transform);
    }
}

QQuickContents::QQuickContents(QQuickItem *item)
: m_item(item), m_x(0), m_y(0), m_width(0), m_height(0)
{
}

QQuickContents::~QQuickContents()
{
    for (QQuickItem *child : m_item->childItems()) {
        QQuickItemPrivate::get(child)->removeItemChangeListener(this, QQuickItemPrivate::Geometry | QQuickItemPrivate::Destroyed);
    }
}

bool QQuickContents::calcHeight(QQuickItem *changed)
{
    qreal oldy = m_y;
    qreal oldheight = m_height;

    if (changed) {
        qreal top = oldy;
        qreal bottom = oldy + oldheight;
        qreal y = changed->y();
        if (y + changed->height() > bottom)
            bottom = y + changed->height();
        if (y < top)
            top = y;
        m_y = top;
        m_height = bottom - top;
    } else {
        qreal top = std::numeric_limits<qreal>::max();
        qreal bottom = -std::numeric_limits<qreal>::max();
        const QList<QQuickItem*> children = m_item->childItems();
        for (QQuickItem *child : qAsConst(children)) {
            qreal y = child->y();
            if (y + child->height() > bottom)
                bottom = y + child->height();
            if (y < top)
                top = y;
        }
        if (!children.isEmpty())
            m_y = top;
        m_height = qMax(bottom - top, qreal(0.0));
    }

    return (m_height != oldheight || m_y != oldy);
}

bool QQuickContents::calcWidth(QQuickItem *changed)
{
    qreal oldx = m_x;
    qreal oldwidth = m_width;

    if (changed) {
        qreal left = oldx;
        qreal right = oldx + oldwidth;
        qreal x = changed->x();
        if (x + changed->width() > right)
            right = x + changed->width();
        if (x < left)
            left = x;
        m_x = left;
        m_width = right - left;
    } else {
        qreal left = std::numeric_limits<qreal>::max();
        qreal right = -std::numeric_limits<qreal>::max();
        const QList<QQuickItem*> children = m_item->childItems();
        for (QQuickItem *child : qAsConst(children)) {
            qreal x = child->x();
            if (x + child->width() > right)
                right = x + child->width();
            if (x < left)
                left = x;
        }
        if (!children.isEmpty())
            m_x = left;
        m_width = qMax(right - left, qreal(0.0));
    }

    return (m_width != oldwidth || m_x != oldx);
}

void QQuickContents::complete()
{
    QQuickItemPrivate::get(m_item)->addItemChangeListener(this, QQuickItemPrivate::Children);

    for (QQuickItem *child : m_item->childItems()) {
        QQuickItemPrivate::get(child)->addItemChangeListener(this, QQuickItemPrivate::Geometry | QQuickItemPrivate::Destroyed);
        //###what about changes to visibility?
    }
    calcGeometry();
}

void QQuickContents::updateRect()
{
    QQuickItemPrivate::get(m_item)->emitChildrenRectChanged(rectF());
}

void QQuickContents::itemGeometryChanged(QQuickItem *changed, QQuickGeometryChange change, const QRectF &)
{
    Q_UNUSED(changed)
    bool wChanged = false;
    bool hChanged = false;
    //### we can only pass changed if the left edge has moved left, or the right edge has moved right
    if (change.horizontalChange())
        wChanged = calcWidth(/*changed*/);
    if (change.verticalChange())
        hChanged = calcHeight(/*changed*/);
    if (wChanged || hChanged)
        updateRect();
}

void QQuickContents::itemDestroyed(QQuickItem *item)
{
    if (item)
        QQuickItemPrivate::get(item)->removeItemChangeListener(this, QQuickItemPrivate::Geometry | QQuickItemPrivate::Destroyed);
    calcGeometry();
}

void QQuickContents::itemChildRemoved(QQuickItem *, QQuickItem *item)
{
    if (item)
        QQuickItemPrivate::get(item)->removeItemChangeListener(this, QQuickItemPrivate::Geometry | QQuickItemPrivate::Destroyed);
    calcGeometry();
}

void QQuickContents::itemChildAdded(QQuickItem *, QQuickItem *item)
{
    if (item)
        QQuickItemPrivate::get(item)->addItemChangeListener(this, QQuickItemPrivate::Geometry | QQuickItemPrivate::Destroyed);
    calcGeometry(item);
}

QQuickItemKeyFilter::QQuickItemKeyFilter(QQuickItem *item)
: m_processPost(false), m_next(0)
{
    QQuickItemPrivate *p = item?QQuickItemPrivate::get(item):0;
    if (p) {
        m_next = p->extra.value().keyHandler;
        p->extra->keyHandler = this;
    }
}

QQuickItemKeyFilter::~QQuickItemKeyFilter()
{
}

void QQuickItemKeyFilter::keyPressed(QKeyEvent *event, bool post)
{
    if (m_next) m_next->keyPressed(event, post);
}

void QQuickItemKeyFilter::keyReleased(QKeyEvent *event, bool post)
{
    if (m_next) m_next->keyReleased(event, post);
}

#ifndef QT_NO_IM
void QQuickItemKeyFilter::inputMethodEvent(QInputMethodEvent *event, bool post)
{
    if (m_next)
        m_next->inputMethodEvent(event, post);
    else
        event->ignore();
}

QVariant QQuickItemKeyFilter::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (m_next) return m_next->inputMethodQuery(query);
    return QVariant();
}
#endif // QT_NO_IM

void QQuickItemKeyFilter::componentComplete()
{
    if (m_next) m_next->componentComplete();
}
/*!
    \qmltype KeyNavigation
    \instantiates QQuickKeyNavigationAttached
    \inqmlmodule QtQuick
    \ingroup qtquick-input
    \brief Supports key navigation by arrow keys

    Key-based user interfaces commonly allow the use of arrow keys to navigate between
    focusable items.  The KeyNavigation attached property enables this behavior by providing a
    convenient way to specify the item that should gain focus when an arrow or tab key is pressed.

    The following example provides key navigation for a 2x2 grid of items:

    \snippet qml/keynavigation.qml 0

    The top-left item initially receives focus by setting \l {Item::}{focus} to
    \c true. When an arrow key is pressed, the focus will move to the
    appropriate item, as defined by the value that has been set for
    the KeyNavigation \l left, \l right, \l up or \l down properties.

    Note that if a KeyNavigation attached property receives the key press and release
    events for a requested arrow or tab key, the event is accepted and does not
    propagate any further.

    By default, KeyNavigation receives key events after the item to which it is attached.
    If the item accepts the key event, the KeyNavigation attached property will not
    receive an event for that key.  Setting the \l priority property to
    \c KeyNavigation.BeforeItem allows the event to be used for key navigation
    before the item, rather than after.

    If the item to which the focus is switching is not enabled or visible, an attempt will
    be made to skip this item and focus on the next. This is possible if there are
    a chain of items with the same KeyNavigation handler. If multiple items in a row are not enabled
    or visible, they will also be skipped.

    KeyNavigation will implicitly set the other direction to return focus to this item. So if you set
    \l left to another item, \l right will be set on that item's KeyNavigation to set focus back to this
    item. However, if that item's KeyNavigation has had right explicitly set then no change will occur.
    This means that the example above could achieve the same behavior without specifying
    KeyNavigation.right or KeyNavigation.down for any of the items.

    \sa {Keys}{Keys attached property}
*/

/*!
    \qmlproperty Item QtQuick::KeyNavigation::left

    This property holds the item to assign focus to
    when the left cursor key is pressed.
*/

/*!
    \qmlproperty Item QtQuick::KeyNavigation::right

    This property holds the item to assign focus to
    when the right cursor key is pressed.
*/

/*!
    \qmlproperty Item QtQuick::KeyNavigation::up

    This property holds the item to assign focus to
    when the up cursor key is pressed.
*/

/*!
    \qmlproperty Item QtQuick::KeyNavigation::down

    This property holds the item to assign focus to
    when the down cursor key is pressed.
*/

/*!
    \qmlproperty Item QtQuick::KeyNavigation::tab

    This property holds the item to assign focus to
    when the Tab key is pressed.
*/

/*!
    \qmlproperty Item QtQuick::KeyNavigation::backtab

    This property holds the item to assign focus to
    when the Shift+Tab key combination (Backtab) is pressed.
*/

QQuickKeyNavigationAttached::QQuickKeyNavigationAttached(QObject *parent)
: QObject(*(new QQuickKeyNavigationAttachedPrivate), parent),
  QQuickItemKeyFilter(qmlobject_cast<QQuickItem*>(parent))
{
    m_processPost = true;
}

QQuickKeyNavigationAttached *
QQuickKeyNavigationAttached::qmlAttachedProperties(QObject *obj)
{
    return new QQuickKeyNavigationAttached(obj);
}

QQuickItem *QQuickKeyNavigationAttached::left() const
{
    Q_D(const QQuickKeyNavigationAttached);
    return d->left;
}

void QQuickKeyNavigationAttached::setLeft(QQuickItem *i)
{
    Q_D(QQuickKeyNavigationAttached);
    if (d->leftSet && d->left == i)
        return;
    d->left = i;
    d->leftSet = true;
    QQuickKeyNavigationAttached* other =
            qobject_cast<QQuickKeyNavigationAttached*>(qmlAttachedPropertiesObject<QQuickKeyNavigationAttached>(i));
    if (other && !other->d_func()->rightSet){
        other->d_func()->right = qobject_cast<QQuickItem*>(parent());
        emit other->rightChanged();
    }
    emit leftChanged();
}

QQuickItem *QQuickKeyNavigationAttached::right() const
{
    Q_D(const QQuickKeyNavigationAttached);
    return d->right;
}

void QQuickKeyNavigationAttached::setRight(QQuickItem *i)
{
    Q_D(QQuickKeyNavigationAttached);
    if (d->rightSet && d->right == i)
        return;
    d->right = i;
    d->rightSet = true;
    QQuickKeyNavigationAttached* other =
            qobject_cast<QQuickKeyNavigationAttached*>(qmlAttachedPropertiesObject<QQuickKeyNavigationAttached>(i));
    if (other && !other->d_func()->leftSet){
        other->d_func()->left = qobject_cast<QQuickItem*>(parent());
        emit other->leftChanged();
    }
    emit rightChanged();
}

QQuickItem *QQuickKeyNavigationAttached::up() const
{
    Q_D(const QQuickKeyNavigationAttached);
    return d->up;
}

void QQuickKeyNavigationAttached::setUp(QQuickItem *i)
{
    Q_D(QQuickKeyNavigationAttached);
    if (d->upSet && d->up == i)
        return;
    d->up = i;
    d->upSet = true;
    QQuickKeyNavigationAttached* other =
            qobject_cast<QQuickKeyNavigationAttached*>(qmlAttachedPropertiesObject<QQuickKeyNavigationAttached>(i));
    if (other && !other->d_func()->downSet){
        other->d_func()->down = qobject_cast<QQuickItem*>(parent());
        emit other->downChanged();
    }
    emit upChanged();
}

QQuickItem *QQuickKeyNavigationAttached::down() const
{
    Q_D(const QQuickKeyNavigationAttached);
    return d->down;
}

void QQuickKeyNavigationAttached::setDown(QQuickItem *i)
{
    Q_D(QQuickKeyNavigationAttached);
    if (d->downSet && d->down == i)
        return;
    d->down = i;
    d->downSet = true;
    QQuickKeyNavigationAttached* other =
            qobject_cast<QQuickKeyNavigationAttached*>(qmlAttachedPropertiesObject<QQuickKeyNavigationAttached>(i));
    if (other && !other->d_func()->upSet) {
        other->d_func()->up = qobject_cast<QQuickItem*>(parent());
        emit other->upChanged();
    }
    emit downChanged();
}

QQuickItem *QQuickKeyNavigationAttached::tab() const
{
    Q_D(const QQuickKeyNavigationAttached);
    return d->tab;
}

void QQuickKeyNavigationAttached::setTab(QQuickItem *i)
{
    Q_D(QQuickKeyNavigationAttached);
    if (d->tabSet && d->tab == i)
        return;
    d->tab = i;
    d->tabSet = true;
    QQuickKeyNavigationAttached* other =
            qobject_cast<QQuickKeyNavigationAttached*>(qmlAttachedPropertiesObject<QQuickKeyNavigationAttached>(i));
    if (other && !other->d_func()->backtabSet) {
        other->d_func()->backtab = qobject_cast<QQuickItem*>(parent());
        emit other->backtabChanged();
    }
    emit tabChanged();
}

QQuickItem *QQuickKeyNavigationAttached::backtab() const
{
    Q_D(const QQuickKeyNavigationAttached);
    return d->backtab;
}

void QQuickKeyNavigationAttached::setBacktab(QQuickItem *i)
{
    Q_D(QQuickKeyNavigationAttached);
    if (d->backtabSet && d->backtab == i)
        return;
    d->backtab = i;
    d->backtabSet = true;
    QQuickKeyNavigationAttached* other =
            qobject_cast<QQuickKeyNavigationAttached*>(qmlAttachedPropertiesObject<QQuickKeyNavigationAttached>(i));
    if (other && !other->d_func()->tabSet) {
        other->d_func()->tab = qobject_cast<QQuickItem*>(parent());
        emit other->tabChanged();
    }
    emit backtabChanged();
}

/*!
    \qmlproperty enumeration QtQuick::KeyNavigation::priority

    This property determines whether the keys are processed before
    or after the attached item's own key handling.

    \list
    \li KeyNavigation.BeforeItem - process the key events before normal
    item key processing.  If the event is used for key navigation, it will be accepted and will not
    be passed on to the item.
    \li KeyNavigation.AfterItem (default) - process the key events after normal item key
    handling.  If the item accepts the key event it will not be
    handled by the KeyNavigation attached property handler.
    \endlist
*/
QQuickKeyNavigationAttached::Priority QQuickKeyNavigationAttached::priority() const
{
    return m_processPost ? AfterItem : BeforeItem;
}

void QQuickKeyNavigationAttached::setPriority(Priority order)
{
    bool processPost = order == AfterItem;
    if (processPost != m_processPost) {
        m_processPost = processPost;
        emit priorityChanged();
    }
}

void QQuickKeyNavigationAttached::keyPressed(QKeyEvent *event, bool post)
{
    Q_D(QQuickKeyNavigationAttached);
    event->ignore();

    if (post != m_processPost) {
        QQuickItemKeyFilter::keyPressed(event, post);
        return;
    }

    bool mirror = false;
    switch (event->key()) {
    case Qt::Key_Left: {
        if (QQuickItem *parentItem = qobject_cast<QQuickItem*>(parent()))
            mirror = QQuickItemPrivate::get(parentItem)->effectiveLayoutMirror;
        QQuickItem* leftItem = mirror ? d->right : d->left;
        if (leftItem) {
            setFocusNavigation(leftItem, mirror ? "right" : "left", mirror ? Qt::TabFocusReason : Qt::BacktabFocusReason);
            event->accept();
        }
        break;
    }
    case Qt::Key_Right: {
        if (QQuickItem *parentItem = qobject_cast<QQuickItem*>(parent()))
            mirror = QQuickItemPrivate::get(parentItem)->effectiveLayoutMirror;
        QQuickItem* rightItem = mirror ? d->left : d->right;
        if (rightItem) {
            setFocusNavigation(rightItem, mirror ? "left" : "right", mirror ? Qt::BacktabFocusReason : Qt::TabFocusReason);
            event->accept();
        }
        break;
    }
    case Qt::Key_Up:
        if (d->up) {
            setFocusNavigation(d->up, "up", Qt::BacktabFocusReason);
            event->accept();
        }
        break;
    case Qt::Key_Down:
        if (d->down) {
            setFocusNavigation(d->down, "down", Qt::TabFocusReason);
            event->accept();
        }
        break;
    case Qt::Key_Tab:
        if (d->tab) {
            setFocusNavigation(d->tab, "tab", Qt::TabFocusReason);
            event->accept();
        }
        break;
    case Qt::Key_Backtab:
        if (d->backtab) {
            setFocusNavigation(d->backtab, "backtab", Qt::BacktabFocusReason);
            event->accept();
        }
        break;
    default:
        break;
    }

    if (!event->isAccepted()) QQuickItemKeyFilter::keyPressed(event, post);
}

void QQuickKeyNavigationAttached::keyReleased(QKeyEvent *event, bool post)
{
    Q_D(QQuickKeyNavigationAttached);
    event->ignore();

    if (post != m_processPost) {
        QQuickItemKeyFilter::keyReleased(event, post);
        return;
    }

    bool mirror = false;
    switch (event->key()) {
    case Qt::Key_Left:
        if (QQuickItem *parentItem = qobject_cast<QQuickItem*>(parent()))
            mirror = QQuickItemPrivate::get(parentItem)->effectiveLayoutMirror;
        if (mirror ? d->right : d->left)
            event->accept();
        break;
    case Qt::Key_Right:
        if (QQuickItem *parentItem = qobject_cast<QQuickItem*>(parent()))
            mirror = QQuickItemPrivate::get(parentItem)->effectiveLayoutMirror;
        if (mirror ? d->left : d->right)
            event->accept();
        break;
    case Qt::Key_Up:
        if (d->up) {
            event->accept();
        }
        break;
    case Qt::Key_Down:
        if (d->down) {
            event->accept();
        }
        break;
    case Qt::Key_Tab:
        if (d->tab) {
            event->accept();
        }
        break;
    case Qt::Key_Backtab:
        if (d->backtab) {
            event->accept();
        }
        break;
    default:
        break;
    }

    if (!event->isAccepted()) QQuickItemKeyFilter::keyReleased(event, post);
}

void QQuickKeyNavigationAttached::setFocusNavigation(QQuickItem *currentItem, const char *dir,
                                                     Qt::FocusReason reason)
{
    QQuickItem *initialItem = currentItem;
    bool isNextItem = false;
    QVector<QQuickItem *> visitedItems;
    do {
        isNextItem = false;
        if (currentItem->isVisible() && currentItem->isEnabled()) {
            currentItem->forceActiveFocus(reason);
        } else {
            QObject *attached =
                qmlAttachedPropertiesObject<QQuickKeyNavigationAttached>(currentItem, false);
            if (attached) {
                QQuickItem *tempItem = qvariant_cast<QQuickItem*>(attached->property(dir));
                if (tempItem) {
                    visitedItems.append(currentItem);
                    currentItem = tempItem;
                    isNextItem = true;
                }
            }
        }
    }
    while (currentItem != initialItem && isNextItem && !visitedItems.contains(currentItem));
}

struct SigMap {
    int key;
    const char *sig;
};

const SigMap sigMap[] = {
    { Qt::Key_Left, "leftPressed" },
    { Qt::Key_Right, "rightPressed" },
    { Qt::Key_Up, "upPressed" },
    { Qt::Key_Down, "downPressed" },
    { Qt::Key_Tab, "tabPressed" },
    { Qt::Key_Backtab, "backtabPressed" },
    { Qt::Key_Asterisk, "asteriskPressed" },
    { Qt::Key_NumberSign, "numberSignPressed" },
    { Qt::Key_Escape, "escapePressed" },
    { Qt::Key_Return, "returnPressed" },
    { Qt::Key_Enter, "enterPressed" },
    { Qt::Key_Delete, "deletePressed" },
    { Qt::Key_Space, "spacePressed" },
    { Qt::Key_Back, "backPressed" },
    { Qt::Key_Cancel, "cancelPressed" },
    { Qt::Key_Select, "selectPressed" },
    { Qt::Key_Yes, "yesPressed" },
    { Qt::Key_No, "noPressed" },
    { Qt::Key_Context1, "context1Pressed" },
    { Qt::Key_Context2, "context2Pressed" },
    { Qt::Key_Context3, "context3Pressed" },
    { Qt::Key_Context4, "context4Pressed" },
    { Qt::Key_Call, "callPressed" },
    { Qt::Key_Hangup, "hangupPressed" },
    { Qt::Key_Flip, "flipPressed" },
    { Qt::Key_Menu, "menuPressed" },
    { Qt::Key_VolumeUp, "volumeUpPressed" },
    { Qt::Key_VolumeDown, "volumeDownPressed" },
    { 0, 0 }
};

const QByteArray QQuickKeysAttached::keyToSignal(int key)
{
    QByteArray keySignal;
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        keySignal = "digit0Pressed";
        keySignal[5] = '0' + (key - Qt::Key_0);
    } else {
        int i = 0;
        while (sigMap[i].key && sigMap[i].key != key)
            ++i;
        keySignal = sigMap[i].sig;
    }
    return keySignal;
}

bool QQuickKeysAttached::isConnected(const char *signalName)
{
    Q_D(QQuickKeysAttached);
    int signal_index = d->signalIndex(signalName);
    return d->isSignalConnected(signal_index);
}

/*!
    \qmltype Keys
    \instantiates QQuickKeysAttached
    \inqmlmodule QtQuick
    \ingroup qtquick-input
    \brief Provides key handling to Items

    All visual primitives support key handling via the Keys
    attached property.  Keys can be handled via the onPressed
    and onReleased signal properties.

    The signal properties have a \l KeyEvent parameter, named
    \e event which contains details of the event.  If a key is
    handled \e event.accepted should be set to true to prevent the
    event from propagating up the item hierarchy.

    \section1 Example Usage

    The following example shows how the general onPressed handler can
    be used to test for a certain key; in this case, the left cursor
    key:

    \snippet qml/keys/keys-pressed.qml key item

    Some keys may alternatively be handled via specific signal properties,
    for example \e onSelectPressed.  These handlers automatically set
    \e event.accepted to true.

    \snippet qml/keys/keys-handler.qml key item

    See \l{Qt::Key}{Qt.Key} for the list of keyboard codes.

    \section1 Key Handling Priorities

    The Keys attached property can be configured to handle key events
    before or after the item it is attached to. This makes it possible
    to intercept events in order to override an item's default behavior,
    or act as a fallback for keys not handled by the item.

    If \l priority is Keys.BeforeItem (default) the order of key event processing is:

    \list 1
    \li Items specified in \c forwardTo
    \li specific key handlers, e.g. onReturnPressed
    \li onPressed, onReleased handlers
    \li Item specific key handling, e.g. TextInput key handling
    \li parent item
    \endlist

    If priority is Keys.AfterItem the order of key event processing is:

    \list 1
    \li Item specific key handling, e.g. TextInput key handling
    \li Items specified in \c forwardTo
    \li specific key handlers, e.g. onReturnPressed
    \li onPressed, onReleased handlers
    \li parent item
    \endlist

    If the event is accepted during any of the above steps, key
    propagation stops.

    \sa KeyEvent, {KeyNavigation}{KeyNavigation attached property}
*/

/*!
    \qmlproperty bool QtQuick::Keys::enabled

    This flags enables key handling if true (default); otherwise
    no key handlers will be called.
*/

/*!
    \qmlproperty enumeration QtQuick::Keys::priority

    This property determines whether the keys are processed before
    or after the attached item's own key handling.

    \list
    \li Keys.BeforeItem (default) - process the key events before normal
    item key processing.  If the event is accepted it will not
    be passed on to the item.
    \li Keys.AfterItem - process the key events after normal item key
    handling.  If the item accepts the key event it will not be
    handled by the Keys attached property handler.
    \endlist

    \sa {Key Handling Priorities}
*/

/*!
    \qmlproperty list<Object> QtQuick::Keys::forwardTo

    This property provides a way to forward key presses, key releases, and keyboard input
    coming from input methods to other items. This can be useful when you want
    one item to handle some keys (e.g. the up and down arrow keys), and another item to
    handle other keys (e.g. the left and right arrow keys).  Once an item that has been
    forwarded keys accepts the event it is no longer forwarded to items later in the
    list.

    This example forwards key events to two lists:
    \qml
    Item {
        ListView {
            id: list1
            // ...
        }
        ListView {
            id: list2
            // ...
        }
        Keys.forwardTo: [list1, list2]
        focus: true
    }
    \endqml

    To see the order in which events are received when using forwardTo, see
    \l {Key Handling Priorities}.
*/

/*!
    \qmlsignal QtQuick::Keys::pressed(KeyEvent event)

    This signal is emitted when a key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::released(KeyEvent event)

    This signal is emitted when a key has been released. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onReleased.
*/

/*!
    \qmlsignal QtQuick::Keys::digit0Pressed(KeyEvent event)

    This signal is emitted when the digit '0' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit0Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit1Pressed(KeyEvent event)

    This signal is emitted when the digit '1' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit1Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit2Pressed(KeyEvent event)

    This signal is emitted when the digit '2' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit2Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit3Pressed(KeyEvent event)

    This signal is emitted when the digit '3' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit3Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit4Pressed(KeyEvent event)

    This signal is emitted when the digit '4' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit4Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit5Pressed(KeyEvent event)

    This signal is emitted when the digit '5' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit5Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit6Pressed(KeyEvent event)

    This signal is emitted when the digit '6' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit6Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit7Pressed(KeyEvent event)

    This signal is emitted when the digit '7' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit7Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit8Pressed(KeyEvent event)

    This signal is emitted when the digit '8' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit8Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::digit9Pressed(KeyEvent event)

    This signal is emitted when the digit '9' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDigit9Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::leftPressed(KeyEvent event)

    This signal is emitted when the Left arrow has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onLeftPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::rightPressed(KeyEvent event)

    This signal is emitted when the Right arrow has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onRightPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::upPressed(KeyEvent event)

    This signal is emitted when the Up arrow has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onUpPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::downPressed(KeyEvent event)

    This signal is emitted when the Down arrow has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDownPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::tabPressed(KeyEvent event)

    This signal is emitted when the Tab key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onTabPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::backtabPressed(KeyEvent event)

    This signal is emitted when the Shift+Tab key combination (Backtab) has
    been pressed. The \a event parameter provides information about the event.

    The corresponding handler is \c onBacktabPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::asteriskPressed(KeyEvent event)

    This signal is emitted when the Asterisk '*' has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onAsteriskPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::escapePressed(KeyEvent event)

    This signal is emitted when the Escape key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onEscapePressed.
*/

/*!
    \qmlsignal QtQuick::Keys::returnPressed(KeyEvent event)

    This signal is emitted when the Return key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onReturnPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::enterPressed(KeyEvent event)

    This signal is emitted when the Enter key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onEnterPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::deletePressed(KeyEvent event)

    This signal is emitted when the Delete key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onDeletePressed.
*/

/*!
    \qmlsignal QtQuick::Keys::spacePressed(KeyEvent event)

    This signal is emitted when the Space key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onSpacePressed.
*/

/*!
    \qmlsignal QtQuick::Keys::backPressed(KeyEvent event)

    This signal is emitted when the Back key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onBackPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::cancelPressed(KeyEvent event)

    This signal is emitted when the Cancel key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onCancelPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::selectPressed(KeyEvent event)

    This signal is emitted when the Select key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onSelectPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::yesPressed(KeyEvent event)

    This signal is emitted when the Yes key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onYesPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::noPressed(KeyEvent event)

    This signal is emitted when the No key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onNoPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::context1Pressed(KeyEvent event)

    This signal is emitted when the Context1 key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onContext1Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::context2Pressed(KeyEvent event)

    This signal is emitted when the Context2 key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onContext2Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::context3Pressed(KeyEvent event)

    This signal is emitted when the Context3 key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onContext3Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::context4Pressed(KeyEvent event)

    This signal is emitted when the Context4 key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onContext4Pressed.
*/

/*!
    \qmlsignal QtQuick::Keys::callPressed(KeyEvent event)

    This signal is emitted when the Call key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onCallPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::hangupPressed(KeyEvent event)

    This signal is emitted when the Hangup key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onHangupPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::flipPressed(KeyEvent event)

    This signal is emitted when the Flip key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onFlipPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::menuPressed(KeyEvent event)

    This signal is emitted when the Menu key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onMenuPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::volumeUpPressed(KeyEvent event)

    This signal is emitted when the VolumeUp key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onVolumeUpPressed.
*/

/*!
    \qmlsignal QtQuick::Keys::volumeDownPressed(KeyEvent event)

    This signal is emitted when the VolumeDown key has been pressed. The \a event
    parameter provides information about the event.

    The corresponding handler is \c onVolumeDownPressed.
*/

QQuickKeysAttached::QQuickKeysAttached(QObject *parent)
: QObject(*(new QQuickKeysAttachedPrivate), parent),
  QQuickItemKeyFilter(qmlobject_cast<QQuickItem*>(parent))
{
    Q_D(QQuickKeysAttached);
    m_processPost = false;
    d->item = qmlobject_cast<QQuickItem*>(parent);
    if (d->item != parent)
        qWarning() << "Could not attach Keys property to: " << parent << " is not an Item";
}

QQuickKeysAttached::~QQuickKeysAttached()
{
}

QQuickKeysAttached::Priority QQuickKeysAttached::priority() const
{
    return m_processPost ? AfterItem : BeforeItem;
}

void QQuickKeysAttached::setPriority(Priority order)
{
    bool processPost = order == AfterItem;
    if (processPost != m_processPost) {
        m_processPost = processPost;
        emit priorityChanged();
    }
}

void QQuickKeysAttached::componentComplete()
{
#ifndef QT_NO_IM
    Q_D(QQuickKeysAttached);
    if (d->item) {
        for (QQuickItem *targetItem : qAsConst(d->targets)) {
            if (targetItem && (targetItem->flags() & QQuickItem::ItemAcceptsInputMethod)) {
                d->item->setFlag(QQuickItem::ItemAcceptsInputMethod);
                break;
            }
        }
    }
#endif
}

void QQuickKeysAttached::keyPressed(QKeyEvent *event, bool post)
{
    Q_D(QQuickKeysAttached);
    if (post != m_processPost || !d->enabled || d->inPress) {
        event->ignore();
        QQuickItemKeyFilter::keyPressed(event, post);
        return;
    }

    // first process forwards
    if (d->item && d->item->window()) {
        d->inPress = true;
        for (QQuickItem *targetItem : qAsConst(d->targets)) {
            if (targetItem && targetItem->isVisible()) {
                event->accept();
                QCoreApplication::sendEvent(targetItem, event);
                if (event->isAccepted()) {
                    d->inPress = false;
                    return;
                }
            }
        }
        d->inPress = false;
    }

    QQuickKeyEvent &ke = d->theKeyEvent;
    ke.reset(*event);
    QByteArray keySignal = keyToSignal(event->key());
    if (!keySignal.isEmpty()) {
        keySignal += "(QQuickKeyEvent*)";
        if (isConnected(keySignal)) {
            // If we specifically handle a key then default to accepted
            ke.setAccepted(true);
            int idx = QQuickKeysAttached::staticMetaObject.indexOfSignal(keySignal);
            metaObject()->method(idx).invoke(this, Qt::DirectConnection, Q_ARG(QQuickKeyEvent*, &ke));
        }
    }
    if (!ke.isAccepted())
        emit pressed(&ke);
    event->setAccepted(ke.isAccepted());

    if (!event->isAccepted()) QQuickItemKeyFilter::keyPressed(event, post);
}

void QQuickKeysAttached::keyReleased(QKeyEvent *event, bool post)
{
    Q_D(QQuickKeysAttached);
    if (post != m_processPost || !d->enabled || d->inRelease) {
        event->ignore();
        QQuickItemKeyFilter::keyReleased(event, post);
        return;
    }

    if (d->item && d->item->window()) {
        d->inRelease = true;
        for (QQuickItem *targetItem : qAsConst(d->targets)) {
            if (targetItem && targetItem->isVisible()) {
                event->accept();
                QCoreApplication::sendEvent(targetItem, event);
                if (event->isAccepted()) {
                    d->inRelease = false;
                    return;
                }
            }
        }
        d->inRelease = false;
    }

    QQuickKeyEvent &ke = d->theKeyEvent;
    ke.reset(*event);
    emit released(&ke);
    event->setAccepted(ke.isAccepted());

    if (!event->isAccepted()) QQuickItemKeyFilter::keyReleased(event, post);
}

#ifndef QT_NO_IM
void QQuickKeysAttached::inputMethodEvent(QInputMethodEvent *event, bool post)
{
    Q_D(QQuickKeysAttached);
    if (post == m_processPost && d->item && !d->inIM && d->item->window()) {
        d->inIM = true;
        for (QQuickItem *targetItem : qAsConst(d->targets)) {
            if (targetItem && targetItem->isVisible() && (targetItem->flags() & QQuickItem::ItemAcceptsInputMethod)) {
                QCoreApplication::sendEvent(targetItem, event);
                if (event->isAccepted()) {
                    d->imeItem = targetItem;
                    d->inIM = false;
                    return;
                }
            }
        }
        d->inIM = false;
    }
    QQuickItemKeyFilter::inputMethodEvent(event, post);
}

QVariant QQuickKeysAttached::inputMethodQuery(Qt::InputMethodQuery query) const
{
    Q_D(const QQuickKeysAttached);
    if (d->item) {
        for (QQuickItem *targetItem : qAsConst(d->targets)) {
            if (targetItem && targetItem->isVisible() && (targetItem->flags() & QQuickItem::ItemAcceptsInputMethod) && targetItem == d->imeItem) {
                //### how robust is targetItem == d->imeItem check?
                QVariant v = targetItem->inputMethodQuery(query);
                if (v.userType() == QVariant::RectF)
                    v = d->item->mapRectFromItem(targetItem, v.toRectF());  //### cost?
                return v;
            }
        }
    }
    return QQuickItemKeyFilter::inputMethodQuery(query);
}
#endif // QT_NO_IM

QQuickKeysAttached *QQuickKeysAttached::qmlAttachedProperties(QObject *obj)
{
    return new QQuickKeysAttached(obj);
}

/*!
    \qmltype LayoutMirroring
    \instantiates QQuickLayoutMirroringAttached
    \inqmlmodule QtQuick
    \ingroup qtquick-positioners
    \ingroup qml-utility-elements
    \brief Property used to mirror layout behavior

    The LayoutMirroring attached property is used to horizontally mirror \l {anchor-layout}{Item anchors},
    \l{Item Positioners}{positioner} types (such as \l Row and \l Grid)
    and views (such as \l GridView and horizontal \l ListView). Mirroring is a visual change: left
    anchors become right anchors, and positioner types like \l Grid and \l Row reverse the
    horizontal layout of child items.

    Mirroring is enabled for an item by setting the \l enabled property to true. By default, this
    only affects the item itself; setting the \l childrenInherit property to true propagates the mirroring
    behavior to all child items as well. If the \c LayoutMirroring attached property has not been defined
    for an item, mirroring is not enabled.

    \note Since Qt 5.8, \c LayoutMirroring can be attached to a \l Window. In practice, it is the same as
    attaching \c LayoutMirroring to the window's \c contentItem.

    The following example shows mirroring in action. The \l Row below is specified as being anchored
    to the left of its parent. However, since mirroring has been enabled, the anchor is horizontally
    reversed and it is now anchored to the right. Also, since items in a \l Row are positioned
    from left to right by default, they are now positioned from right to left instead, as demonstrated
    by the numbering and opacity of the items:

    \snippet qml/layoutmirroring.qml 0

    \image layoutmirroring.png

    Layout mirroring is useful when it is necessary to support both left-to-right and right-to-left
    layout versions of an application to target different language areas. The \l childrenInherit
    property allows layout mirroring to be applied without manually setting layout configurations
    for every item in an application. Keep in mind, however, that mirroring does not affect any
    positioning that is defined by the \l Item \l {Item::}{x} coordinate value, so even with
    mirroring enabled, it will often be necessary to apply some layout fixes to support the
    desired layout direction. Also, it may be necessary to disable the mirroring of individual
    child items (by setting \l {enabled}{LayoutMirroring.enabled} to false for such items) if
    mirroring is not the desired behavior, or if the child item already implements mirroring in
    some custom way.

    See \l {Right-to-left User Interfaces} for further details on using \c LayoutMirroring and
    other related features to implement right-to-left support for an application.
*/

/*!
    \qmlproperty bool QtQuick::LayoutMirroring::enabled

    This property holds whether the item's layout is mirrored horizontally. Setting this to true
    horizontally reverses \l {anchor-layout}{anchor} settings such that left anchors become right,
    and right anchors become left. For \l{Item Positioners}{positioner} types
    (such as \l Row and \l Grid) and view types (such as \l {GridView}{GridView} and \l {ListView}{ListView})
    this also mirrors the horizontal layout direction of the item.

    The default value is false.
*/

/*!
    \qmlproperty bool QtQuick::LayoutMirroring::childrenInherit

    This property holds whether the \l {enabled}{LayoutMirroring.enabled} value for this item
    is inherited by its children.

    The default value is false.
*/


QQuickLayoutMirroringAttached::QQuickLayoutMirroringAttached(QObject *parent) : QObject(parent), itemPrivate(0)
{
    if (QQuickItem *item = qobject_cast<QQuickItem *>(parent))
        itemPrivate = QQuickItemPrivate::get(item);
    else if (QQuickWindow *window = qobject_cast<QQuickWindow *>(parent))
        itemPrivate = QQuickItemPrivate::get(window->contentItem());

    if (itemPrivate)
        itemPrivate->extra.value().layoutDirectionAttached = this;
    else
        qmlInfo(parent) << tr("LayoutDirection attached property only works with Items and Windows");
}

QQuickLayoutMirroringAttached * QQuickLayoutMirroringAttached::qmlAttachedProperties(QObject *object)
{
    return new QQuickLayoutMirroringAttached(object);
}

bool QQuickLayoutMirroringAttached::enabled() const
{
    return itemPrivate ? itemPrivate->effectiveLayoutMirror : false;
}

void QQuickLayoutMirroringAttached::setEnabled(bool enabled)
{
    if (!itemPrivate)
        return;

    itemPrivate->isMirrorImplicit = false;
    if (enabled != itemPrivate->effectiveLayoutMirror) {
        itemPrivate->setLayoutMirror(enabled);
        if (itemPrivate->inheritMirrorFromItem)
             itemPrivate->resolveLayoutMirror();
    }
}

void QQuickLayoutMirroringAttached::resetEnabled()
{
    if (itemPrivate && !itemPrivate->isMirrorImplicit) {
        itemPrivate->isMirrorImplicit = true;
        itemPrivate->resolveLayoutMirror();
    }
}

bool QQuickLayoutMirroringAttached::childrenInherit() const
{
    return itemPrivate ? itemPrivate->inheritMirrorFromItem : false;
}

void QQuickLayoutMirroringAttached::setChildrenInherit(bool childrenInherit) {
    if (itemPrivate && childrenInherit != itemPrivate->inheritMirrorFromItem) {
        itemPrivate->inheritMirrorFromItem = childrenInherit;
        itemPrivate->resolveLayoutMirror();
        childrenInheritChanged();
    }
}

void QQuickItemPrivate::resolveLayoutMirror()
{
    Q_Q(QQuickItem);
    if (QQuickItem *parentItem = q->parentItem()) {
        QQuickItemPrivate *parentPrivate = QQuickItemPrivate::get(parentItem);
        setImplicitLayoutMirror(parentPrivate->inheritedLayoutMirror, parentPrivate->inheritMirrorFromParent);
    } else {
        setImplicitLayoutMirror(isMirrorImplicit ? false : effectiveLayoutMirror, inheritMirrorFromItem);
    }
}

void QQuickItemPrivate::setImplicitLayoutMirror(bool mirror, bool inherit)
{
    inherit = inherit || inheritMirrorFromItem;
    if (!isMirrorImplicit && inheritMirrorFromItem)
        mirror = effectiveLayoutMirror;
    if (mirror == inheritedLayoutMirror && inherit == inheritMirrorFromParent)
        return;

    inheritMirrorFromParent = inherit;
    inheritedLayoutMirror = inheritMirrorFromParent ? mirror : false;

    if (isMirrorImplicit)
        setLayoutMirror(inherit ? inheritedLayoutMirror : false);
    for (QQuickItem *child : qAsConst(childItems)) {
        QQuickItemPrivate *childPrivate = QQuickItemPrivate::get(child);
        childPrivate->setImplicitLayoutMirror(inheritedLayoutMirror, inheritMirrorFromParent);
    }
}

void QQuickItemPrivate::setLayoutMirror(bool mirror)
{
    if (mirror != effectiveLayoutMirror) {
        effectiveLayoutMirror = mirror;
        if (_anchors) {
            QQuickAnchorsPrivate *anchor_d = QQuickAnchorsPrivate::get(_anchors);
            anchor_d->fillChanged();
            anchor_d->centerInChanged();
            anchor_d->updateHorizontalAnchors();
        }
        mirrorChange();
        if (extra.isAllocated() && extra->layoutDirectionAttached) {
            emit extra->layoutDirectionAttached->enabledChanged();
        }
    }
}

/*!
    \qmltype EnterKey
    \instantiates QQuickEnterKeyAttached
    \inqmlmodule QtQuick
    \ingroup qtquick-input
    \since 5.6
    \brief Provides a property to manipulate the appearance of Enter key on
           an on-screen keyboard.

    The EnterKey attached property is used to manipulate the appearance and
    behavior of the Enter key on an on-screen keyboard.
*/

/*!
    \qmlattachedproperty enumeration QtQuick::EnterKey::type

    Holds the type of the Enter key.

    \note Not all of these values are supported on all platforms. For
          unsupported values the default key is used instead.

    \value Qt.EnterKeyDefault   The default Enter key. This can be either a
                                button to accept the input and close the
                                keyboard, or a \e Return button to enter a
                                newline in case of a multi-line input field.

    \value Qt.EnterKeyReturn    Show a \e Return button that inserts a
                                newline.

    \value Qt.EnterKeyDone      Show a \e {"Done"} button. Typically, the
                                keyboard is expected to close when the button
                                is pressed.

    \value Qt.EnterKeyGo        Show a \e {"Go"} button. Typically used in an
                                address bar when entering a URL.

    \value Qt.EnterKeySend      Show a \e {"Send"} button.

    \value Qt.EnterKeySearch    Show a \e {"Search"} button.

    \value Qt.EnterKeyNext      Show a \e {"Next"} button. Typically used in a
                                form to allow navigating to the next input
                                field without the keyboard closing.

    \value Qt.EnterKeyPrevious  Show a \e {"Previous"} button.
*/

QQuickEnterKeyAttached::QQuickEnterKeyAttached(QObject *parent)
    : QObject(parent), itemPrivate(0), keyType(Qt::EnterKeyDefault)
{
    if (QQuickItem *item = qobject_cast<QQuickItem*>(parent)) {
        itemPrivate = QQuickItemPrivate::get(item);
        itemPrivate->extra.value().enterKeyAttached = this;
    } else
        qmlInfo(parent) << tr("EnterKey attached property only works with Items");
}

QQuickEnterKeyAttached *QQuickEnterKeyAttached::qmlAttachedProperties(QObject *object)
{
    return new QQuickEnterKeyAttached(object);
}

Qt::EnterKeyType QQuickEnterKeyAttached::type() const
{
    return keyType;
}

void QQuickEnterKeyAttached::setType(Qt::EnterKeyType type)
{
    if (keyType != type) {
        keyType = type;
#ifndef QT_NO_IM
        if (itemPrivate && itemPrivate->activeFocus)
            QGuiApplication::inputMethod()->update(Qt::ImEnterKeyType);
#endif
        typeChanged();
    }
}

void QQuickItemPrivate::setAccessible()
{
    isAccessible = true;
}

/*!
Clears all sub focus items from \a scope.
If \a focus is true, sets the scope's subFocusItem
to be this item.
*/
void QQuickItemPrivate::updateSubFocusItem(QQuickItem *scope, bool focus)
{
    Q_Q(QQuickItem);
    Q_ASSERT(scope);

    QQuickItemPrivate *scopePrivate = QQuickItemPrivate::get(scope);

    QQuickItem *oldSubFocusItem = scopePrivate->subFocusItem;
    // Correct focus chain in scope
    if (oldSubFocusItem) {
        QQuickItem *sfi = scopePrivate->subFocusItem->parentItem();
        while (sfi && sfi != scope) {
            QQuickItemPrivate::get(sfi)->subFocusItem = 0;
            sfi = sfi->parentItem();
        }
    }

    if (focus) {
        scopePrivate->subFocusItem = q;
        QQuickItem *sfi = scopePrivate->subFocusItem->parentItem();
        while (sfi && sfi != scope) {
            QQuickItemPrivate::get(sfi)->subFocusItem = q;
            sfi = sfi->parentItem();
        }
    } else {
        scopePrivate->subFocusItem = 0;
    }
}

/*!
    \class QQuickItem
    \brief The QQuickItem class provides the most basic of all visual items in \l {Qt Quick}.
    \inmodule QtQuick

    All visual items in Qt Quick inherit from QQuickItem. Although a QQuickItem
    instance has no visual appearance, it defines all the attributes that are
    common across visual items, such as x and y position, width and height,
    \l {Positioning with Anchors}{anchoring} and key handling support.

    You can subclass QQuickItem to provide your own custom visual item
    that inherits these features.

    \section1 Custom Scene Graph Items

    All visual QML items are rendered using the scene graph, a
    low-level, high-performance rendering stack, closely tied to
    OpenGL. It is possible for subclasses of QQuickItem to add their
    own custom content into the scene graph by setting the
    QQuickItem::ItemHasContents flag and reimplementing the
    QQuickItem::updatePaintNode() function.

    \warning It is crucial that OpenGL operations and interaction with
    the scene graph happens exclusively on the rendering thread,
    primarily during the updatePaintNode() call. The best rule of
    thumb is to only use classes with the "QSG" prefix inside the
    QQuickItem::updatePaintNode() function.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

    \section2 Graphics Resource Handling

    The preferred way to handle cleanup of graphics resources used in
    the scene graph, is to rely on the automatic cleanup of nodes. A
    QSGNode returned from QQuickItem::updatePaintNode() is
    automatically deleted on the right thread at the right time. Trees
    of QSGNode instances are managed through the use of
    QSGNode::OwnedByParent, which is set by default. So, for the
    majority of custom scene graph items, no extra work will be
    required.

    Implementations that store graphics resources outside the node
    tree, such as an item implementing QQuickItem::textureProvider(),
    will need to take care in cleaning it up correctly depending on
    how the item is used in QML. The situations to handle are:

    \list

    \li The scene graph is invalidated; This can happen, for instance,
    if the window is hidden using QQuickWindow::hide(). If the item
    class implements a \c slot named \c invalidateSceneGraph(), this
    slot will be called on the rendering thread while the GUI thread
    is blocked. This is equivalent to connecting to
    QQuickWindow::sceneGraphInvalidated(). The OpenGL context of this
    item's window will be bound when this slot is called. The only
    exception is if the native OpenGL has been destroyed outside Qt's
    control, for instance through \c EGL_CONTEXT_LOST.

    \li The item is removed from the scene; If an item is taken out of
    the scene, for instance because it's parent was set to \c null or
    an item in another window, the QQuickItem::releaseResources() will
    be called on the GUI thread. QQuickWindow::scheduleRenderJob()
    should be used to schedule cleanup of rendering resources.

    \li The item is deleted; When the destructor if an item runs, it
    should delete any graphics resources it has. If neither of the two
    conditions above were already met, the item will be part of a
    window and it is possible to use QQuickWindow::scheduleRenderJob()
    to have them cleaned up. If an implementation ignores the call to
    QQuickItem::releaseResources(), the item will in many cases no
    longer have access to a QQuickWindow and thus no means of
    scheduling cleanup.

    \endlist

    When scheduling cleanup of graphics resources using
    QQuickWindow::scheduleRenderJob(), one should use either
    QQuickWindow::BeforeSynchronizingStage or
    QQuickWindow::AfterSynchronizingStage. The \l {Scene Graph and
    Rendering}{synchronization stage} is where the scene graph is
    changed as a result of changes to the QML tree. If cleanup is
    scheduled at any other time, it may result in other parts of the
    scene graph referencing the newly deleted objects as these parts
    have not been updated.

    \note Use of QObject::deleteLater() to clean up graphics resources
    is not recommended as this will run at an arbitrary time and it is
    unknown if there will be an OpenGL context bound when the deletion
    takes place.

    \section1 Custom QPainter Items

    The QQuickItem provides a subclass, QQuickPaintedItem, which
    allows the users to render content using QPainter.

    \warning Using QQuickPaintedItem uses an indirect 2D surface to
    render its content, either using software rasterization or using
    an OpenGL framebuffer object (FBO), so the rendering is a two-step
    operation. First rasterize the surface, then draw the
    surface. Using scene graph API directly is always significantly
    faster.

    \section1 Behavior Animations

    If your Item uses the \l Behavior type to define animations for property
    changes, you should always use either QObject::setProperty(),
    QQmlProperty(), or QMetaProperty::write() when you need to modify those
    properties from C++. This ensures that the QML engine knows about the
    property change. Otherwise, the engine won't be able to carry out your
    requested animation.
    Note that these functions incur a slight performance penalty. For more
    details, see \l {Accessing Members of a QML Object Type from C++}.

    \sa QQuickWindow, QQuickPaintedItem
*/

/*!
    \qmltype Item
    \instantiates QQuickItem
    \inherits QtObject
    \inqmlmodule QtQuick
    \ingroup qtquick-visual
    \brief A basic visual QML type

    The Item type is the base type for all visual items in Qt Quick.

    All visual items in Qt Quick inherit from Item. Although an Item
    object has no visual appearance, it defines all the attributes that are
    common across visual items, such as x and y position, width and height,
    \l {Positioning with Anchors}{anchoring} and key handling support.

    The Item type can be useful for grouping several items under a single
    root visual item. For example:

    \qml
    import QtQuick 2.0

    Item {
        Image {
            source: "tile.png"
        }
        Image {
            x: 80
            width: 100
            height: 100
            source: "tile.png"
        }
        Image {
            x: 190
            width: 100
            height: 100
            fillMode: Image.Tile
            source: "tile.png"
        }
    }
    \endqml


    \section2 Key Handling

    Key handling is available to all Item-based visual types via the \l Keys
    attached property.  The \e Keys attached property provides basic signals
    such as \l {Keys::}{pressed} and \l {Keys::}{released}, as well as
    signals for specific keys, such as \l {Keys::}{spacePressed}.  The
    example below assigns \l {Keyboard Focus in Qt Quick}{keyboard focus} to
    the item and handles the left key via the general \c onPressed handler
    and the return key via the \c onReturnPressed handler:

    \qml
    import QtQuick 2.0

    Item {
        focus: true
        Keys.onPressed: {
            if (event.key == Qt.Key_Left) {
                console.log("move left");
                event.accepted = true;
            }
        }
        Keys.onReturnPressed: console.log("Pressed return");
    }
    \endqml

    See the \l Keys attached property for detailed documentation.

    \section2 Layout Mirroring

    Item layouts can be mirrored using the \l LayoutMirroring attached
    property. This causes \l{anchors.top}{anchors} to be horizontally
    reversed, and also causes items that lay out or position their children
    (such as ListView or \l Row) to horizontally reverse the direction of
    their layouts.

    See LayoutMirroring for more details.

    \section1 Item Layers

    An Item will normally be rendered directly into the window it
    belongs to. However, by setting \l layer.enabled, it is possible
    to delegate the item and its entire subtree into an offscreen
    surface. Only the offscreen surface, a texture, will be then drawn
    into the window.

    If it is desired to have a texture size different from that of the
    item, this is possible using \l layer.textureSize. To render only
    a section of the item into the texture, use \l
    layer.sourceRect. It is also possible to specify \l
    layer.sourceRect so it extends beyond the bounds of the item. In
    this case, the exterior will be padded with transparent pixels.

    The item will use linear interpolation for scaling if
    \l layer.smooth is set to \c true and will use mipmap for
    downsampling if \l layer.mipmap is set to \c true. Mipmapping may
    improve visual quality of downscaled items. For mipmapping of
    single Image items, prefer Image::mipmap.

    \section2 Layer Opacity vs Item Opacity

    When applying \l opacity to an item hierarchy the opacity is
    applied to each item individually. This can lead to undesired
    visual results when the opacity is applied to a subtree. Consider
    the following example:

    \table
    \row
      \li \inlineimage qml-blending-nonlayered.png
      \li \b {Non-layered Opacity} \snippet qml/layerblending.qml non-layered
    \endtable

    A layer is rendered with the root item's opacity being 1, and then
    the root item's opacity is applied to the texture when it is
    drawn. This means that fading in a large item hierarchy from
    transparent to opaque, or vice versa, can be done without the
    overlap artifacts that the normal item by item alpha blending
    has. Here is the same example with layer enabled:

    \table
    \row
      \li \image qml-blending-layered.png
      \li \b {Layered Opacity} \snippet qml/layerblending.qml layered
    \endtable

    \section2 Combined with ShaderEffects

    Setting \l layer.enabled to true will turn the item into a \l
    {QQuickItem::isTextureProvider}{texture provider}, making it
    possible to use the item directly as a texture, for instance
    in combination with the ShaderEffect type.

    It is possible to apply an effect on a layer at runtime using
    layer.effect:

    \snippet qml/layerwitheffect.qml 1

    In this example, we implement the shader effect manually. The \l
    {Qt Graphical Effects} module contains a suite of ready-made
    effects for use with Qt Quick.

    See ShaderEffect for more information about using effects.

    \note \l layer.enabled is actually just a more convenient way of using
    ShaderEffectSource.


    \section2 Memory and Performance

    When an item's layer is enabled, the scene graph will allocate memory
    in the GPU equal to \c {width x height x 4}. In memory constrained
    configurations, large layers should be used with care.

    In the QPainter / QWidget world, it is some times favorable to
    cache complex content in a pixmap, image or texture. In Qt Quick,
    because of the techniques already applied by the \l {Qt Quick
    Scene Graph Renderer} {scene graph renderer}, this will in most
    cases not be the case. Excessive draw calls are already reduced
    because of batching and a cache will in most cases end up blending
    more pixels than the original content. The overhead of rendering
    to an offscreen and the blending involved with drawing the
    resulting texture is therefore often more costly than simply
    letting the item and its children be drawn normally.

    Also, an item using a layer can not be \l {Batching} {batched} during
    rendering. This means that a scene with many layered items may
    have performance problems.

    Layering can be convenient and useful for visual effects, but
    should in most cases be enabled for the duration of the effect and
    disabled afterwards.

*/

/*!
    \enum QQuickItem::Flag

    This enum type is used to specify various item properties.

    \value ItemClipsChildrenToShape Indicates this item should visually clip
    its children so that they are rendered only within the boundaries of this
    item.
    \value ItemAcceptsInputMethod Indicates the item supports text input
    methods.
    \value ItemIsFocusScope Indicates the item is a focus scope. See
    \l {Keyboard Focus in Qt Quick} for more information.
    \value ItemHasContents Indicates the item has visual content and should be
    rendered by the scene graph.
    \value ItemAcceptsDrops Indicates the item accepts drag and drop events.

    \sa setFlag(), setFlags(), flags()
*/

/*!
    \enum QQuickItem::ItemChange
    \brief Used in conjunction with QQuickItem::itemChange() to notify
    the item about certain types of changes.

    \value ItemChildAddedChange A child was added. ItemChangeData::item contains
    the added child.

    \value ItemChildRemovedChange A child was removed. ItemChangeData::item
    contains the removed child.

    \value ItemSceneChange The item was added to or removed from a scene. The
    QQuickWindow rendering the scene is specified in using ItemChangeData::window.
    The window parameter is null when the item is removed from a scene.

    \value ItemVisibleHasChanged The item's visibility has changed.
    ItemChangeData::boolValue contains the new visibility.

    \value ItemParentHasChanged The item's parent has changed.
    ItemChangeData::item contains the new parent.

    \value ItemOpacityHasChanged The item's opacity has changed.
    ItemChangeData::realValue contains the new opacity.

    \value ItemActiveFocusHasChanged The item's focus has changed.
    ItemChangeData::boolValue contains whether the item has focus or not.

    \value ItemRotationHasChanged The item's rotation has changed.
    ItemChangeData::realValue contains the new rotation.

    \value ItemDevicePixelRatioHasChanged The device pixel ratio of the screen
    the item is on has changed. ItemChangedData::realValue contains the new
    device pixel ratio.

    \value ItemAntialiasingHasChanged The antialiasing has changed. The current
    (boolean) value can be found in QQuickItem::antialiasing.
*/

/*!
    \class QQuickItem::ItemChangeData
    \inmodule QtQuick
    \brief Adds supplimentary information to the QQuickItem::itemChange()
    function.

    The meaning of each member of this class is defined by the change type.

    \sa QQuickItem::ItemChange
*/

/*!
    \fn QQuickItem::ItemChangeData::ItemChangeData(QQuickItem *)
    \internal
 */

/*!
    \fn QQuickItem::ItemChangeData::ItemChangeData(QQuickWindow *)
    \internal
 */

/*!
    \fn QQuickItem::ItemChangeData::ItemChangeData(qreal)
    \internal
 */

/*!
    \fn QQuickItem::ItemChangeData::ItemChangeData(bool)
    \internal
 */

/*!
    \variable QQuickItem::ItemChangeData::realValue
    Contains supplimentary information to the QQuickItem::itemChange() function.
    \sa QQuickItem::ItemChange
 */

/*!
    \variable QQuickItem::ItemChangeData::boolValue
    Contains supplimentary information to the QQuickItem::itemChange() function.
    \sa QQuickItem::ItemChange
 */

/*!
    \variable QQuickItem::ItemChangeData::item
    Contains supplimentary information to the QQuickItem::itemChange() function.
    \sa QQuickItem::ItemChange
 */

/*!
    \variable QQuickItem::ItemChangeData::window
    Contains supplimentary information to the QQuickItem::itemChange() function.
    \sa QQuickItem::ItemChange
 */

/*!
    \enum QQuickItem::TransformOrigin

    Controls the point about which simple transforms like scale apply.

    \value TopLeft The top-left corner of the item.
    \value Top The center point of the top of the item.
    \value TopRight The top-right corner of the item.
    \value Left The left most point of the vertical middle.
    \value Center The center of the item.
    \value Right The right most point of the vertical middle.
    \value BottomLeft The bottom-left corner of the item.
    \value Bottom The center point of the bottom of the item.
    \value BottomRight The bottom-right corner of the item.

    \sa transformOrigin(), setTransformOrigin()
*/

/*!
    \fn void QQuickItem::childrenRectChanged(const QRectF &)
    \internal
*/

/*!
    \fn void QQuickItem::baselineOffsetChanged(qreal)
    \internal
*/

/*!
    \fn void QQuickItem::stateChanged(const QString &state)
    \internal
*/

/*!
    \fn void QQuickItem::parentChanged(QQuickItem *)
    \internal
*/

/*!
    \fn void QQuickItem::smoothChanged(bool)
    \internal
*/

/*!
    \fn void QQuickItem::antialiasingChanged(bool)
    \internal
*/

/*!
    \fn void QQuickItem::clipChanged(bool)
    \internal
*/

/*!
    \fn void QQuickItem::transformOriginChanged(TransformOrigin)
    \internal
*/

/*!
    \fn void QQuickItem::focusChanged(bool)
    \internal
*/

/*!
    \fn void QQuickItem::activeFocusChanged(bool)
    \internal
*/

/*!
    \fn void QQuickItem::activeFocusOnTabChanged(bool)
    \internal
*/

/*!
    \fn void QQuickItem::childrenChanged()
    \internal
*/

/*!
    \fn void QQuickItem::opacityChanged()
    \internal
*/

/*!
    \fn void QQuickItem::enabledChanged()
    \internal
*/

/*!
    \fn void QQuickItem::visibleChanged()
    \internal
*/

/*!
    \fn void QQuickItem::visibleChildrenChanged()
    \internal
*/

/*!
    \fn void QQuickItem::rotationChanged()
    \internal
*/

/*!
    \fn void QQuickItem::scaleChanged()
    \internal
*/

/*!
    \fn void QQuickItem::xChanged()
    \internal
*/

/*!
    \fn void QQuickItem::yChanged()
    \internal
*/

/*!
    \fn void QQuickItem::widthChanged()
    \internal
*/

/*!
    \fn void QQuickItem::heightChanged()
    \internal
*/

/*!
    \fn void QQuickItem::zChanged()
    \internal
*/

/*!
    \fn void QQuickItem::implicitWidthChanged()
    \internal
*/

/*!
    \fn void QQuickItem::implicitHeightChanged()
    \internal
*/

/*!
    \fn QQuickItem::QQuickItem(QQuickItem *parent)

    Constructs a QQuickItem with the given \a parent.
*/
QQuickItem::QQuickItem(QQuickItem* parent)
: QObject(*(new QQuickItemPrivate), parent)
{
    Q_D(QQuickItem);
    d->init(parent);
}

/*! \internal
*/
QQuickItem::QQuickItem(QQuickItemPrivate &dd, QQuickItem *parent)
: QObject(dd, parent)
{
    Q_D(QQuickItem);
    d->init(parent);
}

/*!
    Destroys the QQuickItem.
*/
QQuickItem::~QQuickItem()
{
    Q_D(QQuickItem);

    if (d->windowRefCount > 1)
        d->windowRefCount = 1; // Make sure window is set to null in next call to derefWindow().
    if (d->parentItem)
        setParentItem(0);
    else if (d->window)
        d->derefWindow();

    // XXX todo - optimize
    while (!d->childItems.isEmpty())
        d->childItems.constFirst()->setParentItem(0);

    if (!d->changeListeners.isEmpty()) {
        const auto listeners = d->changeListeners; // NOTE: intentional copy (QTBUG-54732)
        for (const QQuickItemPrivate::ChangeListener &change : listeners) {
            QQuickAnchorsPrivate *anchor = change.listener->anchorPrivate();
            if (anchor)
                anchor->clearItem(this);
        }

        /*
        update item anchors that depended on us unless they are our child (and will also be destroyed),
        or our sibling, and our parent is also being destroyed.
    */
        for (const QQuickItemPrivate::ChangeListener &change : listeners) {
            QQuickAnchorsPrivate *anchor = change.listener->anchorPrivate();
            if (anchor && anchor->item && anchor->item->parentItem() && anchor->item->parentItem() != this)
                anchor->update();
        }

        for (const QQuickItemPrivate::ChangeListener &change : listeners) {
            if (change.types & QQuickItemPrivate::Destroyed)
                change.listener->itemDestroyed(this);
        }

        d->changeListeners.clear();
    }

    /*
       Remove any references our transforms have to us, in case they try to
       remove themselves from our list of transforms when that list has already
       been destroyed after ~QQuickItem() has run.
    */
    for (QQuickTransform *t : qAsConst(d->transforms)) {
        QQuickTransformPrivate *tp = QQuickTransformPrivate::get(t);
        tp->items.removeOne(this);
    }

    if (d->extra.isAllocated()) {
        delete d->extra->contents; d->extra->contents = 0;
#if QT_CONFIG(quick_shadereffect)
        delete d->extra->layer; d->extra->layer = 0;
#endif
    }

    delete d->_anchors; d->_anchors = 0;
    delete d->_stateGroup; d->_stateGroup = 0;
}

/*!
    \internal
*/
bool QQuickItemPrivate::canAcceptTabFocus(QQuickItem *item)
{
    if (!item->window())
        return false;

    if (item == item->window()->contentItem())
        return true;

#ifndef QT_NO_ACCESSIBILITY
    if (QObject *acc = qmlAttachedPropertiesObject<QQuickAccessibleAttached>(item, false)) {
        int role = acc->property("role").toInt();
        if (role == QAccessible::EditableText
                || role == QAccessible::Table
                || role == QAccessible::List
                || role == QAccessible::SpinBox) {
            return true;
        } else if (role == QAccessible::ComboBox) {
            QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item);
            return iface->state().editable;
        }
    }
#endif

    QVariant readonly = item->property("readOnly");
    if (readonly.isValid() && !readonly.toBool() && item->property("text").isValid())
        return true;

    return false;
}

/*!
    \internal
    \brief QQuickItemPrivate::focusNextPrev focuses the next/prev item in the tab-focus-chain
    \param item The item that currently has the focus
    \param forward The direction
    \return Whether the next item in the focus chain is found or not

    If \a next is true, the next item visited will be in depth-first order relative to \a item.
    If \a next is false, the next item visited will be in reverse depth-first order relative to \a item.
*/
bool QQuickItemPrivate::focusNextPrev(QQuickItem *item, bool forward)
{
    QQuickItem *next = QQuickItemPrivate::nextPrevItemInTabFocusChain(item, forward);

    if (next == item)
        return false;

    next->forceActiveFocus(forward ? Qt::TabFocusReason : Qt::BacktabFocusReason);

    return true;
}

QQuickItem *QQuickItemPrivate::nextTabChildItem(const QQuickItem *item, int start)
{
    if (!item) {
        qWarning() << "QQuickItemPrivate::nextTabChildItem called with null item.";
        return Q_NULLPTR;
    }
    const QList<QQuickItem *> &children = item->childItems();
    const int count = children.count();
    if (start < 0 || start >= count) {
        qWarning() << "QQuickItemPrivate::nextTabChildItem: Start index value out of range for item" << item;
        return Q_NULLPTR;
    }
    while (start < count) {
        QQuickItem *child = children.at(start);
        if (!child->d_func()->isTabFence)
            return child;
        ++start;
    }
    return Q_NULLPTR;
}

QQuickItem *QQuickItemPrivate::prevTabChildItem(const QQuickItem *item, int start)
{
    if (!item) {
        qWarning() << "QQuickItemPrivate::prevTabChildItem called with null item.";
        return Q_NULLPTR;
    }
    const QList<QQuickItem *> &children = item->childItems();
    const int count = children.count();
    if (start == -1)
        start = count - 1;
    if (start < 0 || start >= count) {
        qWarning() << "QQuickItemPrivate::prevTabChildItem: Start index value out of range for item" << item;
        return Q_NULLPTR;
    }
    while (start >= 0) {
        QQuickItem *child = children.at(start);
        if (!child->d_func()->isTabFence)
            return child;
        --start;
    }
    return Q_NULLPTR;
}

QQuickItem* QQuickItemPrivate::nextPrevItemInTabFocusChain(QQuickItem *item, bool forward)
{
    Q_ASSERT(item);
    qCDebug(DBG_FOCUS) << "QQuickItemPrivate::nextPrevItemInTabFocusChain: item:" << item << ", forward:" << forward;

    if (!item->window())
        return item;
    const QQuickItem * const contentItem = item->window()->contentItem();
    if (!contentItem)
        return item;

    bool all = QGuiApplication::styleHints()->tabFocusBehavior() == Qt::TabFocusAllControls;

    QQuickItem *from = 0;
    bool isTabFence = item->d_func()->isTabFence;
    if (forward) {
        if (!isTabFence)
            from = item->parentItem();
    } else {
        if (!item->childItems().isEmpty())
            from = item->d_func()->childItems.constFirst();
        else if (!isTabFence)
            from = item->parentItem();
    }
    bool skip = false;
    QQuickItem * startItem = item;
    QQuickItem * firstFromItem = from;
    QQuickItem *current = item;
    qCDebug(DBG_FOCUS) << "QQuickItemPrivate::nextPrevItemInTabFocusChain: startItem:" << startItem;
    qCDebug(DBG_FOCUS) << "QQuickItemPrivate::nextPrevItemInTabFocusChain: firstFromItem:" << firstFromItem;
    do {
        qCDebug(DBG_FOCUS) << "QQuickItemPrivate::nextPrevItemInTabFocusChain: current:" << current;
        qCDebug(DBG_FOCUS) << "QQuickItemPrivate::nextPrevItemInTabFocusChain: from:" << from;
        skip = false;
        QQuickItem *last = current;

        bool hasChildren = !current->childItems().isEmpty() && current->isEnabled() && current->isVisible();
        QQuickItem *firstChild = Q_NULLPTR;
        QQuickItem *lastChild = Q_NULLPTR;
        if (hasChildren) {
            firstChild = nextTabChildItem(current, 0);
            if (!firstChild)
                hasChildren = false;
            else
                lastChild = prevTabChildItem(current, -1);
        }
        isTabFence = current->d_func()->isTabFence;
        if (isTabFence && !hasChildren)
            return current;

        // coming from parent: check children
        if (hasChildren && from == current->parentItem()) {
            if (forward) {
                current = firstChild;
            } else {
                current = lastChild;
                if (!current->childItems().isEmpty())
                    skip = true;
            }
        } else if (hasChildren && forward && from != lastChild) {
            // not last child going forwards
            int nextChild = current->childItems().indexOf(from) + 1;
            current = nextTabChildItem(current, nextChild);
        } else if (hasChildren && !forward && from != firstChild) {
            // not first child going backwards
            int prevChild = current->childItems().indexOf(from) - 1;
            current = prevTabChildItem(current, prevChild);
            if (!current->childItems().isEmpty())
                skip = true;
        // back to the parent
        } else if (QQuickItem *parent = !isTabFence ? current->parentItem() : Q_NULLPTR) {
            // we would evaluate the parent twice, thus we skip
            if (forward) {
                skip = true;
            } else if (QQuickItem *firstSibling = !forward ? nextTabChildItem(parent, 0) : Q_NULLPTR) {
                if (last != firstSibling
                    || (parent->isFocusScope() && parent->activeFocusOnTab() && parent->hasActiveFocus()))
                        skip = true;
            }
            current = parent;
        } else if (hasChildren) {
            // Wrap around after checking all items forward
            if (forward) {
                current = firstChild;
            } else {
                current = lastChild;
                if (!current->childItems().isEmpty())
                    skip = true;
            }
        }
        from = last;
        if (current == startItem && from == firstFromItem) {
            // wrapped around, avoid endless loops
            if (item == contentItem) {
                qCDebug(DBG_FOCUS) << "QQuickItemPrivate::nextPrevItemInTabFocusChain: looped, return contentItem";
                return item;
            } else {
                qCDebug(DBG_FOCUS) << "QQuickItemPrivate::nextPrevItemInTabFocusChain: looped, return " << startItem;
                return startItem;
            }
        }
        if (!firstFromItem) {
            if (startItem->d_func()->isTabFence) {
                if (current == startItem)
                    firstFromItem = from;
            } else { //start from root
                startItem = current;
                firstFromItem = from;
            }
        }
    } while (skip || !current->activeFocusOnTab() || !current->isEnabled() || !current->isVisible()
                  || !(all || QQuickItemPrivate::canAcceptTabFocus(current)));

    return current;
}

/*!
    \qmlproperty Item QtQuick::Item::parent
    This property holds the visual parent of the item.

    \note The concept of the \e {visual parent} differs from that of the
    \e {QObject parent}. An item's visual parent may not necessarily be the
    same as its object parent. See \l {Concepts - Visual Parent in Qt Quick}
    for more details.
*/
/*!
    \property QQuickItem::parent
    This property holds the visual parent of the item.

    \note The concept of the \e {visual parent} differs from that of the
    \e {QObject parent}. An item's visual parent may not necessarily be the
    same as its object parent. See \l {Concepts - Visual Parent in Qt Quick}
    for more details.
*/
QQuickItem *QQuickItem::parentItem() const
{
    Q_D(const QQuickItem);
    return d->parentItem;
}

void QQuickItem::setParentItem(QQuickItem *parentItem)
{
    Q_D(QQuickItem);
    if (parentItem == d->parentItem)
        return;

    if (parentItem) {
        QQuickItem *itemAncestor = parentItem;
        while (itemAncestor != 0) {
            if (Q_UNLIKELY(itemAncestor == this)) {
                qWarning() << "QQuickItem::setParentItem: Parent" << parentItem << "is already part of the subtree of" << this;
                return;
            }
            itemAncestor = itemAncestor->parentItem();
        }
    }

    d->removeFromDirtyList();

    QQuickItem *oldParentItem = d->parentItem;
    QQuickItem *scopeFocusedItem = 0;

    if (oldParentItem) {
        QQuickItemPrivate *op = QQuickItemPrivate::get(oldParentItem);

        QQuickItem *scopeItem = 0;

        if (hasFocus() || op->subFocusItem == this)
            scopeFocusedItem = this;
        else if (!isFocusScope() && d->subFocusItem)
            scopeFocusedItem = d->subFocusItem;

        if (scopeFocusedItem) {
            scopeItem = oldParentItem;
            while (!scopeItem->isFocusScope() && scopeItem->parentItem())
                scopeItem = scopeItem->parentItem();
            if (d->window) {
                QQuickWindowPrivate::get(d->window)->clearFocusInScope(scopeItem, scopeFocusedItem, Qt::OtherFocusReason,
                                                                QQuickWindowPrivate::DontChangeFocusProperty);
                if (scopeFocusedItem != this)
                    QQuickItemPrivate::get(scopeFocusedItem)->updateSubFocusItem(this, true);
            } else {
                QQuickItemPrivate::get(scopeFocusedItem)->updateSubFocusItem(scopeItem, false);
            }
        }

        const bool wasVisible = isVisible();
        op->removeChild(this);
        if (wasVisible) {
            emit oldParentItem->visibleChildrenChanged();
        }
    } else if (d->window) {
        QQuickWindowPrivate::get(d->window)->parentlessItems.remove(this);
    }

    QQuickWindow *parentWindow = parentItem ? QQuickItemPrivate::get(parentItem)->window : 0;
    if (d->window == parentWindow) {
        // Avoid freeing and reallocating resources if the window stays the same.
        d->parentItem = parentItem;
    } else {
        if (d->window)
            d->derefWindow();
        d->parentItem = parentItem;
        if (parentWindow)
            d->refWindow(parentWindow);
    }

    d->dirty(QQuickItemPrivate::ParentChanged);

    if (d->parentItem)
        QQuickItemPrivate::get(d->parentItem)->addChild(this);
    else if (d->window)
        QQuickWindowPrivate::get(d->window)->parentlessItems.insert(this);

    d->setEffectiveVisibleRecur(d->calcEffectiveVisible());
    d->setEffectiveEnableRecur(0, d->calcEffectiveEnable());

    if (d->parentItem) {
        if (!scopeFocusedItem) {
            if (hasFocus())
                scopeFocusedItem = this;
            else if (!isFocusScope() && d->subFocusItem)
                scopeFocusedItem = d->subFocusItem;
        }

        if (scopeFocusedItem) {
            // We need to test whether this item becomes scope focused
            QQuickItem *scopeItem = d->parentItem;
            while (!scopeItem->isFocusScope() && scopeItem->parentItem())
                scopeItem = scopeItem->parentItem();

            if (QQuickItemPrivate::get(scopeItem)->subFocusItem
                    || (!scopeItem->isFocusScope() && scopeItem->hasFocus())) {
                if (scopeFocusedItem != this)
                    QQuickItemPrivate::get(scopeFocusedItem)->updateSubFocusItem(this, false);
                QQuickItemPrivate::get(scopeFocusedItem)->focus = false;
                emit scopeFocusedItem->focusChanged(false);
            } else {
                if (d->window) {
                    QQuickWindowPrivate::get(d->window)->setFocusInScope(scopeItem, scopeFocusedItem, Qt::OtherFocusReason,
                                                                  QQuickWindowPrivate::DontChangeFocusProperty);
                } else {
                    QQuickItemPrivate::get(scopeFocusedItem)->updateSubFocusItem(scopeItem, true);
                }
            }
        }
    }

    if (d->parentItem)
        d->resolveLayoutMirror();

    d->itemChange(ItemParentHasChanged, d->parentItem);

    emit parentChanged(d->parentItem);
    if (isVisible() && d->parentItem)
        emit d->parentItem->visibleChildrenChanged();
}

/*!
    Moves the specified \a sibling item to the index before this item
    within the list of children. The order of children affects both the
    visual stacking order and tab focus navigation order.

    Assuming the z values of both items are the same, this will cause \a
    sibling to be rendered above this item.

    If both items have activeFocusOnTab set to \c true, this will also cause
    the tab focus order to change, with \a sibling receiving focus after this
    item.

    The given \a sibling must be a sibling of this item; that is, they must
    have the same immediate \l parent.

    \sa {Concepts - Visual Parent in Qt Quick}
*/
void QQuickItem::stackBefore(const QQuickItem *sibling)
{
    Q_D(QQuickItem);
    if (!sibling || sibling == this || !d->parentItem || d->parentItem != QQuickItemPrivate::get(sibling)->parentItem) {
        qWarning("QQuickItem::stackBefore: Cannot stack before %p, which must be a sibling", sibling);
        return;
    }

    QQuickItemPrivate *parentPrivate = QQuickItemPrivate::get(d->parentItem);

    int myIndex = parentPrivate->childItems.lastIndexOf(this);
    int siblingIndex = parentPrivate->childItems.lastIndexOf(const_cast<QQuickItem *>(sibling));

    Q_ASSERT(myIndex != -1 && siblingIndex != -1);

    if (myIndex == siblingIndex - 1)
        return;

    parentPrivate->childItems.move(myIndex, myIndex < siblingIndex ? siblingIndex - 1 : siblingIndex);

    parentPrivate->dirty(QQuickItemPrivate::ChildrenStackingChanged);
    parentPrivate->markSortedChildrenDirty(this);

    for (int ii = qMin(siblingIndex, myIndex); ii < parentPrivate->childItems.count(); ++ii)
        QQuickItemPrivate::get(parentPrivate->childItems.at(ii))->siblingOrderChanged();
}

/*!
    Moves the specified \a sibling item to the index after this item
    within the list of children. The order of children affects both the
    visual stacking order and tab focus navigation order.

    Assuming the z values of both items are the same, this will cause \a
    sibling to be rendered below this item.

    If both items have activeFocusOnTab set to \c true, this will also cause
    the tab focus order to change, with \a sibling receiving focus before this
    item.

    The given \a sibling must be a sibling of this item; that is, they must
    have the same immediate \l parent.

    \sa {Concepts - Visual Parent in Qt Quick}
*/
void QQuickItem::stackAfter(const QQuickItem *sibling)
{
    Q_D(QQuickItem);
    if (!sibling || sibling == this || !d->parentItem || d->parentItem != QQuickItemPrivate::get(sibling)->parentItem) {
        qWarning("QQuickItem::stackAfter: Cannot stack after %p, which must be a sibling", sibling);
        return;
    }

    QQuickItemPrivate *parentPrivate = QQuickItemPrivate::get(d->parentItem);

    int myIndex = parentPrivate->childItems.lastIndexOf(this);
    int siblingIndex = parentPrivate->childItems.lastIndexOf(const_cast<QQuickItem *>(sibling));

    Q_ASSERT(myIndex != -1 && siblingIndex != -1);

    if (myIndex == siblingIndex + 1)
        return;

    parentPrivate->childItems.move(myIndex, myIndex > siblingIndex ? siblingIndex + 1 : siblingIndex);

    parentPrivate->dirty(QQuickItemPrivate::ChildrenStackingChanged);
    parentPrivate->markSortedChildrenDirty(this);

    for (int ii = qMin(myIndex, siblingIndex + 1); ii < parentPrivate->childItems.count(); ++ii)
        QQuickItemPrivate::get(parentPrivate->childItems.at(ii))->siblingOrderChanged();
}

/*! \fn void QQuickItem::windowChanged(QQuickWindow *window)
    This signal is emitted when the item's \a window changes.
*/

/*!
  Returns the window in which this item is rendered.

  The item does not have a window until it has been assigned into a scene. The
  \l windowChanged() signal provides a notification both when the item is entered
  into a scene and when it is removed from a scene.
  */
QQuickWindow *QQuickItem::window() const
{
    Q_D(const QQuickItem);
    return d->window;
}

static bool itemZOrder_sort(QQuickItem *lhs, QQuickItem *rhs)
{
    return lhs->z() < rhs->z();
}

QList<QQuickItem *> QQuickItemPrivate::paintOrderChildItems() const
{
    if (sortedChildItems)
        return *sortedChildItems;

    // If none of the items have set Z then the paint order list is the same as
    // the childItems list.  This is by far the most common case.
    bool haveZ = false;
    for (QQuickItem *childItem : qAsConst(childItems)) {
        if (QQuickItemPrivate::get(childItem)->z() != 0.) {
            haveZ = true;
            break;
        }
    }
    if (haveZ) {
        sortedChildItems = new QList<QQuickItem*>(childItems);
        std::stable_sort(sortedChildItems->begin(), sortedChildItems->end(), itemZOrder_sort);
        return *sortedChildItems;
    }

    sortedChildItems = const_cast<QList<QQuickItem*>*>(&childItems);

    return childItems;
}

void QQuickItemPrivate::addChild(QQuickItem *child)
{
    Q_Q(QQuickItem);

    Q_ASSERT(!childItems.contains(child));

    childItems.append(child);

#ifndef QT_NO_CURSOR
    QQuickItemPrivate *childPrivate = QQuickItemPrivate::get(child);

    // if the added child has a cursor and we do not currently have any children
    // with cursors, bubble the notification up
    if (childPrivate->subtreeCursorEnabled && !subtreeCursorEnabled)
        setHasCursorInChild(true);
#endif
    if (childPrivate->subtreeHoverEnabled && !subtreeHoverEnabled)
        setHasHoverInChild(true);

    markSortedChildrenDirty(child);
    dirty(QQuickItemPrivate::ChildrenChanged);

    itemChange(QQuickItem::ItemChildAddedChange, child);

    emit q->childrenChanged();
}

void QQuickItemPrivate::removeChild(QQuickItem *child)
{
    Q_Q(QQuickItem);

    Q_ASSERT(child);
    Q_ASSERT(childItems.contains(child));
    childItems.removeOne(child);
    Q_ASSERT(!childItems.contains(child));

#ifndef QT_NO_CURSOR
    QQuickItemPrivate *childPrivate = QQuickItemPrivate::get(child);

    // turn it off, if nothing else is using it
    if (childPrivate->subtreeCursorEnabled && subtreeCursorEnabled)
        setHasCursorInChild(false);
#endif
    if (childPrivate->subtreeHoverEnabled && subtreeHoverEnabled)
        setHasHoverInChild(false);

    markSortedChildrenDirty(child);
    dirty(QQuickItemPrivate::ChildrenChanged);

    itemChange(QQuickItem::ItemChildRemovedChange, child);

    emit q->childrenChanged();
}

void QQuickItemPrivate::refWindow(QQuickWindow *c)
{
    // An item needs a window if it is referenced by another item which has a window.
    // Typically the item is referenced by a parent, but can also be referenced by a
    // ShaderEffect or ShaderEffectSource. 'windowRefCount' counts how many items with
    // a window is referencing this item. When the reference count goes from zero to one,
    // or one to zero, the window of this item is updated and propagated to the children.
    // As long as the reference count stays above zero, the window is unchanged.
    // refWindow() increments the reference count.
    // derefWindow() decrements the reference count.

    Q_Q(QQuickItem);
    Q_ASSERT((window != 0) == (windowRefCount > 0));
    Q_ASSERT(c);
    if (++windowRefCount > 1) {
        if (c != window)
            qWarning("QQuickItem: Cannot use same item on different windows at the same time.");
        return; // Window already set.
    }

    Q_ASSERT(window == 0);
    window = c;

    if (polishScheduled)
        QQuickWindowPrivate::get(window)->itemsToPolish.append(q);

    if (!parentItem)
        QQuickWindowPrivate::get(window)->parentlessItems.insert(q);

    for (QQuickItem *child : qAsConst(childItems)) {
        QQuickItemPrivate::get(child)->refWindow(c);
    }

    dirty(Window);

    if (extra.isAllocated() && extra->screenAttached)
        extra->screenAttached->windowChanged(c);
    itemChange(QQuickItem::ItemSceneChange, c);
}

void QQuickItemPrivate::derefWindow()
{
    Q_Q(QQuickItem);
    Q_ASSERT((window != 0) == (windowRefCount > 0));

    if (!window)
        return; // This can happen when destroying recursive shader effect sources.

    if (--windowRefCount > 0)
        return; // There are still other references, so don't set window to null yet.

    q->releaseResources();
    removeFromDirtyList();
    QQuickWindowPrivate *c = QQuickWindowPrivate::get(window);
    if (polishScheduled)
        c->itemsToPolish.removeOne(q);
    c->removeGrabber(q);
#ifndef QT_NO_CURSOR
    if (c->cursorItem == q) {
        c->cursorItem = 0;
        window->unsetCursor();
    }
#endif
    c->hoverItems.removeAll(q);
    if (itemNodeInstance)
        c->cleanup(itemNodeInstance);
    if (!parentItem)
        c->parentlessItems.remove(q);

    window = 0;

    itemNodeInstance = 0;

    if (extra.isAllocated()) {
        extra->opacityNode = 0;
        extra->clipNode = 0;
        extra->rootNode = 0;
    }

    paintNode = 0;

    for (QQuickItem *child : qAsConst(childItems)) {
        QQuickItemPrivate::get(child)->derefWindow();
    }

    dirty(Window);

    if (extra.isAllocated() && extra->screenAttached)
        extra->screenAttached->windowChanged(0);
    itemChange(QQuickItem::ItemSceneChange, (QQuickWindow *)0);
}


/*!
Returns a transform that maps points from window space into item space.
*/
QTransform QQuickItemPrivate::windowToItemTransform() const
{
    // XXX todo - optimize
    return itemToWindowTransform().inverted();
}

/*!
Returns a transform that maps points from item space into window space.
*/
QTransform QQuickItemPrivate::itemToWindowTransform() const
{
    // XXX todo
    QTransform rv = parentItem?QQuickItemPrivate::get(parentItem)->itemToWindowTransform():QTransform();
    itemToParentTransform(rv);
    return rv;
}

/*!
Motifies \a t with this items local transform relative to its parent.
*/
void QQuickItemPrivate::itemToParentTransform(QTransform &t) const
{
    if (x || y)
        t.translate(x, y);

    if (!transforms.isEmpty()) {
        QMatrix4x4 m(t);
        for (int ii = transforms.count() - 1; ii >= 0; --ii)
            transforms.at(ii)->applyTo(&m);
        t = m.toTransform();
    }

    if (scale() != 1. || rotation() != 0.) {
        QPointF tp = computeTransformOrigin();
        t.translate(tp.x(), tp.y());
        t.scale(scale(), scale());
        t.rotate(rotation());
        t.translate(-tp.x(), -tp.y());
    }
}

/*!
    Returns a transform that maps points from window space into global space.
*/
QTransform QQuickItemPrivate::windowToGlobalTransform() const
{
    QPoint quickWidgetOffset;
    QWindow *renderWindow = QQuickRenderControl::renderWindowFor(window, &quickWidgetOffset);
    QPointF pos = (renderWindow ? renderWindow : window)->mapToGlobal(quickWidgetOffset);
    return QTransform::fromTranslate(pos.x(), pos.y());
}

/*!
    Returns a transform that maps points from global space into window space.
*/
QTransform QQuickItemPrivate::globalToWindowTransform() const
{
    QPoint quickWidgetOffset;
    QWindow *renderWindow = QQuickRenderControl::renderWindowFor(window, &quickWidgetOffset);
    QPointF pos = (renderWindow ? renderWindow : window)->mapToGlobal(quickWidgetOffset);
    return QTransform::fromTranslate(-pos.x(), -pos.y());
}

/*!
    Returns true if construction of the QML component is complete; otherwise
    returns false.

    It is often desirable to delay some processing until the component is
    completed.

    \sa componentComplete()
*/
bool QQuickItem::isComponentComplete() const
{
    Q_D(const QQuickItem);
    return d->componentComplete;
}

QQuickItemPrivate::QQuickItemPrivate()
    : _anchors(0)
    , _stateGroup(0)
    , flags(0)
    , widthValid(false)
    , heightValid(false)
    , componentComplete(true)
    , keepMouse(false)
    , keepTouch(false)
    , hoverEnabled(false)
    , smooth(true)
    , antialiasing(false)
    , focus(false)
    , activeFocus(false)
    , notifiedFocus(false)
    , notifiedActiveFocus(false)
    , filtersChildMouseEvents(false)
    , explicitVisible(true)
    , effectiveVisible(true)
    , explicitEnable(true)
    , effectiveEnable(true)
    , polishScheduled(false)
    , inheritedLayoutMirror(false)
    , effectiveLayoutMirror(false)
    , isMirrorImplicit(true)
    , inheritMirrorFromParent(false)
    , inheritMirrorFromItem(false)
    , isAccessible(false)
    , culled(false)
    , hasCursor(false)
    , subtreeCursorEnabled(false)
    , subtreeHoverEnabled(false)
    , activeFocusOnTab(false)
    , implicitAntialiasing(false)
    , antialiasingValid(false)
    , isTabFence(false)
    , replayingPressEvent(false)
    , dirtyAttributes(0)
    , nextDirtyItem(0)
    , prevDirtyItem(0)
    , window(0)
    , windowRefCount(0)
    , parentItem(0)
    , sortedChildItems(&childItems)
    , subFocusItem(0)
    , x(0)
    , y(0)
    , width(0)
    , height(0)
    , implicitWidth(0)
    , implicitHeight(0)
    , baselineOffset(0)
    , itemNodeInstance(0)
    , paintNode(0)
{
}

QQuickItemPrivate::~QQuickItemPrivate()
{
    if (sortedChildItems != &childItems)
        delete sortedChildItems;
}

void QQuickItemPrivate::init(QQuickItem *parent)
{
    Q_Q(QQuickItem);

    baselineOffset = 0.0;

    if (parent) {
        q->setParentItem(parent);
        QQuickItemPrivate *parentPrivate = QQuickItemPrivate::get(parent);
        setImplicitLayoutMirror(parentPrivate->inheritedLayoutMirror, parentPrivate->inheritMirrorFromParent);
    }
}

void QQuickItemPrivate::data_append(QQmlListProperty<QObject> *prop, QObject *o)
{
    if (!o)
        return;

    QQuickItem *that = static_cast<QQuickItem *>(prop->object);

    if (QQuickItem *item = qmlobject_cast<QQuickItem *>(o)) {
        item->setParentItem(that);
    } else {
        if (o->inherits("QGraphicsItem"))
            qWarning("Cannot add a QtQuick 1.0 item (%s) into a QtQuick 2.0 scene!", o->metaObject()->className());
        else {
            QQuickWindow *thisWindow = qmlobject_cast<QQuickWindow *>(o);
            QQuickItem *item = that;
            QQuickWindow *itemWindow = that->window();
            while (!itemWindow && item && item->parentItem()) {
                item = item->parentItem();
                itemWindow = item->window();
            }

            if (thisWindow) {
                if (itemWindow)
                    thisWindow->setTransientParent(itemWindow);
                else
                    QObject::connect(item, SIGNAL(windowChanged(QQuickWindow*)),
                                     thisWindow, SLOT(setTransientParent_helper(QQuickWindow*)));
            }
            o->setParent(that);
        }

        resources_append(prop, o);
    }
}

/*!
    \qmlproperty list<Object> QtQuick::Item::data
    \default

    The data property allows you to freely mix visual children and resources
    in an item.  If you assign a visual item to the data list it becomes
    a child and if you assign any other object type, it is added as a resource.

    So you can write:
    \qml
    Item {
        Text {}
        Rectangle {}
        Timer {}
    }
    \endqml

    instead of:
    \qml
    Item {
        children: [
            Text {},
            Rectangle {}
        ]
        resources: [
            Timer {}
        ]
    }
    \endqml

    It should not generally be necessary to refer to the \c data property,
    as it is the default property for Item and thus all child items are
    automatically assigned to this property.
 */

int QQuickItemPrivate::data_count(QQmlListProperty<QObject> *property)
{
    QQuickItem *item = static_cast<QQuickItem*>(property->object);
    QQuickItemPrivate *privateItem = QQuickItemPrivate::get(item);
    QQmlListProperty<QObject> resourcesProperty = privateItem->resources();
    QQmlListProperty<QQuickItem> childrenProperty = privateItem->children();

    return resources_count(&resourcesProperty) + children_count(&childrenProperty);
}

QObject *QQuickItemPrivate::data_at(QQmlListProperty<QObject> *property, int i)
{
    QQuickItem *item = static_cast<QQuickItem*>(property->object);
    QQuickItemPrivate *privateItem = QQuickItemPrivate::get(item);
    QQmlListProperty<QObject> resourcesProperty = privateItem->resources();
    QQmlListProperty<QQuickItem> childrenProperty = privateItem->children();

    int resourcesCount = resources_count(&resourcesProperty);
    if (i < resourcesCount)
        return resources_at(&resourcesProperty, i);
    const int j = i - resourcesCount;
    if (j < children_count(&childrenProperty))
        return children_at(&childrenProperty, j);
    return 0;
}

void QQuickItemPrivate::data_clear(QQmlListProperty<QObject> *property)
{
    QQuickItem *item = static_cast<QQuickItem*>(property->object);
    QQuickItemPrivate *privateItem = QQuickItemPrivate::get(item);
    QQmlListProperty<QObject> resourcesProperty = privateItem->resources();
    QQmlListProperty<QQuickItem> childrenProperty = privateItem->children();

    resources_clear(&resourcesProperty);
    children_clear(&childrenProperty);
}

QObject *QQuickItemPrivate::resources_at(QQmlListProperty<QObject> *prop, int index)
{
    QQuickItemPrivate *quickItemPrivate = QQuickItemPrivate::get(static_cast<QQuickItem *>(prop->object));
    return quickItemPrivate->extra.isAllocated() ? quickItemPrivate->extra->resourcesList.value(index) : 0;
}

void QQuickItemPrivate::resources_append(QQmlListProperty<QObject> *prop, QObject *object)
{
    QQuickItem *quickItem = static_cast<QQuickItem *>(prop->object);
    QQuickItemPrivate *quickItemPrivate = QQuickItemPrivate::get(quickItem);
    if (!quickItemPrivate->extra.value().resourcesList.contains(object)) {
        quickItemPrivate->extra.value().resourcesList.append(object);
        qmlobject_connect(object, QObject, SIGNAL(destroyed(QObject*)),
                          quickItem, QQuickItem, SLOT(_q_resourceObjectDeleted(QObject*)));
    }
}

int QQuickItemPrivate::resources_count(QQmlListProperty<QObject> *prop)
{
    QQuickItemPrivate *quickItemPrivate = QQuickItemPrivate::get(static_cast<QQuickItem *>(prop->object));
    return  quickItemPrivate->extra.isAllocated() ? quickItemPrivate->extra->resourcesList.count() : 0;
}

void QQuickItemPrivate::resources_clear(QQmlListProperty<QObject> *prop)
{
    QQuickItem *quickItem = static_cast<QQuickItem *>(prop->object);
    QQuickItemPrivate *quickItemPrivate = QQuickItemPrivate::get(quickItem);
    if (quickItemPrivate->extra.isAllocated()) {//If extra is not allocated resources is empty.
        for (QObject *object : qAsConst(quickItemPrivate->extra->resourcesList)) {
            qmlobject_disconnect(object, QObject, SIGNAL(destroyed(QObject*)),
                                 quickItem, QQuickItem, SLOT(_q_resourceObjectDeleted(QObject*)));
        }
        quickItemPrivate->extra->resourcesList.clear();
    }
}

QQuickItem *QQuickItemPrivate::children_at(QQmlListProperty<QQuickItem> *prop, int index)
{
    QQuickItemPrivate *p = QQuickItemPrivate::get(static_cast<QQuickItem *>(prop->object));
    if (index >= p->childItems.count() || index < 0)
        return 0;
    else
        return p->childItems.at(index);
}

void QQuickItemPrivate::children_append(QQmlListProperty<QQuickItem> *prop, QQuickItem *o)
{
    if (!o)
        return;

    QQuickItem *that = static_cast<QQuickItem *>(prop->object);
    if (o->parentItem() == that)
        o->setParentItem(0);

    o->setParentItem(that);
}

int QQuickItemPrivate::children_count(QQmlListProperty<QQuickItem> *prop)
{
    QQuickItemPrivate *p = QQuickItemPrivate::get(static_cast<QQuickItem *>(prop->object));
    return p->childItems.count();
}

void QQuickItemPrivate::children_clear(QQmlListProperty<QQuickItem> *prop)
{
    QQuickItem *that = static_cast<QQuickItem *>(prop->object);
    QQuickItemPrivate *p = QQuickItemPrivate::get(that);
    while (!p->childItems.isEmpty())
        p->childItems.at(0)->setParentItem(0);
}

int QQuickItemPrivate::visibleChildren_count(QQmlListProperty<QQuickItem> *prop)
{
    QQuickItemPrivate *p = QQuickItemPrivate::get(static_cast<QQuickItem *>(prop->object));
    int visibleCount = 0;
    int c = p->childItems.count();
    while (c--) {
        if (p->childItems.at(c)->isVisible()) visibleCount++;
    }

    return visibleCount;
}

QQuickItem *QQuickItemPrivate::visibleChildren_at(QQmlListProperty<QQuickItem> *prop, int index)
{
    QQuickItemPrivate *p = QQuickItemPrivate::get(static_cast<QQuickItem *>(prop->object));
    const int childCount = p->childItems.count();
    if (index >= childCount || index < 0)
        return 0;

    int visibleCount = -1;
    for (int i = 0; i < childCount; i++) {
        if (p->childItems.at(i)->isVisible()) visibleCount++;
        if (visibleCount == index) return p->childItems.at(i);
    }
    return 0;
}

int QQuickItemPrivate::transform_count(QQmlListProperty<QQuickTransform> *prop)
{
    QQuickItem *that = static_cast<QQuickItem *>(prop->object);
    QQuickItemPrivate *p = QQuickItemPrivate::get(that);

    return p->transforms.count();
}

void QQuickTransform::appendToItem(QQuickItem *item)
{
    Q_D(QQuickTransform);
    if (!item)
        return;

    QQuickItemPrivate *p = QQuickItemPrivate::get(item);

    if (!d->items.isEmpty() && !p->transforms.isEmpty() && p->transforms.contains(this)) {
        p->transforms.removeOne(this);
        p->transforms.append(this);
    } else {
        p->transforms.append(this);
        d->items.append(item);
    }

    p->dirty(QQuickItemPrivate::Transform);
}

void QQuickTransform::prependToItem(QQuickItem *item)
{
    Q_D(QQuickTransform);
    if (!item)
        return;

    QQuickItemPrivate *p = QQuickItemPrivate::get(item);

    if (!d->items.isEmpty() && !p->transforms.isEmpty() && p->transforms.contains(this)) {
        p->transforms.removeOne(this);
        p->transforms.prepend(this);
    } else {
        p->transforms.prepend(this);
        d->items.append(item);
    }

    p->dirty(QQuickItemPrivate::Transform);
}

void QQuickItemPrivate::transform_append(QQmlListProperty<QQuickTransform> *prop, QQuickTransform *transform)
{
    if (!transform)
        return;

    QQuickItem *that = static_cast<QQuickItem *>(prop->object);
    transform->appendToItem(that);
}

QQuickTransform *QQuickItemPrivate::transform_at(QQmlListProperty<QQuickTransform> *prop, int idx)
{
    QQuickItem *that = static_cast<QQuickItem *>(prop->object);
    QQuickItemPrivate *p = QQuickItemPrivate::get(that);

    if (idx < 0 || idx >= p->transforms.count())
        return 0;
    else
        return p->transforms.at(idx);
}

void QQuickItemPrivate::transform_clear(QQmlListProperty<QQuickTransform> *prop)
{
    QQuickItem *that = static_cast<QQuickItem *>(prop->object);
    QQuickItemPrivate *p = QQuickItemPrivate::get(that);

    for (QQuickTransform *t : qAsConst(p->transforms)) {
        QQuickTransformPrivate *tp = QQuickTransformPrivate::get(t);
        tp->items.removeOne(that);
    }

    p->transforms.clear();

    p->dirty(QQuickItemPrivate::Transform);
}

void QQuickItemPrivate::_q_resourceObjectDeleted(QObject *object)
{
    if (extra.isAllocated() && extra->resourcesList.contains(object))
        extra->resourcesList.removeAll(object);
}

/*!
  \qmlpropertygroup QtQuick::Item::anchors
  \qmlproperty AnchorLine QtQuick::Item::anchors.top
  \qmlproperty AnchorLine QtQuick::Item::anchors.bottom
  \qmlproperty AnchorLine QtQuick::Item::anchors.left
  \qmlproperty AnchorLine QtQuick::Item::anchors.right
  \qmlproperty AnchorLine QtQuick::Item::anchors.horizontalCenter
  \qmlproperty AnchorLine QtQuick::Item::anchors.verticalCenter
  \qmlproperty AnchorLine QtQuick::Item::anchors.baseline

  \qmlproperty Item QtQuick::Item::anchors.fill
  \qmlproperty Item QtQuick::Item::anchors.centerIn

  \qmlproperty real QtQuick::Item::anchors.margins
  \qmlproperty real QtQuick::Item::anchors.topMargin
  \qmlproperty real QtQuick::Item::anchors.bottomMargin
  \qmlproperty real QtQuick::Item::anchors.leftMargin
  \qmlproperty real QtQuick::Item::anchors.rightMargin
  \qmlproperty real QtQuick::Item::anchors.horizontalCenterOffset
  \qmlproperty real QtQuick::Item::anchors.verticalCenterOffset
  \qmlproperty real QtQuick::Item::anchors.baselineOffset

  \qmlproperty bool QtQuick::Item::anchors.alignWhenCentered

  Anchors provide a way to position an item by specifying its
  relationship with other items.

  Margins apply to top, bottom, left, right, and fill anchors.
  The \l anchors.margins property can be used to set all of the various margins at once, to the same value.
  It will not override a specific margin that has been previously set; to clear an explicit margin
  set its value to \c undefined.
  Note that margins are anchor-specific and are not applied if an item does not
  use anchors.

  Offsets apply for horizontal center, vertical center, and baseline anchors.

  \table
  \row
  \li \image declarative-anchors_example.png
  \li Text anchored to Image, horizontally centered and vertically below, with a margin.
  \qml
  Item {
      Image {
          id: pic
          // ...
      }
      Text {
          id: label
          anchors.horizontalCenter: pic.horizontalCenter
          anchors.top: pic.bottom
          anchors.topMargin: 5
          // ...
      }
  }
  \endqml
  \row
  \li \image declarative-anchors_example2.png
  \li
  Left of Text anchored to right of Image, with a margin. The y
  property of both defaults to 0.

  \qml
  Item {
      Image {
          id: pic
          // ...
      }
      Text {
          id: label
          anchors.left: pic.right
          anchors.leftMargin: 5
          // ...
      }
  }
  \endqml
  \endtable

  \l anchors.fill provides a convenient way for one item to have the
  same geometry as another item, and is equivalent to connecting all
  four directional anchors.

  To clear an anchor value, set it to \c undefined.

  \l anchors.alignWhenCentered (default \c true) forces centered anchors to align to a
  whole pixel; if the item being centered has an odd \l width or \l height, the item
  will be positioned on a whole pixel rather than being placed on a half-pixel.
  This ensures the item is painted crisply.  There are cases where this is not
  desirable, for example when rotating the item jitters may be apparent as the
  center is rounded.

  \note You can only anchor an item to siblings or a parent.

  For more information see \l {anchor-layout}{Anchor Layouts}.
*/
QQuickAnchors *QQuickItemPrivate::anchors() const
{
    if (!_anchors) {
        Q_Q(const QQuickItem);
        _anchors = new QQuickAnchors(const_cast<QQuickItem *>(q));
        if (!componentComplete)
            _anchors->classBegin();
    }
    return _anchors;
}

void QQuickItemPrivate::siblingOrderChanged()
{
    Q_Q(QQuickItem);
    if (!changeListeners.isEmpty()) {
        const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
        for (const QQuickItemPrivate::ChangeListener &change : listeners) {
            if (change.types & QQuickItemPrivate::SiblingOrder) {
                change.listener->itemSiblingOrderChanged(q);
            }
        }
    }
}

QQmlListProperty<QObject> QQuickItemPrivate::data()
{
    return QQmlListProperty<QObject>(q_func(), 0, QQuickItemPrivate::data_append,
                                             QQuickItemPrivate::data_count,
                                             QQuickItemPrivate::data_at,
                                             QQuickItemPrivate::data_clear);
}

/*!
    \qmlpropertygroup QtQuick::Item::childrenRect
    \qmlproperty real QtQuick::Item::childrenRect.x
    \qmlproperty real QtQuick::Item::childrenRect.y
    \qmlproperty real QtQuick::Item::childrenRect.width
    \qmlproperty real QtQuick::Item::childrenRect.height

    This property holds the collective position and size of the item's
    children.

    This property is useful if you need to access the collective geometry
    of an item's children in order to correctly size the item.
*/
/*!
    \property QQuickItem::childrenRect

    This property holds the collective position and size of the item's
    children.

    This property is useful if you need to access the collective geometry
    of an item's children in order to correctly size the item.
*/
QRectF QQuickItem::childrenRect()
{
    Q_D(QQuickItem);
    if (!d->extra.isAllocated() || !d->extra->contents) {
        d->extra.value().contents = new QQuickContents(this);
        if (d->componentComplete)
            d->extra->contents->complete();
    }
    return d->extra->contents->rectF();
}

/*!
    Returns the children of this item.
  */
QList<QQuickItem *> QQuickItem::childItems() const
{
    Q_D(const QQuickItem);
    return d->childItems;
}

/*!
  \qmlproperty bool QtQuick::Item::clip
  This property holds whether clipping is enabled. The default clip value is \c false.

  If clipping is enabled, an item will clip its own painting, as well
  as the painting of its children, to its bounding rectangle.
*/
/*!
  \property QQuickItem::clip
  This property holds whether clipping is enabled. The default clip value is \c false.

  If clipping is enabled, an item will clip its own painting, as well
  as the painting of its children, to its bounding rectangle. If you set
  clipping during an item's paint operation, remember to re-set it to
  prevent clipping the rest of your scene.
*/
bool QQuickItem::clip() const
{
    return flags() & ItemClipsChildrenToShape;
}

void QQuickItem::setClip(bool c)
{
    if (clip() == c)
        return;

    setFlag(ItemClipsChildrenToShape, c);

    emit clipChanged(c);
}


/*!
  This function is called to handle this item's changes in
  geometry from \a oldGeometry to \a newGeometry. If the two
  geometries are the same, it doesn't do anything.

  Derived classes must call the base class method within their implementation.
 */
void QQuickItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickItem);

    if (d->_anchors)
        QQuickAnchorsPrivate::get(d->_anchors)->updateMe();

    QQuickGeometryChange change;
    change.setXChange(newGeometry.x() != oldGeometry.x());
    change.setYChange(newGeometry.y() != oldGeometry.y());
    change.setWidthChange(newGeometry.width() != oldGeometry.width());
    change.setHeightChange(newGeometry.height() != oldGeometry.height());

    if (!d->changeListeners.isEmpty()) {
        const auto listeners = d->changeListeners; // NOTE: intentional copy (QTBUG-54732)
        for (const QQuickItemPrivate::ChangeListener &listener : listeners) {
            if (listener.types & QQuickItemPrivate::Geometry) {
                if (change.matches(listener.gTypes))
                    listener.listener->itemGeometryChanged(this, change, oldGeometry);
            }
        }
    }

    if (change.xChange())
        emit xChanged();
    if (change.yChange())
        emit yChanged();
    if (change.widthChange())
        emit widthChanged();
    if (change.heightChange())
        emit heightChanged();
}

/*!
    Called on the render thread when it is time to sync the state
    of the item with the scene graph.

    The function is called as a result of QQuickItem::update(), if
    the user has set the QQuickItem::ItemHasContents flag on the item.

    The function should return the root of the scene graph subtree for
    this item. Most implementations will return a single
    QSGGeometryNode containing the visual representation of this item.
    \a oldNode is the node that was returned the last time the
    function was called. \a updatePaintNodeData provides a pointer to
    the QSGTransformNode associated with this QQuickItem.

    \code
    QSGNode *MyItem::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
    {
        QSGSimpleRectNode *n = static_cast<QSGSimpleRectNode *>(node);
        if (!n) {
            n = new QSGSimpleRectNode();
            n->setColor(Qt::red);
        }
        n->setRect(boundingRect());
        return n;
    }
    \endcode

    The main thread is blocked while this function is executed so it is safe to read
    values from the QQuickItem instance and other objects in the main thread.

    If no call to QQuickItem::updatePaintNode() result in actual scene graph
    changes, like QSGNode::markDirty() or adding and removing nodes, then
    the underlying implementation may decide to not render the scene again as
    the visual outcome is identical.

    \warning It is crucial that OpenGL operations and interaction with
    the scene graph happens exclusively on the render thread,
    primarily during the QQuickItem::updatePaintNode() call. The best
    rule of thumb is to only use classes with the "QSG" prefix inside
    the QQuickItem::updatePaintNode() function.

    \warning This function is called on the render thread. This means any
    QObjects or thread local storage that is created will have affinity to the
    render thread, so apply caution when doing anything other than rendering
    in this function. Similarly for signals, these will be emitted on the render
    thread and will thus often be delivered via queued connections.

    \note All classes with QSG prefix should be used solely on the scene graph's
    rendering thread. See \l {Scene Graph and Rendering} for more information.

    \sa QSGMaterial, QSGSimpleMaterial, QSGGeometryNode, QSGGeometry,
    QSGFlatColorMaterial, QSGTextureMaterial, QSGNode::markDirty(), {Graphics Resource Handling}
 */

QSGNode *QQuickItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData)
{
    Q_UNUSED(updatePaintNodeData)
    delete oldNode;
    return 0;
}

QQuickItem::UpdatePaintNodeData::UpdatePaintNodeData()
: transformNode(0)
{
}

/*!
    This function is called when an item should release graphics
    resources which are not already managed by the nodes returend from
    QQuickItem::updatePaintNode().

    This happens when the item is about to be removed from window it
    was previously rendering to. The item is guaranteed to have a
    \l {QQuickItem::window()}{window} when the function is called.

    The function is called on the GUI thread and the state of the
    rendering thread, when it is used, is unknown. Objects should
    not be deleted directly, but instead scheduled for cleanup
    using QQuickWindow::scheduleRenderJob().

    \sa {Graphics Resource Handling}
 */

void QQuickItem::releaseResources()
{
}

QSGTransformNode *QQuickItemPrivate::createTransformNode()
{
    return new QSGTransformNode;
}

/*!
    This function should perform any layout as required for this item.

    When polish() is called, the scene graph schedules a polish event for this
    item. When the scene graph is ready to render this item, it calls
    updatePolish() to do any item layout as required before it renders the
    next frame.
  */
void QQuickItem::updatePolish()
{
}

void QQuickItemPrivate::addItemChangeListener(QQuickItemChangeListener *listener, ChangeTypes types)
{
    changeListeners.append(ChangeListener(listener, types));
}

void QQuickItemPrivate::removeItemChangeListener(QQuickItemChangeListener *listener, ChangeTypes types)
{
    ChangeListener change(listener, types);
    changeListeners.removeOne(change);
}

void QQuickItemPrivate::updateOrAddGeometryChangeListener(QQuickItemChangeListener *listener,
                                                          QQuickGeometryChange types)
{
    ChangeListener change(listener, types);
    int index = changeListeners.indexOf(change);
    if (index > -1)
        changeListeners[index].gTypes = change.gTypes;  //we may have different GeometryChangeTypes
    else
        changeListeners.append(change);
}

void QQuickItemPrivate::updateOrRemoveGeometryChangeListener(QQuickItemChangeListener *listener,
                                                             QQuickGeometryChange types)
{
    ChangeListener change(listener, types);
    if (types.noChange()) {
        changeListeners.removeOne(change);
    } else {
        int index = changeListeners.indexOf(change);
        if (index > -1)
            changeListeners[index].gTypes = change.gTypes;  //we may have different GeometryChangeTypes
    }
}

/*!
    This event handler can be reimplemented in a subclass to receive key
    press events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::keyPressEvent(QKeyEvent *event)
{
    event->ignore();
}

/*!
    This event handler can be reimplemented in a subclass to receive key
    release events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::keyReleaseEvent(QKeyEvent *event)
{
    event->ignore();
}

#ifndef QT_NO_IM
/*!
    This event handler can be reimplemented in a subclass to receive input
    method events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::inputMethodEvent(QInputMethodEvent *event)
{
    event->ignore();
}
#endif // QT_NO_IM

/*!
    This event handler can be reimplemented in a subclass to receive focus-in
    events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::focusInEvent(QFocusEvent * /*event*/)
{
#ifndef QT_NO_ACCESSIBILITY
    if (QAccessible::isActive()) {
        if (QObject *acc = QQuickAccessibleAttached::findAccessible(this)) {
            QAccessibleEvent ev(acc, QAccessible::Focus);
            QAccessible::updateAccessibility(&ev);
        }
    }
#endif
}

/*!
    This event handler can be reimplemented in a subclass to receive focus-out
    events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::focusOutEvent(QFocusEvent * /*event*/)
{
}

/*!
    This event handler can be reimplemented in a subclass to receive mouse
    press events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::mousePressEvent(QMouseEvent *event)
{
    event->ignore();
}

/*!
    This event handler can be reimplemented in a subclass to receive mouse
    move events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::mouseMoveEvent(QMouseEvent *event)
{
    event->ignore();
}

/*!
    This event handler can be reimplemented in a subclass to receive mouse
    release events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::mouseReleaseEvent(QMouseEvent *event)
{
    event->ignore();
}

/*!
    This event handler can be reimplemented in a subclass to receive mouse
    double-click events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::mouseDoubleClickEvent(QMouseEvent *)
{
}

/*!
    This event handler can be reimplemented in a subclass to be notified
    when a mouse ungrab event has occurred on this item.

    \sa ungrabMouse()
  */
void QQuickItem::mouseUngrabEvent()
{
    // XXX todo
}

/*!
    This event handler can be reimplemented in a subclass to be notified
    when a touch ungrab event has occurred on this item.
  */
void QQuickItem::touchUngrabEvent()
{
    // XXX todo
}

#ifndef QT_NO_WHEELEVENT
/*!
    This event handler can be reimplemented in a subclass to receive
    wheel events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}
#endif

/*!
    This event handler can be reimplemented in a subclass to receive touch
    events for an item. The event information is provided by the
    \a event parameter.
  */
void QQuickItem::touchEvent(QTouchEvent *event)
{
    event->ignore();
}

/*!
    This event handler can be reimplemented in a subclass to receive hover-enter
    events for an item. The event information is provided by the
    \a event parameter.

    Hover events are only provided if acceptHoverEvents() is true.
  */
void QQuickItem::hoverEnterEvent(QHoverEvent *event)
{
    Q_UNUSED(event);
}

/*!
    This event handler can be reimplemented in a subclass to receive hover-move
    events for an item. The event information is provided by the
    \a event parameter.

    Hover events are only provided if acceptHoverEvents() is true.
  */
void QQuickItem::hoverMoveEvent(QHoverEvent *event)
{
    Q_UNUSED(event);
}

/*!
    This event handler can be reimplemented in a subclass to receive hover-leave
    events for an item. The event information is provided by the
    \a event parameter.

    Hover events are only provided if acceptHoverEvents() is true.
  */
void QQuickItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_UNUSED(event);
}

#ifndef QT_NO_DRAGANDDROP
/*!
    This event handler can be reimplemented in a subclass to receive drag-enter
    events for an item. The event information is provided by the
    \a event parameter.

    Drag and drop events are only provided if the ItemAcceptsDrops flag
    has been set for this item.

    \sa Drag, {Drag and Drop}
  */
void QQuickItem::dragEnterEvent(QDragEnterEvent *event)
{
    Q_UNUSED(event);
}

/*!
    This event handler can be reimplemented in a subclass to receive drag-move
    events for an item. The event information is provided by the
    \a event parameter.

    Drag and drop events are only provided if the ItemAcceptsDrops flag
    has been set for this item.

    \sa Drag, {Drag and Drop}
  */
void QQuickItem::dragMoveEvent(QDragMoveEvent *event)
{
    Q_UNUSED(event);
}

/*!
    This event handler can be reimplemented in a subclass to receive drag-leave
    events for an item. The event information is provided by the
    \a event parameter.

    Drag and drop events are only provided if the ItemAcceptsDrops flag
    has been set for this item.

    \sa Drag, {Drag and Drop}
  */
void QQuickItem::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
}

/*!
    This event handler can be reimplemented in a subclass to receive drop
    events for an item. The event information is provided by the
    \a event parameter.

    Drag and drop events are only provided if the ItemAcceptsDrops flag
    has been set for this item.

    \sa Drag, {Drag and Drop}
  */
void QQuickItem::dropEvent(QDropEvent *event)
{
    Q_UNUSED(event);
}
#endif // QT_NO_DRAGANDDROP

/*!
    Reimplement this method to filter the mouse events that are received by
    this item's children.

    This method will only be called if filtersChildMouseEvents() is true.

    Return true if the specified \a event should not be passed onto the
    specified child \a item, and false otherwise.

    \sa setFiltersChildMouseEvents()
  */
bool QQuickItem::childMouseEventFilter(QQuickItem *item, QEvent *event)
{
    Q_UNUSED(item);
    Q_UNUSED(event);
    return false;
}

/*!
    \internal
  */
void QQuickItem::windowDeactivateEvent()
{
    const auto children = childItems();
    for (QQuickItem* item : children) {
        item->windowDeactivateEvent();
    }
}

#ifndef QT_NO_IM
/*!
    This method is only relevant for input items.

    If this item is an input item, this method should be reimplemented to
    return the relevant input method flags for the given \a query.

    \sa QWidget::inputMethodQuery()
  */
QVariant QQuickItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    Q_D(const QQuickItem);
    QVariant v;

    switch (query) {
    case Qt::ImEnabled:
        v = (bool)(flags() & ItemAcceptsInputMethod);
        break;
    case Qt::ImHints:
    case Qt::ImAnchorRectangle:
    case Qt::ImCursorRectangle:
    case Qt::ImFont:
    case Qt::ImCursorPosition:
    case Qt::ImSurroundingText:
    case Qt::ImCurrentSelection:
    case Qt::ImMaximumTextLength:
    case Qt::ImAnchorPosition:
    case Qt::ImPreferredLanguage:
        if (d->extra.isAllocated() && d->extra->keyHandler)
            v = d->extra->keyHandler->inputMethodQuery(query);
        break;
    case Qt::ImEnterKeyType:
        if (d->extra.isAllocated() && d->extra->enterKeyAttached)
            v = d->extra->enterKeyAttached->type();
        break;
    case Qt::ImInputItemClipRectangle:
        if (!(!window() ||!isVisible() || qFuzzyIsNull(opacity()))) {
            QRectF rect = QRectF(0,0, width(), height());
            const QQuickItem *par = this;
            while (QQuickItem *parpar = par->parentItem()) {
                rect = parpar->mapRectFromItem(par, rect);
                if (parpar->clip())
                    rect = rect.intersected(parpar->clipRect());
                par = parpar;
            }
            rect = par->mapRectToScene(rect);
            // once we have the rect in scene coordinates, clip to window
            rect = rect.intersected(QRectF(QPoint(0,0), window()->size()));
            // map it back to local coordinates
            v = mapRectFromScene(rect);
        }
        break;
    default:
        break;
    }

    return v;
}
#endif // QT_NO_IM

QQuickAnchorLine QQuickItemPrivate::left() const
{
    Q_Q(const QQuickItem);
    return QQuickAnchorLine(const_cast<QQuickItem *>(q), QQuickAnchors::LeftAnchor);
}

QQuickAnchorLine QQuickItemPrivate::right() const
{
    Q_Q(const QQuickItem);
    return QQuickAnchorLine(const_cast<QQuickItem *>(q), QQuickAnchors::RightAnchor);
}

QQuickAnchorLine QQuickItemPrivate::horizontalCenter() const
{
    Q_Q(const QQuickItem);
    return QQuickAnchorLine(const_cast<QQuickItem *>(q), QQuickAnchors::HCenterAnchor);
}

QQuickAnchorLine QQuickItemPrivate::top() const
{
    Q_Q(const QQuickItem);
    return QQuickAnchorLine(const_cast<QQuickItem *>(q), QQuickAnchors::TopAnchor);
}

QQuickAnchorLine QQuickItemPrivate::bottom() const
{
    Q_Q(const QQuickItem);
    return QQuickAnchorLine(const_cast<QQuickItem *>(q), QQuickAnchors::BottomAnchor);
}

QQuickAnchorLine QQuickItemPrivate::verticalCenter() const
{
    Q_Q(const QQuickItem);
    return QQuickAnchorLine(const_cast<QQuickItem *>(q), QQuickAnchors::VCenterAnchor);
}

QQuickAnchorLine QQuickItemPrivate::baseline() const
{
    Q_Q(const QQuickItem);
    return QQuickAnchorLine(const_cast<QQuickItem *>(q), QQuickAnchors::BaselineAnchor);
}

/*!
  \qmlproperty int QtQuick::Item::baselineOffset

  Specifies the position of the item's baseline in local coordinates.

  The baseline of a \l Text item is the imaginary line on which the text
  sits. Controls containing text usually set their baseline to the
  baseline of their text.

  For non-text items, a default baseline offset of 0 is used.
*/
/*!
  \property QQuickItem::baselineOffset

  Specifies the position of the item's baseline in local coordinates.

  The baseline of a \l Text item is the imaginary line on which the text
  sits. Controls containing text usually set their baseline to the
  baseline of their text.

  For non-text items, a default baseline offset of 0 is used.
*/
qreal QQuickItem::baselineOffset() const
{
    Q_D(const QQuickItem);
    return d->baselineOffset;
}

void QQuickItem::setBaselineOffset(qreal offset)
{
    Q_D(QQuickItem);
    if (offset == d->baselineOffset)
        return;

    d->baselineOffset = offset;

    if (!d->changeListeners.isEmpty()) {
        const auto listeners = d->changeListeners; // NOTE: intentional copy (QTBUG-54732)
        for (const QQuickItemPrivate::ChangeListener &change : listeners) {
            if (change.types & QQuickItemPrivate::Geometry) {
                QQuickAnchorsPrivate *anchor = change.listener->anchorPrivate();
                if (anchor)
                    anchor->updateVerticalAnchors();
            }
        }
    }

    if (d->_anchors && (d->_anchors->usedAnchors() & QQuickAnchors::BaselineAnchor))
        QQuickAnchorsPrivate::get(d->_anchors)->updateVerticalAnchors();

    emit baselineOffsetChanged(offset);
}


/*!
 * Schedules a call to updatePaintNode() for this item.
 *
 * The call to QQuickItem::updatePaintNode() will always happen if the
 * item is showing in a QQuickWindow.
 *
 * Only items which specify QQuickItem::ItemHasContents are allowed
 * to call QQuickItem::update().
 */
void QQuickItem::update()
{
    Q_D(QQuickItem);
    if (!(flags() & ItemHasContents)) {
#ifndef QT_NO_DEBUG
        qWarning() << metaObject()->className() << ": Update called for a item without content";
#endif
        return;
    }
    d->dirty(QQuickItemPrivate::Content);
}

/*!
    Schedules a polish event for this item.

    When the scene graph processes the request, it will call updatePolish()
    on this item.
  */
void QQuickItem::polish()
{
    Q_D(QQuickItem);
    if (!d->polishScheduled) {
        d->polishScheduled = true;
        if (d->window) {
            QQuickWindowPrivate *p = QQuickWindowPrivate::get(d->window);
            bool maybeupdate = p->itemsToPolish.isEmpty();
            p->itemsToPolish.append(this);
            if (maybeupdate) d->window->maybeUpdate();
        }
    }
}

/*!
    \qmlmethod object QtQuick::Item::mapFromItem(Item item, real x, real y)
    \qmlmethod object QtQuick::Item::mapFromItem(Item item, real x, real y, real width, real height)

    Maps the point (\a x, \a y) or rect (\a x, \a y, \a width, \a height), which is in \a
    item's coordinate system, to this item's coordinate system, and returns a \l point or \l rect
    matching the mapped coordinate.

    If \a item is a \c null value, this maps the point or rect from the coordinate system of
    the root QML view.
*/
/*!
    \internal
  */
void QQuickItem::mapFromItem(QQmlV4Function *args) const
{
    QV4::ExecutionEngine *v4 = args->v4engine();
    if (args->length() != 3 && args->length() != 5) {
        v4->throwTypeError();
        return;
    }

    QV4::Scope scope(v4);
    QV4::ScopedValue item(scope, (*args)[0]);

    QQuickItem *itemObj = 0;
    if (!item->isNull()) {
        QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, item->as<QV4::QObjectWrapper>());
        if (qobjectWrapper)
            itemObj = qobject_cast<QQuickItem*>(qobjectWrapper->object());
    }

    if (!itemObj && !item->isNull()) {
        qmlInfo(this) << "mapFromItem() given argument \"" << item->toQStringNoThrow()
                      << "\" which is neither null nor an Item";
        v4->throwTypeError();
        return;
    }

    QV4::ScopedValue vx(scope, (*args)[1]);
    QV4::ScopedValue vy(scope, (*args)[2]);

    if (!vx->isNumber() || !vy->isNumber()) {
        v4->throwTypeError();
        return;
    }

    qreal x = vx->asDouble();
    qreal y = vy->asDouble();

    QVariant result;

    if (args->length() > 3) {
        QV4::ScopedValue vw(scope, (*args)[3]);
        QV4::ScopedValue vh(scope, (*args)[4]);
        if (!vw->isNumber() || !vh->isNumber()) {
            v4->throwTypeError();
            return;
        }
        qreal w = vw->asDouble();
        qreal h = vh->asDouble();

        result = mapRectFromItem(itemObj, QRectF(x, y, w, h));
    } else {
        result = mapFromItem(itemObj, QPointF(x, y));
    }

    QV4::ScopedObject rv(scope, v4->fromVariant(result));
    args->setReturnValue(rv.asReturnedValue());
}

/*!
    \internal
  */
QTransform QQuickItem::itemTransform(QQuickItem *other, bool *ok) const
{
    Q_D(const QQuickItem);

    // XXX todo - we need to be able to handle common parents better and detect
    // invalid cases
    if (ok) *ok = true;

    QTransform t = d->itemToWindowTransform();
    if (other) t *= QQuickItemPrivate::get(other)->windowToItemTransform();

    return t;
}

/*!
    \qmlmethod object QtQuick::Item::mapToItem(Item item, real x, real y)
    \qmlmethod object QtQuick::Item::mapToItem(Item item, real x, real y, real width, real height)

    Maps the point (\a x, \a y) or rect (\a x, \a y, \a width, \a height), which is in this
    item's coordinate system, to \a item's coordinate system, and returns a \l point or \l rect
    matching the mapped coordinate.

    If \a item is a \c null value, this maps the point or rect to the coordinate system of the
    root QML view.
*/
/*!
    \internal
  */
void QQuickItem::mapToItem(QQmlV4Function *args) const
{
    QV4::ExecutionEngine *v4 = args->v4engine();
    if (args->length() != 3 && args->length() != 5) {
        v4->throwTypeError();
        return;
    }

    QV4::Scope scope(v4);
    QV4::ScopedValue item(scope, (*args)[0]);

    QQuickItem *itemObj = 0;
    if (!item->isNull()) {
        QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, item->as<QV4::QObjectWrapper>());
        if (qobjectWrapper)
            itemObj = qobject_cast<QQuickItem*>(qobjectWrapper->object());
    }

    if (!itemObj && !item->isNull()) {
        qmlInfo(this) << "mapToItem() given argument \"" << item->toQStringNoThrow()
                      << "\" which is neither null nor an Item";
        v4->throwTypeError();
        return;
    }

    QV4::ScopedValue vx(scope, (*args)[1]);
    QV4::ScopedValue vy(scope, (*args)[2]);

    if (!vx->isNumber() || !vy->isNumber()) {
        v4->throwTypeError();
        return;
    }

    qreal x = vx->asDouble();
    qreal y = vy->asDouble();

    QVariant result;

    if (args->length() > 3) {
        QV4::ScopedValue vw(scope, (*args)[3]);
        QV4::ScopedValue vh(scope, (*args)[4]);
        if (!vw->isNumber() || !vh->isNumber()) {
            v4->throwTypeError();
            return;
        }
        qreal w = vw->asDouble();
        qreal h = vh->asDouble();

        result = mapRectToItem(itemObj, QRectF(x, y, w, h));
    } else {
        result = mapToItem(itemObj, QPointF(x, y));
    }

    QV4::ScopedObject rv(scope, v4->fromVariant(result));
    args->setReturnValue(rv.asReturnedValue());
}

/*!
    \since 5.7
    \qmlmethod object QtQuick::Item::mapFromGlobal(real x, real y)

    Maps the point (\a x, \a y), which is in the global coordinate system, to the
    item's coordinate system, and returns a \l point  matching the mapped coordinate.
*/
/*!
    \internal
  */
void QQuickItem::mapFromGlobal(QQmlV4Function *args) const
{
    QV4::ExecutionEngine *v4 = args->v4engine();
    if (args->length() != 2) {
        v4->throwTypeError();
        return;
    }

    QV4::Scope scope(v4);
    QV4::ScopedValue vx(scope, (*args)[0]);
    QV4::ScopedValue vy(scope, (*args)[1]);

    if (!vx->isNumber() || !vy->isNumber()) {
        v4->throwTypeError();
        return;
    }

    qreal x = vx->asDouble();
    qreal y = vy->asDouble();
    QVariant result = mapFromGlobal(QPointF(x, y));

    QV4::ScopedObject rv(scope, v4->fromVariant(result));
    args->setReturnValue(rv.asReturnedValue());
}

/*!
    \since 5.7
    \qmlmethod object QtQuick::Item::mapToGlobal(real x, real y)

    Maps the point (\a x, \a y), which is in this item's coordinate system, to the
    global coordinate system, and returns a \l point  matching the mapped coordinate.
*/
/*!
    \internal
  */
void QQuickItem::mapToGlobal(QQmlV4Function *args) const
{
    QV4::ExecutionEngine *v4 = args->v4engine();
    if (args->length() != 2) {
        v4->throwTypeError();
        return;
    }

    QV4::Scope scope(v4);
    QV4::ScopedValue vx(scope, (*args)[0]);
    QV4::ScopedValue vy(scope, (*args)[1]);

    if (!vx->isNumber() || !vy->isNumber()) {
        v4->throwTypeError();
        return;
    }

    qreal x = vx->asDouble();
    qreal y = vy->asDouble();
    QVariant result = mapToGlobal(QPointF(x, y));

    QV4::ScopedObject rv(scope, v4->fromVariant(result));
    args->setReturnValue(rv.asReturnedValue());
}

/*!
    \qmlmethod QtQuick::Item::forceActiveFocus()

    Forces active focus on the item.

    This method sets focus on the item and ensures that all ancestor
    FocusScope objects in the object hierarchy are also given \l focus.

    The reason for the focus change will be \l [CPP] Qt::OtherFocusReason. Use
    the overloaded method to specify the focus reason to enable better
    handling of the focus change.

    \sa activeFocus
*/
/*!
    Forces active focus on the item.

    This method sets focus on the item and ensures that all ancestor
    FocusScope objects in the object hierarchy are also given \l focus.

    The reason for the focus change will be \l [CPP] Qt::OtherFocusReason. Use
    the overloaded method to specify the focus reason to enable better
    handling of the focus change.

    \sa activeFocus
*/
void QQuickItem::forceActiveFocus()
{
    forceActiveFocus(Qt::OtherFocusReason);
}

/*!
    \qmlmethod QtQuick::Item::forceActiveFocus(Qt::FocusReason reason)
    \overload

    Forces active focus on the item with the given \a reason.

    This method sets focus on the item and ensures that all ancestor
    FocusScope objects in the object hierarchy are also given \l focus.

    \since 5.1

    \sa activeFocus, Qt::FocusReason
*/
/*!
    \overload
    Forces active focus on the item with the given \a reason.

    This method sets focus on the item and ensures that all ancestor
    FocusScope objects in the object hierarchy are also given \l focus.

    \since 5.1

    \sa activeFocus, Qt::FocusReason
*/

void QQuickItem::forceActiveFocus(Qt::FocusReason reason)
{
    setFocus(true, reason);
    QQuickItem *parent = parentItem();
    while (parent) {
        if (parent->flags() & QQuickItem::ItemIsFocusScope) {
            parent->setFocus(true, reason);
        }
        parent = parent->parentItem();
    }
}

/*!
    \qmlmethod QtQuick::Item::nextItemInFocusChain(bool forward)

    \since 5.1

    Returns the item in the focus chain which is next to this item.
    If \a forward is \c true, or not supplied, it is the next item in
    the forwards direction. If \a forward is \c false, it is the next
    item in the backwards direction.
*/
/*!
    Returns the item in the focus chain which is next to this item.
    If \a forward is \c true, or not supplied, it is the next item in
    the forwards direction. If \a forward is \c false, it is the next
    item in the backwards direction.
*/

QQuickItem *QQuickItem::nextItemInFocusChain(bool forward)
{
    return QQuickItemPrivate::nextPrevItemInTabFocusChain(this, forward);
}

/*!
    \qmlmethod QtQuick::Item::childAt(real x, real y)

    Returns the first visible child item found at point (\a x, \a y) within
    the coordinate system of this item.

    Returns \c null if there is no such item.
*/
/*!
    Returns the first visible child item found at point (\a x, \a y) within
    the coordinate system of this item.

    Returns 0 if there is no such item.
*/
QQuickItem *QQuickItem::childAt(qreal x, qreal y) const
{
    const QList<QQuickItem *> children = childItems();
    for (int i = children.count()-1; i >= 0; --i) {
        QQuickItem *child = children.at(i);
        // Map coordinates to the child element's coordinate space
        QPointF point = mapToItem(child, QPointF(x, y));
        if (child->isVisible() && point.x() >= 0
                && child->width() > point.x()
                && point.y() >= 0
                && child->height() > point.y())
            return child;
    }
    return 0;
}

QQmlListProperty<QObject> QQuickItemPrivate::resources()
{
    return QQmlListProperty<QObject>(q_func(), 0, QQuickItemPrivate::resources_append,
                                             QQuickItemPrivate::resources_count,
                                             QQuickItemPrivate::resources_at,
                                             QQuickItemPrivate::resources_clear);
}

/*!
    \qmlproperty list<Item> QtQuick::Item::children
    \qmlproperty list<Object> QtQuick::Item::resources

    The children property contains the list of visual children of this item.
    The resources property contains non-visual resources that you want to
    reference by name.

    It is not generally necessary to refer to these properties when adding
    child items or resources, as the default \l data property will
    automatically assign child objects to the \c children and \c resources
    properties as appropriate. See the \l data documentation for details.
*/
/*!
    \property QQuickItem::children
    \internal
*/
QQmlListProperty<QQuickItem> QQuickItemPrivate::children()
{
    return QQmlListProperty<QQuickItem>(q_func(), 0, QQuickItemPrivate::children_append,
                                             QQuickItemPrivate::children_count,
                                             QQuickItemPrivate::children_at,
                                             QQuickItemPrivate::children_clear);

}

/*!
  \qmlproperty list<Item> QtQuick::Item::visibleChildren
  This read-only property lists all of the item's children that are currently visible.
  Note that a child's visibility may have changed explicitly, or because the visibility
  of this (it's parent) item or another grandparent changed.
*/
/*!
    \property QQuickItem::visibleChildren
    \internal
*/
QQmlListProperty<QQuickItem> QQuickItemPrivate::visibleChildren()
{
    return QQmlListProperty<QQuickItem>(q_func(),
                                        0,
                                        QQuickItemPrivate::visibleChildren_count,
                                        QQuickItemPrivate::visibleChildren_at);

}

/*!
    \qmlproperty list<State> QtQuick::Item::states

    This property holds the list of possible states for this item. To change
    the state of this item, set the \l state property to one of these states,
    or set the \l state property to an empty string to revert the item to its
    default state.

    This property is specified as a list of \l State objects. For example,
    below is an item with "red_color" and "blue_color" states:

    \qml
    import QtQuick 2.0

    Rectangle {
        id: root
        width: 100; height: 100

        states: [
            State {
                name: "red_color"
                PropertyChanges { target: root; color: "red" }
            },
            State {
                name: "blue_color"
                PropertyChanges { target: root; color: "blue" }
            }
        ]
    }
    \endqml

    See \l{Qt Quick States} and \l{Animation and Transitions in Qt Quick} for
    more details on using states and transitions.

    \sa transitions
*/
/*!
    \property QQuickItem::states
    \internal
  */
QQmlListProperty<QQuickState> QQuickItemPrivate::states()
{
    return _states()->statesProperty();
}

/*!
    \qmlproperty list<Transition> QtQuick::Item::transitions

    This property holds the list of transitions for this item. These define the
    transitions to be applied to the item whenever it changes its \l state.

    This property is specified as a list of \l Transition objects. For example:

    \qml
    import QtQuick 2.0

    Item {
        transitions: [
            Transition {
                //...
            },
            Transition {
                //...
            }
        ]
    }
    \endqml

    See \l{Qt Quick States} and \l{Animation and Transitions in Qt Quick} for
    more details on using states and transitions.

    \sa states
*/
/*!
    \property QQuickItem::transitions
    \internal
  */
QQmlListProperty<QQuickTransition> QQuickItemPrivate::transitions()
{
    return _states()->transitionsProperty();
}

QString QQuickItemPrivate::state() const
{
    if (!_stateGroup)
        return QString();
    else
        return _stateGroup->state();
}

void QQuickItemPrivate::setState(const QString &state)
{
    _states()->setState(state);
}

/*!
    \qmlproperty string QtQuick::Item::state

    This property holds the name of the current state of the item.

    If the item is in its default state, that is, no explicit state has been
    set, then this property holds an empty string. Likewise, you can return
    an item to its default state by setting this property to an empty string.

    \sa {Qt Quick States}
*/
/*!
    \property QQuickItem::state

    This property holds the name of the current state of the item.

    If the item is in its default state, that is, no explicit state has been
    set, then this property holds an empty string. Likewise, you can return
    an item to its default state by setting this property to an empty string.

    \sa {Qt Quick States}
*/
QString QQuickItem::state() const
{
    Q_D(const QQuickItem);
    return d->state();
}

void QQuickItem::setState(const QString &state)
{
    Q_D(QQuickItem);
    d->setState(state);
}

/*!
  \qmlproperty list<Transform> QtQuick::Item::transform
  This property holds the list of transformations to apply.

  For more information see \l Transform.
*/
/*!
    \property QQuickItem::transform
    \internal
  */
/*!
    \internal
  */
QQmlListProperty<QQuickTransform> QQuickItem::transform()
{
    return QQmlListProperty<QQuickTransform>(this, 0, QQuickItemPrivate::transform_append,
                                                     QQuickItemPrivate::transform_count,
                                                     QQuickItemPrivate::transform_at,
                                                     QQuickItemPrivate::transform_clear);
}

/*!
  \reimp
  Derived classes should call the base class method before adding their own action to
  perform at classBegin.
*/
void QQuickItem::classBegin()
{
    Q_D(QQuickItem);
    d->componentComplete = false;
    if (d->_stateGroup)
        d->_stateGroup->classBegin();
    if (d->_anchors)
        d->_anchors->classBegin();
#if QT_CONFIG(quick_shadereffect)
    if (d->extra.isAllocated() && d->extra->layer)
        d->extra->layer->classBegin();
#endif
}

/*!
  \reimp
  Derived classes should call the base class method before adding their own actions to
  perform at componentComplete.
*/
void QQuickItem::componentComplete()
{
    Q_D(QQuickItem);
    d->componentComplete = true;
    if (d->_stateGroup)
        d->_stateGroup->componentComplete();
    if (d->_anchors) {
        d->_anchors->componentComplete();
        QQuickAnchorsPrivate::get(d->_anchors)->updateOnComplete();
    }

    if (d->extra.isAllocated()) {
#if QT_CONFIG(quick_shadereffect)
        if (d->extra->layer)
            d->extra->layer->componentComplete();
#endif

        if (d->extra->keyHandler)
            d->extra->keyHandler->componentComplete();

        if (d->extra->contents)
            d->extra->contents->complete();
    }

    if (d->window && d->dirtyAttributes) {
        d->addToDirtyList();
        QQuickWindowPrivate::get(d->window)->dirtyItem(this);
    }
}

QQuickStateGroup *QQuickItemPrivate::_states()
{
    Q_Q(QQuickItem);
    if (!_stateGroup) {
        _stateGroup = new QQuickStateGroup;
        if (!componentComplete)
            _stateGroup->classBegin();
        qmlobject_connect(_stateGroup, QQuickStateGroup, SIGNAL(stateChanged(QString)),
                          q, QQuickItem, SIGNAL(stateChanged(QString)))
    }

    return _stateGroup;
}

QPointF QQuickItemPrivate::computeTransformOrigin() const
{
    switch (origin()) {
    default:
    case QQuickItem::TopLeft:
        return QPointF(0, 0);
    case QQuickItem::Top:
        return QPointF(width / 2., 0);
    case QQuickItem::TopRight:
        return QPointF(width, 0);
    case QQuickItem::Left:
        return QPointF(0, height / 2.);
    case QQuickItem::Center:
        return QPointF(width / 2., height / 2.);
    case QQuickItem::Right:
        return QPointF(width, height / 2.);
    case QQuickItem::BottomLeft:
        return QPointF(0, height);
    case QQuickItem::Bottom:
        return QPointF(width / 2., height);
    case QQuickItem::BottomRight:
        return QPointF(width, height);
    }
}

void QQuickItemPrivate::transformChanged()
{
#if QT_CONFIG(quick_shadereffect)
    if (extra.isAllocated() && extra->layer)
        extra->layer->updateMatrix();
#endif
}

void QQuickItemPrivate::deliverKeyEvent(QKeyEvent *e)
{
    Q_Q(QQuickItem);

    Q_ASSERT(e->isAccepted());
    if (extra.isAllocated() && extra->keyHandler) {
        if (e->type() == QEvent::KeyPress)
            extra->keyHandler->keyPressed(e, false);
        else
            extra->keyHandler->keyReleased(e, false);

        if (e->isAccepted())
            return;
        else
            e->accept();
    }

    if (e->type() == QEvent::KeyPress)
        q->keyPressEvent(e);
    else
        q->keyReleaseEvent(e);

    if (e->isAccepted())
        return;

    if (extra.isAllocated() && extra->keyHandler) {
        e->accept();

        if (e->type() == QEvent::KeyPress)
            extra->keyHandler->keyPressed(e, true);
        else
            extra->keyHandler->keyReleased(e, true);
    }

    if (e->isAccepted() || !q->window())
        return;

    //only care about KeyPress now
    if (e->type() == QEvent::KeyPress &&
            (q == q->window()->contentItem() || q->activeFocusOnTab())) {
        bool res = false;
        if (!(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {  //### Add MetaModifier?
            if (e->key() == Qt::Key_Backtab
                || (e->key() == Qt::Key_Tab && (e->modifiers() & Qt::ShiftModifier)))
                res = QQuickItemPrivate::focusNextPrev(q, false);
            else if (e->key() == Qt::Key_Tab)
                res = QQuickItemPrivate::focusNextPrev(q, true);
            if (res)
                e->setAccepted(true);
        }
    }
}

#ifndef QT_NO_IM
void QQuickItemPrivate::deliverInputMethodEvent(QInputMethodEvent *e)
{
    Q_Q(QQuickItem);

    Q_ASSERT(e->isAccepted());
    if (extra.isAllocated() && extra->keyHandler) {
        extra->keyHandler->inputMethodEvent(e, false);

        if (e->isAccepted())
            return;
        else
            e->accept();
    }

    q->inputMethodEvent(e);

    if (e->isAccepted())
        return;

    if (extra.isAllocated() && extra->keyHandler) {
        e->accept();

        extra->keyHandler->inputMethodEvent(e, true);
    }
}
#endif // QT_NO_IM

/*!
    Called when \a change occurs for this item.

    \a value contains extra information relating to the change, when
    applicable.

    If you re-implement this method in a subclass, be sure to call
    \code
    QQuickItem::itemChange(change, value);
    \endcode
    typically at the end of your implementation, to ensure the
    \l windowChanged() signal will be emitted.
  */
void QQuickItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemSceneChange)
        emit windowChanged(value.window);
}

#ifndef QT_NO_IM
/*!
    Notify input method on updated query values if needed. \a queries indicates
    the changed attributes.
*/
void QQuickItem::updateInputMethod(Qt::InputMethodQueries queries)
{
    if (hasActiveFocus())
        QGuiApplication::inputMethod()->update(queries);
}
#endif // QT_NO_IM

/*! \internal */
// XXX todo - do we want/need this anymore?
QRectF QQuickItem::boundingRect() const
{
    Q_D(const QQuickItem);
    return QRectF(0, 0, d->width, d->height);
}

/*! \internal */
QRectF QQuickItem::clipRect() const
{
    Q_D(const QQuickItem);
    return QRectF(0, 0, d->width, d->height);
}

/*!
    \qmlproperty enumeration QtQuick::Item::transformOrigin
    This property holds the origin point around which scale and rotation transform.

    Nine transform origins are available, as shown in the image below.
    The default transform origin is \c Item.Center.

    \image declarative-transformorigin.png

    This example rotates an image around its bottom-right corner.
    \qml
    Image {
        source: "myimage.png"
        transformOrigin: Item.BottomRight
        rotation: 45
    }
    \endqml

    To set an arbitrary transform origin point use the \l Scale or \l Rotation
    transform types with \l transform.
*/
/*!
    \property QQuickItem::transformOrigin
    This property holds the origin point around which scale and rotation transform.

    Nine transform origins are available, as shown in the image below.
    The default transform origin is \c Item.Center.

    \image declarative-transformorigin.png
*/
QQuickItem::TransformOrigin QQuickItem::transformOrigin() const
{
    Q_D(const QQuickItem);
    return d->origin();
}

void QQuickItem::setTransformOrigin(TransformOrigin origin)
{
    Q_D(QQuickItem);
    if (origin == d->origin())
        return;

    d->extra.value().origin = origin;
    d->dirty(QQuickItemPrivate::TransformOrigin);

    emit transformOriginChanged(d->origin());
}

/*!
    \property QQuickItem::transformOriginPoint
    \internal
  */
/*!
  \internal
  */
QPointF QQuickItem::transformOriginPoint() const
{
    Q_D(const QQuickItem);
    if (d->extra.isAllocated() && !d->extra->userTransformOriginPoint.isNull())
        return d->extra->userTransformOriginPoint;
    return d->computeTransformOrigin();
}

/*!
  \internal
  */
void QQuickItem::setTransformOriginPoint(const QPointF &point)
{
    Q_D(QQuickItem);
    if (d->extra.value().userTransformOriginPoint == point)
        return;

    d->extra->userTransformOriginPoint = point;
    d->dirty(QQuickItemPrivate::TransformOrigin);
}

/*!
  \qmlproperty real QtQuick::Item::z

  Sets the stacking order of sibling items.  By default the stacking order is 0.

  Items with a higher stacking value are drawn on top of siblings with a
  lower stacking order.  Items with the same stacking value are drawn
  bottom up in the order they appear.  Items with a negative stacking
  value are drawn under their parent's content.

  The following example shows the various effects of stacking order.

  \table
  \row
  \li \image declarative-item_stacking1.png
  \li Same \c z - later children above earlier children:
  \qml
  Item {
      Rectangle {
          color: "red"
          width: 100; height: 100
      }
      Rectangle {
          color: "blue"
          x: 50; y: 50; width: 100; height: 100
      }
  }
  \endqml
  \row
  \li \image declarative-item_stacking2.png
  \li Higher \c z on top:
  \qml
  Item {
      Rectangle {
          z: 1
          color: "red"
          width: 100; height: 100
      }
      Rectangle {
          color: "blue"
          x: 50; y: 50; width: 100; height: 100
      }
  }
  \endqml
  \row
  \li \image declarative-item_stacking3.png
  \li Same \c z - children above parents:
  \qml
  Item {
      Rectangle {
          color: "red"
          width: 100; height: 100
          Rectangle {
              color: "blue"
              x: 50; y: 50; width: 100; height: 100
          }
      }
  }
  \endqml
  \row
  \li \image declarative-item_stacking4.png
  \li Lower \c z below:
  \qml
  Item {
      Rectangle {
          color: "red"
          width: 100; height: 100
          Rectangle {
              z: -1
              color: "blue"
              x: 50; y: 50; width: 100; height: 100
          }
      }
  }
  \endqml
  \endtable
 */
/*!
  \property QQuickItem::z

  Sets the stacking order of sibling items.  By default the stacking order is 0.

  Items with a higher stacking value are drawn on top of siblings with a
  lower stacking order.  Items with the same stacking value are drawn
  bottom up in the order they appear.  Items with a negative stacking
  value are drawn under their parent's content.

  The following example shows the various effects of stacking order.

  \table
  \row
  \li \image declarative-item_stacking1.png
  \li Same \c z - later children above earlier children:
  \qml
  Item {
      Rectangle {
          color: "red"
          width: 100; height: 100
      }
      Rectangle {
          color: "blue"
          x: 50; y: 50; width: 100; height: 100
      }
  }
  \endqml
  \row
  \li \image declarative-item_stacking2.png
  \li Higher \c z on top:
  \qml
  Item {
      Rectangle {
          z: 1
          color: "red"
          width: 100; height: 100
      }
      Rectangle {
          color: "blue"
          x: 50; y: 50; width: 100; height: 100
      }
  }
  \endqml
  \row
  \li \image declarative-item_stacking3.png
  \li Same \c z - children above parents:
  \qml
  Item {
      Rectangle {
          color: "red"
          width: 100; height: 100
          Rectangle {
              color: "blue"
              x: 50; y: 50; width: 100; height: 100
          }
      }
  }
  \endqml
  \row
  \li \image declarative-item_stacking4.png
  \li Lower \c z below:
  \qml
  Item {
      Rectangle {
          color: "red"
          width: 100; height: 100
          Rectangle {
              z: -1
              color: "blue"
              x: 50; y: 50; width: 100; height: 100
          }
      }
  }
  \endqml
  \endtable
  */
qreal QQuickItem::z() const
{
    Q_D(const QQuickItem);
    return d->z();
}

void QQuickItem::setZ(qreal v)
{
    Q_D(QQuickItem);
    if (d->z() == v)
        return;

    d->extra.value().z = v;

    d->dirty(QQuickItemPrivate::ZValue);
    if (d->parentItem) {
        QQuickItemPrivate::get(d->parentItem)->dirty(QQuickItemPrivate::ChildrenStackingChanged);
        QQuickItemPrivate::get(d->parentItem)->markSortedChildrenDirty(this);
    }

    emit zChanged();

#if QT_CONFIG(quick_shadereffect)
    if (d->extra.isAllocated() && d->extra->layer)
        d->extra->layer->updateZ();
#endif
}

/*!
  \qmlproperty real QtQuick::Item::rotation
  This property holds the rotation of the item in degrees clockwise around
  its transformOrigin.

  The default value is 0 degrees (that is, no rotation).

  \table
  \row
  \li \image declarative-rotation.png
  \li
  \qml
  Rectangle {
      color: "blue"
      width: 100; height: 100
      Rectangle {
          color: "red"
          x: 25; y: 25; width: 50; height: 50
          rotation: 30
      }
  }
  \endqml
  \endtable

  \sa Transform, Rotation
*/
/*!
  \property QQuickItem::rotation
  This property holds the rotation of the item in degrees clockwise around
  its transformOrigin.

  The default value is 0 degrees (that is, no rotation).

  \table
  \row
  \li \image declarative-rotation.png
  \li
  \qml
  Rectangle {
      color: "blue"
      width: 100; height: 100
      Rectangle {
          color: "red"
          x: 25; y: 25; width: 50; height: 50
          rotation: 30
      }
  }
  \endqml
  \endtable

  \sa Transform, Rotation
  */
qreal QQuickItem::rotation() const
{
    Q_D(const QQuickItem);
    return d->rotation();
}

void QQuickItem::setRotation(qreal r)
{
    Q_D(QQuickItem);
    if (d->rotation() == r)
        return;

    d->extra.value().rotation = r;

    d->dirty(QQuickItemPrivate::BasicTransform);

    d->itemChange(ItemRotationHasChanged, r);

    emit rotationChanged();
}

/*!
  \qmlproperty real QtQuick::Item::scale
  This property holds the scale factor for this item.

  A scale of less than 1.0 causes the item to be rendered at a smaller
  size, and a scale greater than 1.0 renders the item at a larger size.
  A negative scale causes the item to be mirrored when rendered.

  The default value is 1.0.

  Scaling is applied from the transformOrigin.

  \table
  \row
  \li \image declarative-scale.png
  \li
  \qml
  import QtQuick 2.0

  Rectangle {
      color: "blue"
      width: 100; height: 100

      Rectangle {
          color: "green"
          width: 25; height: 25
      }

      Rectangle {
          color: "red"
          x: 25; y: 25; width: 50; height: 50
          scale: 1.4
      }
  }
  \endqml
  \endtable

  \sa Transform, Scale
*/
/*!
  \property QQuickItem::scale
  This property holds the scale factor for this item.

  A scale of less than 1.0 causes the item to be rendered at a smaller
  size, and a scale greater than 1.0 renders the item at a larger size.
  A negative scale causes the item to be mirrored when rendered.

  The default value is 1.0.

  Scaling is applied from the transformOrigin.

  \table
  \row
  \li \image declarative-scale.png
  \li
  \qml
  import QtQuick 2.0

  Rectangle {
      color: "blue"
      width: 100; height: 100

      Rectangle {
          color: "green"
          width: 25; height: 25
      }

      Rectangle {
          color: "red"
          x: 25; y: 25; width: 50; height: 50
          scale: 1.4
      }
  }
  \endqml
  \endtable

  \sa Transform, Scale
  */
qreal QQuickItem::scale() const
{
    Q_D(const QQuickItem);
    return d->scale();
}

void QQuickItem::setScale(qreal s)
{
    Q_D(QQuickItem);
    if (d->scale() == s)
        return;

    d->extra.value().scale = s;

    d->dirty(QQuickItemPrivate::BasicTransform);

    emit scaleChanged();
}

/*!
  \qmlproperty real QtQuick::Item::opacity

  This property holds the opacity of the item.  Opacity is specified as a
  number between 0.0 (fully transparent) and 1.0 (fully opaque). The default
  value is 1.0.

  When this property is set, the specified opacity is also applied
  individually to child items. This may have an unintended effect in some
  circumstances. For example in the second set of rectangles below, the red
  rectangle has specified an opacity of 0.5, which affects the opacity of
  its blue child rectangle even though the child has not specified an opacity.

  \table
  \row
  \li \image declarative-item_opacity1.png
  \li
  \qml
    Item {
        Rectangle {
            color: "red"
            width: 100; height: 100
            Rectangle {
                color: "blue"
                x: 50; y: 50; width: 100; height: 100
            }
        }
    }
  \endqml
  \row
  \li \image declarative-item_opacity2.png
  \li
  \qml
    Item {
        Rectangle {
            opacity: 0.5
            color: "red"
            width: 100; height: 100
            Rectangle {
                color: "blue"
                x: 50; y: 50; width: 100; height: 100
            }
        }
    }
  \endqml
  \endtable

  Changing an item's opacity does not affect whether the item receives user
  input events. (In contrast, setting \l visible property to \c false stops
  mouse events, and setting the \l enabled property to \c false stops mouse
  and keyboard events, and also removes active focus from the item.)

  \sa visible
*/
/*!
  \property QQuickItem::opacity

  This property holds the opacity of the item.  Opacity is specified as a
  number between 0.0 (fully transparent) and 1.0 (fully opaque). The default
  value is 1.0.

  When this property is set, the specified opacity is also applied
  individually to child items. This may have an unintended effect in some
  circumstances. For example in the second set of rectangles below, the red
  rectangle has specified an opacity of 0.5, which affects the opacity of
  its blue child rectangle even though the child has not specified an opacity.

  Values outside the range of 0 to 1 will be clamped.

  \table
  \row
  \li \image declarative-item_opacity1.png
  \li
  \qml
    Item {
        Rectangle {
            color: "red"
            width: 100; height: 100
            Rectangle {
                color: "blue"
                x: 50; y: 50; width: 100; height: 100
            }
        }
    }
  \endqml
  \row
  \li \image declarative-item_opacity2.png
  \li
  \qml
    Item {
        Rectangle {
            opacity: 0.5
            color: "red"
            width: 100; height: 100
            Rectangle {
                color: "blue"
                x: 50; y: 50; width: 100; height: 100
            }
        }
    }
  \endqml
  \endtable

  Changing an item's opacity does not affect whether the item receives user
  input events. (In contrast, setting \l visible property to \c false stops
  mouse events, and setting the \l enabled property to \c false stops mouse
  and keyboard events, and also removes active focus from the item.)

  \sa visible
*/
qreal QQuickItem::opacity() const
{
    Q_D(const QQuickItem);
    return d->opacity();
}

void QQuickItem::setOpacity(qreal newOpacity)
{
    Q_D(QQuickItem);
    qreal o = qBound<qreal>(0, newOpacity, 1);
    if (d->opacity() == o)
        return;

    d->extra.value().opacity = o;

    d->dirty(QQuickItemPrivate::OpacityValue);

    d->itemChange(ItemOpacityHasChanged, o);

    emit opacityChanged();
}

/*!
    \qmlproperty bool QtQuick::Item::visible

    This property holds whether the item is visible. By default this is true.

    Setting this property directly affects the \c visible value of child
    items. When set to \c false, the \c visible values of all child items also
    become \c false. When set to \c true, the \c visible values of child items
    are returned to \c true, unless they have explicitly been set to \c false.

    (Because of this flow-on behavior, using the \c visible property may not
    have the intended effect if a property binding should only respond to
    explicit property changes. In such cases it may be better to use the
    \l opacity property instead.)

    If this property is set to \c false, the item will no longer receive mouse
    events, but will continue to receive key events and will retain the keyboard
    \l focus if it has been set. (In contrast, setting the \l enabled property
    to \c false stops both mouse and keyboard events, and also removes focus
    from the item.)

    \note This property's value is only affected by changes to this property or
    the parent's \c visible property. It does not change, for example, if this
    item moves off-screen, or if the \l opacity changes to 0.

    \sa opacity, enabled
*/
/*!
    \property QQuickItem::visible

    This property holds whether the item is visible. By default this is true.

    Setting this property directly affects the \c visible value of child
    items. When set to \c false, the \c visible values of all child items also
    become \c false. When set to \c true, the \c visible values of child items
    are returned to \c true, unless they have explicitly been set to \c false.

    (Because of this flow-on behavior, using the \c visible property may not
    have the intended effect if a property binding should only respond to
    explicit property changes. In such cases it may be better to use the
    \l opacity property instead.)

    If this property is set to \c false, the item will no longer receive mouse
    events, but will continue to receive key events and will retain the keyboard
    \l focus if it has been set. (In contrast, setting the \l enabled property
    to \c false stops both mouse and keyboard events, and also removes focus
    from the item.)

    \note This property's value is only affected by changes to this property or
    the parent's \c visible property. It does not change, for example, if this
    item moves off-screen, or if the \l opacity changes to 0.

    \sa opacity, enabled
*/
bool QQuickItem::isVisible() const
{
    Q_D(const QQuickItem);
    return d->effectiveVisible;
}

void QQuickItem::setVisible(bool v)
{
    Q_D(QQuickItem);
    if (v == d->explicitVisible)
        return;

    d->explicitVisible = v;
    if (!v)
        d->dirty(QQuickItemPrivate::Visible);

    const bool childVisibilityChanged = d->setEffectiveVisibleRecur(d->calcEffectiveVisible());
    if (childVisibilityChanged && d->parentItem)
        emit d->parentItem->visibleChildrenChanged();   // signal the parent, not this!
}

/*!
    \qmlproperty bool QtQuick::Item::enabled

    This property holds whether the item receives mouse and keyboard events.
    By default this is true.

    Setting this property directly affects the \c enabled value of child
    items. When set to \c false, the \c enabled values of all child items also
    become \c false. When set to \c true, the \c enabled values of child items
    are returned to \c true, unless they have explicitly been set to \c false.

    Setting this property to \c false automatically causes \l activeFocus to be
    set to \c false, and this item will longer receive keyboard events.

    \sa visible
*/
/*!
    \property QQuickItem::enabled

    This property holds whether the item receives mouse and keyboard events.
    By default this is true.

    Setting this property directly affects the \c enabled value of child
    items. When set to \c false, the \c enabled values of all child items also
    become \c false. When set to \c true, the \c enabled values of child items
    are returned to \c true, unless they have explicitly been set to \c false.

    Setting this property to \c false automatically causes \l activeFocus to be
    set to \c false, and this item will longer receive keyboard events.

    \sa visible
*/
bool QQuickItem::isEnabled() const
{
    Q_D(const QQuickItem);
    return d->effectiveEnable;
}

void QQuickItem::setEnabled(bool e)
{
    Q_D(QQuickItem);
    if (e == d->explicitEnable)
        return;

    d->explicitEnable = e;

    QQuickItem *scope = parentItem();
    while (scope && !scope->isFocusScope())
        scope = scope->parentItem();

    d->setEffectiveEnableRecur(scope, d->calcEffectiveEnable());
}

bool QQuickItemPrivate::calcEffectiveVisible() const
{
    // XXX todo - Should the effective visible of an element with no parent just be the current
    // effective visible?  This would prevent pointless re-processing in the case of an element
    // moving to/from a no-parent situation, but it is different from what graphics view does.
    return explicitVisible && (!parentItem || QQuickItemPrivate::get(parentItem)->effectiveVisible);
}

bool QQuickItemPrivate::setEffectiveVisibleRecur(bool newEffectiveVisible)
{
    Q_Q(QQuickItem);

    if (newEffectiveVisible && !explicitVisible) {
        // This item locally overrides visibility
        return false;   // effective visibility didn't change
    }

    if (newEffectiveVisible == effectiveVisible) {
        // No change necessary
        return false;   // effective visibility didn't change
    }

    effectiveVisible = newEffectiveVisible;
    dirty(Visible);
    if (parentItem) QQuickItemPrivate::get(parentItem)->dirty(ChildrenStackingChanged);

    if (window) {
        QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(window);
        windowPriv->removeGrabber(q);
    }

    bool childVisibilityChanged = false;
    for (QQuickItem *childItem : qAsConst(childItems)) {
        childVisibilityChanged |= QQuickItemPrivate::get(childItem)->setEffectiveVisibleRecur(newEffectiveVisible);
    }

    itemChange(QQuickItem::ItemVisibleHasChanged, effectiveVisible);
#ifndef QT_NO_ACCESSIBILITY
    if (isAccessible) {
        QAccessibleEvent ev(q, effectiveVisible ? QAccessible::ObjectShow : QAccessible::ObjectHide);
        QAccessible::updateAccessibility(&ev);
    }
#endif
    emit q->visibleChanged();
    if (childVisibilityChanged)
        emit q->visibleChildrenChanged();

    return true;    // effective visibility DID change
}

bool QQuickItemPrivate::calcEffectiveEnable() const
{
    // XXX todo - Should the effective enable of an element with no parent just be the current
    // effective enable?  This would prevent pointless re-processing in the case of an element
    // moving to/from a no-parent situation, but it is different from what graphics view does.
    return explicitEnable && (!parentItem || QQuickItemPrivate::get(parentItem)->effectiveEnable);
}

void QQuickItemPrivate::setEffectiveEnableRecur(QQuickItem *scope, bool newEffectiveEnable)
{
    Q_Q(QQuickItem);

    if (newEffectiveEnable && !explicitEnable) {
        // This item locally overrides enable
        return;
    }

    if (newEffectiveEnable == effectiveEnable) {
        // No change necessary
        return;
    }

    effectiveEnable = newEffectiveEnable;

    if (window) {
        QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(window);
        windowPriv->removeGrabber(q);
        if (scope && !effectiveEnable && activeFocus) {
            windowPriv->clearFocusInScope(
                    scope, q, Qt::OtherFocusReason, QQuickWindowPrivate::DontChangeFocusProperty | QQuickWindowPrivate::DontChangeSubFocusItem);
        }
    }

    for (QQuickItem *childItem : qAsConst(childItems)) {
        QQuickItemPrivate::get(childItem)->setEffectiveEnableRecur(
                (flags & QQuickItem::ItemIsFocusScope) && scope ? q : scope, newEffectiveEnable);
    }

    if (window && scope && effectiveEnable && focus) {
        QQuickWindowPrivate::get(window)->setFocusInScope(
                scope, q, Qt::OtherFocusReason, QQuickWindowPrivate::DontChangeFocusProperty | QQuickWindowPrivate::DontChangeSubFocusItem);
    }

    emit q->enabledChanged();
}

bool QQuickItemPrivate::isTransparentForPositioner() const
{
    return extra.isAllocated() && extra.value().transparentForPositioner;
}

void QQuickItemPrivate::setTransparentForPositioner(bool transparent)
{
    extra.value().transparentForPositioner = transparent;
}


QString QQuickItemPrivate::dirtyToString() const
{
#define DIRTY_TO_STRING(value) if (dirtyAttributes & value) { \
    if (!rv.isEmpty()) \
        rv.append(QLatin1Char('|')); \
    rv.append(QLatin1String(#value)); \
}

//    QString rv = QLatin1String("0x") + QString::number(dirtyAttributes, 16);
    QString rv;

    DIRTY_TO_STRING(TransformOrigin);
    DIRTY_TO_STRING(Transform);
    DIRTY_TO_STRING(BasicTransform);
    DIRTY_TO_STRING(Position);
    DIRTY_TO_STRING(Size);
    DIRTY_TO_STRING(ZValue);
    DIRTY_TO_STRING(Content);
    DIRTY_TO_STRING(Smooth);
    DIRTY_TO_STRING(OpacityValue);
    DIRTY_TO_STRING(ChildrenChanged);
    DIRTY_TO_STRING(ChildrenStackingChanged);
    DIRTY_TO_STRING(ParentChanged);
    DIRTY_TO_STRING(Clip);
    DIRTY_TO_STRING(Window);
    DIRTY_TO_STRING(EffectReference);
    DIRTY_TO_STRING(Visible);
    DIRTY_TO_STRING(HideReference);
    DIRTY_TO_STRING(Antialiasing);

    return rv;
}

void QQuickItemPrivate::dirty(DirtyType type)
{
    Q_Q(QQuickItem);
    if (type & (TransformOrigin | Transform | BasicTransform | Position | Size))
        transformChanged();

    if (!(dirtyAttributes & type) || (window && !prevDirtyItem)) {
        dirtyAttributes |= type;
        if (window && componentComplete) {
            addToDirtyList();
            QQuickWindowPrivate::get(window)->dirtyItem(q);
        }
    }
}

void QQuickItemPrivate::addToDirtyList()
{
    Q_Q(QQuickItem);

    Q_ASSERT(window);
    if (!prevDirtyItem) {
        Q_ASSERT(!nextDirtyItem);

        QQuickWindowPrivate *p = QQuickWindowPrivate::get(window);
        nextDirtyItem = p->dirtyItemList;
        if (nextDirtyItem) QQuickItemPrivate::get(nextDirtyItem)->prevDirtyItem = &nextDirtyItem;
        prevDirtyItem = &p->dirtyItemList;
        p->dirtyItemList = q;
        p->dirtyItem(q);
    }
    Q_ASSERT(prevDirtyItem);
}

void QQuickItemPrivate::removeFromDirtyList()
{
    if (prevDirtyItem) {
        if (nextDirtyItem) QQuickItemPrivate::get(nextDirtyItem)->prevDirtyItem = prevDirtyItem;
        *prevDirtyItem = nextDirtyItem;
        prevDirtyItem = 0;
        nextDirtyItem = 0;
    }
    Q_ASSERT(!prevDirtyItem);
    Q_ASSERT(!nextDirtyItem);
}

void QQuickItemPrivate::refFromEffectItem(bool hide)
{
    ++extra.value().effectRefCount;
    if (1 == extra->effectRefCount) {
        dirty(EffectReference);
        if (parentItem) QQuickItemPrivate::get(parentItem)->dirty(ChildrenStackingChanged);
    }
    if (hide) {
        if (++extra->hideRefCount == 1)
            dirty(HideReference);
    }
}

void QQuickItemPrivate::derefFromEffectItem(bool unhide)
{
    Q_ASSERT(extra->effectRefCount);
    --extra->effectRefCount;
    if (0 == extra->effectRefCount) {
        dirty(EffectReference);
        if (parentItem) QQuickItemPrivate::get(parentItem)->dirty(ChildrenStackingChanged);
    }
    if (unhide) {
        if (--extra->hideRefCount == 0)
            dirty(HideReference);
    }
}

void QQuickItemPrivate::setCulled(bool cull)
{
    if (cull == culled)
        return;

    culled = cull;
    if ((cull && ++extra.value().hideRefCount == 1) || (!cull && --extra.value().hideRefCount == 0))
        dirty(HideReference);
}

void QQuickItemPrivate::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    Q_Q(QQuickItem);
    switch (change) {
    case QQuickItem::ItemChildAddedChange: {
        q->itemChange(change, data);
        if (!changeListeners.isEmpty()) {
            const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
            for (const QQuickItemPrivate::ChangeListener &change : listeners) {
                if (change.types & QQuickItemPrivate::Children) {
                    change.listener->itemChildAdded(q, data.item);
                }
            }
        }
        break;
    }
    case QQuickItem::ItemChildRemovedChange: {
        q->itemChange(change, data);
        if (!changeListeners.isEmpty()) {
            const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
            for (const QQuickItemPrivate::ChangeListener &change : listeners) {
                if (change.types & QQuickItemPrivate::Children) {
                    change.listener->itemChildRemoved(q, data.item);
                }
            }
        }
        break;
    }
    case QQuickItem::ItemSceneChange:
        q->itemChange(change, data);
        break;
    case QQuickItem::ItemVisibleHasChanged: {
        q->itemChange(change, data);
        if (!changeListeners.isEmpty()) {
            const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
            for (const QQuickItemPrivate::ChangeListener &change : listeners) {
                if (change.types & QQuickItemPrivate::Visibility) {
                    change.listener->itemVisibilityChanged(q);
                }
            }
        }
        break;
    }
    case QQuickItem::ItemParentHasChanged: {
        q->itemChange(change, data);
        if (!changeListeners.isEmpty()) {
            const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
            for (const QQuickItemPrivate::ChangeListener &change : listeners) {
                if (change.types & QQuickItemPrivate::Parent) {
                    change.listener->itemParentChanged(q, data.item);
                }
            }
        }
        break;
    }
    case QQuickItem::ItemOpacityHasChanged: {
        q->itemChange(change, data);
        if (!changeListeners.isEmpty()) {
            const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
            for (const QQuickItemPrivate::ChangeListener &change : listeners) {
                if (change.types & QQuickItemPrivate::Opacity) {
                    change.listener->itemOpacityChanged(q);
                }
            }
        }
        break;
    }
    case QQuickItem::ItemActiveFocusHasChanged:
        q->itemChange(change, data);
        break;
    case QQuickItem::ItemRotationHasChanged: {
        q->itemChange(change, data);
        if (!changeListeners.isEmpty()) {
            const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
            for (const QQuickItemPrivate::ChangeListener &change : listeners) {
                if (change.types & QQuickItemPrivate::Rotation) {
                    change.listener->itemRotationChanged(q);
                }
            }
        }
        break;
    }
    case QQuickItem::ItemAntialiasingHasChanged:
        // fall through
    case QQuickItem::ItemDevicePixelRatioHasChanged:
        q->itemChange(change, data);
        break;
    }
}

/*!
    \qmlproperty bool QtQuick::Item::smooth

    Primarily used in image based items to decide if the item should use smooth
    sampling or not. Smooth sampling is performed using linear interpolation, while
    non-smooth is performed using nearest neighbor.

    In Qt Quick 2.0, this property has minimal impact on performance.

    By default, this property is set to \c true.
*/
/*!
    \property QQuickItem::smooth
    \brief Specifies whether the item is smoothed or not

    Primarily used in image based items to decide if the item should use smooth
    sampling or not. Smooth sampling is performed using linear interpolation, while
    non-smooth is performed using nearest neighbor.

    In Qt Quick 2.0, this property has minimal impact on performance.

    By default, this property is set to \c true.
*/
bool QQuickItem::smooth() const
{
    Q_D(const QQuickItem);
    return d->smooth;
}
void QQuickItem::setSmooth(bool smooth)
{
    Q_D(QQuickItem);
    if (d->smooth == smooth)
        return;

    d->smooth = smooth;
    d->dirty(QQuickItemPrivate::Smooth);

    emit smoothChanged(smooth);
}

/*!
    \qmlproperty bool QtQuick::Item::activeFocusOnTab

    This property holds whether the item wants to be in the tab focus
    chain. By default, this is set to \c false.

    The tab focus chain traverses elements by first visiting the
    parent, and then its children in the order they occur in the
    children property. Pressing the tab key on an item in the tab
    focus chain will move keyboard focus to the next item in the
    chain. Pressing BackTab (normally Shift+Tab) will move focus
    to the previous item.

    To set up a manual tab focus chain, see \l KeyNavigation. Tab
    key events used by Keys or KeyNavigation have precedence over
    focus chain behavior; ignore the events in other key handlers
    to allow it to propagate.
*/
/*!
    \property QQuickItem::activeFocusOnTab

    This property holds whether the item wants to be in the tab focus
    chain. By default, this is set to \c false.
*/
bool QQuickItem::activeFocusOnTab() const
{
    Q_D(const QQuickItem);
    return d->activeFocusOnTab;
}
void QQuickItem::setActiveFocusOnTab(bool activeFocusOnTab)
{
    Q_D(QQuickItem);
    if (d->activeFocusOnTab == activeFocusOnTab)
        return;

    if (window()) {
        if ((this == window()->activeFocusItem()) && this != window()->contentItem() && !activeFocusOnTab) {
            qWarning("QQuickItem: Cannot set activeFocusOnTab to false once item is the active focus item.");
            return;
        }
    }

    d->activeFocusOnTab = activeFocusOnTab;

    emit activeFocusOnTabChanged(activeFocusOnTab);
}

/*!
    \qmlproperty bool QtQuick::Item::antialiasing

    Used by visual elements to decide if the item should use antialiasing or not.
    In some cases items with antialiasing require more memory and are potentially
    slower to render (see \l {Antialiasing} for more details).

    The default is false, but may be overridden by derived elements.
*/
/*!
    \property QQuickItem::antialiasing
    \brief Specifies whether the item is antialiased or not

    Used by visual elements to decide if the item should use antialiasing or not.
    In some cases items with antialiasing require more memory and are potentially
    slower to render (see \l {Antialiasing} for more details).

    The default is false, but may be overridden by derived elements.
*/
bool QQuickItem::antialiasing() const
{
    Q_D(const QQuickItem);
    return d->antialiasingValid ? d->antialiasing : d->implicitAntialiasing;
}

void QQuickItem::setAntialiasing(bool aa)
{
    Q_D(QQuickItem);

    if (!d->antialiasingValid) {
        d->antialiasingValid = true;
        d->antialiasing = d->implicitAntialiasing;
    }

    if (aa == d->antialiasing)
        return;

    d->antialiasing = aa;
    d->dirty(QQuickItemPrivate::Antialiasing);

    d->itemChange(ItemAntialiasingHasChanged, d->antialiasing);

    emit antialiasingChanged(antialiasing());
}

void QQuickItem::resetAntialiasing()
{
    Q_D(QQuickItem);
    if (!d->antialiasingValid)
        return;

    d->antialiasingValid = false;

    if (d->implicitAntialiasing != d->antialiasing)
        emit antialiasingChanged(antialiasing());
}

void QQuickItemPrivate::setImplicitAntialiasing(bool antialiasing)
{
    Q_Q(QQuickItem);
    bool prev = q->antialiasing();
    implicitAntialiasing = antialiasing;
    if (componentComplete && (q->antialiasing() != prev))
        emit q->antialiasingChanged(q->antialiasing());
}

/*!
    Returns the item flags for this item.

    \sa setFlag()
  */
QQuickItem::Flags QQuickItem::flags() const
{
    Q_D(const QQuickItem);
    return (QQuickItem::Flags)d->flags;
}

/*!
    Enables the specified \a flag for this item if \a enabled is true;
    if \a enabled is false, the flag is disabled.

    These provide various hints for the item; for example, the
    ItemClipsChildrenToShape flag indicates that all children of this
    item should be clipped to fit within the item area.
  */
void QQuickItem::setFlag(Flag flag, bool enabled)
{
    Q_D(QQuickItem);
    if (enabled)
        setFlags((Flags)(d->flags | (quint32)flag));
    else
        setFlags((Flags)(d->flags & ~(quint32)flag));
}

/*!
    Enables the specified \a flags for this item.

    \sa setFlag()
  */
void QQuickItem::setFlags(Flags flags)
{
    Q_D(QQuickItem);

    if (int(flags & ItemIsFocusScope) != int(d->flags & ItemIsFocusScope)) {
        if (flags & ItemIsFocusScope && !d->childItems.isEmpty() && d->window) {
            qWarning("QQuickItem: Cannot set FocusScope once item has children and is in a window.");
            flags &= ~ItemIsFocusScope;
        } else if (d->flags & ItemIsFocusScope) {
            qWarning("QQuickItem: Cannot unset FocusScope flag.");
            flags |= ItemIsFocusScope;
        }
    }

    if (int(flags & ItemClipsChildrenToShape) != int(d->flags & ItemClipsChildrenToShape))
        d->dirty(QQuickItemPrivate::Clip);

    d->flags = flags;
}

/*!
  \qmlproperty real QtQuick::Item::x
  \qmlproperty real QtQuick::Item::y
  \qmlproperty real QtQuick::Item::width
  \qmlproperty real QtQuick::Item::height

  Defines the item's position and size.

  The (x,y) position is relative to the \l parent.

  \qml
  Item { x: 100; y: 100; width: 100; height: 100 }
  \endqml
 */
/*!
  \property QQuickItem::x

  Defines the item's x position relative to its parent.
  */
/*!
  \property QQuickItem::y

  Defines the item's y position relative to its parent.
  */
qreal QQuickItem::x() const
{
    Q_D(const QQuickItem);
    return d->x;
}

qreal QQuickItem::y() const
{
    Q_D(const QQuickItem);
    return d->y;
}

/*!
    \internal
  */
QPointF QQuickItem::position() const
{
    Q_D(const QQuickItem);
    return QPointF(d->x, d->y);
}

void QQuickItem::setX(qreal v)
{
    Q_D(QQuickItem);
    if (qt_is_nan(v))
        return;
    if (d->x == v)
        return;

    qreal oldx = d->x;
    d->x = v;

    d->dirty(QQuickItemPrivate::Position);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(oldx, d->y, d->width, d->height));
}

void QQuickItem::setY(qreal v)
{
    Q_D(QQuickItem);
    if (qt_is_nan(v))
        return;
    if (d->y == v)
        return;

    qreal oldy = d->y;
    d->y = v;

    d->dirty(QQuickItemPrivate::Position);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(d->x, oldy, d->width, d->height));
}

/*!
    \internal
  */
void QQuickItem::setPosition(const QPointF &pos)
{
    Q_D(QQuickItem);
    if (QPointF(d->x, d->y) == pos)
        return;

    qreal oldx = d->x;
    qreal oldy = d->y;

    d->x = pos.x();
    d->y = pos.y();

    d->dirty(QQuickItemPrivate::Position);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(oldx, oldy, d->width, d->height));
}

/*!
    \property QQuickItem::width

    This property holds the width of this item.
  */
qreal QQuickItem::width() const
{
    Q_D(const QQuickItem);
    return d->width;
}

void QQuickItem::setWidth(qreal w)
{
    Q_D(QQuickItem);
    if (qt_is_nan(w))
        return;

    d->widthValid = true;
    if (d->width == w)
        return;

    qreal oldWidth = d->width;
    d->width = w;

    d->dirty(QQuickItemPrivate::Size);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(d->x, d->y, oldWidth, d->height));
}

void QQuickItem::resetWidth()
{
    Q_D(QQuickItem);
    d->widthValid = false;
    setImplicitWidth(implicitWidth());
}

void QQuickItemPrivate::implicitWidthChanged()
{
    Q_Q(QQuickItem);
    if (!changeListeners.isEmpty()) {
        const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
        for (const QQuickItemPrivate::ChangeListener &change : listeners) {
            if (change.types & QQuickItemPrivate::ImplicitWidth) {
                change.listener->itemImplicitWidthChanged(q);
            }
        }
    }
    emit q->implicitWidthChanged();
}

qreal QQuickItemPrivate::getImplicitWidth() const
{
    return implicitWidth;
}
/*!
    Returns the width of the item that is implied by other properties that determine the content.
*/
qreal QQuickItem::implicitWidth() const
{
    Q_D(const QQuickItem);
    return d->getImplicitWidth();
}

/*!
    \qmlproperty real QtQuick::Item::implicitWidth
    \qmlproperty real QtQuick::Item::implicitHeight

    Defines the natural width or height of the Item if no \l width or \l height is specified.

    The default implicit size for most items is 0x0, however some items have an inherent
    implicit size which cannot be overridden, for example, \l [QML] Image and \l [QML] Text.

    Setting the implicit size is useful for defining components that have a preferred size
    based on their content, for example:

    \qml
    // Label.qml
    import QtQuick 2.0

    Item {
        property alias icon: image.source
        property alias label: text.text
        implicitWidth: text.implicitWidth + image.implicitWidth
        implicitHeight: Math.max(text.implicitHeight, image.implicitHeight)
        Image { id: image }
        Text {
            id: text
            wrapMode: Text.Wrap
            anchors.left: image.right; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    \endqml

    \note Using implicitWidth of \l [QML] Text or \l [QML] TextEdit and setting the width explicitly
    incurs a performance penalty as the text must be laid out twice.
*/
/*!
    \property QQuickItem::implicitWidth
    \property QQuickItem::implicitHeight

    Defines the natural width or height of the Item if no \l width or \l height is specified.

    The default implicit size for most items is 0x0, however some items have an inherent
    implicit size which cannot be overridden, for example, \l [QML] Image and \l [QML] Text.

    Setting the implicit size is useful for defining components that have a preferred size
    based on their content, for example:

    \qml
    // Label.qml
    import QtQuick 2.0

    Item {
        property alias icon: image.source
        property alias label: text.text
        implicitWidth: text.implicitWidth + image.implicitWidth
        implicitHeight: Math.max(text.implicitHeight, image.implicitHeight)
        Image { id: image }
        Text {
            id: text
            wrapMode: Text.Wrap
            anchors.left: image.right; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    \endqml

    \note Using implicitWidth of \l [QML] Text or \l [QML] TextEdit and setting the width explicitly
    incurs a performance penalty as the text must be laid out twice.
*/
void QQuickItem::setImplicitWidth(qreal w)
{
    Q_D(QQuickItem);
    bool changed = w != d->implicitWidth;
    d->implicitWidth = w;
    if (d->width == w || widthValid()) {
        if (changed)
            d->implicitWidthChanged();
        if (d->width == w || widthValid())
            return;
        changed = false;
    }

    qreal oldWidth = d->width;
    d->width = w;

    d->dirty(QQuickItemPrivate::Size);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(d->x, d->y, oldWidth, d->height));

    if (changed)
        d->implicitWidthChanged();
}

/*!
    Returns whether the width property has been set explicitly.
*/
bool QQuickItem::widthValid() const
{
    Q_D(const QQuickItem);
    return d->widthValid;
}

/*!
    \property QQuickItem::height

    This property holds the height of this item.
  */
qreal QQuickItem::height() const
{
    Q_D(const QQuickItem);
    return d->height;
}

void QQuickItem::setHeight(qreal h)
{
    Q_D(QQuickItem);
    if (qt_is_nan(h))
        return;

    d->heightValid = true;
    if (d->height == h)
        return;

    qreal oldHeight = d->height;
    d->height = h;

    d->dirty(QQuickItemPrivate::Size);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(d->x, d->y, d->width, oldHeight));
}

void QQuickItem::resetHeight()
{
    Q_D(QQuickItem);
    d->heightValid = false;
    setImplicitHeight(implicitHeight());
}

void QQuickItemPrivate::implicitHeightChanged()
{
    Q_Q(QQuickItem);
    if (!changeListeners.isEmpty()) {
        const auto listeners = changeListeners; // NOTE: intentional copy (QTBUG-54732)
        for (const QQuickItemPrivate::ChangeListener &change : listeners) {
            if (change.types & QQuickItemPrivate::ImplicitHeight) {
                change.listener->itemImplicitHeightChanged(q);
            }
        }
    }
    emit q->implicitHeightChanged();
}

qreal QQuickItemPrivate::getImplicitHeight() const
{
    return implicitHeight;
}

qreal QQuickItem::implicitHeight() const
{
    Q_D(const QQuickItem);
    return d->getImplicitHeight();
}

void QQuickItem::setImplicitHeight(qreal h)
{
    Q_D(QQuickItem);
    bool changed = h != d->implicitHeight;
    d->implicitHeight = h;
    if (d->height == h || heightValid()) {
        if (changed)
            d->implicitHeightChanged();
        if (d->height == h || heightValid())
            return;
        changed = false;
    }

    qreal oldHeight = d->height;
    d->height = h;

    d->dirty(QQuickItemPrivate::Size);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(d->x, d->y, d->width, oldHeight));

    if (changed)
        d->implicitHeightChanged();
}

/*!
    \internal
  */
void QQuickItem::setImplicitSize(qreal w, qreal h)
{
    Q_D(QQuickItem);
    bool wChanged = w != d->implicitWidth;
    bool hChanged = h != d->implicitHeight;

    d->implicitWidth = w;
    d->implicitHeight = h;

    bool wDone = false;
    bool hDone = false;
    if (d->width == w || widthValid()) {
        if (wChanged)
            d->implicitWidthChanged();
        wDone = d->width == w || widthValid();
        wChanged = false;
    }
    if (d->height == h || heightValid()) {
        if (hChanged)
            d->implicitHeightChanged();
        hDone = d->height == h || heightValid();
        hChanged = false;
    }
    if (wDone && hDone)
        return;

    qreal oldWidth = d->width;
    qreal oldHeight = d->height;
    if (!wDone)
        d->width = w;
    if (!hDone)
        d->height = h;

    d->dirty(QQuickItemPrivate::Size);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(d->x, d->y, oldWidth, oldHeight));

    if (!wDone && wChanged)
        d->implicitWidthChanged();
    if (!hDone && hChanged)
        d->implicitHeightChanged();
}

/*!
    Returns whether the height property has been set explicitly.
*/
bool QQuickItem::heightValid() const
{
    Q_D(const QQuickItem);
    return d->heightValid;
}

/*!
    \internal
  */
void QQuickItem::setSize(const QSizeF &size)
{
    Q_D(QQuickItem);
    d->heightValid = true;
    d->widthValid = true;

    if (d->width == size.width() && d->height == size.height())
        return;

    qreal oldHeight = d->height;
    qreal oldWidth = d->width;
    d->height = size.height();
    d->width = size.width();

    d->dirty(QQuickItemPrivate::Size);

    geometryChanged(QRectF(d->x, d->y, d->width, d->height),
                    QRectF(d->x, d->y, oldWidth, oldHeight));
}

/*!
    \qmlproperty bool QtQuick::Item::activeFocus

    This read-only property indicates whether the item has active focus.

    If activeFocus is true, either this item is the one that currently
    receives keyboard input, or it is a FocusScope ancestor of the item
    that currently receives keyboard input.

    Usually, activeFocus is gained by setting \l focus on an item and its
    enclosing FocusScope objects. In the following example, the \c input
    and \c focusScope objects will have active focus, while the root
    rectangle object will not.

    \qml
    import QtQuick 2.0

    Rectangle {
        width: 100; height: 100

        FocusScope {
            id: focusScope
            focus: true

            TextInput {
                id: input
                focus: true
            }
        }
    }
    \endqml

    \sa focus, {Keyboard Focus in Qt Quick}
*/
/*!
    \property QQuickItem::activeFocus

    This read-only property indicates whether the item has active focus.

    If activeFocus is true, either this item is the one that currently
    receives keyboard input, or it is a FocusScope ancestor of the item
    that currently receives keyboard input.

    Usually, activeFocus is gained by setting \l focus on an item and its
    enclosing FocusScope objects. In the following example, the \c input
    and \c focusScope objects will have active focus, while the root
    rectangle object will not.

    \qml
    import QtQuick 2.0

    Rectangle {
        width: 100; height: 100

        FocusScope {
            focus: true

            TextInput {
                id: input
                focus: true
            }
        }
    }
    \endqml

    \sa focus, {Keyboard Focus in Qt Quick}
*/
bool QQuickItem::hasActiveFocus() const
{
    Q_D(const QQuickItem);
    return d->activeFocus;
}

/*!
    \qmlproperty bool QtQuick::Item::focus

    This property holds whether the item has focus within the enclosing
    FocusScope. If true, this item will gain active focus when the
    enclosing FocusScope gains active focus.

    In the following example, \c input will be given active focus when
    \c scope gains active focus:

    \qml
    import QtQuick 2.0

    Rectangle {
        width: 100; height: 100

        FocusScope {
            id: scope

            TextInput {
                id: input
                focus: true
            }
        }
    }
    \endqml

    For the purposes of this property, the scene as a whole is assumed
    to act like a focus scope. On a practical level, that means the
    following QML will give active focus to \c input on startup.

    \qml
    Rectangle {
        width: 100; height: 100

        TextInput {
              id: input
              focus: true
        }
    }
    \endqml

    \sa activeFocus, {Keyboard Focus in Qt Quick}
*/
/*!
    \property QQuickItem::focus

    This property holds whether the item has focus within the enclosing
    FocusScope. If true, this item will gain active focus when the
    enclosing FocusScope gains active focus.

    In the following example, \c input will be given active focus when
    \c scope gains active focus:

    \qml
    import QtQuick 2.0

    Rectangle {
        width: 100; height: 100

        FocusScope {
            id: scope

            TextInput {
                id: input
                focus: true
            }
        }
    }
    \endqml

    For the purposes of this property, the scene as a whole is assumed
    to act like a focus scope. On a practical level, that means the
    following QML will give active focus to \c input on startup.

    \qml
    Rectangle {
        width: 100; height: 100

        TextInput {
              id: input
              focus: true
        }
    }
    \endqml

    \sa activeFocus, {Keyboard Focus in Qt Quick}
*/
bool QQuickItem::hasFocus() const
{
    Q_D(const QQuickItem);
    return d->focus;
}

void QQuickItem::setFocus(bool focus)
{
    setFocus(focus, Qt::OtherFocusReason);
}

void QQuickItem::setFocus(bool focus, Qt::FocusReason reason)
{
    Q_D(QQuickItem);
    if (d->focus == focus)
        return;

    if (d->window || d->parentItem) {
        // Need to find our nearest focus scope
        QQuickItem *scope = parentItem();
        while (scope && !scope->isFocusScope() && scope->parentItem())
            scope = scope->parentItem();
        if (d->window) {
            if (reason != Qt::PopupFocusReason) {
                if (focus)
                    QQuickWindowPrivate::get(d->window)->setFocusInScope(scope, this, reason);
                else
                    QQuickWindowPrivate::get(d->window)->clearFocusInScope(scope, this, reason);
            }
        } else {
            // do the focus changes from setFocusInScope/clearFocusInScope that are
            // unrelated to a window
            QVarLengthArray<QQuickItem *, 20> changed;
            QQuickItem *oldSubFocusItem = QQuickItemPrivate::get(scope)->subFocusItem;
            if (oldSubFocusItem) {
                QQuickItemPrivate::get(oldSubFocusItem)->updateSubFocusItem(scope, false);
                QQuickItemPrivate::get(oldSubFocusItem)->focus = false;
                changed << oldSubFocusItem;
            } else if (!scope->isFocusScope() && scope->hasFocus()) {
                QQuickItemPrivate::get(scope)->focus = false;
                changed << scope;
            }
            d->updateSubFocusItem(scope, focus);

            d->focus = focus;
            changed << this;
            emit focusChanged(focus);

            QQuickWindowPrivate::notifyFocusChangesRecur(changed.data(), changed.count() - 1);
        }
    } else {
        QVarLengthArray<QQuickItem *, 20> changed;
        QQuickItem *oldSubFocusItem = d->subFocusItem;
        if (!isFocusScope() && oldSubFocusItem) {
            QQuickItemPrivate::get(oldSubFocusItem)->updateSubFocusItem(this, false);
            QQuickItemPrivate::get(oldSubFocusItem)->focus = false;
            changed << oldSubFocusItem;
        }

        d->focus = focus;
        changed << this;
        emit focusChanged(focus);

        QQuickWindowPrivate::notifyFocusChangesRecur(changed.data(), changed.count() - 1);
    }
}

/*!
    Returns true if this item is a focus scope, and false otherwise.
  */
bool QQuickItem::isFocusScope() const
{
    return flags() & ItemIsFocusScope;
}

/*!
    If this item is a focus scope, this returns the item in its focus chain
    that currently has focus.

    Returns 0 if this item is not a focus scope.
  */
QQuickItem *QQuickItem::scopedFocusItem() const
{
    Q_D(const QQuickItem);
    if (!isFocusScope())
        return 0;
    else
        return d->subFocusItem;
}

/*!
    Returns \c true if this item is an ancestor of \a child (i.e., if this item
    is \a child's parent, or one of \a child's parent's ancestors).

    \since 5.7

    \sa parentItem()
  */
bool QQuickItem::isAncestorOf(const QQuickItem *child) const
{
    if (!child || child == this)
        return false;
    const QQuickItem *ancestor = child;
    while ((ancestor = ancestor->parentItem())) {
        if (ancestor == this)
            return true;
    }
    return false;
}

/*!
    Returns the mouse buttons accepted by this item.

    The default value is Qt::NoButton; that is, no mouse buttons are accepted.

    If an item does not accept the mouse button for a particular mouse event,
    the mouse event will not be delivered to the item and will be delivered
    to the next item in the item hierarchy instead.
  */
Qt::MouseButtons QQuickItem::acceptedMouseButtons() const
{
    Q_D(const QQuickItem);
    return d->acceptedMouseButtons();
}

/*!
    Sets the mouse buttons accepted by this item to \a buttons.
  */
void QQuickItem::setAcceptedMouseButtons(Qt::MouseButtons buttons)
{
    Q_D(QQuickItem);
    if (buttons & Qt::LeftButton)
        d->extra.setFlag();
    else
        d->extra.clearFlag();

    buttons &= ~Qt::LeftButton;
    if (buttons || d->extra.isAllocated())
        d->extra.value().acceptedMouseButtons = buttons;
}

/*!
    Returns whether mouse and touch events of this item's children should be filtered
    through this item.

    \sa setFiltersChildMouseEvents(), childMouseEventFilter()
  */
bool QQuickItem::filtersChildMouseEvents() const
{
    Q_D(const QQuickItem);
    return d->filtersChildMouseEvents;
}

/*!
    Sets whether mouse and touch events of this item's children should be filtered
    through this item.

    If \a filter is true, childMouseEventFilter() will be called when
    a mouse event is triggered for a child item.

    \sa filtersChildMouseEvents()
  */
void QQuickItem::setFiltersChildMouseEvents(bool filter)
{
    Q_D(QQuickItem);
    d->filtersChildMouseEvents = filter;
}

/*!
    \internal
  */
bool QQuickItem::isUnderMouse() const
{
    Q_D(const QQuickItem);
    if (!d->window)
        return false;

    QPointF cursorPos = QGuiApplicationPrivate::lastCursorPosition;
    return contains(mapFromScene(d->window->mapFromGlobal(cursorPos.toPoint())));
}

/*!
    Returns whether hover events are accepted by this item.

    The default value is false.

    If this is false, then the item will not receive any hover events through
    the hoverEnterEvent(), hoverMoveEvent() and hoverLeaveEvent() functions.
*/
bool QQuickItem::acceptHoverEvents() const
{
    Q_D(const QQuickItem);
    return d->hoverEnabled;
}

/*!
    If \a enabled is true, this sets the item to accept hover events;
    otherwise, hover events are not accepted by this item.

    \sa acceptHoverEvents()
*/
void QQuickItem::setAcceptHoverEvents(bool enabled)
{
    Q_D(QQuickItem);
    d->hoverEnabled = enabled;
    d->setHasHoverInChild(enabled);
}

void QQuickItemPrivate::setHasCursorInChild(bool hasCursor)
{
#ifndef QT_NO_CURSOR
    Q_Q(QQuickItem);

    // if we're asked to turn it off (because of an unsetcursor call, or a node
    // removal) then we should make sure it's really ok to turn it off.
    if (!hasCursor && subtreeCursorEnabled) {
        if (hasCursor)
            return; // nope! sorry, I have a cursor myself
        for (QQuickItem *otherChild : qAsConst(childItems)) {
            QQuickItemPrivate *otherChildPrivate = QQuickItemPrivate::get(otherChild);
            if (otherChildPrivate->subtreeCursorEnabled || otherChildPrivate->hasCursor)
                return; // nope! sorry, something else wants it kept on.
        }
    }

    subtreeCursorEnabled = hasCursor;
    QQuickItem *parent = q->parentItem();
    if (parent) {
        QQuickItemPrivate *parentPrivate = QQuickItemPrivate::get(parent);
        parentPrivate->setHasCursorInChild(hasCursor);
    }
#endif
}

void QQuickItemPrivate::setHasHoverInChild(bool hasHover)
{
    Q_Q(QQuickItem);

    // if we're asked to turn it off (because of a setAcceptHoverEvents call, or a node
    // removal) then we should make sure it's really ok to turn it off.
    if (!hasHover && subtreeHoverEnabled) {
        if (hoverEnabled)
            return; // nope! sorry, I need hover myself
        for (QQuickItem *otherChild : qAsConst(childItems)) {
            QQuickItemPrivate *otherChildPrivate = QQuickItemPrivate::get(otherChild);
            if (otherChildPrivate->subtreeHoverEnabled || otherChildPrivate->hoverEnabled)
                return; // nope! sorry, something else wants it kept on.
        }
    }

    qCDebug(DBG_HOVER_TRACE) << q << subtreeHoverEnabled << "->" << hasHover;
    subtreeHoverEnabled = hasHover;
    QQuickItem *parent = q->parentItem();
    if (parent) {
        QQuickItemPrivate *parentPrivate = QQuickItemPrivate::get(parent);
        parentPrivate->setHasHoverInChild(hasHover);
    }
}

#ifndef QT_NO_CURSOR

/*!
    Returns the cursor shape for this item.

    The mouse cursor will assume this shape when it is over this
    item, unless an override cursor is set.
    See the \l{Qt::CursorShape}{list of predefined cursor objects} for a
    range of useful shapes.

    If no cursor shape has been set this returns a cursor with the Qt::ArrowCursor shape, however
    another cursor shape may be displayed if an overlapping item has a valid cursor.

    \sa setCursor(), unsetCursor()
*/

QCursor QQuickItem::cursor() const
{
    Q_D(const QQuickItem);
    return d->extra.isAllocated()
            ? d->extra->cursor
            : QCursor();
}

/*!
    Sets the \a cursor shape for this item.

    \sa cursor(), unsetCursor()
*/

void QQuickItem::setCursor(const QCursor &cursor)
{
    Q_D(QQuickItem);

    Qt::CursorShape oldShape = d->extra.isAllocated() ? d->extra->cursor.shape() : Qt::ArrowCursor;

    if (oldShape != cursor.shape() || oldShape >= Qt::LastCursor || cursor.shape() >= Qt::LastCursor) {
        d->extra.value().cursor = cursor;
        if (d->window) {
            QWindow *renderWindow = QQuickRenderControl::renderWindowFor(d->window);
            QWindow *window = renderWindow ? renderWindow : d->window; // this may not be a QQuickWindow
            if (QQuickWindowPrivate::get(d->window)->cursorItem == this)
                window->setCursor(cursor);
        }
    }

    if (!d->hasCursor) {
        d->setHasCursorInChild(true);
        d->hasCursor = true;
        if (d->window) {
            QWindow *renderWindow = QQuickRenderControl::renderWindowFor(d->window);
            QWindow *window = renderWindow ? renderWindow : d->window;
            QPointF pos = window->mapFromGlobal(QGuiApplicationPrivate::lastCursorPosition.toPoint());
            if (contains(mapFromScene(pos)))
                QQuickWindowPrivate::get(d->window)->updateCursor(pos);
        }
    }
}

/*!
    Clears the cursor shape for this item.

    \sa cursor(), setCursor()
*/

void QQuickItem::unsetCursor()
{
    Q_D(QQuickItem);
    if (!d->hasCursor)
        return;
    d->setHasCursorInChild(false);
    d->hasCursor = false;
    if (d->extra.isAllocated())
        d->extra->cursor = QCursor();

    if (d->window) {
        QQuickWindowPrivate *windowPrivate = QQuickWindowPrivate::get(d->window);
        if (windowPrivate->cursorItem == this) {
            QPointF pos = d->window->mapFromGlobal(QGuiApplicationPrivate::lastCursorPosition.toPoint());
            windowPrivate->updateCursor(pos);
        }
    }
}

#endif

/*!
    Grabs the mouse input.

    This item will receive all mouse events until ungrabMouse() is called.
    Usually this function should not be called, since accepting for example
    a mouse press event makes sure that the following events are delivered
    to the item.
    If an item wants to take over mouse events from the current receiver,
    it needs to call this function.

    \warning This function should be used with caution.
  */
void QQuickItem::grabMouse()
{
    Q_D(QQuickItem);
    if (!d->window)
        return;
    QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(d->window);
    windowPriv->setMouseGrabber(this);
}

/*!
    Releases the mouse grab following a call to grabMouse().

    Note that this function should only be called when the item wants
    to stop handling further events. There is no need to call this function
    after a release or cancel event since no future events will be received
    in any case. No move or release events will be delivered after this
    function was called.
*/
void QQuickItem::ungrabMouse()
{
    Q_D(QQuickItem);
    if (!d->window)
        return;
    QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(d->window);
    windowPriv->removeGrabber(this, true, false);
}


/*!
    Returns whether mouse input should exclusively remain with this item.

    \sa setKeepMouseGrab()
 */
bool QQuickItem::keepMouseGrab() const
{
    Q_D(const QQuickItem);
    return d->keepMouse;
}

/*!
  Sets whether the mouse input should remain exclusively with this item.

  This is useful for items that wish to grab and keep mouse
  interaction following a predefined gesture.  For example,
  an item that is interested in horizontal mouse movement
  may set keepMouseGrab to true once a threshold has been
  exceeded.  Once keepMouseGrab has been set to true, filtering
  items will not react to mouse events.

  If \a keep is false, a filtering item may steal the grab. For example,
  \l Flickable may attempt to steal a mouse grab if it detects that the
  user has begun to move the viewport.

  \sa keepMouseGrab()
 */
void QQuickItem::setKeepMouseGrab(bool keep)
{
    Q_D(QQuickItem);
    d->keepMouse = keep;
}

/*!
    Grabs the touch points specified by \a ids.

    These touch points will be owned by the item until
    they are released. Alternatively, the grab can be stolen
    by a filtering item like Flickable. Use setKeepTouchGrab()
    to prevent the grab from being stolen.

    \sa ungrabTouchPoints(), setKeepTouchGrab()
*/
void QQuickItem::grabTouchPoints(const QVector<int> &ids)
{
    Q_D(QQuickItem);
    if (!d->window)
        return;
    QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(d->window);
    windowPriv->grabTouchPoints(this, ids);
}

/*!
    Ungrabs the touch points owned by this item.

    \note there is hardly any reason to call this function. It should only be
    called when an item does not want to receive any further events, so no
    move or release events will be delivered after calling this function.

    \sa grabTouchPoints()
*/
void QQuickItem::ungrabTouchPoints()
{
    Q_D(QQuickItem);
    if (!d->window)
        return;
    QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(d->window);
    windowPriv->removeGrabber(this, false, true);
}

/*!
    Returns whether the touch points grabbed by this item should exclusively
    remain with this item.

    \sa setKeepTouchGrab(), keepMouseGrab()
*/
bool QQuickItem::keepTouchGrab() const
{
    Q_D(const QQuickItem);
    return d->keepTouch;
}

/*!
  Sets whether the touch points grabbed by this item should remain
  exclusively with this item.

  This is useful for items that wish to grab and keep specific touch
  points following a predefined gesture.  For example,
  an item that is interested in horizontal touch point movement
  may set setKeepTouchGrab to true once a threshold has been
  exceeded.  Once setKeepTouchGrab has been set to true, filtering
  items will not react to the relevant touch points.

  If \a keep is false, a filtering item may steal the grab. For example,
  \l Flickable may attempt to steal a touch point grab if it detects that the
  user has begun to move the viewport.

  \sa keepTouchGrab(), setKeepMouseGrab()
 */
void QQuickItem::setKeepTouchGrab(bool keep)
{
    Q_D(QQuickItem);
    d->keepTouch = keep;
}

/*!
  \qmlmethod bool QtQuick::Item::contains(point point)

  Returns true if this item contains \a point, which is in local coordinates;
  returns false otherwise.
  */
/*!
  Returns true if this item contains \a point, which is in local coordinates;
  returns false otherwise.

  This function can be overwritten in order to handle point collisions in items
  with custom shapes. The default implementation checks if the point is inside
  the item's bounding rect.

  Note that this method is generally used to check whether the item is under the mouse cursor,
  and for that reason, the implementation of this function should be as light-weight
  as possible.
*/
bool QQuickItem::contains(const QPointF &point) const
{
    Q_D(const QQuickItem);
    qreal x = point.x();
    qreal y = point.y();
    return x >= 0 && y >= 0 && x <= d->width && y <= d->height;
}

/*!
    Maps the given \a point in this item's coordinate system to the equivalent
    point within \a item's coordinate system, and returns the mapped
    coordinate.

    If \a item is 0, this maps \a point to the coordinate system of the
    scene.

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QPointF QQuickItem::mapToItem(const QQuickItem *item, const QPointF &point) const
{
    QPointF p = mapToScene(point);
    if (item)
        p = item->mapFromScene(p);
    return p;
}

/*!
    Maps the given \a point in this item's coordinate system to the equivalent
    point within the scene's coordinate system, and returns the mapped
    coordinate.

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QPointF QQuickItem::mapToScene(const QPointF &point) const
{
    Q_D(const QQuickItem);
    return d->itemToWindowTransform().map(point);
}

/*!
    Maps the given \a point in this item's coordinate system to the equivalent
    point within global screen coordinate system, and returns the mapped
    coordinate.

    For example, this may be helpful to add a popup to a Qt Quick component.

    \note Window positioning is done by the window manager and this value is
    treated only as a hint. So, the resulting window position may differ from
    what is expected.

    \since 5.7

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QPointF QQuickItem::mapToGlobal(const QPointF &point) const
{
    Q_D(const QQuickItem);
    return d->windowToGlobalTransform().map(mapToScene(point));
}

/*!
    Maps the given \a rect in this item's coordinate system to the equivalent
    rectangular area within \a item's coordinate system, and returns the mapped
    rectangle value.

    If \a item is 0, this maps \a rect to the coordinate system of the
    scene.

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QRectF QQuickItem::mapRectToItem(const QQuickItem *item, const QRectF &rect) const
{
    Q_D(const QQuickItem);
    QTransform t = d->itemToWindowTransform();
    if (item)
        t *= QQuickItemPrivate::get(item)->windowToItemTransform();
    return t.mapRect(rect);
}

/*!
    Maps the given \a rect in this item's coordinate system to the equivalent
    rectangular area within the scene's coordinate system, and returns the mapped
    rectangle value.

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QRectF QQuickItem::mapRectToScene(const QRectF &rect) const
{
    Q_D(const QQuickItem);
    return d->itemToWindowTransform().mapRect(rect);
}

/*!
    Maps the given \a point in \a item's coordinate system to the equivalent
    point within this item's coordinate system, and returns the mapped
    coordinate.

    If \a item is 0, this maps \a point from the coordinate system of the
    scene.

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QPointF QQuickItem::mapFromItem(const QQuickItem *item, const QPointF &point) const
{
    QPointF p = item?item->mapToScene(point):point;
    return mapFromScene(p);
}

/*!
    Maps the given \a point in the scene's coordinate system to the equivalent
    point within this item's coordinate system, and returns the mapped
    coordinate.

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QPointF QQuickItem::mapFromScene(const QPointF &point) const
{
    Q_D(const QQuickItem);
    return d->windowToItemTransform().map(point);
}

/*!
    Maps the given \a point in the global screen coordinate system to the
    equivalent point within this item's coordinate system, and returns the
    mapped coordinate.

    For example, this may be helpful to add a popup to a Qt Quick component.

    \note Window positioning is done by the window manager and this value is
    treated only as a hint. So, the resulting window position may differ from
    what is expected.

    \since 5.7

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QPointF QQuickItem::mapFromGlobal(const QPointF &point) const
{
    Q_D(const QQuickItem);
    return mapFromScene(d->globalToWindowTransform().map(point));
}

/*!
    Maps the given \a rect in \a item's coordinate system to the equivalent
    rectangular area within this item's coordinate system, and returns the mapped
    rectangle value.

    If \a item is 0, this maps \a rect from the coordinate system of the
    scene.

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QRectF QQuickItem::mapRectFromItem(const QQuickItem *item, const QRectF &rect) const
{
    Q_D(const QQuickItem);
    QTransform t = item?QQuickItemPrivate::get(item)->itemToWindowTransform():QTransform();
    t *= d->windowToItemTransform();
    return t.mapRect(rect);
}

/*!
    Maps the given \a rect in the scene's coordinate system to the equivalent
    rectangular area within this item's coordinate system, and returns the mapped
    rectangle value.

    \sa {Concepts - Visual Coordinates in Qt Quick}
*/
QRectF QQuickItem::mapRectFromScene(const QRectF &rect) const
{
    Q_D(const QQuickItem);
    return d->windowToItemTransform().mapRect(rect);
}

/*!
  \property QQuickItem::anchors
  \internal
*/

/*!
  \property QQuickItem::left
  \internal
*/

/*!
  \property QQuickItem::right
  \internal
*/

/*!
  \property QQuickItem::horizontalCenter
  \internal
*/

/*!
  \property QQuickItem::top
  \internal
*/

/*!
  \property QQuickItem::bottom
  \internal
*/

/*!
  \property QQuickItem::verticalCenter
  \internal
*/

/*!
  \property QQuickItem::baseline
  \internal
*/

/*!
  \property QQuickItem::data
  \internal
*/

/*!
  \property QQuickItem::resources
  \internal
*/

/*!
  \reimp
  */
bool QQuickItem::event(QEvent *ev)
{
    Q_D(QQuickItem);

    switch (ev->type()) {
#if 0
    case QEvent::PolishRequest:
        d->polishScheduled = false;
        updatePolish();
        break;
#endif
#ifndef QT_NO_IM
    case QEvent::InputMethodQuery: {
        QInputMethodQueryEvent *query = static_cast<QInputMethodQueryEvent *>(ev);
        Qt::InputMethodQueries queries = query->queries();
        for (uint i = 0; i < 32; ++i) {
            Qt::InputMethodQuery q = (Qt::InputMethodQuery)(int)(queries & (1<<i));
            if (q) {
                QVariant v = inputMethodQuery(q);
                query->setValue(q, v);
            }
        }
        query->accept();
        break;
    }
    case QEvent::InputMethod:
        inputMethodEvent(static_cast<QInputMethodEvent *>(ev));
        break;
#endif // QT_NO_IM
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        touchEvent(static_cast<QTouchEvent*>(ev));
        break;
    case QEvent::StyleAnimationUpdate:
        if (isVisible()) {
            ev->accept();
            update();
        }
        break;
    case QEvent::HoverEnter:
        hoverEnterEvent(static_cast<QHoverEvent*>(ev));
        break;
    case QEvent::HoverLeave:
        hoverLeaveEvent(static_cast<QHoverEvent*>(ev));
        break;
    case QEvent::HoverMove:
        hoverMoveEvent(static_cast<QHoverEvent*>(ev));
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        d->deliverKeyEvent(static_cast<QKeyEvent*>(ev));
        break;
    case QEvent::FocusIn:
        focusInEvent(static_cast<QFocusEvent*>(ev));
        break;
    case QEvent::FocusOut:
        focusOutEvent(static_cast<QFocusEvent*>(ev));
        break;
    case QEvent::MouseMove:
        mouseMoveEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent(static_cast<QMouseEvent*>(ev));
        break;
#ifndef QT_NO_WHEELEVENT
    case QEvent::Wheel:
        wheelEvent(static_cast<QWheelEvent*>(ev));
        break;
#endif
#ifndef QT_NO_DRAGANDDROP
    case QEvent::DragEnter:
        dragEnterEvent(static_cast<QDragEnterEvent*>(ev));
        break;
    case QEvent::DragLeave:
        dragLeaveEvent(static_cast<QDragLeaveEvent*>(ev));
        break;
    case QEvent::DragMove:
        dragMoveEvent(static_cast<QDragMoveEvent*>(ev));
        break;
    case QEvent::Drop:
        dropEvent(static_cast<QDropEvent*>(ev));
        break;
#endif // QT_NO_DRAGANDDROP
#ifndef QT_NO_GESTURES
    case QEvent::NativeGesture:
        ev->ignore();
        break;
#endif // QT_NO_GESTURES
    default:
        return QObject::event(ev);
    }

    return true;
}

#ifndef QT_NO_DEBUG_STREAM
// FIXME: Qt 6: Make this QDebug operator<<(QDebug debug, const QQuickItem *item)
QDebug operator<<(QDebug debug, QQuickItem *item)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    if (!item) {
        debug << "QQuickItem(0)";
        return debug;
    }

    const QRectF rect(item->position(), QSizeF(item->width(), item->height()));

    debug << item->metaObject()->className() << '(' << static_cast<void *>(item);
    if (!item->objectName().isEmpty())
        debug << ", name=" << item->objectName();
    debug << ", parent=" << static_cast<void *>(item->parentItem())
          << ", geometry=";
    QtDebugUtils::formatQRect(debug, rect);
    if (const qreal z = item->z())
        debug << ", z=" << z;
    debug << ')';
    return debug;
}
#endif

/*!
    \fn bool QQuickItem::isTextureProvider() const

    Returns true if this item is a texture provider. The default
    implementation returns false.

    This function can be called from any thread.
 */

bool QQuickItem::isTextureProvider() const
{
#if QT_CONFIG(quick_shadereffect)
    Q_D(const QQuickItem);
    return d->extra.isAllocated() && d->extra->layer && d->extra->layer->effectSource() ?
           d->extra->layer->effectSource()->isTextureProvider() : false;
#else
    return false;
#endif
}

/*!
    \fn QSGTextureProvider *QQuickItem::textureProvider() const

    Returns the texture provider for an item. The default implementation
    returns 0.

    This function may only be called on the rendering thread.
 */

QSGTextureProvider *QQuickItem::textureProvider() const
{
#if QT_CONFIG(quick_shadereffect)
    Q_D(const QQuickItem);
    return d->extra.isAllocated() && d->extra->layer && d->extra->layer->effectSource() ?
           d->extra->layer->effectSource()->textureProvider() : 0;
#else
    return 0;
#endif
}

/*!
    \property QQuickItem::layer
    \internal
  */
QQuickItemLayer *QQuickItemPrivate::layer() const
{
#if QT_CONFIG(quick_shadereffect)
    if (!extra.isAllocated() || !extra->layer) {
        extra.value().layer = new QQuickItemLayer(const_cast<QQuickItem *>(q_func()));
        if (!componentComplete)
            extra->layer->classBegin();
    }
    return extra->layer;
#else
    return 0;
#endif
}

#if QT_CONFIG(quick_shadereffect)
QQuickItemLayer::QQuickItemLayer(QQuickItem *item)
    : m_item(item)
    , m_enabled(false)
    , m_mipmap(false)
    , m_smooth(false)
    , m_componentComplete(true)
    , m_wrapMode(QQuickShaderEffectSource::ClampToEdge)
    , m_format(QQuickShaderEffectSource::RGBA)
    , m_name("source")
    , m_effectComponent(0)
    , m_effect(0)
    , m_effectSource(0)
    , m_textureMirroring(QQuickShaderEffectSource::MirrorVertically)
{
}

QQuickItemLayer::~QQuickItemLayer()
{
    delete m_effectSource;
    delete m_effect;
}

/*!
    \qmlproperty bool QtQuick::Item::layer.enabled

    Holds whether the item is layered or not. Layering is disabled by default.

    A layered item is rendered into an offscreen surface and cached until
    it is changed. Enabling layering for complex QML item hierarchies can
    sometimes be an optimization.

    None of the other layer properties have any effect when the layer
    is disabled.

    \sa {Item Layers}
 */
void QQuickItemLayer::setEnabled(bool e)
{
    if (e == m_enabled)
        return;
    m_enabled = e;
    if (m_componentComplete) {
        if (m_enabled)
            activate();
        else
            deactivate();
    }

    emit enabledChanged(e);
}

void QQuickItemLayer::classBegin()
{
    Q_ASSERT(!m_effectSource);
    Q_ASSERT(!m_effect);
    m_componentComplete = false;
}

void QQuickItemLayer::componentComplete()
{
    Q_ASSERT(!m_componentComplete);
    m_componentComplete = true;
    if (m_enabled)
        activate();
}

void QQuickItemLayer::activate()
{
    Q_ASSERT(!m_effectSource);
    m_effectSource = new QQuickShaderEffectSource();
    QQuickItemPrivate::get(m_effectSource)->setTransparentForPositioner(true);

    QQuickItem *parentItem = m_item->parentItem();
    if (parentItem) {
        m_effectSource->setParentItem(parentItem);
        m_effectSource->stackAfter(m_item);
    }

    m_effectSource->setSourceItem(m_item);
    m_effectSource->setHideSource(true);
    m_effectSource->setSmooth(m_smooth);
    m_effectSource->setTextureSize(m_size);
    m_effectSource->setSourceRect(m_sourceRect);
    m_effectSource->setMipmap(m_mipmap);
    m_effectSource->setWrapMode(m_wrapMode);
    m_effectSource->setFormat(m_format);
    m_effectSource->setTextureMirroring(m_textureMirroring);

    if (m_effectComponent)
        activateEffect();

    m_effectSource->setVisible(m_item->isVisible() && !m_effect);

    updateZ();
    updateGeometry();
    updateOpacity();
    updateMatrix();

    QQuickItemPrivate *id = QQuickItemPrivate::get(m_item);
    id->addItemChangeListener(this, QQuickItemPrivate::Geometry | QQuickItemPrivate::Opacity | QQuickItemPrivate::Parent | QQuickItemPrivate::Visibility | QQuickItemPrivate::SiblingOrder);
}

void QQuickItemLayer::deactivate()
{
    Q_ASSERT(m_effectSource);

    if (m_effectComponent)
        deactivateEffect();

    delete m_effectSource;
    m_effectSource = 0;

    QQuickItemPrivate *id = QQuickItemPrivate::get(m_item);
    id->removeItemChangeListener(this,  QQuickItemPrivate::Geometry | QQuickItemPrivate::Opacity | QQuickItemPrivate::Parent | QQuickItemPrivate::Visibility | QQuickItemPrivate::SiblingOrder);
}

void QQuickItemLayer::activateEffect()
{
    Q_ASSERT(m_effectSource);
    Q_ASSERT(m_effectComponent);
    Q_ASSERT(!m_effect);

    QObject *created = m_effectComponent->beginCreate(m_effectComponent->creationContext());
    m_effect = qobject_cast<QQuickItem *>(created);
    if (!m_effect) {
        qWarning("Item: layer.effect is not a QML Item.");
        m_effectComponent->completeCreate();
        delete created;
        return;
    }
    QQuickItem *parentItem = m_item->parentItem();
    if (parentItem) {
        m_effect->setParentItem(parentItem);
        m_effect->stackAfter(m_effectSource);
    }
    m_effect->setVisible(m_item->isVisible());
    m_effect->setProperty(m_name, qVariantFromValue<QObject *>(m_effectSource));
    QQuickItemPrivate::get(m_effect)->setTransparentForPositioner(true);
    m_effectComponent->completeCreate();
}

void QQuickItemLayer::deactivateEffect()
{
    Q_ASSERT(m_effectSource);
    Q_ASSERT(m_effectComponent);

    delete m_effect;
    m_effect = 0;
}


/*!
    \qmlproperty Component QtQuick::Item::layer.effect

    Holds the effect that is applied to this layer.

    The effect is typically a \l ShaderEffect component, although any \l Item component can be
    assigned. The effect should have a source texture property with a name matching \l layer.samplerName.

    \sa layer.samplerName, {Item Layers}
 */

void QQuickItemLayer::setEffect(QQmlComponent *component)
{
    if (component == m_effectComponent)
        return;

    bool updateNeeded = false;
    if (m_effectSource && m_effectComponent) {
        deactivateEffect();
        updateNeeded = true;
    }

    m_effectComponent = component;

    if (m_effectSource && m_effectComponent) {
        activateEffect();
        updateNeeded = true;
    }

    if (updateNeeded) {
        updateZ();
        updateGeometry();
        updateOpacity();
        updateMatrix();
        m_effectSource->setVisible(m_item->isVisible() && !m_effect);
    }

    emit effectChanged(component);
}


/*!
    \qmlproperty bool QtQuick::Item::layer.mipmap

    If this property is true, mipmaps are generated for the texture.

    \note Some OpenGL ES 2 implementations do not support mipmapping of
    non-power-of-two textures.

    \sa {Item Layers}
 */

void QQuickItemLayer::setMipmap(bool mipmap)
{
    if (mipmap == m_mipmap)
        return;
    m_mipmap = mipmap;

    if (m_effectSource)
        m_effectSource->setMipmap(m_mipmap);

    emit mipmapChanged(mipmap);
}


/*!
    \qmlproperty enumeration QtQuick::Item::layer.format

    This property defines the internal OpenGL format of the texture.
    Modifying this property makes most sense when the \a layer.effect is also
    specified. Depending on the OpenGL implementation, this property might
    allow you to save some texture memory.

    \list
    \li ShaderEffectSource.Alpha - GL_ALPHA;
    \li ShaderEffectSource.RGB - GL_RGB
    \li ShaderEffectSource.RGBA - GL_RGBA
    \endlist

    \note ShaderEffectSource.RGB and ShaderEffectSource.Alpha should
    be used with caution, as support for these formats in the underlying
    hardare and driver is often not present.

    \sa {Item Layers}
 */

void QQuickItemLayer::setFormat(QQuickShaderEffectSource::Format f)
{
    if (f == m_format)
        return;
    m_format = f;

    if (m_effectSource)
        m_effectSource->setFormat(m_format);

    emit formatChanged(m_format);
}


/*!
    \qmlproperty rect QtQuick::Item::layer.sourceRect

    This property defines the rectangular area of the item that should be
    rendered into the texture. The source rectangle can be larger than
    the item itself. If the rectangle is null, which is the default,
    then the whole item is rendered to the texture.

    \sa {Item Layers}
 */

void QQuickItemLayer::setSourceRect(const QRectF &sourceRect)
{
    if (sourceRect == m_sourceRect)
        return;
    m_sourceRect = sourceRect;

    if (m_effectSource)
        m_effectSource->setSourceRect(m_sourceRect);

    emit sourceRectChanged(sourceRect);
}

/*!
    \qmlproperty bool QtQuick::Item::layer.smooth

    Holds whether the layer is smoothly transformed.

    \sa {Item Layers}
 */

void QQuickItemLayer::setSmooth(bool s)
{
    if (m_smooth == s)
        return;
    m_smooth = s;

    if (m_effectSource)
        m_effectSource->setSmooth(m_smooth);

    emit smoothChanged(s);
}

/*!
    \qmlproperty size QtQuick::Item::layer.textureSize

    This property holds the requested pixel size of the layers texture. If it is empty,
    which is the default, the size of the item is used.

    \note Some platforms have a limit on how small framebuffer objects can be,
    which means the actual texture size might be larger than the requested
    size.

    \sa {Item Layers}
 */

void QQuickItemLayer::setSize(const QSize &size)
{
    if (size == m_size)
        return;
    m_size = size;

    if (m_effectSource)
        m_effectSource->setTextureSize(size);

    emit sizeChanged(size);
}

/*!
    \qmlproperty enumeration QtQuick::Item::layer.wrapMode

    This property defines the OpenGL wrap modes associated with the texture.
    Modifying this property makes most sense when the \a layer.effect is
    specified.

    \list
    \li ShaderEffectSource.ClampToEdge - GL_CLAMP_TO_EDGE both horizontally and vertically
    \li ShaderEffectSource.RepeatHorizontally - GL_REPEAT horizontally, GL_CLAMP_TO_EDGE vertically
    \li ShaderEffectSource.RepeatVertically - GL_CLAMP_TO_EDGE horizontally, GL_REPEAT vertically
    \li ShaderEffectSource.Repeat - GL_REPEAT both horizontally and vertically
    \endlist

    \note Some OpenGL ES 2 implementations do not support the GL_REPEAT
    wrap mode with non-power-of-two textures.

    \sa {Item Layers}
 */

void QQuickItemLayer::setWrapMode(QQuickShaderEffectSource::WrapMode mode)
{
    if (mode == m_wrapMode)
        return;
    m_wrapMode = mode;

    if (m_effectSource)
        m_effectSource->setWrapMode(m_wrapMode);

    emit wrapModeChanged(mode);
}

/*!
    \qmlproperty enumeration QtQuick::Item::layer.textureMirroring
    \since 5.6

    This property defines how the generated OpenGL texture should be mirrored.
    The default value is \c{ShaderEffectSource.MirrorVertically}.
    Custom mirroring can be useful if the generated texture is directly accessed by custom shaders,
    such as those specified by ShaderEffect. If no effect is specified for the layered
    item, mirroring has no effect on the UI representation of the item.

    \list
    \li ShaderEffectSource.NoMirroring - No mirroring
    \li ShaderEffectSource.MirrorHorizontally - The generated texture is flipped along X-axis.
    \li ShaderEffectSource.MirrorVertically - The generated texture is flipped along Y-axis.
    \endlist
 */

void QQuickItemLayer::setTextureMirroring(QQuickShaderEffectSource::TextureMirroring mirroring)
{
    if (mirroring == m_textureMirroring)
        return;
    m_textureMirroring = mirroring;

    if (m_effectSource)
        m_effectSource->setTextureMirroring(m_textureMirroring);

    emit textureMirroringChanged(mirroring);
}

/*!
    \qmlproperty string QtQuick::Item::layer.samplerName

    Holds the name of the effect's source texture property.

    This value must match the name of the effect's source texture property
    so that the Item can pass the layer's offscreen surface to the effect correctly.

    \sa layer.effect, ShaderEffect, {Item Layers}
 */

void QQuickItemLayer::setName(const QByteArray &name) {
    if (m_name == name)
        return;
    if (m_effect) {
        m_effect->setProperty(m_name, QVariant());
        m_effect->setProperty(name, qVariantFromValue<QObject *>(m_effectSource));
    }
    m_name = name;
    emit nameChanged(name);
}

void QQuickItemLayer::itemOpacityChanged(QQuickItem *item)
{
    Q_UNUSED(item)
    updateOpacity();
}

void QQuickItemLayer::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    updateGeometry();
}

void QQuickItemLayer::itemParentChanged(QQuickItem *item, QQuickItem *parent)
{
    Q_UNUSED(item)
    Q_ASSERT(item == m_item);
    Q_ASSERT(parent != m_effectSource);
    Q_ASSERT(parent == 0 || parent != m_effect);

    m_effectSource->setParentItem(parent);
    if (parent)
        m_effectSource->stackAfter(m_item);

    if (m_effect) {
        m_effect->setParentItem(parent);
        if (parent)
            m_effect->stackAfter(m_effectSource);
    }
}

void QQuickItemLayer::itemSiblingOrderChanged(QQuickItem *)
{
    m_effectSource->stackAfter(m_item);
    if (m_effect)
        m_effect->stackAfter(m_effectSource);
}

void QQuickItemLayer::itemVisibilityChanged(QQuickItem *)
{
    QQuickItem *l = m_effect ? (QQuickItem *) m_effect : (QQuickItem *) m_effectSource;
    Q_ASSERT(l);
    l->setVisible(m_item->isVisible());
}

void QQuickItemLayer::updateZ()
{
    if (!m_componentComplete || !m_enabled)
        return;
    QQuickItem *l = m_effect ? (QQuickItem *) m_effect : (QQuickItem *) m_effectSource;
    Q_ASSERT(l);
    l->setZ(m_item->z());
}

void QQuickItemLayer::updateOpacity()
{
    QQuickItem *l = m_effect ? (QQuickItem *) m_effect : (QQuickItem *) m_effectSource;
    Q_ASSERT(l);
    l->setOpacity(m_item->opacity());
}

void QQuickItemLayer::updateGeometry()
{
    QQuickItem *l = m_effect ? (QQuickItem *) m_effect : (QQuickItem *) m_effectSource;
    Q_ASSERT(l);
    QRectF bounds = m_item->clipRect();
    l->setWidth(bounds.width());
    l->setHeight(bounds.height());
    l->setX(bounds.x() + m_item->x());
    l->setY(bounds.y() + m_item->y());
}

void QQuickItemLayer::updateMatrix()
{
    // Called directly from transformChanged(), so needs some extra
    // checks.
    if (!m_componentComplete || !m_enabled)
        return;
    QQuickItem *l = m_effect ? (QQuickItem *) m_effect : (QQuickItem *) m_effectSource;
    Q_ASSERT(l);
    QQuickItemPrivate *ld = QQuickItemPrivate::get(l);
    l->setScale(m_item->scale());
    l->setRotation(m_item->rotation());
    ld->transforms = QQuickItemPrivate::get(m_item)->transforms;
    if (ld->origin() != QQuickItemPrivate::get(m_item)->origin())
        ld->extra.value().origin = QQuickItemPrivate::get(m_item)->origin();
    ld->dirty(QQuickItemPrivate::Transform);
}
#endif // quick_shadereffect

QQuickItemPrivate::ExtraData::ExtraData()
: z(0), scale(1), rotation(0), opacity(1),
  contents(0), screenAttached(0), layoutDirectionAttached(0),
  enterKeyAttached(0),
  keyHandler(0),
#if QT_CONFIG(quick_shadereffect)
  layer(0),
#endif
  effectRefCount(0), hideRefCount(0),
  opacityNode(0), clipNode(0), rootNode(0),
  acceptedMouseButtons(0), origin(QQuickItem::Center),
  transparentForPositioner(false)
{
}


#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickItemPrivate::accessibleRole() const
{
    Q_Q(const QQuickItem);
    QQuickAccessibleAttached *accessibleAttached = qobject_cast<QQuickAccessibleAttached *>(qmlAttachedPropertiesObject<QQuickAccessibleAttached>(q, false));
    if (accessibleAttached)
        return accessibleAttached->role();

    return QAccessible::NoRole;
}
#endif

// helper code to let a visual parent mark its visual children for the garbage collector

namespace QV4 {
namespace Heap {
struct QQuickItemWrapper : public QObjectWrapper {
};
}
}

struct QQuickItemWrapper : public QV4::QObjectWrapper {
    V4_OBJECT2(QQuickItemWrapper, QV4::QObjectWrapper)
    static void markObjects(QV4::Heap::Base *that, QV4::ExecutionEngine *e);
};

DEFINE_OBJECT_VTABLE(QQuickItemWrapper);

void QQuickItemWrapper::markObjects(QV4::Heap::Base *that, QV4::ExecutionEngine *e)
{
    QObjectWrapper::Data *This = static_cast<QObjectWrapper::Data *>(that);
    if (QQuickItem *item = static_cast<QQuickItem*>(This->object())) {
        foreach (QQuickItem *child, QQuickItemPrivate::get(item)->childItems)
            QV4::QObjectWrapper::markWrapper(child, e);
    }
    QV4::QObjectWrapper::markObjects(that, e);
}

quint64 QQuickItemPrivate::_q_createJSWrapper(QV4::ExecutionEngine *engine)
{
    return (engine->memoryManager->allocObject<QQuickItemWrapper>(q_func()))->asReturnedValue();
}

QT_END_NAMESPACE

#include <moc_qquickitem.cpp>
