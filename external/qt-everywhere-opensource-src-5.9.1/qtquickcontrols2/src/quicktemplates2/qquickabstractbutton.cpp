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

#include "qquickabstractbutton_p.h"
#include "qquickabstractbutton_p_p.h"
#include "qquickbuttongroup_p.h"

#include <QtGui/qstylehints.h>
#include <QtGui/qguiapplication.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QtQml/qqmllist.h>

QT_BEGIN_NAMESPACE

// copied from qabstractbutton.cpp
static const int AUTO_REPEAT_DELAY = 300;
static const int AUTO_REPEAT_INTERVAL = 100;

/*!
    \qmltype AbstractButton
    \inherits Control
    \instantiates QQuickAbstractButton
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-buttons
    \brief Abstract base type providing functionality common to buttons.

    AbstractButton provides the interface for controls with button-like
    behavior; for example, push buttons and checkable controls like
    radio buttons and check boxes. As an abstract control, it has no delegate
    implementations, leaving them to the types that derive from it.

    \sa ButtonGroup, {Button Controls}
*/

/*!
    \qmlsignal QtQuick.Controls::AbstractButton::pressed()

    This signal is emitted when the button is interactively pressed by the user.
*/

/*!
    \qmlsignal QtQuick.Controls::AbstractButton::released()

    This signal is emitted when the button is interactively released by the user.
*/

/*!
    \qmlsignal QtQuick.Controls::AbstractButton::canceled()

    This signal is emitted when the button loses mouse grab
    while being pressed, or when it would emit the \l released
    signal but the mouse cursor is not inside the button.
*/

/*!
    \qmlsignal QtQuick.Controls::AbstractButton::clicked()

    This signal is emitted when the button is interactively clicked by the user.
*/

/*!
    \since QtQuick.Controls 2.2 (Qt 5.9)
    \qmlsignal QtQuick.Controls::AbstractButton::toggled()

    This signal is emitted when a checkable button is interactively toggled by the user.
*/

/*!
    \qmlsignal QtQuick.Controls::AbstractButton::pressAndHold()

    This signal is emitted when the button is interactively pressed and held down by the user.
*/

/*!
    \qmlsignal QtQuick.Controls::AbstractButton::doubleClicked()

    This signal is emitted when the button is interactively double clicked by the user.
*/

QQuickAbstractButtonPrivate::QQuickAbstractButtonPrivate()
    : down(false),
      explicitDown(false),
      pressed(false),
      keepPressed(false),
      checked(false),
      checkable(false),
      autoExclusive(false),
      autoRepeat(false),
      wasHeld(false),
      holdTimer(0),
      delayTimer(0),
      repeatTimer(0),
      pressButtons(Qt::NoButton),
      indicator(nullptr),
      group(nullptr)
{
}

void QQuickAbstractButtonPrivate::handlePress(const QPointF &point)
{
    Q_Q(QQuickAbstractButton);
    QQuickControlPrivate::handlePress(point);
    pressPoint = point;
    q->setPressed(true);

    emit q->pressed();

    if (autoRepeat)
        startRepeatDelay();
    else if (touchId != -1 || Qt::LeftButton == (pressButtons & Qt::LeftButton))
        startPressAndHold();
    else
        stopPressAndHold();
}

void QQuickAbstractButtonPrivate::handleMove(const QPointF &point)
{
    Q_Q(QQuickAbstractButton);
    QQuickControlPrivate::handleMove(point);
    q->setPressed(keepPressed || q->contains(point));

    if (!pressed && autoRepeat)
        stopPressRepeat();
    else if (holdTimer > 0 && (!pressed || QLineF(pressPoint, point).length() > QGuiApplication::styleHints()->startDragDistance()))
        stopPressAndHold();
}

void QQuickAbstractButtonPrivate::handleRelease(const QPointF &point)
{
    Q_Q(QQuickAbstractButton);
    QQuickControlPrivate::handleRelease(point);
    bool wasPressed = pressed;
    q->setPressed(false);
    pressButtons = Qt::NoButton;

    if (!wasHeld && (keepPressed || q->contains(point)))
        q->nextCheckState();

    if (wasPressed) {
        emit q->released();
        if (!wasHeld)
            emit q->clicked();
    } else {
        emit q->canceled();
    }

    if (autoRepeat)
        stopPressRepeat();
    else
        stopPressAndHold();
}

void QQuickAbstractButtonPrivate::handleUngrab()
{
    Q_Q(QQuickAbstractButton);
    QQuickControlPrivate::handleUngrab();
    pressButtons = Qt::NoButton;
    if (!pressed)
        return;

    q->setPressed(false);
    stopPressRepeat();
    stopPressAndHold();
    emit q->canceled();
}

bool QQuickAbstractButtonPrivate::isPressAndHoldConnected()
{
    Q_Q(QQuickAbstractButton);
    IS_SIGNAL_CONNECTED(q, QQuickAbstractButton, pressAndHold, ());
}

void QQuickAbstractButtonPrivate::startPressAndHold()
{
    Q_Q(QQuickAbstractButton);
    wasHeld = false;
    stopPressAndHold();
    if (isPressAndHoldConnected())
        holdTimer = q->startTimer(QGuiApplication::styleHints()->mousePressAndHoldInterval());
}

void QQuickAbstractButtonPrivate::stopPressAndHold()
{
    Q_Q(QQuickAbstractButton);
    if (holdTimer > 0) {
        q->killTimer(holdTimer);
        holdTimer = 0;
    }
}

void QQuickAbstractButtonPrivate::startRepeatDelay()
{
    Q_Q(QQuickAbstractButton);
    stopPressRepeat();
    delayTimer = q->startTimer(AUTO_REPEAT_DELAY);
}

void QQuickAbstractButtonPrivate::startPressRepeat()
{
    Q_Q(QQuickAbstractButton);
    stopPressRepeat();
    repeatTimer = q->startTimer(AUTO_REPEAT_INTERVAL);
}

void QQuickAbstractButtonPrivate::stopPressRepeat()
{
    Q_Q(QQuickAbstractButton);
    if (delayTimer > 0) {
        q->killTimer(delayTimer);
        delayTimer = 0;
    }
    if (repeatTimer > 0) {
        q->killTimer(repeatTimer);
        repeatTimer = 0;
    }
}

void QQuickAbstractButtonPrivate::toggle(bool value)
{
    Q_Q(QQuickAbstractButton);
    const bool wasChecked = checked;
    q->setChecked(value);
    if (wasChecked != checked)
        emit q->toggled();
}

QQuickAbstractButton *QQuickAbstractButtonPrivate::findCheckedButton() const
{
    Q_Q(const QQuickAbstractButton);
    if (group)
        return qobject_cast<QQuickAbstractButton *>(group->checkedButton());

    const QList<QQuickAbstractButton *> buttons = findExclusiveButtons();
    // TODO: A singular QRadioButton can be unchecked, which seems logical,
    // because there's nothing to be exclusive with. However, a RadioButton
    // from QtQuick.Controls 1.x can never be unchecked, which is the behavior
    // that QQuickRadioButton adopted. Uncommenting the following count check
    // gives the QRadioButton behavior. Notice that tst_radiobutton.qml needs
    // to be updated.
    if (!autoExclusive /*|| buttons.count() == 1*/)
        return nullptr;

    for (QQuickAbstractButton *button : buttons) {
        if (button->isChecked() && button != q)
            return button;
    }
    return checked ? const_cast<QQuickAbstractButton *>(q) : nullptr;
}

QList<QQuickAbstractButton *> QQuickAbstractButtonPrivate::findExclusiveButtons() const
{
    QList<QQuickAbstractButton *> buttons;
    if (group) {
        QQmlListProperty<QQuickAbstractButton> groupButtons = group->buttons();
        int count = groupButtons.count(&groupButtons);
        for (int i = 0; i < count; ++i) {
            QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton *>(groupButtons.at(&groupButtons, i));
            if (button)
                buttons += button;
        }
    } else if (parentItem) {
        const auto childItems = parentItem->childItems();
        for (QQuickItem *child : childItems) {
            QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton *>(child);
            if (button && button->autoExclusive() && !QQuickAbstractButtonPrivate::get(button)->group)
                buttons += button;
        }
    }
    return buttons;
}

QQuickAbstractButton::QQuickAbstractButton(QQuickItem *parent)
    : QQuickControl(*(new QQuickAbstractButtonPrivate), parent)
{
    setActiveFocusOnTab(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptedMouseButtons(Qt::LeftButton);
#if QT_CONFIG(cursor)
    setCursor(Qt::ArrowCursor);
#endif
}

QQuickAbstractButton::QQuickAbstractButton(QQuickAbstractButtonPrivate &dd, QQuickItem *parent)
    : QQuickControl(dd, parent)
{
    setActiveFocusOnTab(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptedMouseButtons(Qt::LeftButton);
#if QT_CONFIG(cursor)
    setCursor(Qt::ArrowCursor);
#endif
}

QQuickAbstractButton::~QQuickAbstractButton()
{
    Q_D(QQuickAbstractButton);
    if (d->group)
        d->group->removeButton(this);
}

/*!
    \qmlproperty string QtQuick.Controls::AbstractButton::text

    This property holds a textual description of the button.

    \note The text is used for accessibility purposes, so it makes sense to
          set a textual description even if the content item is an image.

    \sa {Control::contentItem}{contentItem}
*/
QString QQuickAbstractButton::text() const
{
    Q_D(const QQuickAbstractButton);
    return d->text;
}

void QQuickAbstractButton::setText(const QString &text)
{
    Q_D(QQuickAbstractButton);
    if (d->text == text)
        return;

    d->text = text;
    setAccessibleName(text);
    buttonChange(ButtonTextChange);
    emit textChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::AbstractButton::down

    This property holds whether the button is visually down.

    Unless explicitly set, this property follows the value of \l pressed. To
    return to the default value, set this property to \c undefined.

    \sa pressed
*/
bool QQuickAbstractButton::isDown() const
{
    Q_D(const QQuickAbstractButton);
    return d->down;
}

void QQuickAbstractButton::setDown(bool down)
{
    Q_D(QQuickAbstractButton);
    d->explicitDown = true;

    if (d->down == down)
        return;

    d->down = down;
    emit downChanged();
}

void QQuickAbstractButton::resetDown()
{
    Q_D(QQuickAbstractButton);
    if (!d->explicitDown)
        return;

    setDown(d->pressed);
    d->explicitDown = false;
}

/*!
    \qmlproperty bool QtQuick.Controls::AbstractButton::pressed
    \readonly

    This property holds whether the button is physically pressed. A button can
    be pressed by either touch or key events.

    \sa down
*/
bool QQuickAbstractButton::isPressed() const
{
    Q_D(const QQuickAbstractButton);
    return d->pressed;
}

void QQuickAbstractButton::setPressed(bool isPressed)
{
    Q_D(QQuickAbstractButton);
    if (d->pressed == isPressed)
        return;

    d->pressed = isPressed;
    setAccessibleProperty("pressed", isPressed);
    emit pressedChanged();
    buttonChange(ButtonPressedChanged);

    if (!d->explicitDown) {
        setDown(d->pressed);
        d->explicitDown = false;
    }
}

/*!
    \qmlproperty bool QtQuick.Controls::AbstractButton::checked

    This property holds whether the button is checked.

    \sa checkable
*/
bool QQuickAbstractButton::isChecked() const
{
    Q_D(const QQuickAbstractButton);
    return d->checked;
}

void QQuickAbstractButton::setChecked(bool checked)
{
    Q_D(QQuickAbstractButton);
    if (d->checked == checked)
        return;

    if (checked && !d->checkable)
        setCheckable(true);

    d->checked = checked;
    setAccessibleProperty("checked", checked);
    buttonChange(ButtonCheckedChange);
    emit checkedChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::AbstractButton::checkable

    This property holds whether the button is checkable.

    A checkable button toggles between checked (on) and unchecked (off) when
    the user clicks on it or presses the space bar while the button has active
    focus.

    Setting \l checked to \c true forces this property to \c true.

    The default value is \c false.

    \sa checked
*/
bool QQuickAbstractButton::isCheckable() const
{
    Q_D(const QQuickAbstractButton);
    return d->checkable;
}

void QQuickAbstractButton::setCheckable(bool checkable)
{
    Q_D(QQuickAbstractButton);
    if (d->checkable == checkable)
        return;

    d->checkable = checkable;
    setAccessibleProperty("checkable", checkable);
    buttonChange(ButtonCheckableChange);
    emit checkableChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::AbstractButton::autoExclusive

    This property holds whether auto-exclusivity is enabled.

    If auto-exclusivity is enabled, checkable buttons that belong to the same
    parent item behave as if they were part of the same ButtonGroup. Only
    one button can be checked at any time; checking another button automatically
    unchecks the previously checked one.

    \note The property has no effect on buttons that belong to a ButtonGroup.

    RadioButton and TabButton are auto-exclusive by default.
*/
bool QQuickAbstractButton::autoExclusive() const
{
    Q_D(const QQuickAbstractButton);
    return d->autoExclusive;
}

void QQuickAbstractButton::setAutoExclusive(bool exclusive)
{
    Q_D(QQuickAbstractButton);
    if (d->autoExclusive == exclusive)
        return;

    d->autoExclusive = exclusive;
    emit autoExclusiveChanged();
}

bool QQuickAbstractButton::autoRepeat() const
{
    Q_D(const QQuickAbstractButton);
    return d->autoRepeat;
}

void QQuickAbstractButton::setAutoRepeat(bool repeat)
{
    Q_D(QQuickAbstractButton);
    if (d->autoRepeat == repeat)
        return;

    d->stopPressRepeat();
    d->autoRepeat = repeat;
    buttonChange(ButtonAutoRepeatChange);
}

/*!
    \qmlproperty Item QtQuick.Controls::AbstractButton::indicator

    This property holds the indicator item.
*/
QQuickItem *QQuickAbstractButton::indicator() const
{
    Q_D(const QQuickAbstractButton);
    return d->indicator;
}

void QQuickAbstractButton::setIndicator(QQuickItem *indicator)
{
    Q_D(QQuickAbstractButton);
    if (d->indicator == indicator)
        return;

    QQuickControlPrivate::destroyDelegate(d->indicator, this);
    d->indicator = indicator;
    if (indicator) {
        if (!indicator->parentItem())
            indicator->setParentItem(this);
        indicator->setAcceptedMouseButtons(Qt::LeftButton);
    }
    emit indicatorChanged();
}

/*!
    \qmlmethod void QtQuick.Controls::AbstractButton::toggle()

    Toggles the checked state of the button.
*/
void QQuickAbstractButton::toggle()
{
    Q_D(QQuickAbstractButton);
    setChecked(!d->checked);
}

void QQuickAbstractButton::focusOutEvent(QFocusEvent *event)
{
    Q_D(QQuickAbstractButton);
    QQuickControl::focusOutEvent(event);
    if (d->touchId == -1) // don't ungrab on multi-touch if another control gets focused
        d->handleUngrab();
}

void QQuickAbstractButton::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickAbstractButton);
    QQuickControl::keyPressEvent(event);
    if (event->key() == Qt::Key_Space) {
        d->pressPoint = QPoint(qRound(width() / 2), qRound(height() / 2));
        setPressed(true);

        if (d->autoRepeat)
            d->startRepeatDelay();

        emit pressed();
        event->accept();
    }
}

void QQuickAbstractButton::keyReleaseEvent(QKeyEvent *event)
{
    Q_D(QQuickAbstractButton);
    QQuickControl::keyReleaseEvent(event);
    if (event->key() == Qt::Key_Space) {
        setPressed(false);

        nextCheckState();
        emit released();
        emit clicked();

        if (d->autoRepeat)
            d->stopPressRepeat();
        event->accept();
    }
}

void QQuickAbstractButton::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickAbstractButton);
    d->pressButtons = event->buttons();
    QQuickControl::mousePressEvent(event);
}

void QQuickAbstractButton::mouseDoubleClickEvent(QMouseEvent *event)
{
    QQuickControl::mouseDoubleClickEvent(event);
    emit doubleClicked();
}

void QQuickAbstractButton::timerEvent(QTimerEvent *event)
{
    Q_D(QQuickAbstractButton);
    QQuickControl::timerEvent(event);
    if (event->timerId() == d->holdTimer) {
        d->stopPressAndHold();
        d->wasHeld = true;
        emit pressAndHold();
    } else if (event->timerId() == d->delayTimer) {
        d->startPressRepeat();
    } else if (event->timerId() == d->repeatTimer) {
        emit released();
        emit clicked();
        emit pressed();
    }
}

void QQuickAbstractButton::buttonChange(ButtonChange change)
{
    Q_D(QQuickAbstractButton);
    switch (change) {
    case ButtonCheckedChange:
        if (d->checked) {
            QQuickAbstractButton *button = d->findCheckedButton();
            if (button && button != this)
                button->setChecked(false);
        }
        break;
    default:
        break;
    }
}

void QQuickAbstractButton::nextCheckState()
{
    Q_D(QQuickAbstractButton);
    if (d->checkable && (!d->checked || d->findCheckedButton() != this))
        d->toggle(!d->checked);
}

#if QT_CONFIG(accessibility)
void QQuickAbstractButton::accessibilityActiveChanged(bool active)
{
    QQuickControl::accessibilityActiveChanged(active);

    Q_D(QQuickAbstractButton);
    if (active) {
        setAccessibleName(d->text);
        setAccessibleProperty("pressed", d->pressed);
        setAccessibleProperty("checked", d->checked);
        setAccessibleProperty("checkable", d->checkable);
    }
}

QAccessible::Role QQuickAbstractButton::accessibleRole() const
{
    return QAccessible::Button;
}
#endif

QT_END_NAMESPACE
