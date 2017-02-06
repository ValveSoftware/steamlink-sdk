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

#include "qquickcheckdelegate_p.h"
#include "qquickitemdelegate_p_p.h"

#include <QtGui/qpa/qplatformtheme.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype CheckDelegate
    \inherits ItemDelegate
    \instantiates QQuickCheckDelegate
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-delegates
    \brief Item delegate with a check indicator that can be toggled on or off.

    \image qtquickcontrols2-checkdelegate.gif

    CheckDelegate presents an item delegate that can be toggled on (checked) or
    off (unchecked). Check delegates are typically used to select one or more
    options from a set of options in a list. For smaller sets of options, or
    for options that need to be uniquely identifiable, consider using
    \l CheckBox instead.

    CheckDelegate inherits its API from \l ItemDelegate, which is inherited
    from AbstractButton. For instance, you can set \l {AbstractButton::text}{text},
    and react to \l {AbstractButton::clicked}{clicks} using the AbstractButton
    API. The state of the check delegate can be set with the
    \l {AbstractButton::}{checked} property.

    In addition to the checked and unchecked states, there is a third state:
    partially checked. The partially checked state can be enabled using the
    \l tristate property. This state indicates that the regular checked/unchecked
    state can not be determined; generally because of other states that affect
    the check delegate. This state is useful when several child nodes are selected
    in a treeview, for example.

    \code
    ListView {
        model: ["Option 1", "Option 2", "Option 3"]
        delegate: CheckDelegate {
            text: modelData
        }
    }
    \endcode

    \sa {Customizing CheckDelegate}, {Delegate Controls}, CheckBox
*/

class QQuickCheckDelegatePrivate : public QQuickItemDelegatePrivate
{
    Q_DECLARE_PUBLIC(QQuickCheckDelegate)

public:
    QQuickCheckDelegatePrivate()
        : tristate(false), checkState(Qt::Unchecked)
    {
    }

    bool tristate;
    Qt::CheckState checkState;
};

QQuickCheckDelegate::QQuickCheckDelegate(QQuickItem *parent) :
    QQuickItemDelegate(*(new QQuickCheckDelegatePrivate), parent)
{
    setCheckable(true);
}

/*!
    \qmlproperty bool QtQuick.Controls::CheckDelegate::tristate

    This property determines whether the check delegate has three states.

    In the animation below, the first checkdelegate is tri-state:

    \image qtquickcontrols2-checkdelegate-tristate.gif

    The default is \c false, i.e., the delegate has only two states.
*/
bool QQuickCheckDelegate::isTristate() const
{
    Q_D(const QQuickCheckDelegate);
    return d->tristate;
}

void QQuickCheckDelegate::setTristate(bool tristate)
{
    Q_D(QQuickCheckDelegate);
    if (d->tristate == tristate)
        return;

    d->tristate = tristate;
    emit tristateChanged();
}

/*!
    \qmlproperty enumeration QtQuick.Controls::CheckDelegate::checkState

    This property determines the check state of the check delegate.

    Available states:
    \value Qt.Unchecked The delegate is unchecked.
    \value Qt.PartiallyChecked The delegate is partially checked. This state is only used when \l tristate is enabled.
    \value Qt.Checked The delegate is checked.

    \sa tristate, {AbstractButton::checked}{checked}
*/
Qt::CheckState QQuickCheckDelegate::checkState() const
{
    Q_D(const QQuickCheckDelegate);
    return d->checkState;
}

void QQuickCheckDelegate::setCheckState(Qt::CheckState state)
{
    Q_D(QQuickCheckDelegate);
    if (d->checkState == state)
        return;

    if (!d->tristate && state == Qt::PartiallyChecked)
        setTristate(true);

    bool wasChecked = isChecked();
    d->checked = state != Qt::Unchecked;
    d->checkState = state;
    emit checkStateChanged();
    if (d->checked != wasChecked)
        emit checkedChanged();
}

QFont QQuickCheckDelegate::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::ListViewFont);
}

void QQuickCheckDelegate::checkStateSet()
{
    setCheckState(isChecked() ? Qt::Checked : Qt::Unchecked);
}

void QQuickCheckDelegate::nextCheckState()
{
    Q_D(QQuickCheckDelegate);
    if (d->tristate)
        setCheckState(static_cast<Qt::CheckState>((d->checkState + 1) % 3));
    else
        QQuickItemDelegate::nextCheckState();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickCheckDelegate::accessibleRole() const
{
    return QAccessible::CheckBox;
}
#endif

QT_END_NAMESPACE
