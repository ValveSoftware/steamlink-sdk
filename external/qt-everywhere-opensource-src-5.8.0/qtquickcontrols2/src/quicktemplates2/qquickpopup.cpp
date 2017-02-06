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

#include "qquickpopup_p.h"
#include "qquickpopup_p_p.h"
#include "qquickapplicationwindow_p.h"
#include "qquickshortcutcontext_p_p.h"
#include "qquickoverlay_p_p.h"
#include "qquickcontrol_p_p.h"
#include "qquickdialog_p.h"

#include <QtGui/private/qshortcutmap_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtQml/qqmlinfo.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/private/qquicktransition_p.h>
#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Popup
    \inherits QtObject
    \instantiates QQuickPopup
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-popups
    \brief Base type of popup-like user interface controls.

    Popup is the base type of popup-like user interface controls. It can be
    used with \l Window or \l ApplicationWindow.

    \qml
    import QtQuick.Window 2.2
    import QtQuick.Controls 2.1

    ApplicationWindow {
        id: window
        width: 400
        height: 400
        visible: true

        Button {
            text: "Open"
            onClicked: popup.open()
        }

        Popup {
            id: popup
            x: 100
            y: 100
            width: 200
            height: 300
            modal: true
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        }
    }
    \endqml

    In order to ensure that a popup is displayed above other items in the
    scene, it is recommended to use ApplicationWindow. ApplicationWindow also
    provides background dimming effects.

    Popup does not provide a layout of its own, but requires you to position
    its contents, for instance by creating a \l RowLayout or a \l ColumnLayout.

    Items declared as children of a Popup are automatically parented to the
    Popups's \l contentItem. Items created dynamically need to be explicitly
    parented to the contentItem.

    \section1 Popup Layout

    The following diagram illustrates the layout of a typical popup:

    \image qtquickcontrols2-popup.png

    The \l implicitWidth and \l implicitHeight of a popup are typically based
    on the implicit sizes of the background and the content item plus any
    \l padding. These properties determine how large the popup will be when no
    explicit \l width or \l height is specified.

    The \l background item fills the entire width and height of the popup,
    unless an explicit size has been given for it.

    The geometry of the \l contentItem is determined by the \l padding.

    \section1 Popup Sizing

    If only a single item is used within a Popup, it will resize to fit the
    implicit size of its contained item. This makes it particularly suitable
    for use together with layouts.

    \code
    Popup {
        ColumnLayout {
            anchors.fill: parent
            CheckBox { text: qsTr("E-mail") }
            CheckBox { text: qsTr("Calendar") }
            CheckBox { text: qsTr("Contacts") }
        }
    }
    \endcode

    Sometimes there might be two items within the popup:

    \code
    Popup {
        SwipeView {
            // ...
        }
        PageIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
        }
    }
    \endcode

    In this case, Popup cannot calculate a sensible implicit size. Since we're
    anchoring the \l PageIndicator over the \l SwipeView, we can simply set the
    content size to the view's implicit size:

    \code
    Popup {
        contentWidth: view.implicitWidth
        contentHeight: view.implicitHeight

        SwipeView {
            id: view
            // ...
        }
        PageIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
        }
     }
    \endcode

    \sa {Popup Controls}, {Customizing Popup}, ApplicationWindow
*/

/*!
    \qmlsignal void QtQuick.Controls::Popup::opened()

    This signal is emitted when the popup is opened.

    \sa closed
*/

/*!
    \qmlsignal void QtQuick.Controls::Popup::closed()

    This signal is emitted when the popup is closed.

    \sa opened
*/

static const QQuickItemPrivate::ChangeTypes AncestorChangeTypes = QQuickItemPrivate::Geometry
                                                                  | QQuickItemPrivate::Parent
                                                                  | QQuickItemPrivate::Children;

static const QQuickItemPrivate::ChangeTypes ItemChangeTypes = QQuickItemPrivate::Geometry
                                                             | QQuickItemPrivate::Parent;

QQuickPopupPrivate::QQuickPopupPrivate()
    : QObjectPrivate()
    , focus(false)
    , modal(false)
    , dim(false)
    , hasDim(false)
    , visible(false)
    , complete(false)
    , positioning(false)
    , hasWidth(false)
    , hasHeight(false)
    , hasTopMargin(false)
    , hasLeftMargin(false)
    , hasRightMargin(false)
    , hasBottomMargin(false)
    , allowVerticalFlip(false)
    , allowHorizontalFlip(false)
    , allowVerticalMove(true)
    , allowHorizontalMove(true)
    , allowVerticalResize(true)
    , allowHorizontalResize(true)
    , hadActiveFocusBeforeExitTransition(false)
    , x(0)
    , y(0)
    , effectiveX(0)
    , effectiveY(0)
    , margins(-1)
    , topMargin(0)
    , leftMargin(0)
    , rightMargin(0)
    , bottomMargin(0)
    , contentWidth(0)
    , contentHeight(0)
    , transitionState(QQuickPopupPrivate::NoTransition)
    , closePolicy(QQuickPopup::CloseOnEscape | QQuickPopup::CloseOnPressOutside)
    , parentItem(nullptr)
    , dimmer(nullptr)
    , window(nullptr)
    , enter(nullptr)
    , exit(nullptr)
    , popupItem(nullptr)
    , positioner(this)
    , transitionManager(this)
{
}

QQuickPopupPrivate::~QQuickPopupPrivate()
{
}

void QQuickPopupPrivate::init()
{
    Q_Q(QQuickPopup);
    popupItem = new QQuickPopupItem(q);
    q->setParentItem(qobject_cast<QQuickItem *>(parent));
    QObject::connect(popupItem, &QQuickControl::paddingChanged, q, &QQuickPopup::paddingChanged);
}

static void closeOrReject(QQuickPopup *popup)
{
    if (QQuickDialog *dialog = qobject_cast<QQuickDialog*>(popup))
        dialog->reject();
    else
        popup->close();
}

bool QQuickPopupPrivate::tryClose(QQuickItem *item, QMouseEvent *event)
{
    Q_Q(QQuickPopup);
    const bool isPress = event->type() == QEvent::MouseButtonPress;
    const bool onOutside = closePolicy.testFlag(isPress ? QQuickPopup::CloseOnPressOutside : QQuickPopup::CloseOnReleaseOutside);
    const bool onOutsideParent = closePolicy.testFlag(isPress ? QQuickPopup::CloseOnPressOutsideParent : QQuickPopup::CloseOnReleaseOutsideParent);
    if (onOutside || onOutsideParent) {
        if (!popupItem->contains(item->mapToItem(popupItem, event->pos()))) {
            if (!onOutsideParent || !parentItem || !parentItem->contains(item->mapToItem(parentItem, event->pos()))) {
                closeOrReject(q);
                return true;
            }
        }
    }
    return false;
}

bool QQuickPopupPrivate::prepareEnterTransition()
{
    Q_Q(QQuickPopup);
    if (!window) {
        qmlInfo(q) << "cannot find any window to open popup in.";
        return false;
    }

    if (transitionState == EnterTransition && transitionManager.isRunning())
        return false;

    if (transitionState != EnterTransition) {
        popupItem->setParentItem(QQuickOverlay::overlay(window));
        emit q->aboutToShow();
        visible = true;
        transitionState = EnterTransition;
        popupItem->setVisible(true);
        positioner.setParentItem(parentItem);
        emit q->visibleChanged();
    }
    return true;
}

bool QQuickPopupPrivate::prepareExitTransition()
{
    Q_Q(QQuickPopup);
    if (transitionState == ExitTransition && transitionManager.isRunning())
        return false;

    if (transitionState != ExitTransition) {
        if (focus) {
            // The setFocus(false) call below removes any active focus before we're
            // able to check it in finalizeExitTransition.
            hadActiveFocusBeforeExitTransition = popupItem->hasActiveFocus();
            popupItem->setFocus(false);
        }
        transitionState = ExitTransition;
        emit q->aboutToHide();
    }
    return true;
}

void QQuickPopupPrivate::finalizeEnterTransition()
{
    Q_Q(QQuickPopup);
    if (focus)
        popupItem->setFocus(true);
    transitionState = NoTransition;
    emit q->opened();
}

void QQuickPopupPrivate::finalizeExitTransition()
{
    Q_Q(QQuickPopup);
    positioner.setParentItem(nullptr);
    popupItem->setParentItem(nullptr);
    popupItem->setVisible(false);

    if (hadActiveFocusBeforeExitTransition && window) {
        if (!qobject_cast<QQuickPopupItem *>(window->activeFocusItem())) {
            QQuickApplicationWindow *applicationWindow = qobject_cast<QQuickApplicationWindow*>(window);
            if (applicationWindow)
                applicationWindow->contentItem()->setFocus(true);
            else
                window->contentItem()->setFocus(true);
        }
    }

    visible = false;
    transitionState = NoTransition;
    hadActiveFocusBeforeExitTransition = false;
    emit q->visibleChanged();
    emit q->closed();
}

QMarginsF QQuickPopupPrivate::getMargins() const
{
    Q_Q(const QQuickPopup);
    return QMarginsF(q->leftMargin(), q->topMargin(), q->rightMargin(), q->bottomMargin());
}

void QQuickPopupPrivate::setTopMargin(qreal value, bool reset)
{
    Q_Q(QQuickPopup);
    qreal oldMargin = q->topMargin();
    topMargin = value;
    hasTopMargin = !reset;
    if ((!reset && !qFuzzyCompare(oldMargin, value)) || (reset && !qFuzzyCompare(oldMargin, margins))) {
        emit q->topMarginChanged();
        q->marginsChange(QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin),
                         QMarginsF(leftMargin, oldMargin, rightMargin, bottomMargin));
    }
}

void QQuickPopupPrivate::setLeftMargin(qreal value, bool reset)
{
    Q_Q(QQuickPopup);
    qreal oldMargin = q->leftMargin();
    leftMargin = value;
    hasLeftMargin = !reset;
    if ((!reset && !qFuzzyCompare(oldMargin, value)) || (reset && !qFuzzyCompare(oldMargin, margins))) {
        emit q->leftMarginChanged();
        q->marginsChange(QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin),
                         QMarginsF(oldMargin, topMargin, rightMargin, bottomMargin));
    }
}

void QQuickPopupPrivate::setRightMargin(qreal value, bool reset)
{
    Q_Q(QQuickPopup);
    qreal oldMargin = q->rightMargin();
    rightMargin = value;
    hasRightMargin = !reset;
    if ((!reset && !qFuzzyCompare(oldMargin, value)) || (reset && !qFuzzyCompare(oldMargin, margins))) {
        emit q->rightMarginChanged();
        q->marginsChange(QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin),
                         QMarginsF(leftMargin, topMargin, oldMargin, bottomMargin));
    }
}

void QQuickPopupPrivate::setBottomMargin(qreal value, bool reset)
{
    Q_Q(QQuickPopup);
    qreal oldMargin = q->bottomMargin();
    bottomMargin = value;
    hasBottomMargin = !reset;
    if ((!reset && !qFuzzyCompare(oldMargin, value)) || (reset && !qFuzzyCompare(oldMargin, margins))) {
        emit q->bottomMarginChanged();
        q->marginsChange(QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin),
                         QMarginsF(leftMargin, topMargin, rightMargin, oldMargin));
    }
}

void QQuickPopupPrivate::setWindow(QQuickWindow *newWindow)
{
    Q_Q(QQuickPopup);
    if (window == newWindow)
        return;

    if (window) {
        QQuickOverlay *overlay = QQuickOverlay::overlay(window);
        if (overlay)
            QQuickOverlayPrivate::get(overlay)->removePopup(q);
    }

    if (newWindow) {
        QQuickOverlay *overlay = QQuickOverlay::overlay(newWindow);
        if (overlay)
            QQuickOverlayPrivate::get(overlay)->addPopup(q);
    }

    window = newWindow;
    emit q->windowChanged(newWindow);
}

class QQuickPopupItemPrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickPopupItem)

public:
    QQuickPopupItemPrivate(QQuickPopup *popup);

    void implicitWidthChanged() override;
    void implicitHeightChanged() override;

    void resolveFont() override;

    QQuickItem *getContentItem() override;

    int backId;
    int escapeId;
    QQuickPopup *popup;
};

QQuickPopupItemPrivate::QQuickPopupItemPrivate(QQuickPopup *popup)
    : backId(0),
      escapeId(0),
      popup(popup)
{
    isTabFence = true;
}

void QQuickPopupItemPrivate::implicitWidthChanged()
{
    QQuickControlPrivate::implicitWidthChanged();
    emit popup->implicitWidthChanged();
}

void QQuickPopupItemPrivate::implicitHeightChanged()
{
    QQuickControlPrivate::implicitHeightChanged();
    emit popup->implicitHeightChanged();
}

void QQuickPopupItemPrivate::resolveFont()
{
    if (QQuickApplicationWindow *window = qobject_cast<QQuickApplicationWindow *>(popup->window()))
        inheritFont(window->font());
}

QQuickItem *QQuickPopupItemPrivate::getContentItem()
{
    Q_Q(QQuickPopupItem);
    if (!contentItem)
        contentItem = new QQuickItem(q);
    return contentItem;
}

QQuickPopupItem::QQuickPopupItem(QQuickPopup *popup) :
    QQuickControl(*(new QQuickPopupItemPrivate(popup)), nullptr)
{
    setParent(popup);
    setVisible(false);
    setFlag(ItemIsFocusScope);
    setAcceptedMouseButtons(Qt::AllButtons);

    // TODO: switch to QStyleHints::useHoverEffects in Qt 5.8
    setHoverEnabled(true);
    // setAcceptHoverEvents(QGuiApplication::styleHints()->useHoverEffects());
    // connect(QGuiApplication::styleHints(), &QStyleHints::useHoverEffectsChanged, this, &QQuickItem::setAcceptHoverEvents);
}

void QQuickPopupItem::updatePolish()
{
    Q_D(QQuickPopupItem);
    return QQuickPopupPrivate::get(d->popup)->reposition();
}

void QQuickPopupItem::grabShortcut()
{
#ifndef QT_NO_SHORTCUT
    Q_D(QQuickPopupItem);
    QGuiApplicationPrivate *pApp = QGuiApplicationPrivate::instance();
    if (!d->backId)
        d->backId = pApp->shortcutMap.addShortcut(this, Qt::Key_Back, Qt::WindowShortcut, QQuickShortcutContext::matcher);
    if (!d->escapeId)
        d->escapeId = pApp->shortcutMap.addShortcut(this, Qt::Key_Escape, Qt::WindowShortcut, QQuickShortcutContext::matcher);
#endif // QT_NO_SHORTCUT
}

void QQuickPopupItem::ungrabShortcut()
{
#ifndef QT_NO_SHORTCUT
    Q_D(QQuickPopupItem);
    QGuiApplicationPrivate *pApp = QGuiApplicationPrivate::instance();
    if (d->backId) {
        pApp->shortcutMap.removeShortcut(d->backId, this);
        d->backId = 0;
    }
    if (d->escapeId) {
        pApp->shortcutMap.removeShortcut(d->escapeId, this);
        d->escapeId = 0;
    }
#endif // QT_NO_SHORTCUT
}

bool QQuickPopupItem::event(QEvent *event)
{
    Q_D(QQuickPopupItem);
    if (event->type() == QEvent::Shortcut) {
        QShortcutEvent *se = static_cast<QShortcutEvent *>(event);
        if (se->shortcutId() == d->escapeId || se->shortcutId() == d->backId) {
            closeOrReject(d->popup);
            return true;
        }
    }
    return QQuickItem::event(event);
}

bool QQuickPopupItem::childMouseEventFilter(QQuickItem *child, QEvent *event)
{
    Q_D(QQuickPopupItem);
    return d->popup->childMouseEventFilter(child, event);
}

void QQuickPopupItem::focusInEvent(QFocusEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->focusInEvent(event);
}

void QQuickPopupItem::focusOutEvent(QFocusEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->focusOutEvent(event);
}

void QQuickPopupItem::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->keyPressEvent(event);
}

void QQuickPopupItem::keyReleaseEvent(QKeyEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->keyReleaseEvent(event);
}

void QQuickPopupItem::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->mousePressEvent(event);
}

void QQuickPopupItem::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->mouseMoveEvent(event);
}

void QQuickPopupItem::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->mouseReleaseEvent(event);
}

void QQuickPopupItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->mouseDoubleClickEvent(event);
}

void QQuickPopupItem::mouseUngrabEvent()
{
    Q_D(QQuickPopupItem);
    d->popup->mouseUngrabEvent();
}

void QQuickPopupItem::wheelEvent(QWheelEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->wheelEvent(event);
}

void QQuickPopupItem::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_D(QQuickPopupItem);
    QQuickControl::contentItemChange(newItem, oldItem);
    d->popup->contentItemChange(newItem, oldItem);
}

void QQuickPopupItem::fontChange(const QFont &newFont, const QFont &oldFont)
{
    Q_D(QQuickPopupItem);
    QQuickControl::fontChange(newFont, oldFont);
    d->popup->fontChange(newFont, oldFont);
}

void QQuickPopupItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickPopupItem);
    QQuickControl::geometryChanged(newGeometry, oldGeometry);
    d->popup->geometryChanged(newGeometry, oldGeometry);
}

void QQuickPopupItem::localeChange(const QLocale &newLocale, const QLocale &oldLocale)
{
    Q_D(QQuickPopupItem);
    QQuickControl::localeChange(newLocale, oldLocale);
    d->popup->localeChange(newLocale, oldLocale);
}

void QQuickPopupItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    Q_D(QQuickPopupItem);
    QQuickControl::itemChange(change, data);
    d->popup->itemChange(change, data);
}

void QQuickPopupItem::paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding)
{
    Q_D(QQuickPopupItem);
    QQuickControl::paddingChange(newPadding, oldPadding);
    d->popup->paddingChange(newPadding, oldPadding);
}

void QQuickPopupItem::spacingChange(qreal newSpacing, qreal oldSpacing)
{
    Q_D(QQuickPopupItem);
    QQuickControl::spacingChange(newSpacing, oldSpacing);
    d->popup->spacingChange(newSpacing, oldSpacing);
}

QFont QQuickPopupItem::defaultFont() const
{
    Q_D(const QQuickPopupItem);
    return d->popup->defaultFont();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickPopupItem::accessibleRole() const
{
    Q_D(const QQuickPopupItem);
    return d->popup->accessibleRole();
}

void QQuickPopupItem::accessibilityActiveChanged(bool active)
{
    Q_D(const QQuickPopupItem);
    QQuickControl::accessibilityActiveChanged(active);
    d->popup->accessibilityActiveChanged(active);
}
#endif // QT_NO_ACCESSIBILITY

QQuickPopupPositioner::QQuickPopupPositioner(QQuickPopupPrivate *popup) :
    m_parentItem(nullptr),
    m_popup(popup)
{
}

QQuickPopupPositioner::~QQuickPopupPositioner()
{
    if (m_parentItem) {
        QQuickItemPrivate::get(m_parentItem)->removeItemChangeListener(this, ItemChangeTypes);
        removeAncestorListeners(m_parentItem->parentItem());
    }
}

QQuickItem *QQuickPopupPositioner::parentItem() const
{
    return m_parentItem;
}

void QQuickPopupPositioner::setParentItem(QQuickItem *parent)
{
    if (m_parentItem == parent)
        return;

    if (m_parentItem) {
        QQuickItemPrivate::get(m_parentItem)->removeItemChangeListener(this, ItemChangeTypes);
        removeAncestorListeners(m_parentItem->parentItem());
    }

    m_parentItem = parent;

    if (!parent)
        return;

    QQuickItemPrivate::get(parent)->addItemChangeListener(this, ItemChangeTypes);
    addAncestorListeners(parent->parentItem());

    if (m_popup->popupItem->isVisible())
        m_popup->reposition();
}

void QQuickPopupPositioner::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    if (m_parentItem && m_popup->popupItem->isVisible())
        m_popup->reposition();
}

void QQuickPopupPositioner::itemParentChanged(QQuickItem *, QQuickItem *parent)
{
    addAncestorListeners(parent);
}

void QQuickPopupPositioner::itemChildRemoved(QQuickItem *item, QQuickItem *child)
{
    if (isAncestor(child))
        removeAncestorListeners(item);
}

void QQuickPopupPrivate::itemDestroyed(QQuickItem *item)
{
    Q_Q(QQuickPopup);
    if (item == parentItem)
        q->setParentItem(nullptr);
}

void QQuickPopupPrivate::reposition()
{
    Q_Q(QQuickPopup);
    if (!popupItem->isVisible())
        return;

    if (positioning) {
        popupItem->polish();
        return;
    }

    const qreal w = popupItem->width();
    const qreal h = popupItem->height();
    const qreal iw = popupItem->implicitWidth();
    const qreal ih = popupItem->implicitHeight();

    bool widthAdjusted = false;
    bool heightAdjusted = false;

    QRectF rect(allowHorizontalMove ? x : popupItem->x(),
                allowVerticalMove ? y : popupItem->y(),
                !hasWidth && iw > 0 ? iw : w,
                !hasHeight && ih > 0 ? ih : h);
    if (parentItem) {
        rect = parentItem->mapRectToScene(rect);

        if (window) {
            const QMarginsF margins = getMargins();
            const QRectF bounds(qMax<qreal>(0.0, margins.left()),
                                qMax<qreal>(0.0, margins.top()),
                                window->width() - qMax<qreal>(0.0, margins.left()) - qMax<qreal>(0.0, margins.right()),
                                window->height() - qMax<qreal>(0.0, margins.top()) - qMax<qreal>(0.0, margins.bottom()));

            // if the popup doesn't fit horizontally inside the window, try flipping it around (left <-> right)
            if (allowHorizontalFlip && (rect.left() < bounds.left() || rect.right() > bounds.right())) {
                const QRectF flipped = parentItem->mapRectToScene(QRectF(parentItem->width() - x - rect.width(), y, rect.width(), rect.height()));
                if (flipped.intersected(bounds).width() > rect.intersected(bounds).width())
                    rect.moveLeft(flipped.left());
            }

            // if the popup doesn't fit vertically inside the window, try flipping it around (above <-> below)
            if (allowVerticalFlip && (rect.top() < bounds.top() || rect.bottom() > bounds.bottom())) {
                const QRectF flipped = parentItem->mapRectToScene(QRectF(x, parentItem->height() - y - rect.height(), rect.width(), rect.height()));
                if (flipped.intersected(bounds).height() > rect.intersected(bounds).height())
                    rect.moveTop(flipped.top());
            }

            // push inside the margins if specified
            if (allowVerticalMove) {
                if (margins.top() >= 0 && rect.top() < bounds.top())
                    rect.moveTop(margins.top());
                if (margins.bottom() >= 0 && rect.bottom() > bounds.bottom())
                    rect.moveBottom(bounds.bottom());
            }
            if (allowHorizontalMove) {
                if (margins.left() >= 0 && rect.left() < bounds.left())
                    rect.moveLeft(margins.left());
                if (margins.right() >= 0 && rect.right() > bounds.right())
                    rect.moveRight(bounds.right());
            }

            if (iw > 0 && (rect.left() < bounds.left() || rect.right() > bounds.right())) {
                // neither the flipped or pushed geometry fits inside the window, choose
                // whichever side (left vs. right) fits larger part of the popup
                if (allowHorizontalMove && allowHorizontalFlip) {
                    if (rect.left() < bounds.left() && bounds.left() + rect.width() <= bounds.right())
                        rect.moveLeft(bounds.left());
                    else if (rect.right() > bounds.right() && bounds.right() - rect.width() >= bounds.left())
                        rect.moveRight(bounds.right());
                }

                // as a last resort, adjust the width to fit the window
                if (allowHorizontalResize) {
                    if (rect.left() < bounds.left()) {
                        rect.setLeft(bounds.left());
                        widthAdjusted = true;
                    }
                    if (rect.right() > bounds.right()) {
                        rect.setRight(bounds.right());
                        widthAdjusted = true;
                    }
                }
            } else if (iw > 0 && rect.left() >= bounds.left() && rect.right() <= bounds.right()
                       && iw != w) {
                // restore original width
                rect.setWidth(iw);
                widthAdjusted = true;
            }

            if (ih > 0 && (rect.top() < bounds.top() || rect.bottom() > bounds.bottom())) {
                // neither the flipped or pushed geometry fits inside the window, choose
                // whichever side (above vs. below) fits larger part of the popup
                if (allowVerticalMove && allowVerticalFlip) {
                    if (rect.top() < bounds.top() && bounds.top() + rect.height() <= bounds.bottom())
                        rect.moveTop(bounds.top());
                    else if (rect.bottom() > bounds.bottom() && bounds.bottom() - rect.height() >= bounds.top())
                        rect.moveBottom(bounds.bottom());
                }

                // as a last resort, adjust the height to fit the window
                if (allowVerticalResize) {
                    if (rect.top() < bounds.top()) {
                        rect.setTop(bounds.top());
                        heightAdjusted = true;
                    }
                    if (rect.bottom() > bounds.bottom()) {
                        rect.setBottom(bounds.bottom());
                        heightAdjusted = true;
                    }
                }
            } else if (ih > 0 && rect.top() >= bounds.top() && rect.bottom() <= bounds.bottom()
                       && ih != h) {
                // restore original height
                rect.setHeight(ih);
                heightAdjusted = true;
            }
        }
    }

    positioning = true;

    popupItem->setPosition(rect.topLeft());

    const QPointF effectivePos = parentItem ? parentItem->mapFromScene(rect.topLeft()) : rect.topLeft();
    if (!qFuzzyCompare(effectiveX, effectivePos.x())) {
        effectiveX = effectivePos.x();
        emit q->xChanged();
    }
    if (!qFuzzyCompare(effectiveY, effectivePos.y())) {
        effectiveY = effectivePos.y();
        emit q->yChanged();
    }

    if (!hasWidth && widthAdjusted && rect.width() > 0)
        popupItem->setWidth(rect.width());
    if (!hasHeight && heightAdjusted && rect.height() > 0)
        popupItem->setHeight(rect.height());

    positioning = false;
}

void QQuickPopupPrivate::resizeOverlay()
{
    if (!dimmer)
        return;

    qreal w = window ? window->width() : 0;
    qreal h = window ? window->height() : 0;
    dimmer->setSize(QSizeF(w, h));
}

void QQuickPopupPositioner::removeAncestorListeners(QQuickItem *item)
{
    if (item == m_parentItem)
        return;

    QQuickItem *p = item;
    while (p) {
        QQuickItemPrivate::get(p)->removeItemChangeListener(this, AncestorChangeTypes);
        p = p->parentItem();
    }
}

void QQuickPopupPositioner::addAncestorListeners(QQuickItem *item)
{
    if (item == m_parentItem)
        return;

    QQuickItem *p = item;
    while (p) {
        QQuickItemPrivate::get(p)->addItemChangeListener(this, AncestorChangeTypes);
        p = p->parentItem();
    }
}

// TODO: use QQuickItem::isAncestorOf() in dev/5.7
bool QQuickPopupPositioner::isAncestor(QQuickItem *item) const
{
    if (!m_parentItem)
        return false;

    QQuickItem *parent = m_parentItem;
    while (parent) {
        if (parent == item)
            return true;
        parent = parent->parentItem();
    }
    return false;
}

QQuickPopupTransitionManager::QQuickPopupTransitionManager(QQuickPopupPrivate *popup)
    : QQuickTransitionManager(), popup(popup)
{
}

void QQuickPopupTransitionManager::transitionEnter()
{
    if (popup->transitionState == QQuickPopupPrivate::ExitTransition)
        cancel();

    if (!popup->prepareEnterTransition())
        return;

    if (popup->window)
        transition(popup->enterActions, popup->enter, popup->q_func());
    else
        finished();
}

void QQuickPopupTransitionManager::transitionExit()
{
    if (!popup->prepareExitTransition())
        return;

    if (popup->window)
        transition(popup->exitActions, popup->exit, popup->q_func());
    else
        finished();
}

void QQuickPopupTransitionManager::finished()
{
    if (popup->transitionState == QQuickPopupPrivate::EnterTransition)
        popup->finalizeEnterTransition();
    else if (popup->transitionState == QQuickPopupPrivate::ExitTransition)
        popup->finalizeExitTransition();
}

QQuickPopup::QQuickPopup(QObject *parent)
    : QObject(*(new QQuickPopupPrivate), parent)
{
    Q_D(QQuickPopup);
    d->init();
}

QQuickPopup::QQuickPopup(QQuickPopupPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
    Q_D(QQuickPopup);
    d->init();
}

QQuickPopup::~QQuickPopup()
{
    Q_D(QQuickPopup);
    setParentItem(nullptr);
    d->popupItem->ungrabShortcut();
    delete d->popupItem;
}

/*!
    \qmlmethod void QtQuick.Controls::Popup::open()

    Opens the popup.

    \sa visible
*/
void QQuickPopup::open()
{
    setVisible(true);
}

/*!
    \qmlmethod void QtQuick.Controls::Popup::close()

    Closes the popup.

    \sa visible
*/
void QQuickPopup::close()
{
    setVisible(false);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::x

    This property holds the x-coordinate of the popup.

    \sa y, z
*/
qreal QQuickPopup::x() const
{
    Q_D(const QQuickPopup);
    return d->effectiveX;
}

void QQuickPopup::setX(qreal x)
{
    Q_D(QQuickPopup);
    setPosition(QPointF(x, d->y));
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::y

    This property holds the y-coordinate of the popup.

    \sa x, z
*/
qreal QQuickPopup::y() const
{
    Q_D(const QQuickPopup);
    return d->effectiveY;
}

void QQuickPopup::setY(qreal y)
{
    Q_D(QQuickPopup);
    setPosition(QPointF(d->x, y));
}

QPointF QQuickPopup::position() const
{
    Q_D(const QQuickPopup);
    return QPointF(d->effectiveX, d->effectiveY);
}

void QQuickPopup::setPosition(const QPointF &pos)
{
    Q_D(QQuickPopup);
    const bool xChange = !qFuzzyCompare(d->x, pos.x());
    const bool yChange = !qFuzzyCompare(d->y, pos.y());
    if (!xChange && !yChange)
        return;

    d->x = pos.x();
    d->y = pos.y();
    if (d->popupItem->isVisible()) {
        d->reposition();
    } else {
        if (xChange)
            emit xChanged();
        if (yChange)
            emit yChanged();
    }
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::z

    This property holds the z-value of the popup. Z-value determines
    the stacking order of popups.

    If two visible popups have the same z-value, the last one that
    was opened will be on top.

    The default z-value is \c 0.

    \sa x, y
*/
qreal QQuickPopup::z() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->z();
}

void QQuickPopup::setZ(qreal z)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(z, d->popupItem->z()))
        return;
    d->popupItem->setZ(z);
    emit zChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::width

    This property holds the width of the popup.
*/
qreal QQuickPopup::width() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->width();
}

void QQuickPopup::setWidth(qreal width)
{
    Q_D(QQuickPopup);
    d->hasWidth = true;
    d->popupItem->setWidth(width);
}

void QQuickPopup::resetWidth()
{
    Q_D(QQuickPopup);
    if (!d->hasWidth)
        return;

    d->hasWidth = false;
    d->popupItem->resetWidth();
    if (d->popupItem->isVisible())
        d->reposition();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::height

    This property holds the height of the popup.
*/
qreal QQuickPopup::height() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->height();
}

void QQuickPopup::setHeight(qreal height)
{
    Q_D(QQuickPopup);
    d->hasHeight = true;
    d->popupItem->setHeight(height);
}

void QQuickPopup::resetHeight()
{
    Q_D(QQuickPopup);
    if (!d->hasHeight)
        return;

    d->hasHeight = false;
    d->popupItem->resetHeight();
    if (d->popupItem->isVisible())
        d->reposition();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::implicitWidth

    This property holds the implicit width of the popup.
*/
qreal QQuickPopup::implicitWidth() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->implicitWidth();
}

void QQuickPopup::setImplicitWidth(qreal width)
{
    Q_D(QQuickPopup);
    d->popupItem->setImplicitWidth(width);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::implicitHeight

    This property holds the implicit height of the popup.
*/
qreal QQuickPopup::implicitHeight() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->implicitHeight();
}

void QQuickPopup::setImplicitHeight(qreal height)
{
    Q_D(QQuickPopup);
    d->popupItem->setImplicitHeight(height);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::contentWidth

    This property holds the content width. It is used for calculating the
    total implicit width of the Popup.

    For more information, see \l {Popup Sizing}.

    \sa contentHeight
*/
qreal QQuickPopup::contentWidth() const
{
    Q_D(const QQuickPopup);
    return d->contentWidth;
}

void QQuickPopup::setContentWidth(qreal width)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(d->contentWidth, width))
        return;

    d->contentWidth = width;
    emit contentWidthChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::contentHeight

    This property holds the content height. It is used for calculating the
    total implicit height of the Popup.

    For more information, see \l {Popup Sizing}.

    \sa contentWidth
*/
qreal QQuickPopup::contentHeight() const
{
    Q_D(const QQuickPopup);
    return d->contentHeight;
}

void QQuickPopup::setContentHeight(qreal height)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(d->contentHeight, height))
        return;

    d->contentHeight = height;
    emit contentHeightChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::availableWidth
    \readonly

    This property holds the width available to the \l contentItem after
    deducting horizontal padding from the \l {Item::}{width} of the popup.

    \sa padding, leftPadding, rightPadding
*/
qreal QQuickPopup::availableWidth() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->availableWidth();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::availableHeight
    \readonly

    This property holds the height available to the \l contentItem after
    deducting vertical padding from the \l {Item::}{height} of the popup.

    \sa padding, topPadding, bottomPadding
*/
qreal QQuickPopup::availableHeight() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->availableHeight();
}

/*!
    \since QtQuick.Controls 2.1
    \qmlproperty real QtQuick.Controls::Popup::spacing

    This property holds the spacing.

    Spacing is useful for popups that have multiple or repetitive building
    blocks. For example, some styles use spacing to determine the distance
    between the header, content, and footer of \l Dialog. Spacing is not
    enforced by Popup, so each style may interpret it differently, and some
    may ignore it altogether.
*/
qreal QQuickPopup::spacing() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->spacing();
}

void QQuickPopup::setSpacing(qreal spacing)
{
    Q_D(QQuickPopup);
    d->popupItem->setSpacing(spacing);
}

void QQuickPopup::resetSpacing()
{
    setSpacing(0);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::margins

    This property holds the default margins around the popup.

    A popup with negative margins is not pushed within the bounds
    of the enclosing window. The default value is \c -1.

    \sa topMargin, leftMargin, rightMargin, bottomMargin
*/
qreal QQuickPopup::margins() const
{
    Q_D(const QQuickPopup);
    return d->margins;
}

void QQuickPopup::setMargins(qreal margins)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(d->margins, margins))
        return;
    QMarginsF oldMargins(leftMargin(), topMargin(), rightMargin(), bottomMargin());
    d->margins = margins;
    emit marginsChanged();
    QMarginsF newMargins(leftMargin(), topMargin(), rightMargin(), bottomMargin());
    if (!qFuzzyCompare(newMargins.top(), oldMargins.top()))
        emit topMarginChanged();
    if (!qFuzzyCompare(newMargins.left(), oldMargins.left()))
        emit leftMarginChanged();
    if (!qFuzzyCompare(newMargins.right(), oldMargins.right()))
        emit rightMarginChanged();
    if (!qFuzzyCompare(newMargins.bottom(), oldMargins.bottom()))
        emit bottomMarginChanged();
    marginsChange(newMargins, oldMargins);
}

void QQuickPopup::resetMargins()
{
    setMargins(-1);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::topMargin

    This property holds the top margin around the popup.

    A popup with a negative top margin is not pushed within the top edge
    of the enclosing window. The default value is \c -1.

    \sa margins, bottomMargin
*/
qreal QQuickPopup::topMargin() const
{
    Q_D(const QQuickPopup);
    if (d->hasTopMargin)
        return d->topMargin;
    return d->margins;
}

void QQuickPopup::setTopMargin(qreal margin)
{
    Q_D(QQuickPopup);
    d->setTopMargin(margin);
}

void QQuickPopup::resetTopMargin()
{
    Q_D(QQuickPopup);
    d->setTopMargin(-1, true);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::leftMargin

    This property holds the left margin around the popup.

    A popup with a negative left margin is not pushed within the left edge
    of the enclosing window. The default value is \c -1.

    \sa margins, rightMargin
*/
qreal QQuickPopup::leftMargin() const
{
    Q_D(const QQuickPopup);
    if (d->hasLeftMargin)
        return d->leftMargin;
    return d->margins;
}

void QQuickPopup::setLeftMargin(qreal margin)
{
    Q_D(QQuickPopup);
    d->setLeftMargin(margin);
}

void QQuickPopup::resetLeftMargin()
{
    Q_D(QQuickPopup);
    d->setLeftMargin(-1, true);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::rightMargin

    This property holds the right margin around the popup.

    A popup with a negative right margin is not pushed within the right edge
    of the enclosing window. The default value is \c -1.

    \sa margins, leftMargin
*/
qreal QQuickPopup::rightMargin() const
{
    Q_D(const QQuickPopup);
    if (d->hasRightMargin)
        return d->rightMargin;
    return d->margins;
}

void QQuickPopup::setRightMargin(qreal margin)
{
    Q_D(QQuickPopup);
    d->setRightMargin(margin);
}

void QQuickPopup::resetRightMargin()
{
    Q_D(QQuickPopup);
    d->setRightMargin(-1, true);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::bottomMargin

    This property holds the bottom margin around the popup.

    A popup with a negative bottom margin is not pushed within the bottom edge
    of the enclosing window. The default value is \c -1.

    \sa margins, topMargin
*/
qreal QQuickPopup::bottomMargin() const
{
    Q_D(const QQuickPopup);
    if (d->hasBottomMargin)
        return d->bottomMargin;
    return d->margins;
}

void QQuickPopup::setBottomMargin(qreal margin)
{
    Q_D(QQuickPopup);
    d->setBottomMargin(margin);
}

void QQuickPopup::resetBottomMargin()
{
    Q_D(QQuickPopup);
    d->setBottomMargin(-1, true);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::padding

    This property holds the default padding.

    \include qquickpopup-padding.qdocinc

    \sa availableWidth, availableHeight, topPadding, leftPadding, rightPadding, bottomPadding
*/
qreal QQuickPopup::padding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->padding();
}

void QQuickPopup::setPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setPadding(padding);
}

void QQuickPopup::resetPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetPadding();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::topPadding

    This property holds the top padding.

    \include qquickpopup-padding.qdocinc

    \sa padding, bottomPadding, availableHeight
*/
qreal QQuickPopup::topPadding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->topPadding();
}

void QQuickPopup::setTopPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setTopPadding(padding);
}

void QQuickPopup::resetTopPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetTopPadding();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::leftPadding

    This property holds the left padding.

    \include qquickpopup-padding.qdocinc

    \sa padding, rightPadding, availableWidth
*/
qreal QQuickPopup::leftPadding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->leftPadding();
}

void QQuickPopup::setLeftPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setLeftPadding(padding);
}

void QQuickPopup::resetLeftPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetLeftPadding();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::rightPadding

    This property holds the right padding.

    \include qquickpopup-padding.qdocinc

    \sa padding, leftPadding, availableWidth
*/
qreal QQuickPopup::rightPadding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->rightPadding();
}

void QQuickPopup::setRightPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setRightPadding(padding);
}

void QQuickPopup::resetRightPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetRightPadding();
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::bottomPadding

    This property holds the bottom padding.

    \include qquickpopup-padding.qdocinc

    \sa padding, topPadding, availableHeight
*/
qreal QQuickPopup::bottomPadding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->bottomPadding();
}

void QQuickPopup::setBottomPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setBottomPadding(padding);
}

void QQuickPopup::resetBottomPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetBottomPadding();
}

/*!
    \qmlproperty Locale QtQuick.Controls::Popup::locale

    This property holds the locale of the popup.

    \sa {LayoutMirroring}{LayoutMirroring}
*/
QLocale QQuickPopup::locale() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->locale();
}

void QQuickPopup::setLocale(const QLocale &locale)
{
    Q_D(QQuickPopup);
    d->popupItem->setLocale(locale);
}

void QQuickPopup::resetLocale()
{
    Q_D(QQuickPopup);
    d->popupItem->resetLocale();
}

/*!
    \qmlproperty font QtQuick.Controls::Popup::font

    This property holds the font currently set for the popup.

    Popup propagates explicit font properties to its children. If you change a specific
    property on a popup's font, that property propagates to all of the popup's children,
    overriding any system defaults for that property.

    \code
    Popup {
        font.family: "Courier"

        Column {
            Label {
                text: qsTr("This will use Courier...")
            }

            Switch {
                text: qsTr("... and so will this")
            }
        }
    }
    \endcode

    \sa Control::font, ApplicationWindow::font
*/
QFont QQuickPopup::font() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->font();
}

void QQuickPopup::setFont(const QFont &font)
{
    Q_D(QQuickPopup);
    d->popupItem->setFont(font);
}

void QQuickPopup::resetFont()
{
    Q_D(QQuickPopup);
    d->popupItem->resetFont();
}

QQuickWindow *QQuickPopup::window() const
{
    Q_D(const QQuickPopup);
    return d->window;
}

QQuickItem *QQuickPopup::popupItem() const
{
    Q_D(const QQuickPopup);
    return d->popupItem;
}

/*!
    \qmlproperty Item QtQuick.Controls::Popup::parent

    This property holds the parent item.
*/
QQuickItem *QQuickPopup::parentItem() const
{
    Q_D(const QQuickPopup);
    return d->parentItem;
}

void QQuickPopup::setParentItem(QQuickItem *parent)
{
    Q_D(QQuickPopup);
    if (d->parentItem == parent)
        return;

    if (d->parentItem) {
        QObjectPrivate::disconnect(d->parentItem, &QQuickItem::windowChanged, d, &QQuickPopupPrivate::setWindow);
        QQuickItemPrivate::get(d->parentItem)->removeItemChangeListener(d, QQuickItemPrivate::Destroyed);
    }
    d->parentItem = parent;
    if (d->positioner.parentItem())
        d->positioner.setParentItem(parent);
    if (parent) {
        QObjectPrivate::connect(parent, &QQuickItem::windowChanged, d, &QQuickPopupPrivate::setWindow);
        QQuickItemPrivate::get(d->parentItem)->addItemChangeListener(d, QQuickItemPrivate::Destroyed);

        QQuickControlPrivate *p = QQuickControlPrivate::get(d->popupItem);
        p->resolveFont();
        if (QQuickApplicationWindow *window = qobject_cast<QQuickApplicationWindow *>(parent->window()))
            p->updateLocale(window->locale(), false); // explicit=false
    } else {
        close();
    }
    d->setWindow(parent ? parent->window() : nullptr);
    emit parentChanged();
}

/*!
    \qmlproperty Item QtQuick.Controls::Popup::background

    This property holds the background item.

    \note If the background item has no explicit size specified, it automatically
          follows the popup's size. In most cases, there is no need to specify
          width or height for a background item.

    \note Most popups use the implicit size of the background item to calculate
    the implicit size of the popup itself. If you replace the background item
    with a custom one, you should also consider providing a sensible implicit
    size for it (unless it is an item like \l Image which has its own implicit
    size).

    \sa {Customizing Popup}
*/
QQuickItem *QQuickPopup::background() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->background();
}

void QQuickPopup::setBackground(QQuickItem *background)
{
    Q_D(QQuickPopup);
    if (d->popupItem->background() == background)
        return;

    d->popupItem->setBackground(background);
    emit backgroundChanged();
}

/*!
    \qmlproperty Item QtQuick.Controls::Popup::contentItem

    This property holds the content item of the popup.

    The content item is the visual implementation of the popup. When the
    popup is made visible, the content item is automatically reparented to
    the \l {ApplicationWindow::overlay}{overlay item} of its application
    window.

    \note The content item is automatically resized to fit within the
    \l padding of the popup.

    \note Most popups use the implicit size of the content item to calculate
    the implicit size of the popup itself. If you replace the content item
    with a custom one, you should also consider providing a sensible implicit
    size for it (unless it is an item like \l Text which has its own implicit
    size).

    \sa {Customizing Popup}
*/
QQuickItem *QQuickPopup::contentItem() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->contentItem();
}

void QQuickPopup::setContentItem(QQuickItem *item)
{
    Q_D(QQuickPopup);
    d->popupItem->setContentItem(item);
}

/*!
    \qmlproperty list<Object> QtQuick.Controls::Popup::contentData
    \default

    This property holds the list of content data.

    The list contains all objects that have been declared in QML as children
    of the popup.

    \note Unlike \c contentChildren, \c contentData does include non-visual QML
    objects.

    \sa Item::data, contentChildren
*/
QQmlListProperty<QObject> QQuickPopup::contentData()
{
    Q_D(QQuickPopup);
    return QQmlListProperty<QObject>(d->popupItem->contentItem(), nullptr,
                                     QQuickItemPrivate::data_append,
                                     QQuickItemPrivate::data_count,
                                     QQuickItemPrivate::data_at,
                                     QQuickItemPrivate::data_clear);
}

/*!
    \qmlproperty list<Item> QtQuick.Controls::Popup::contentChildren

    This property holds the list of content children.

    The list contains all items that have been declared in QML as children
    of the popup.

    \note Unlike \c contentData, \c contentChildren does not include non-visual
    QML objects.

    \sa Item::children, contentData
*/
QQmlListProperty<QQuickItem> QQuickPopup::contentChildren()
{
    Q_D(QQuickPopup);
    return QQmlListProperty<QQuickItem>(d->popupItem->contentItem(), nullptr,
                                        QQuickItemPrivate::children_append,
                                        QQuickItemPrivate::children_count,
                                        QQuickItemPrivate::children_at,
                                        QQuickItemPrivate::children_clear);
}

/*!
    \qmlproperty bool QtQuick.Controls::Popup::clip

    This property holds whether clipping is enabled. The default value is \c false.
*/
bool QQuickPopup::clip() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->clip();
}

void QQuickPopup::setClip(bool clip)
{
    Q_D(QQuickPopup);
    if (clip == d->popupItem->clip())
        return;
    d->popupItem->setClip(clip);
    emit clipChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::Popup::focus

    This property holds whether the popup wants focus.

    When the popup actually receives focus, \l activeFocus will be \c true.
    For more information, see \l {Keyboard Focus in Qt Quick}.

    The default value is \c false.

    \sa activeFocus
*/
bool QQuickPopup::hasFocus() const
{
    Q_D(const QQuickPopup);
    return d->focus;
}

void QQuickPopup::setFocus(bool focus)
{
    Q_D(QQuickPopup);
    if (d->focus == focus)
        return;
    d->focus = focus;
    emit focusChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::Popup::activeFocus
    \readonly

    This property holds whether the popup has active focus.

    \sa focus, {Keyboard Focus in Qt Quick}
*/
bool QQuickPopup::hasActiveFocus() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->hasActiveFocus();
}

/*!
    \qmlproperty bool QtQuick.Controls::Popup::modal

    This property holds whether the popup is modal. The default value is \c false.
*/
bool QQuickPopup::isModal() const
{
    Q_D(const QQuickPopup);
    return d->modal;
}

void QQuickPopup::setModal(bool modal)
{
    Q_D(QQuickPopup);
    if (d->modal == modal)
        return;
    d->modal = modal;
    emit modalChanged();

    if (!d->hasDim) {
        setDim(modal);
        d->hasDim = false;
    }
}

/*!
    \qmlproperty bool QtQuick.Controls::Popup::dim

    This property holds whether the popup dims the background.

    Unless explicitly set, this property follows the value of \l modal. To
    return to the default value, set this property to \c undefined.

    \sa modal
*/
bool QQuickPopup::dim() const
{
    Q_D(const QQuickPopup);
    return d->dim;
}

void QQuickPopup::setDim(bool dim)
{
    Q_D(QQuickPopup);
    d->hasDim = true;

    if (d->dim == dim)
        return;

    d->dim = dim;
    emit dimChanged();
}

void QQuickPopup::resetDim()
{
    Q_D(QQuickPopup);
    if (!d->hasDim)
        return;

    setDim(d->modal);
    d->hasDim = false;
}

/*!
    \qmlproperty bool QtQuick.Controls::Popup::visible

    This property holds whether the popup is visible. The default value is \c false.

    \sa open(), close()
*/
bool QQuickPopup::isVisible() const
{
    Q_D(const QQuickPopup);
    return d->visible && d->popupItem->isVisible();
}

void QQuickPopup::setVisible(bool visible)
{
    Q_D(QQuickPopup);
    if (d->visible == visible && d->transitionState != QQuickPopupPrivate::ExitTransition)
        return;

    if (d->complete) {
        if (visible)
            d->transitionManager.transitionEnter();
        else
            d->transitionManager.transitionExit();
    } else {
        d->visible = visible;
    }
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::opacity

    This property holds the opacity of the popup. Opacity is specified as a number between
    \c 0.0 (fully transparent) and \c 1.0 (fully opaque). The default value is \c 1.0.

    \sa visible
*/
qreal QQuickPopup::opacity() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->opacity();
}

void QQuickPopup::setOpacity(qreal opacity)
{
    Q_D(QQuickPopup);
    d->popupItem->setOpacity(opacity);
}

/*!
    \qmlproperty real QtQuick.Controls::Popup::scale

    This property holds the scale factor of the popup. The default value is \c 1.0.

    A scale of less than \c 1.0 causes the popup to be rendered at a smaller size,
    and a scale greater than \c 1.0 renders the popup at a larger size. A negative
    scale causes the popup to be mirrored when rendered.
*/
qreal QQuickPopup::scale() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->scale();
}

void QQuickPopup::setScale(qreal scale)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(scale, d->popupItem->scale()))
        return;
    d->popupItem->setScale(scale);
    emit scaleChanged();
}

/*!
    \qmlproperty enumeration QtQuick.Controls::Popup::closePolicy

    This property determines the circumstances under which the popup closes.
    The flags can be combined to allow several ways of closing the popup.

    The available values are:
    \value Popup.NoAutoClose The popup will only close when manually instructed to do so.
    \value Popup.CloseOnPressOutside The popup will close when the mouse is pressed outside of it.
    \value Popup.CloseOnPressOutsideParent The popup will close when the mouse is pressed outside of its parent.
    \value Popup.CloseOnReleaseOutside The popup will close when the mouse is released outside of it.
    \value Popup.CloseOnReleaseOutsideParent The popup will close when the mouse is released outside of its parent.
    \value Popup.CloseOnEscape The popup will close when the escape key is pressed while the popup
        has active focus.

    The default value is \c {Popup.CloseOnEscape | Popup.CloseOnPressOutside}.

    \note There is a known limitation that the \c Popup.CloseOnReleaseOutside
        and \c Popup.CloseOnReleaseOutsideParent policies only work with
        \l modal popups.
*/
QQuickPopup::ClosePolicy QQuickPopup::closePolicy() const
{
    Q_D(const QQuickPopup);
    return d->closePolicy;
}

void QQuickPopup::setClosePolicy(ClosePolicy policy)
{
    Q_D(QQuickPopup);
    if (d->closePolicy == policy)
        return;
    d->closePolicy = policy;
    if (isVisible()) {
        if (policy & QQuickPopup::CloseOnEscape)
            d->popupItem->grabShortcut();
        else
            d->popupItem->ungrabShortcut();
    }
    emit closePolicyChanged();
}

/*!
    \qmlproperty enumeration QtQuick.Controls::Popup::transformOrigin

    This property holds the origin point for transformations in enter and exit transitions.

    Nine transform origins are available, as shown in the image below.
    The default transform origin is \c Popup.Center.

    \image qtquickcontrols2-popup-transformorigin.png

    \sa enter, exit, Item::transformOrigin
*/
QQuickPopup::TransformOrigin QQuickPopup::transformOrigin() const
{
    Q_D(const QQuickPopup);
    return static_cast<TransformOrigin>(d->popupItem->transformOrigin());
}

void QQuickPopup::setTransformOrigin(TransformOrigin origin)
{
    Q_D(QQuickPopup);
    d->popupItem->setTransformOrigin(static_cast<QQuickItem::TransformOrigin>(origin));
}

/*!
    \qmlproperty Transition QtQuick.Controls::Popup::enter

    This property holds the transition that is applied to the popup item
    when the popup is opened and enters the screen.

    The following example animates the opacity of the popup when it enters
    the screen:
    \code
    Popup {
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0 }
        }
    }
    \endcode

    \sa exit
*/
QQuickTransition *QQuickPopup::enter() const
{
    Q_D(const QQuickPopup);
    return d->enter;
}

void QQuickPopup::setEnter(QQuickTransition *transition)
{
    Q_D(QQuickPopup);
    if (d->enter == transition)
        return;
    d->enter = transition;
    emit enterChanged();
}

/*!
    \qmlproperty Transition QtQuick.Controls::Popup::exit

    This property holds the transition that is applied to the popup item
    when the popup is closed and exits the screen.

    The following example animates the opacity of the popup when it exits
    the screen:
    \code
    Popup {
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0 }
        }
    }
    \endcode

    \sa enter
*/
QQuickTransition *QQuickPopup::exit() const
{
    Q_D(const QQuickPopup);
    return d->exit;
}

void QQuickPopup::setExit(QQuickTransition *transition)
{
    Q_D(QQuickPopup);
    if (d->exit == transition)
        return;
    d->exit = transition;
    emit exitChanged();
}

bool QQuickPopup::filtersChildMouseEvents() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->filtersChildMouseEvents();
}

void QQuickPopup::setFiltersChildMouseEvents(bool filter)
{
    Q_D(QQuickPopup);
    d->popupItem->setFiltersChildMouseEvents(filter);
}

/*!
    \qmlmethod QtQuick.Controls::Popup::forceActiveFocus(reason = Qt.OtherFocusReason)

    Forces active focus on the popup with the given \a reason.

    This method sets focus on the popup and ensures that all ancestor
    \l FocusScope objects in the object hierarchy are also given \l focus.

    \sa activeFocus, Qt::FocusReason
*/
void QQuickPopup::forceActiveFocus(Qt::FocusReason reason)
{
    Q_D(QQuickPopup);
    d->popupItem->forceActiveFocus(reason);
}

void QQuickPopup::classBegin()
{
    Q_D(QQuickPopup);
    d->popupItem->classBegin();
}

void QQuickPopup::componentComplete()
{
    Q_D(QQuickPopup);
    d->complete = true;
    if (!parentItem())
        setParentItem(qobject_cast<QQuickItem *>(parent()));
    if (d->visible)
        d->transitionManager.transitionEnter();
    d->popupItem->componentComplete();
}

bool QQuickPopup::isComponentComplete() const
{
    Q_D(const QQuickPopup);
    return d->complete;
}

bool QQuickPopup::childMouseEventFilter(QQuickItem *child, QEvent *event)
{
    Q_UNUSED(child);
    Q_UNUSED(event);
    return false;
}

void QQuickPopup::focusInEvent(QFocusEvent *event)
{
    event->accept();
}

void QQuickPopup::focusOutEvent(QFocusEvent *event)
{
    event->accept();
}

void QQuickPopup::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickPopup);
    event->accept();

    if (hasActiveFocus() && (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab))
        QQuickItemPrivate::focusNextPrev(d->popupItem, event->key() == Qt::Key_Tab);
}

void QQuickPopup::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();
}

void QQuickPopup::mousePressEvent(QMouseEvent *event)
{
    event->accept();
}

void QQuickPopup::mouseMoveEvent(QMouseEvent *event)
{
    event->accept();
}

void QQuickPopup::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
}

void QQuickPopup::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();
}

void QQuickPopup::mouseUngrabEvent()
{
    QQuickOverlay *overlay = QQuickOverlay::overlay(window());
    if (overlay) {
        QQuickOverlayPrivate *p = QQuickOverlayPrivate::get(overlay);
        if (p->mouseGrabberPopup == this)
            p->mouseGrabberPopup = nullptr;
    }
}

bool QQuickPopup::overlayEvent(QQuickItem *item, QEvent *event)
{
    Q_D(QQuickPopup);
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::MouseMove:
    case QEvent::Wheel:
        if (d->modal)
            event->accept();
        return d->modal;

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
        if (d->modal)
            event->accept();
        d->tryClose(item, static_cast<QMouseEvent *>(event));
        return d->modal;

    default:
        return false;
    }
}

void QQuickPopup::wheelEvent(QWheelEvent *event)
{
    event->accept();
}

void QQuickPopup::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_UNUSED(newItem);
    Q_UNUSED(oldItem);
    emit contentItemChanged();
}

void QQuickPopup::fontChange(const QFont &newFont, const QFont &oldFont)
{
    Q_UNUSED(newFont);
    Q_UNUSED(oldFont);
    emit fontChanged();
}

void QQuickPopup::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickPopup);
    d->reposition();
    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width())) {
        emit widthChanged();
        emit availableWidthChanged();
    }
    if (!qFuzzyCompare(newGeometry.height(), oldGeometry.height())) {
        emit heightChanged();
        emit availableHeightChanged();
    }
}

void QQuickPopup::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    Q_D(QQuickPopup);

    switch (change) {
    case QQuickItem::ItemActiveFocusHasChanged:
        emit activeFocusChanged();
        break;
    case QQuickItem::ItemOpacityHasChanged:
        emit opacityChanged();
        break;
    case QQuickItem::ItemVisibleHasChanged:
        if (isComponentComplete() && d->closePolicy & CloseOnEscape) {
            if (data.boolValue)
                d->popupItem->grabShortcut();
            else
                d->popupItem->ungrabShortcut();
        }
    default:
        break;
    }
}

void QQuickPopup::localeChange(const QLocale &newLocale, const QLocale &oldLocale)
{
    Q_UNUSED(newLocale);
    Q_UNUSED(oldLocale);
    emit localeChanged();
}

void QQuickPopup::marginsChange(const QMarginsF &newMargins, const QMarginsF &oldMargins)
{
    Q_D(QQuickPopup);
    Q_UNUSED(newMargins);
    Q_UNUSED(oldMargins);
    d->reposition();
}

void QQuickPopup::paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding)
{
    const bool tp = !qFuzzyCompare(newPadding.top(), oldPadding.top());
    const bool lp = !qFuzzyCompare(newPadding.left(), oldPadding.left());
    const bool rp = !qFuzzyCompare(newPadding.right(), oldPadding.right());
    const bool bp = !qFuzzyCompare(newPadding.bottom(), oldPadding.bottom());

    if (tp)
        emit topPaddingChanged();
    if (lp)
        emit leftPaddingChanged();
    if (rp)
        emit rightPaddingChanged();
    if (bp)
        emit bottomPaddingChanged();

    if (lp || rp)
        emit availableWidthChanged();
    if (tp || bp)
        emit availableHeightChanged();
}

void QQuickPopup::spacingChange(qreal newSpacing, qreal oldSpacing)
{
    Q_UNUSED(newSpacing);
    Q_UNUSED(oldSpacing);
    emit spacingChanged();
}

QFont QQuickPopup::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::SystemFont);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickPopup::accessibleRole() const
{
    return QAccessible::Dialog;
}

void QQuickPopup::accessibilityActiveChanged(bool active)
{
    Q_UNUSED(active);
}
#endif // QT_NO_ACCESSIBILITY

QString QQuickPopup::accessibleName() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->accessibleName();
}

void QQuickPopup::setAccessibleName(const QString &name)
{
    Q_D(QQuickPopup);
    d->popupItem->setAccessibleName(name);
}

QVariant QQuickPopup::accessibleProperty(const char *propertyName)
{
    Q_D(const QQuickPopup);
    return d->popupItem->accessibleProperty(propertyName);
}

bool QQuickPopup::setAccessibleProperty(const char *propertyName, const QVariant &value)
{
    Q_D(QQuickPopup);
    return d->popupItem->setAccessibleProperty(propertyName, value);
}

QT_END_NAMESPACE

#include "moc_qquickpopup_p.cpp"
