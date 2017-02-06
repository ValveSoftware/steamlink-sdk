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

#include "qquickaccessibleattached_p.h"

#ifndef QT_NO_ACCESSIBILITY

#include "private/qquickitem_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype Accessible
    \instantiates QQuickAccessibleAttached
    \brief Enables accessibility of QML items

    \inqmlmodule QtQuick
    \ingroup qtquick-visual-utility
    \ingroup accessibility

    This class is part of the \l {Accessibility for Qt Quick Applications}.

    Items the user interacts with or that give information to the user
    need to expose their information to the accessibility framework.
    Then assistive tools can make use of that information to enable
    users to interact with the application in various ways.
    This enables Qt Quick applications to be used with screen-readers for example.

    The most important properties are \l name, \l description and \l role.

    Example implementation of a simple button:
    \qml
    Rectangle {
        id: myButton
        Text {
            id: label
            text: "next"
        }
        Accessible.role: Accessible.Button
        Accessible.name: label.text
        Accessible.description: "shows the next page"
        Accessible.onPressAction: {
            // do a button click
        }
    }
    \endqml
    The \l role is set to \c Button to indicate the type of control.
    \l {Accessible::}{name} is the most important information and bound to the text on the button.
    The name is a short and consise description of the control and should reflect the visual label.
    In this case it is not clear what the button does with the name only, so \l description contains
    an explanation.
    There is also a signal handler \l {Accessible::pressAction}{Accessible.pressAction} which can be invoked by assistive tools to trigger
    the button. This signal handler needs to have the same effect as tapping or clicking the button would have.

    \sa Accessibility
*/

/*!
    \qmlproperty string QtQuick::Accessible::name

    This property sets an accessible name.
    For a button for example, this should have a binding to its text.
    In general this property should be set to a simple and concise
    but human readable name. Do not include the type of control
    you want to represent but just the name.
*/

/*!
    \qmlproperty string QtQuick::Accessible::description

    This property sets an accessible description.
    Similar to the name it describes the item. The description
    can be a little more verbose and tell what the item does,
    for example the functionallity of the button it describes.
*/

/*!
    \qmlproperty enumeration QtQuick::Accessible::role

    This flags sets the semantic type of the widget.
    A button for example would have "Button" as type.
    The value must be one of \l QAccessible::Role.

    Some roles have special semantics.
    In order to implement check boxes for example a "checked" property is expected.

    \table
    \header
        \li \b {Role}
        \li \b {Properties and signals}
        \li \b {Explanation}
    \row
        \li All interactive elements
        \li \l focusable and \l focused
        \li All elements that the user can interact with should have focusable set to \c true and
            set \c focus to \c true when they have the focus. This is important even for applications
            that run on touch-only devices since screen readers often implement a virtual focus that
            can be moved from item to item.
    \row
        \li Button, CheckBox, RadioButton
        \li \l {Accessible::pressAction}{Accessible.pressAction}
        \li A button should have a signal handler with the name \c onPressAction.
            This signal may be emitted by an assistive tool such as a screen-reader.
            The implementation needs to behave the same as a mouse click or tap on the button.
    \row
       \li CheckBox, RadioButton
       \li \l checkable, \l checked, \l {Accessible::toggleAction}{Accessible.toggleAction}

       \li The check state of the check box. Updated on Press, Check and Uncheck actions.
    \row
       \li Slider, SpinBox, Dial, ScrollBar
       \li \c value, \c minimumValue, \c maximumValue, \c stepSize
       \li These properties reflect the state and possible values for the elements.
    \row
       \li Slider, SpinBox, Dial, ScrollBar
       \li \l {Accessible::increaseAction}{Accessible.increaseAction}, \l {Accessible::decreaseAction}{Accessible.decreaseAction}
       \li Actions to increase and decrease the value of the element.
    \endtable
*/

/*! \qmlproperty bool QtQuick::Accessible::focusable
    \brief This property holds whether this item is focusable.

    By default, this property is \c false except for items where the role is one of
    \c CheckBox, \c RadioButton, \c Button, \c MenuItem, \c PageTab, \c EditableText, \c SpinBox, \c ComboBox,
    \c Terminal or \c ScrollBar.
    \sa focused
*/
/*! \qmlproperty bool QtQuick::Accessible::focused
    \brief This property holds whether this item currently has the active focus.

    By default, this property is \c false, but it will return \c true for items that
    have \l QQuickItem::hasActiveFocus() returning \c true.
    \sa focusable
*/
/*! \qmlproperty bool QtQuick::Accessible::checkable
    \brief This property holds whether this item is checkable (like a check box or some buttons).

    By default this property is \c false.
    \sa checked
*/
/*! \qmlproperty bool QtQuick::Accessible::checked
    \brief This property holds whether this item is currently checked.

    By default this property is \c false.
    \sa checkable
*/
/*! \qmlproperty bool QtQuick::Accessible::editable
    \brief This property holds whether this item has editable text.

    By default this property is \c false.
*/
/*! \qmlproperty bool QtQuick::Accessible::searchEdit
    \brief This property holds whether this item is input for a search query.
    This property will only affect editable text.

    By default this property is \c false.
*/
/*! \qmlproperty bool QtQuick::Accessible::ignored
    \brief This property holds whether this item should be ignored by the accessibility framework.

    Sometimes an item is part of a group of items that should be treated as one. For example two labels might be
    visually placed next to each other, but separate items. For accessibility purposes they should be treated as one
    and thus they are represented by a third invisible item with the right geometry.

    For example a speed display adds "m/s" as a smaller label:
    \qml
    Row {
        Label {
            id: speedLabel
            text: "Speed: 5"
            Accessible.ignored: true
        }
        Label {
            text: qsTr("m/s")
            Accessible.ignored: true
        }
        Accessible.role: Accessible.StaticText
        Accessible.name: speedLabel.text + " meters per second"
    }
    \endqml

    \since 5.4
    By default this property is \c false.
*/
/*! \qmlproperty bool QtQuick::Accessible::multiLine
    \brief This property holds whether this item has multiple text lines.

    By default this property is \c false.
*/
/*! \qmlproperty bool QtQuick::Accessible::readOnly
    \brief This property indicates that a text field is read only.

    It is relevant when the role is \l QAccessible::EditableText and set to be read-only.
    By default this property is \c false.
*/
/*! \qmlproperty bool QtQuick::Accessible::selected
    \brief This property holds whether this item is selected.

    By default this property is \c false.
    \sa selectable
*/
/*! \qmlproperty bool QtQuick::Accessible::selectable
    \brief This property holds whether this item can be selected.

    By default this property is \c false.
    \sa selected
*/
/*! \qmlproperty bool QtQuick::Accessible::pressed
    \brief This property holds whether this item is pressed (for example a button during a mouse click).

    By default this property is \c false.
*/
/*! \qmlproperty bool QtQuick::Accessible::checkStateMixed
    \brief This property holds whether this item is in the partially checked state.

    By default this property is \c false.
    \sa checked, checkable
*/
/*! \qmlproperty bool QtQuick::Accessible::defaultButton
    \brief This property holds whether this item is the default button of a dialog.

    By default this property is \c false.
*/
/*! \qmlproperty bool QtQuick::Accessible::passwordEdit
    \brief This property holds whether this item is a password text edit.

    By default this property is \c false.
*/
/*! \qmlproperty bool QtQuick::Accessible::selectableText
    \brief This property holds whether this item contains selectable text.

    By default this property is \c false.
*/

/*!
    \qmlsignal QtQuick::Accessible::pressAction()

    This signal is emitted when a press action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onPressAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::toggleAction()

    This signal is emitted when a toggle action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onToggleAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::increaseAction()

    This signal is emitted when a increase action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onIncreaseAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::decreaseAction()

    This signal is emitted when a decrease action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onDecreaseAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::scrollUpAction()

    This signal is emitted when a scroll up action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onScrollUpAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::scrollDownAction()

    This signal is emitted when a scroll down action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onScrollDownAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::scrollLeftAction()

    This signal is emitted when a scroll left action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onScrollLeftAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::scrollRightAction()

    This signal is emitted when a scroll right action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onScrollRightAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::previousPageAction()

    This signal is emitted when a previous page action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onPreviousPageAction.
*/
/*!
    \qmlsignal QtQuick::Accessible::nextPageAction()

    This signal is emitted when a next page action is received from an assistive tool such as a screen-reader.

    The corresponding handler is \c onNextPageAction.
*/

QMetaMethod QQuickAccessibleAttached::sigPress;
QMetaMethod QQuickAccessibleAttached::sigToggle;
QMetaMethod QQuickAccessibleAttached::sigIncrease;
QMetaMethod QQuickAccessibleAttached::sigDecrease;
QMetaMethod QQuickAccessibleAttached::sigScrollUp;
QMetaMethod QQuickAccessibleAttached::sigScrollDown;
QMetaMethod QQuickAccessibleAttached::sigScrollLeft;
QMetaMethod QQuickAccessibleAttached::sigScrollRight;
QMetaMethod QQuickAccessibleAttached::sigPreviousPage;
QMetaMethod QQuickAccessibleAttached::sigNextPage;

QQuickAccessibleAttached::QQuickAccessibleAttached(QObject *parent)
    : QObject(parent), m_role(QAccessible::NoRole)
{
    Q_ASSERT(parent);
    QQuickItem *item = qobject_cast<QQuickItem*>(parent);
    if (!item)
        return;

    // Enable accessibility for items with accessible content. This also
    // enables accessibility for the ancestors of souch items.
    item->d_func()->setAccessible();
    QAccessibleEvent ev(item, QAccessible::ObjectCreated);
    QAccessible::updateAccessibility(&ev);

    if (!parent->property("value").isNull()) {
        connect(parent, SIGNAL(valueChanged()), this, SLOT(valueChanged()));
    }
    if (!parent->property("cursorPosition").isNull()) {
        connect(parent, SIGNAL(cursorPositionChanged()), this, SLOT(cursorPositionChanged()));
    }

    if (!sigPress.isValid()) {
        sigPress = QMetaMethod::fromSignal(&QQuickAccessibleAttached::pressAction);
        sigToggle = QMetaMethod::fromSignal(&QQuickAccessibleAttached::toggleAction);
        sigIncrease = QMetaMethod::fromSignal(&QQuickAccessibleAttached::increaseAction);
        sigDecrease = QMetaMethod::fromSignal(&QQuickAccessibleAttached::decreaseAction);
        sigScrollUp = QMetaMethod::fromSignal(&QQuickAccessibleAttached::scrollUpAction);
        sigScrollDown = QMetaMethod::fromSignal(&QQuickAccessibleAttached::scrollDownAction);
        sigScrollLeft = QMetaMethod::fromSignal(&QQuickAccessibleAttached::scrollLeftAction);
        sigScrollRight = QMetaMethod::fromSignal(&QQuickAccessibleAttached::scrollRightAction);
        sigPreviousPage = QMetaMethod::fromSignal(&QQuickAccessibleAttached::previousPageAction);
        sigNextPage= QMetaMethod::fromSignal(&QQuickAccessibleAttached::nextPageAction);
    }
}

QQuickAccessibleAttached::~QQuickAccessibleAttached()
{
}

QQuickAccessibleAttached *QQuickAccessibleAttached::qmlAttachedProperties(QObject *obj)
{
    return new QQuickAccessibleAttached(obj);
}

bool QQuickAccessibleAttached::ignored() const
{
    return !item()->d_func()->isAccessible;
}

void QQuickAccessibleAttached::setIgnored(bool ignored)
{
    if (this->ignored() != ignored) {
        item()->d_func()->isAccessible = !ignored;
        emit ignoredChanged();
    }
}

bool QQuickAccessibleAttached::doAction(const QString &actionName)
{
    QMetaMethod *sig = 0;
    if (actionName == QAccessibleActionInterface::pressAction())
        sig = &sigPress;
    else if (actionName == QAccessibleActionInterface::toggleAction())
        sig = &sigToggle;
    else if (actionName == QAccessibleActionInterface::increaseAction())
        sig = &sigIncrease;
    else if (actionName == QAccessibleActionInterface::decreaseAction())
        sig = &sigDecrease;
    else if (actionName == QAccessibleActionInterface::scrollUpAction())
        sig = &sigScrollUp;
    else if (actionName == QAccessibleActionInterface::scrollDownAction())
        sig = &sigScrollDown;
    else if (actionName == QAccessibleActionInterface::scrollLeftAction())
        sig = &sigScrollLeft;
    else if (actionName == QAccessibleActionInterface::scrollRightAction())
        sig = &sigScrollRight;
    else if (actionName == QAccessibleActionInterface::previousPageAction())
        sig = &sigPreviousPage;
    else if (actionName == QAccessibleActionInterface::nextPageAction())
        sig = &sigNextPage;
    if (sig && isSignalConnected(*sig))
        return sig->invoke(this);
    return false;
}

void QQuickAccessibleAttached::availableActions(QStringList *actions) const
{
    if (isSignalConnected(sigPress))
        actions->append(QAccessibleActionInterface::pressAction());
    if (isSignalConnected(sigToggle))
        actions->append(QAccessibleActionInterface::toggleAction());
    if (isSignalConnected(sigIncrease))
        actions->append(QAccessibleActionInterface::increaseAction());
    if (isSignalConnected(sigDecrease))
        actions->append(QAccessibleActionInterface::decreaseAction());
    if (isSignalConnected(sigScrollUp))
        actions->append(QAccessibleActionInterface::scrollUpAction());
    if (isSignalConnected(sigScrollDown))
        actions->append(QAccessibleActionInterface::scrollDownAction());
    if (isSignalConnected(sigScrollLeft))
        actions->append(QAccessibleActionInterface::scrollLeftAction());
    if (isSignalConnected(sigScrollRight))
        actions->append(QAccessibleActionInterface::scrollRightAction());
    if (isSignalConnected(sigPreviousPage))
        actions->append(QAccessibleActionInterface::previousPageAction());
    if (isSignalConnected(sigNextPage))
        actions->append(QAccessibleActionInterface::nextPageAction());
}

QT_END_NAMESPACE

#endif
