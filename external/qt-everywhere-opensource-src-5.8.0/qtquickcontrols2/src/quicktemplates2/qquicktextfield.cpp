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

#include "qquicktextfield_p.h"
#include "qquicktextfield_p_p.h"
#include "qquickcontrol_p.h"
#include "qquickcontrol_p_p.h"

#include <QtCore/qbasictimer.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquicktextinput_p.h>
#include <QtQuick/private/qquickclipnode_p.h>

#ifndef QT_NO_ACCESSIBILITY
#include <QtQuick/private/qquickaccessibleattached_p.h>
#endif

QT_BEGIN_NAMESPACE

/*!
    \qmltype TextField
    \inherits TextInput
    \instantiates QQuickTextField
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-input
    \brief Single-line text input field.

    TextField is a single line text editor. TextField extends TextInput with
    a \l {placeholderText}{placeholder text} functionality, and adds decoration.

    \table
    \row \li \image qtquickcontrols2-textfield-normal.png
         \li A text field in its normal state.
    \row \li \image qtquickcontrols2-textfield-focused.png
         \li A text field that has active focus.
    \row \li \image qtquickcontrols2-textfield-disabled.png
         \li A text field that is disabled.
    \endtable

    \code
    TextField {
        placeholderText: qsTr("Enter name")
    }
    \endcode

    \sa TextArea, {Customizing TextField}, {Input Controls}
*/

/*!
    \qmlsignal QtQuick.Controls::TextField::pressAndHold(MouseEvent event)

    This signal is emitted when there is a long press (the delay depends on the platform plugin).
    The \l {MouseEvent}{event} parameter provides information about the press, including the x and y
    position of the press, and which button is pressed.

    \sa pressed, released
*/

/*!
    \qmlsignal QtQuick.Controls::TextField::pressed(MouseEvent event)
    \since QtQuick.Controls 2.1

    This signal is emitted when the text field is pressed by the user.
    The \l {MouseEvent}{event} parameter provides information about the press,
    including the x and y position of the press, and which button is pressed.

    \sa released, pressAndHold
*/

/*!
    \qmlsignal QtQuick.Controls::TextField::released(MouseEvent event)
    \since QtQuick.Controls 2.1

    This signal is emitted when the text field is released by the user.
    The \l {MouseEvent}{event} parameter provides information about the release,
    including the x and y position of the press, and which button is pressed.

    \sa pressed, pressAndHold
*/

QQuickTextFieldPrivate::QQuickTextFieldPrivate()
    : hovered(false)
    , explicitHoverEnabled(false)
    , background(nullptr)
    , focusReason(Qt::OtherFocusReason)
    , accessibleAttached(nullptr)
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installActivationObserver(this);
#endif
}

QQuickTextFieldPrivate::~QQuickTextFieldPrivate()
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::removeActivationObserver(this);
#endif
}

void QQuickTextFieldPrivate::resizeBackground()
{
    Q_Q(QQuickTextField);
    if (background) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(background);
        if (!p->widthValid && qFuzzyIsNull(background->x())) {
            background->setWidth(q->width());
            p->widthValid = false;
        }
        if (!p->heightValid && qFuzzyIsNull(background->y())) {
            background->setHeight(q->height());
            p->heightValid = false;
        }
    }
}

qreal QQuickTextFieldPrivate::getImplicitWidth() const
{
    return QQuickItemPrivate::getImplicitWidth();
}

qreal QQuickTextFieldPrivate::getImplicitHeight() const
{
    return QQuickItemPrivate::getImplicitHeight();
}

void QQuickTextFieldPrivate::implicitWidthChanged()
{
    Q_Q(QQuickTextField);
    QQuickItemPrivate::implicitWidthChanged();
    emit q->implicitWidthChanged3();
}

void QQuickTextFieldPrivate::implicitHeightChanged()
{
    Q_Q(QQuickTextField);
    QQuickItemPrivate::implicitHeightChanged();
    emit q->implicitHeightChanged3();
}

QQuickTextField::QQuickTextField(QQuickItem *parent) :
    QQuickTextInput(*(new QQuickTextFieldPrivate), parent)
{
    Q_D(QQuickTextField);
    d->pressHandler.control = this;
    d->setImplicitResizeEnabled(false);
    setAcceptedMouseButtons(Qt::AllButtons);
    setActiveFocusOnTab(true);
#ifndef QT_NO_CURSOR
    setCursor(Qt::IBeamCursor);
#endif
    QObjectPrivate::connect(this, &QQuickTextInput::readOnlyChanged,
                            d, &QQuickTextFieldPrivate::_q_readOnlyChanged);
    QObjectPrivate::connect(this, &QQuickTextInput::echoModeChanged,
                            d, &QQuickTextFieldPrivate::_q_echoModeChanged);
}

QQuickTextField::~QQuickTextField()
{
}

/*!
    \internal

    Determine which font is implicitly imposed on this control by its ancestors
    and QGuiApplication::font, resolve this against its own font (attributes from
    the implicit font are copied over). Then propagate this font to this
    control's children.
*/
void QQuickTextFieldPrivate::resolveFont()
{
    Q_Q(QQuickTextField);
    inheritFont(QQuickControlPrivate::parentFont(q));
}

void QQuickTextFieldPrivate::inheritFont(const QFont &f)
{
    Q_Q(QQuickTextField);
    QFont parentFont = font.resolve(f);
    parentFont.resolve(font.resolve() | f.resolve());

    const QFont defaultFont = QQuickControlPrivate::themeFont(QPlatformTheme::EditorFont);
    const QFont resolvedFont = parentFont.resolve(defaultFont);

    const bool changed = resolvedFont != sourceFont;
    q->QQuickTextInput::setFont(resolvedFont);
    if (changed)
        emit q->fontChanged();
}

void QQuickTextFieldPrivate::updateHoverEnabled(bool enabled, bool xplicit)
{
    Q_Q(QQuickTextField);
    if (!xplicit && explicitHoverEnabled)
        return;

    bool wasEnabled = q->isHoverEnabled();
    explicitHoverEnabled = xplicit;
    if (wasEnabled != enabled) {
        q->setAcceptHoverEvents(enabled);
        QQuickControlPrivate::updateHoverEnabledRecur(q, enabled);
        emit q->hoverEnabledChanged();
    }
}

void QQuickTextFieldPrivate::_q_readOnlyChanged(bool isReadOnly)
{
#ifndef QT_NO_ACCESSIBILITY
    if (accessibleAttached)
        accessibleAttached->set_readOnly(isReadOnly);
#else
    Q_UNUSED(isReadOnly)
#endif
}

void QQuickTextFieldPrivate::_q_echoModeChanged(QQuickTextField::EchoMode echoMode)
{
#ifndef QT_NO_ACCESSIBILITY
    if (accessibleAttached)
        accessibleAttached->set_passwordEdit((echoMode == QQuickTextField::Password || echoMode == QQuickTextField::PasswordEchoOnEdit) ? true : false);
#else
    Q_UNUSED(echoMode)
#endif
}

#ifndef QT_NO_ACCESSIBILITY
void QQuickTextFieldPrivate::accessibilityActiveChanged(bool active)
{
    if (accessibleAttached || !active)
        return;

    Q_Q(QQuickTextField);
    accessibleAttached = qobject_cast<QQuickAccessibleAttached *>(qmlAttachedPropertiesObject<QQuickAccessibleAttached>(q, true));
    if (accessibleAttached) {
        accessibleAttached->setRole(accessibleRole());
        accessibleAttached->set_readOnly(m_readOnly);
        accessibleAttached->set_passwordEdit((m_echoMode == QQuickTextField::Password || m_echoMode == QQuickTextField::PasswordEchoOnEdit) ? true : false);
        accessibleAttached->setDescription(placeholder);
    } else {
        qWarning() << "QQuickTextField: " << q << " QQuickAccessibleAttached object creation failed!";
    }
}

QAccessible::Role QQuickTextFieldPrivate::accessibleRole() const
{
    return QAccessible::EditableText;
}
#endif

/*
   Deletes "delegate" if Component.completed() has been emitted,
   otherwise stores it in pendingDeletions.
*/
void QQuickTextFieldPrivate::deleteDelegate(QObject *delegate)
{
    if (componentComplete)
        delete delegate;
    else
        pendingDeletions.append(delegate);
}

QFont QQuickTextField::font() const
{
    return QQuickTextInput::font();
}

void QQuickTextField::setFont(const QFont &font)
{
    Q_D(QQuickTextField);
    if (d->font.resolve() == font.resolve() && d->font == font)
        return;

    d->font = font;
    d->resolveFont();
}

/*!
    \qmlproperty Item QtQuick.Controls::TextField::background

    This property holds the background item.

    \input qquickcontrol-background.qdocinc notes

    \sa {Customizing TextField}
*/
QQuickItem *QQuickTextField::background() const
{
    Q_D(const QQuickTextField);
    return d->background;
}

void QQuickTextField::setBackground(QQuickItem *background)
{
    Q_D(QQuickTextField);
    if (d->background == background)
        return;

    d->deleteDelegate(d->background);
    d->background = background;
    if (background) {
        background->setParentItem(this);
        if (qFuzzyIsNull(background->z()))
            background->setZ(-1);
        if (isComponentComplete())
            d->resizeBackground();
    }
    emit backgroundChanged();
}

/*!
    \qmlproperty string QtQuick.Controls::TextField::placeholderText

    This property holds the hint that is displayed in the TextField before the user
    enters text.
*/
QString QQuickTextField::placeholderText() const
{
    Q_D(const QQuickTextField);
    return d->placeholder;
}

void QQuickTextField::setPlaceholderText(const QString &text)
{
    Q_D(QQuickTextField);
    if (d->placeholder == text)
        return;

    d->placeholder = text;
#ifndef QT_NO_ACCESSIBILITY
    if (d->accessibleAttached)
        d->accessibleAttached->setDescription(text);
#endif
    emit placeholderTextChanged();
}

/*!
    \qmlproperty enumeration QtQuick.Controls::TextField::focusReason

    \include qquickcontrol-focusreason.qdocinc
*/
Qt::FocusReason QQuickTextField::focusReason() const
{
    Q_D(const QQuickTextField);
    return d->focusReason;
}

void QQuickTextField::setFocusReason(Qt::FocusReason reason)
{
    Q_D(QQuickTextField);
    if (d->focusReason == reason)
        return;

    d->focusReason = reason;
    emit focusReasonChanged();
}

/*!
    \since QtQuick.Controls 2.1
    \qmlproperty bool QtQuick.Controls::TextField::hovered
    \readonly

    This property holds whether the text field is hovered.

    \sa hoverEnabled
*/
bool QQuickTextField::isHovered() const
{
    Q_D(const QQuickTextField);
    return d->hovered;
}

void QQuickTextField::setHovered(bool hovered)
{
    Q_D(QQuickTextField);
    if (hovered == d->hovered)
        return;

    d->hovered = hovered;
    emit hoveredChanged();
}

/*!
    \since QtQuick.Controls 2.1
    \qmlproperty bool QtQuick.Controls::TextField::hoverEnabled

    This property determines whether the text field accepts hover events. The default value is \c false.

    \sa hovered
*/
bool QQuickTextField::isHoverEnabled() const
{
    Q_D(const QQuickTextField);
    return d->hoverEnabled;
}

void QQuickTextField::setHoverEnabled(bool enabled)
{
    Q_D(QQuickTextField);
    if (d->explicitHoverEnabled && enabled == d->hoverEnabled)
        return;

    d->updateHoverEnabled(enabled, true); // explicit=true
}

void QQuickTextField::resetHoverEnabled()
{
    Q_D(QQuickTextField);
    if (!d->explicitHoverEnabled)
        return;

    d->explicitHoverEnabled = false;
    d->updateHoverEnabled(QQuickControlPrivate::calcHoverEnabled(d->parentItem), false); // explicit=false
}

void QQuickTextField::classBegin()
{
    Q_D(QQuickTextField);
    QQuickTextInput::classBegin();
    d->resolveFont();
}

void QQuickTextField::componentComplete()
{
    Q_D(QQuickTextField);
    QQuickTextInput::componentComplete();
    if (!d->explicitHoverEnabled)
        setAcceptHoverEvents(QQuickControlPrivate::calcHoverEnabled(d->parentItem));
#ifndef QT_NO_ACCESSIBILITY
    if (!d->accessibleAttached && QAccessible::isActive())
        d->accessibilityActiveChanged(true);
#endif

    qDeleteAll(d->pendingDeletions);
    d->pendingDeletions.clear();
}

void QQuickTextField::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    Q_D(QQuickTextField);
    QQuickTextInput::itemChange(change, value);
    if (change == ItemParentHasChanged && value.item) {
        d->resolveFont();
        if (!d->explicitHoverEnabled)
            d->updateHoverEnabled(QQuickControlPrivate::calcHoverEnabled(d->parentItem), false); // explicit=false
    }
}

void QQuickTextField::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickTextField);
    QQuickTextInput::geometryChanged(newGeometry, oldGeometry);
    d->resizeBackground();
}

QSGNode *QQuickTextField::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    QQuickDefaultClipNode *clipNode = static_cast<QQuickDefaultClipNode *>(oldNode);
    if (!clipNode)
        clipNode = new QQuickDefaultClipNode(QRectF());

    clipNode->setRect(clipRect().adjusted(leftPadding(), topPadding(), -rightPadding(), -bottomPadding()));
    clipNode->update();

    QSGNode *textNode = QQuickTextInput::updatePaintNode(clipNode->firstChild(), data);
    if (!textNode->parent())
        clipNode->appendChildNode(textNode);

    return clipNode;
}

void QQuickTextField::focusInEvent(QFocusEvent *event)
{
    QQuickTextInput::focusInEvent(event);
    setFocusReason(event->reason());
}

void QQuickTextField::focusOutEvent(QFocusEvent *event)
{
    QQuickTextInput::focusOutEvent(event);
    setFocusReason(event->reason());
}

void QQuickTextField::hoverEnterEvent(QHoverEvent *event)
{
    Q_D(QQuickTextField);
    QQuickTextInput::hoverEnterEvent(event);
    setHovered(d->hoverEnabled);
    event->setAccepted(d->hoverEnabled);
}

void QQuickTextField::hoverLeaveEvent(QHoverEvent *event)
{
    Q_D(QQuickTextField);
    QQuickTextInput::hoverLeaveEvent(event);
    setHovered(false);
    event->setAccepted(d->hoverEnabled);
}

void QQuickTextField::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickTextField);
    d->pressHandler.mousePressEvent(event);
    if (d->pressHandler.isActive()) {
        if (d->pressHandler.delayedMousePressEvent) {
            QQuickTextInput::mousePressEvent(d->pressHandler.delayedMousePressEvent);
            d->pressHandler.clearDelayedMouseEvent();
        }
        QQuickTextInput::mousePressEvent(event);
    }
}

void QQuickTextField::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickTextField);
    d->pressHandler.mouseMoveEvent(event);
    if (d->pressHandler.isActive()) {
        if (d->pressHandler.delayedMousePressEvent) {
            QQuickTextInput::mousePressEvent(d->pressHandler.delayedMousePressEvent);
            d->pressHandler.clearDelayedMouseEvent();
        }
        QQuickTextInput::mouseMoveEvent(event);
    }
}

void QQuickTextField::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickTextField);
    d->pressHandler.mouseReleaseEvent(event);
    if (d->pressHandler.isActive()) {
        if (d->pressHandler.delayedMousePressEvent) {
            QQuickTextInput::mousePressEvent(d->pressHandler.delayedMousePressEvent);
            d->pressHandler.clearDelayedMouseEvent();
        }
        QQuickTextInput::mouseReleaseEvent(event);
    }
}

void QQuickTextField::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickTextField);
    if (d->pressHandler.delayedMousePressEvent) {
        QQuickTextInput::mousePressEvent(d->pressHandler.delayedMousePressEvent);
        d->pressHandler.clearDelayedMouseEvent();
    }
    QQuickTextInput::mouseDoubleClickEvent(event);
}

void QQuickTextField::timerEvent(QTimerEvent *event)
{
    Q_D(QQuickTextField);
    if (event->timerId() == d->pressHandler.timer.timerId()) {
        d->pressHandler.timerEvent(event);
    } else {
        QQuickTextInput::timerEvent(event);
    }
}

QT_END_NAMESPACE
