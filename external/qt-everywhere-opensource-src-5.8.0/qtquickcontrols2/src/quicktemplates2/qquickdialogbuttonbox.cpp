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

#include "qquickdialogbuttonbox_p.h"
#include "qquickdialogbuttonbox_p_p.h"
#include "qquickabstractbutton_p.h"
#include "qquickbutton_p.h"

#include <QtCore/qpointer.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlcomponent.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

/*!
    \qmltype DialogButtonBox
    \inherits Container
    \instantiates QQuickDialogButtonBox
    \inqmlmodule QtQuick.Controls
    \ingroup qtquickcontrols2-dialogs
    \brief A button box used in dialogs.
    \since 5.8

    Dialogs and message boxes typically present buttons in an order that
    conforms to the interface guidelines for that platform. Invariably,
    different platforms have their dialog buttons in different orders.
    DialogButtonBox allows a developer to add buttons to it and will
    automatically use the appropriate order for the user's platform.

    Most buttons for a dialog follow certain roles. Such roles include:

    \list
    \li Accepting or rejecting the dialog.
    \li Asking for help.
    \li Performing actions on the dialog itself (such as resetting fields or
       applying changes).
    \endlist

    There can also be alternate ways of dismissing the dialog which may cause
    destructive results.

    Most dialogs have buttons that can almost be considered standard (e.g.
    \uicontrol OK and \uicontrol Cancel buttons). It is sometimes convenient
    to create these buttons in a standard way.

    There are a couple ways of using DialogButtonBox. One way is to specify
    the standard buttons (e.g. \uicontrol OK, \uicontrol Cancel, \uicontrol Save)
    and let the button box setup the buttons.

    \image qtquickcontrols2-dialogbuttonbox.png

    \snippet qtquickcontrols2-dialogbuttonbox.qml 1

    Alternatively, buttons and their roles can be specified by hand:

    \snippet qtquickcontrols2-dialogbuttonbox-attached.qml 1

    You can also mix and match normal buttons and standard buttons.

    When a button is clicked in the button box, the \l clicked() signal is
    emitted for the actual button that is pressed. For convenience, if the
    button has an \c AcceptRole, \c RejectRole, or \c HelpRole, the \l accepted(),
    \l rejected(), or \l helpRequested() signals are emitted respectively.

    \sa Dialog
*/

/*!
    \qmlsignal QtQuick.Controls::DialogButtonBox::accepted()

    This signal is emitted when a button defined with the \c AcceptRole or
    \c YesRole is clicked.

    \sa rejected(), clicked(), helpRequested()
*/

/*!
    \qmlsignal QtQuick.Controls::DialogButtonBox::rejected()

    This signal is emitted when a button defined with the \c RejectRole or
    \c NoRole is clicked.

    \sa accepted(), helpRequested(), clicked()
*/

/*!
    \qmlsignal QtQuick.Controls::DialogButtonBox::helpRequested()

    This signal is emitted when a button defined with the \c HelpRole is clicked.

    \sa accepted(), rejected(), clicked()
*/

/*!
    \qmlsignal QtQuick.Controls::DialogButtonBox::clicked(AbstractButton button)

    This signal is emitted when a \a button inside the button box is clicked.

    \sa accepted(), rejected(), helpRequested()
*/

static QPlatformDialogHelper::ButtonRole buttonRole(QQuickAbstractButton *button)
{
    const QQuickDialogButtonBoxAttached *attached = qobject_cast<QQuickDialogButtonBoxAttached *>(qmlAttachedPropertiesObject<QQuickDialogButtonBox>(button, false));
    return attached ? attached->buttonRole() : QPlatformDialogHelper::InvalidRole;
}

QQuickDialogButtonBoxPrivate::QQuickDialogButtonBoxPrivate() :
    alignment(0),
    position(QQuickDialogButtonBox::Footer),
    standardButtons(QPlatformDialogHelper::NoButton),
    delegate(nullptr)
{
}

void QQuickDialogButtonBoxPrivate::itemImplicitWidthChanged(QQuickItem *item)
{
    Q_UNUSED(item);
    resizeContent();
}

void QQuickDialogButtonBoxPrivate::itemImplicitHeightChanged(QQuickItem *item)
{
    Q_UNUSED(item);
    resizeContent();
}

// adapted from QStyle::alignedRect()
static QRectF alignedRect(Qt::LayoutDirection direction, Qt::Alignment alignment, const QSizeF &size, const QRectF &rectangle)
{
    alignment = QGuiApplicationPrivate::visualAlignment(direction, alignment);
    qreal x = rectangle.x();
    qreal y = rectangle.y();
    qreal w = size.width();
    qreal h = size.height();
    if ((alignment & Qt::AlignVCenter) == Qt::AlignVCenter)
        y += rectangle.size().height() / 2 - h / 2;
    else if ((alignment & Qt::AlignBottom) == Qt::AlignBottom)
        y += rectangle.size().height() - h;
    if ((alignment & Qt::AlignRight) == Qt::AlignRight)
        x += rectangle.size().width() - w;
    else if ((alignment & Qt::AlignHCenter) == Qt::AlignHCenter)
        x += rectangle.size().width() / 2 - w / 2;
    return QRectF(x, y, w, h);
}

void QQuickDialogButtonBoxPrivate::resizeContent()
{
    Q_Q(QQuickDialogButtonBox);
    if (!contentItem)
        return;

    const int halign = alignment & Qt::AlignHorizontal_Mask;
    const int valign = alignment & Qt::AlignVertical_Mask;

    const qreal cw = !halign ? q->availableWidth() : contentItem->implicitWidth();
    const qreal ch = !valign ? q->availableHeight() : contentItem->implicitWidth();

    QRectF geometry = q->boundingRect().adjusted(q->leftPadding(), q->topPadding(), -q->rightPadding(), -q->bottomPadding());
    if (halign || valign)
        geometry = alignedRect(q->isMirrored() ? Qt::RightToLeft : Qt::LeftToRight, alignment, QSizeF(cw, ch), geometry);

    contentItem->setPosition(geometry.topLeft());
    contentItem->setSize(geometry.size());
}

void QQuickDialogButtonBoxPrivate::updateLayout()
{
    Q_Q(QQuickDialogButtonBox);
    const int count = contentModel->count();
    if (count <= 0)
        return;

    const int halign = alignment & Qt::AlignHorizontal_Mask;
    const int valign = alignment & Qt::AlignVertical_Mask;

    QVector<QQuickAbstractButton *> buttons;
    const qreal maxItemWidth = (contentItem->width() - qMax(0, count - 1) * spacing) / count;
    const qreal maxItemHeight = contentItem->height();

    for (int i = 0; i < count; ++i) {
        QQuickItem *item = q->itemAt(i);
        if (item) {
            QQuickItemPrivate *p = QQuickItemPrivate::get(item);
            if (!p->widthValid) {
                if (!halign)
                    item->setWidth(maxItemWidth);
                else
                    item->resetWidth();
                if (!valign)
                    item->setHeight(maxItemHeight);
                else
                    item->resetHeight();
                p->widthValid = false;
            }
        }
        buttons += static_cast<QQuickAbstractButton *>(item);
    }

    struct ButtonLayout {
        bool operator()(QQuickAbstractButton *first, QQuickAbstractButton *second)
        {
            const QPlatformDialogHelper::ButtonRole firstRole = buttonRole(first);
            const QPlatformDialogHelper::ButtonRole secondRole = buttonRole(second);

            if (firstRole != secondRole && firstRole != QPlatformDialogHelper::InvalidRole && secondRole != QPlatformDialogHelper::InvalidRole) {
                const int *l = m_layout;
                while (*l != QPlatformDialogHelper::EOL) {
                    const int role = (*l & ~QPlatformDialogHelper::Reverse);
                    if (role == firstRole)
                        return true;
                    if (role == secondRole)
                        return false;
                    ++l;
                }
            }

            if (firstRole == secondRole)
                return first < second;

            return firstRole != QPlatformDialogHelper::InvalidRole;
        }
        const int *m_layout = QPlatformDialogHelper::buttonLayout();
    };

    std::sort(buttons.begin(), buttons.end(), ButtonLayout());

    for (int i = 0; i < buttons.count() - 1; ++i)
        q->insertItem(i, buttons.at(i));
}

void QQuickDialogButtonBoxPrivate::handleClick()
{
    Q_Q(QQuickDialogButtonBox);
    QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton *>(q->sender());
    if (!button)
        return;

    // Can't fetch this *after* emitting clicked, as clicked may destroy the button
    // or change its role. Now changing the role is not possible yet, but arguably
    // both clicked and accepted/rejected/etc. should be emitted "atomically"
    // depending on whatever role the button had at the time of the click.
    const QPlatformDialogHelper::ButtonRole role = buttonRole(button);
    QPointer<QQuickDialogButtonBox> guard(q);

    emit q->clicked(button);

    if (!guard)
        return;

    switch (role) {
    case QPlatformDialogHelper::AcceptRole:
    case QPlatformDialogHelper::YesRole:
        emit q->accepted();
        break;
    case QPlatformDialogHelper::RejectRole:
    case QPlatformDialogHelper::NoRole:
        emit q->rejected();
        break;
    case QPlatformDialogHelper::HelpRole:
        emit q->helpRequested();
        break;
    default:
        break;
    }
}

QQuickAbstractButton *QQuickDialogButtonBoxPrivate::createStandardButton(QPlatformDialogHelper::StandardButton standardButton)
{
    Q_Q(QQuickDialogButtonBox);
    if (!delegate)
        return nullptr;

    QQmlContext *creationContext = delegate->creationContext();
    if (!creationContext)
        creationContext = qmlContext(q);
    QQmlContext *context = new QQmlContext(creationContext);
    context->setContextObject(q);

    QObject *object = delegate->beginCreate(context);
    QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton*>(object);
    if (button) {
        QQuickDialogButtonBoxAttached *attached = qobject_cast<QQuickDialogButtonBoxAttached *>(qmlAttachedPropertiesObject<QQuickDialogButtonBox>(button, true));
        QQuickDialogButtonBoxAttachedPrivate::get(attached)->standardButton = standardButton;
        attached->setButtonRole(QPlatformDialogHelper::buttonRole(standardButton));
        button->setText(QPlatformTheme::removeMnemonics(QGuiApplicationPrivate::platformTheme()->standardButtonText(standardButton)));
        delegate->completeCreate();
        return button;
    }

    delete object;
    return nullptr;
}

void QQuickDialogButtonBoxPrivate::removeStandardButtons()
{
    Q_Q(QQuickDialogButtonBox);
    int i = q->count() - 1;
    while (i >= 0) {
        QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton *>(q->itemAt(i));
        if (button) {
            QQuickDialogButtonBoxAttached *attached = qobject_cast<QQuickDialogButtonBoxAttached *>(qmlAttachedPropertiesObject<QQuickDialogButtonBox>(button, false));
            if (attached) {
                QQuickDialogButtonBoxAttachedPrivate *p = QQuickDialogButtonBoxAttachedPrivate::get(attached);
                if (p->standardButton != QPlatformDialogHelper::NoButton) {
                    q->removeItem(i);
                    button->deleteLater();
                }
            }
        }
        --i;
    }
}

QQuickDialogButtonBox::QQuickDialogButtonBox(QQuickItem *parent) :
    QQuickContainer(*(new QQuickDialogButtonBoxPrivate), parent)
{
}

QQuickDialogButtonBox::~QQuickDialogButtonBox()
{
}


/*!
    \qmlproperty enumeration QtQuick.Controls::DialogButtonBox::position

    This property holds the position of the button box.

    \note If the button box is assigned as a header or footer of ApplicationWindow
    or Page, the appropriate position is set automatically.

    Possible values:
    \value DialogButtonBox.Header The button box is at the top, as a window or page header.
    \value DialogButtonBox.Footer The button box is at the bottom, as a window or page header.

    The default value is \c Footer.

    \sa Dialog::header, Dialog::footer
*/
QQuickDialogButtonBox::Position QQuickDialogButtonBox::position() const
{
    Q_D(const QQuickDialogButtonBox);
    return d->position;
}

void QQuickDialogButtonBox::setPosition(Position position)
{
    Q_D(QQuickDialogButtonBox);
    if (d->position == position)
        return;

    d->position = position;
    emit positionChanged();
}

/*!
    \qmlproperty flags QtQuick.Controls::DialogButtonBox::alignment

    This property holds the alignment of the buttons.

    Possible values:
    \value undefined The buttons are resized to fill the available space.
    \value Qt.AlignLeft The buttons are aligned to the left.
    \value Qt.AlignHCenter The buttons are horizontally centered.
    \value Qt.AlignRight The buttons are aligned to the right.
    \value Qt.AlignTop The buttons are aligned to the top.
    \value Qt.AlignVCenter The buttons are vertically centered.
    \value Qt.AlignBottom The buttons are aligned to the bottom.
*/
Qt::Alignment QQuickDialogButtonBox::alignment() const
{
    Q_D(const QQuickDialogButtonBox);
    return d->alignment;
}

void QQuickDialogButtonBox::setAlignment(Qt::Alignment alignment)
{
    Q_D(QQuickDialogButtonBox);
    if (d->alignment == alignment)
        return;

    d->alignment = alignment;
    if (isComponentComplete()) {
        d->resizeContent();
        polish();
    }
    emit alignmentChanged();
}

void QQuickDialogButtonBox::resetAlignment()
{
    setAlignment(0);
}

/*!
    \qmlproperty enumeration QtQuick.Controls::DialogButtonBox::standardButtons

    This property holds a combination of standard buttons that are used by the button box.

    \snippet qtquickcontrols2-dialogbuttonbox.qml 1

    Possible flags:
    \value DialogButtonBox.Ok An "OK" button defined with the \c AcceptRole.
    \value DialogButtonBox.Open An "Open" button defined with the \c AcceptRole.
    \value DialogButtonBox.Save A "Save" button defined with the \c AcceptRole.
    \value DialogButtonBox.Cancel A "Cancel" button defined with the \c RejectRole.
    \value DialogButtonBox.Close A "Close" button defined with the \c RejectRole.
    \value DialogButtonBox.Discard A "Discard" or "Don't Save" button, depending on the platform, defined with the \c DestructiveRole.
    \value DialogButtonBox.Apply An "Apply" button defined with the \c ApplyRole.
    \value DialogButtonBox.Reset A "Reset" button defined with the \c ResetRole.
    \value DialogButtonBox.RestoreDefaults A "Restore Defaults" button defined with the \c ResetRole.
    \value DialogButtonBox.Help A "Help" button defined with the \c HelpRole.
    \value DialogButtonBox.SaveAll A "Save All" button defined with the \c AcceptRole.
    \value DialogButtonBox.Yes A "Yes" button defined with the \c YesRole.
    \value DialogButtonBox.YesToAll A "Yes to All" button defined with the \c YesRole.
    \value DialogButtonBox.No A "No" button defined with the \c NoRole.
    \value DialogButtonBox.NoToAll A "No to All" button defined with the \c NoRole.
    \value DialogButtonBox.Abort An "Abort" button defined with the \c RejectRole.
    \value DialogButtonBox.Retry A "Retry" button defined with the \c AcceptRole.
    \value DialogButtonBox.Ignore An "Ignore" button defined with the \c AcceptRole.
    \value DialogButtonBox.NoButton An invalid button.

    \sa standardButton()
*/
QPlatformDialogHelper::StandardButtons QQuickDialogButtonBox::standardButtons() const
{
    Q_D(const QQuickDialogButtonBox);
    return d->standardButtons;
}

void QQuickDialogButtonBox::setStandardButtons(QPlatformDialogHelper::StandardButtons buttons)
{
    Q_D(QQuickDialogButtonBox);
    if (d->standardButtons == buttons)
        return;

    d->removeStandardButtons();

    for (int i = QPlatformDialogHelper::FirstButton; i <= QPlatformDialogHelper::LastButton; i<<=1) {
        QPlatformDialogHelper::StandardButton standardButton = static_cast<QPlatformDialogHelper::StandardButton>(i);
        if (standardButton & buttons) {
            QQuickAbstractButton *button = d->createStandardButton(standardButton);
            if (button)
                addItem(button);
        }
    }

    if (isComponentComplete())
        polish();

    d->standardButtons = buttons;
    emit standardButtonsChanged();
}

/*!
    \qmlmethod AbstractButton QtQuick.Controls::DialogButtonBox::standardButton(StandardButton button)

    Returns the specified standard \a button, or \c null if it does not exist.

    \sa standardButtons
*/
QQuickAbstractButton *QQuickDialogButtonBox::standardButton(QPlatformDialogHelper::StandardButton button) const
{
    Q_D(const QQuickDialogButtonBox);
    if (Q_UNLIKELY(!(d->standardButtons & button)))
        return nullptr;
    for (int i = 0, n = count(); i < n; ++i) {
        QQuickAbstractButton *btn = qobject_cast<QQuickAbstractButton *>(d->itemAt(i));
        if (Q_LIKELY(btn)) {
            QQuickDialogButtonBoxAttached *attached = qobject_cast<QQuickDialogButtonBoxAttached *>(qmlAttachedPropertiesObject<QQuickDialogButtonBox>(btn, false));
            if (attached && QQuickDialogButtonBoxAttachedPrivate::get(attached)->standardButton == button)
                return btn;
        }
    }
    return nullptr;
}

/*!
    \qmlproperty Component QtQuick.Controls::DialogButtonBox::delegate

    This property holds a delegate for creating standard buttons.

    \sa standardButtons
*/
QQmlComponent *QQuickDialogButtonBox::delegate() const
{
    Q_D(const QQuickDialogButtonBox);
    return d->delegate;
}

void QQuickDialogButtonBox::setDelegate(QQmlComponent* delegate)
{
    Q_D(QQuickDialogButtonBox);
    if (d->delegate == delegate)
        return;

    delete d->delegate;
    d->delegate = delegate;
    emit delegateChanged();
}

QQuickDialogButtonBoxAttached *QQuickDialogButtonBox::qmlAttachedProperties(QObject *object)
{
    return new QQuickDialogButtonBoxAttached(object);
}

void QQuickDialogButtonBox::updatePolish()
{
    Q_D(QQuickDialogButtonBox);
    QQuickContainer::updatePolish();
    d->updateLayout();
}

void QQuickDialogButtonBox::componentComplete()
{
    Q_D(QQuickDialogButtonBox);
    QQuickContainer::componentComplete();
    d->updateLayout();
}

void QQuickDialogButtonBox::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickDialogButtonBox);
    QQuickContainer::geometryChanged(newGeometry, oldGeometry);
    d->updateLayout();
}

void QQuickDialogButtonBox::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_D(QQuickDialogButtonBox);
    QQuickContainer::contentItemChange(newItem, oldItem);
    if (oldItem)
        QQuickItemPrivate::get(oldItem)->removeItemChangeListener(d, QQuickItemPrivate::ImplicitWidth | QQuickItemPrivate::ImplicitHeight);
    if (newItem)
        QQuickItemPrivate::get(newItem)->addItemChangeListener(d, QQuickItemPrivate::ImplicitWidth | QQuickItemPrivate::ImplicitHeight);
}

bool QQuickDialogButtonBox::isContent(QQuickItem *item) const
{
    return qobject_cast<QQuickAbstractButton *>(item);
}

void QQuickDialogButtonBox::itemAdded(int index, QQuickItem *item)
{
    Q_D(QQuickDialogButtonBox);
    Q_UNUSED(index);
    if (QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton *>(item))
        QObjectPrivate::connect(button, &QQuickAbstractButton::clicked, d, &QQuickDialogButtonBoxPrivate::handleClick);
    if (QQuickDialogButtonBoxAttached *attached = qobject_cast<QQuickDialogButtonBoxAttached *>(qmlAttachedPropertiesObject<QQuickDialogButtonBox>(item, false)))
        QQuickDialogButtonBoxAttachedPrivate::get(attached)->setButtonBox(this);
    if (isComponentComplete())
        polish();
}

void QQuickDialogButtonBox::itemRemoved(int index, QQuickItem *item)
{
    Q_D(QQuickDialogButtonBox);
    Q_UNUSED(index);
    if (QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton *>(item))
        QObjectPrivate::disconnect(button, &QQuickAbstractButton::clicked, d, &QQuickDialogButtonBoxPrivate::handleClick);
    if (QQuickDialogButtonBoxAttached *attached = qobject_cast<QQuickDialogButtonBoxAttached *>(qmlAttachedPropertiesObject<QQuickDialogButtonBox>(item, false)))
        QQuickDialogButtonBoxAttachedPrivate::get(attached)->setButtonBox(nullptr);
    if (isComponentComplete())
        polish();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickDialogButtonBox::accessibleRole() const
{
    return QAccessible::PageTabList;
}
#endif

void QQuickDialogButtonBoxAttachedPrivate::setButtonBox(QQuickDialogButtonBox *box)
{
    Q_Q(QQuickDialogButtonBoxAttached);
    if (buttonBox == box)
        return;

    buttonBox = box;
    emit q->buttonBoxChanged();
}

QQuickDialogButtonBoxAttached::QQuickDialogButtonBoxAttached(QObject *parent) :
    QObject(*(new QQuickDialogButtonBoxAttachedPrivate), parent)
{
    Q_D(QQuickDialogButtonBoxAttached);
    QQuickItem *parentItem = qobject_cast<QQuickItem *>(parent);
    while (parentItem && !d->buttonBox) {
        d->buttonBox = qobject_cast<QQuickDialogButtonBox *>(parentItem);
        parentItem = parentItem->parentItem();
    }
}

/*!
    \qmlattachedproperty DialogButtonBox QtQuick.Controls::DialogButtonBox::buttonBox
    \readonly

    This attached property holds the button box that manages this button, or
    \c null if the button is not in a button box.
*/
QQuickDialogButtonBox *QQuickDialogButtonBoxAttached::buttonBox() const
{
    Q_D(const QQuickDialogButtonBoxAttached);
    return d->buttonBox;
}

/*!
    \qmlattachedproperty enumeration QtQuick.Controls::DialogButtonBox::buttonRole

    This attached property holds the role of each button in a button box.

    \snippet qtquickcontrols2-dialogbuttonbox-attached.qml 1

    Available values:
    \value DialogButtonBox.InvalidRole The button is invalid.
    \value DialogButtonBox.AcceptRole Clicking the button causes the dialog to be accepted (e.g. \uicontrol OK).
    \value DialogButtonBox.RejectRole Clicking the button causes the dialog to be rejected (e.g. \uicontrol Cancel).
    \value DialogButtonBox.DestructiveRole Clicking the button causes a destructive change (e.g. for discarding changes) and closes the dialog.
    \value DialogButtonBox.ActionRole Clicking the button causes changes to the elements within the dialog.
    \value DialogButtonBox.HelpRole The button can be clicked to request help.
    \value DialogButtonBox.YesRole The button is a "Yes"-like button.
    \value DialogButtonBox.NoRole The button is a "No"-like button.
    \value DialogButtonBox.ResetRole The button resets the dialog's fields to default values.
    \value DialogButtonBox.ApplyRole The button applies current changes.
*/
QPlatformDialogHelper::ButtonRole QQuickDialogButtonBoxAttached::buttonRole() const
{
    Q_D(const QQuickDialogButtonBoxAttached);
    return d->buttonRole;
}

void QQuickDialogButtonBoxAttached::setButtonRole(QPlatformDialogHelper::ButtonRole role)
{
    Q_D(QQuickDialogButtonBoxAttached);
    if (d->buttonRole == role)
        return;

    d->buttonRole = role;
    emit buttonRoleChanged();
}

QT_END_NAMESPACE
