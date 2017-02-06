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

#include "qquickbuttongroup_p.h"

#include <QtCore/private/qobject_p.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qvariant.h>
#include <QtQml/qqmlinfo.h>

#include "qquickabstractbutton_p_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype ButtonGroup
    \inherits QtObject
    \instantiates QQuickButtonGroup
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup utilities
    \brief Mutually-exclusive group of checkable buttons.

    ButtonGroup is a non-visual, mutually exclusive group of buttons.
    It is used with controls such as RadioButton, where only one of the options
    can be selected at a time.

    The most straight-forward way to use ButtonGroup is to assign
    a list of buttons. For example, the list of children of a
    \l{Item Positioners}{positioner} or a \l{Qt Quick Layouts}{layout}
    that manages a group of mutually exclusive buttons.

    \code
    ButtonGroup {
        buttons: column.children
    }

    Column {
        id: column

        RadioButton {
            checked: true
            text: qsTr("DAB")
        }

        RadioButton {
            text: qsTr("FM")
        }

        RadioButton {
            text: qsTr("AM")
        }
    }
    \endcode

    Mutually exclusive buttons do not always share the same parent item,
    or the parent layout may sometimes contain items that should not be
    included in the button group. Such cases are best handled using
    the \l group attached property.

    \code
    ButtonGroup { id: radioGroup }

    Column {
        Label {
            text: qsTr("Radio:")
        }

        RadioButton {
            checked: true
            text: qsTr("DAB")
            ButtonGroup.group: radioGroup
        }

        RadioButton {
            text: qsTr("FM")
            ButtonGroup.group: radioGroup
        }

        RadioButton {
            text: qsTr("AM")
            ButtonGroup.group: radioGroup
        }
    }
    \endcode

    More advanced use cases can be handled using the \c addButton() and
    \c removeButton() methods.

    \sa RadioButton, {Button Controls}
*/

/*!
    \qmlsignal QtQuick.Controls::ButtonGroup::clicked(AbstractButton button)
    \since QtQuick.Controls 2.1

    This signal is emitted when a \a button in the group has been clicked.

    This signal is convenient for implementing a common signal handler for
    all buttons in the same group.

    \code
    ButtonGroup {
        buttons: column.children
        onClicked: console.log("clicked:", button.text)
    }

    Column {
        id: column
        Button { text: "First" }
        Button { text: "Second" }
        Button { text: "Third" }
    }
    \endcode

    \sa AbstractButton::clicked()
*/

class QQuickButtonGroupPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickButtonGroup)

public:
    QQuickButtonGroupPrivate() : checkedButton(nullptr) { }

    void clear();
    void buttonClicked();
    void _q_updateCurrent();

    static void buttons_append(QQmlListProperty<QQuickAbstractButton> *prop, QQuickAbstractButton *obj);
    static int buttons_count(QQmlListProperty<QQuickAbstractButton> *prop);
    static QQuickAbstractButton *buttons_at(QQmlListProperty<QQuickAbstractButton> *prop, int index);
    static void buttons_clear(QQmlListProperty<QQuickAbstractButton> *prop);

    QQuickAbstractButton *checkedButton;
    QVector<QQuickAbstractButton*> buttons;
};

void QQuickButtonGroupPrivate::clear()
{
    for (QQuickAbstractButton *button : qAsConst(buttons)) {
        QQuickAbstractButtonPrivate::get(button)->group = nullptr;
        QObjectPrivate::disconnect(button, &QQuickAbstractButton::clicked, this, &QQuickButtonGroupPrivate::buttonClicked);
        QObjectPrivate::disconnect(button, &QQuickAbstractButton::checkedChanged, this, &QQuickButtonGroupPrivate::_q_updateCurrent);
    }
    buttons.clear();
}

void QQuickButtonGroupPrivate::buttonClicked()
{
    Q_Q(QQuickButtonGroup);
    QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton*>(q->sender());
    if (button)
        emit q->clicked(button);
}

void QQuickButtonGroupPrivate::_q_updateCurrent()
{
    Q_Q(QQuickButtonGroup);
    QQuickAbstractButton *button = qobject_cast<QQuickAbstractButton*>(q->sender());
    if (button && button->isChecked())
        q->setCheckedButton(button);
    else if (!buttons.contains(checkedButton))
        q->setCheckedButton(nullptr);
}

void QQuickButtonGroupPrivate::buttons_append(QQmlListProperty<QQuickAbstractButton> *prop, QQuickAbstractButton *obj)
{
    QQuickButtonGroup *q = static_cast<QQuickButtonGroup *>(prop->object);
    q->addButton(obj);
}

int QQuickButtonGroupPrivate::buttons_count(QQmlListProperty<QQuickAbstractButton> *prop)
{
    QQuickButtonGroupPrivate *p = static_cast<QQuickButtonGroupPrivate *>(prop->data);
    return p->buttons.count();
}

QQuickAbstractButton *QQuickButtonGroupPrivate::buttons_at(QQmlListProperty<QQuickAbstractButton> *prop, int index)
{
    QQuickButtonGroupPrivate *p = static_cast<QQuickButtonGroupPrivate *>(prop->data);
    return p->buttons.value(index);
}

void QQuickButtonGroupPrivate::buttons_clear(QQmlListProperty<QQuickAbstractButton> *prop)
{
    QQuickButtonGroupPrivate *p = static_cast<QQuickButtonGroupPrivate *>(prop->data);
    if (!p->buttons.isEmpty()) {
        p->clear();
        QQuickButtonGroup *q = static_cast<QQuickButtonGroup *>(prop->object);
        // QTBUG-52358: don't clear the checked button immediately
        QMetaObject::invokeMethod(q, "_q_updateCurrent", Qt::QueuedConnection);
        emit q->buttonsChanged();
    }
}

QQuickButtonGroup::QQuickButtonGroup(QObject *parent)
    : QObject(*(new QQuickButtonGroupPrivate), parent)
{
}

QQuickButtonGroup::~QQuickButtonGroup()
{
    Q_D(QQuickButtonGroup);
    d->clear();
}

QQuickButtonGroupAttached *QQuickButtonGroup::qmlAttachedProperties(QObject *object)
{
    return new QQuickButtonGroupAttached(object);
}

/*!
    \qmlproperty AbstractButton QtQuick.Controls::ButtonGroup::checkedButton

    This property holds the currently selected button, or \c null if there is none.

    By default, it is the first checked button added to the button group.
*/
QQuickAbstractButton *QQuickButtonGroup::checkedButton() const
{
    Q_D(const QQuickButtonGroup);
    return d->checkedButton;
}

void QQuickButtonGroup::setCheckedButton(QQuickAbstractButton *checkedButton)
{
    Q_D(QQuickButtonGroup);
    if (d->checkedButton == checkedButton)
        return;

    if (d->checkedButton)
        d->checkedButton->setChecked(false);
    d->checkedButton = checkedButton;
    if (checkedButton)
        checkedButton->setChecked(true);
    emit checkedButtonChanged();
}

/*!
    \qmlproperty list<AbstractButton> QtQuick.Controls::ButtonGroup::buttons
    \default

    This property holds the list of buttons.

    \code
    ButtonGroup {
        buttons: column.children
    }

    Column {
        id: column

        RadioButton {
            checked: true
            text: qsTr("Option A")
        }

        RadioButton {
            text: qsTr("Option B")
        }
    }
    \endcode

    \sa group
*/
QQmlListProperty<QQuickAbstractButton> QQuickButtonGroup::buttons()
{
    Q_D(QQuickButtonGroup);
    return QQmlListProperty<QQuickAbstractButton>(this, d,
        QQuickButtonGroupPrivate::buttons_append,
        QQuickButtonGroupPrivate::buttons_count,
        QQuickButtonGroupPrivate::buttons_at,
        QQuickButtonGroupPrivate::buttons_clear);
}

/*!
    \qmlmethod void QtQuick.Controls::ButtonGroup::addButton(AbstractButton button)

    Adds a \a button to the button group.

    \note Manually adding objects to a button group is typically unnecessary.
          The \l buttons property and the \l group attached property provide a
          convenient and declarative syntax.

    \sa buttons, group
*/
void QQuickButtonGroup::addButton(QQuickAbstractButton *button)
{
    Q_D(QQuickButtonGroup);
    if (!button || d->buttons.contains(button))
        return;

    QQuickAbstractButtonPrivate::get(button)->group = this;
    QObjectPrivate::connect(button, &QQuickAbstractButton::clicked, d, &QQuickButtonGroupPrivate::buttonClicked);
    QObjectPrivate::connect(button, &QQuickAbstractButton::checkedChanged, d, &QQuickButtonGroupPrivate::_q_updateCurrent);

    if (button->isChecked())
        setCheckedButton(button);

    d->buttons.append(button);
    emit buttonsChanged();
}

/*!
    \qmlmethod void QtQuick.Controls::ButtonGroup::removeButton(AbstractButton button)

    Removes a \a button from the button group.

    \note Manually removing objects from a button group is typically unnecessary.
          The \l buttons property and the \l group attached property provide a
          convenient and declarative syntax.

    \sa buttons, group
*/
void QQuickButtonGroup::removeButton(QQuickAbstractButton *button)
{
    Q_D(QQuickButtonGroup);
    if (!button || !d->buttons.contains(button))
        return;

    QQuickAbstractButtonPrivate::get(button)->group = nullptr;
    QObjectPrivate::disconnect(button, &QQuickAbstractButton::clicked, d, &QQuickButtonGroupPrivate::buttonClicked);
    QObjectPrivate::disconnect(button, &QQuickAbstractButton::checkedChanged, d, &QQuickButtonGroupPrivate::_q_updateCurrent);

    if (d->checkedButton == button)
        setCheckedButton(nullptr);

    d->buttons.removeOne(button);
    emit buttonsChanged();
}

class QQuickButtonGroupAttachedPrivate : public QObjectPrivate
{
public:
    QQuickButtonGroupAttachedPrivate() : group(nullptr) { }

    QQuickButtonGroup *group;
};

QQuickButtonGroupAttached::QQuickButtonGroupAttached(QObject *parent) :
    QObject(*(new QQuickButtonGroupAttachedPrivate), parent)
{
}

/*!
    \qmlattachedproperty ButtonGroup QtQuick.Controls::ButtonGroup::group

    This property attaches a button to a button group.

    \code
    ButtonGroup { id: group }

    RadioButton {
        checked: true
        text: qsTr("Option A")
        ButtonGroup.group: group
    }

    RadioButton {
        text: qsTr("Option B")
        ButtonGroup.group: group
    }
    \endcode

    \sa buttons
*/
QQuickButtonGroup *QQuickButtonGroupAttached::group() const
{
    Q_D(const QQuickButtonGroupAttached);
    return d->group;
}

void QQuickButtonGroupAttached::setGroup(QQuickButtonGroup *group)
{
    Q_D(QQuickButtonGroupAttached);
    if (d->group == group)
        return;

    if (d->group)
        d->group->removeButton(qobject_cast<QQuickAbstractButton*>(parent()));
    d->group = group;
    if (group)
        group->addButton(qobject_cast<QQuickAbstractButton*>(parent()));
    emit groupChanged();
}

QT_END_NAMESPACE

#include "moc_qquickbuttongroup_p.cpp"
