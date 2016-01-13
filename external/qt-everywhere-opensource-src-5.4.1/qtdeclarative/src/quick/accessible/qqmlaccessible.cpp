/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qnamespace.h>
#include "qqmlaccessible_p.h"

#ifndef QT_NO_ACCESSIBILITY

QT_BEGIN_NAMESPACE

QQmlAccessible::QQmlAccessible(QObject *object)
    :QAccessibleObject(object)
{
}

void *QQmlAccessible::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::ActionInterface)
        return static_cast<QAccessibleActionInterface*>(this);
    return QAccessibleObject::interface_cast(t);
}

QQmlAccessible::~QQmlAccessible()
{
}

QAccessible::State QQmlAccessible::state() const
{
    QAccessible::State state;

    //QRect viewRect(QPoint(0, 0), m_implementation->size());
    //QRect itemRect(m_item->scenePos().toPoint(), m_item->boundingRect().size().toSize());

    QRect viewRect_ = viewRect();
    QRect itemRect = rect();

   // qDebug() << "viewRect" << viewRect << "itemRect" << itemRect;
    // error case:
    if (viewRect_.isNull() || itemRect.isNull()) {
        state.invisible = true;
    }

    if (!viewRect_.intersects(itemRect)) {
        state.offscreen = true;
        // state.invisible = true; // no set at this point to ease development
    }

    if (!object()->property("visible").toBool() || qFuzzyIsNull(object()->property("opacity").toDouble())) {
        state.invisible = true;
    }

    if ((role() == QAccessible::CheckBox || role() == QAccessible::RadioButton) && object()->property("checked").toBool()) {
        state.checked = true;
    }

    if (role() == QAccessible::EditableText)
        state.focusable = true;

    //qDebug() << "state?" << m_item->property("state").toString() << m_item->property("status").toString() << m_item->property("visible").toString();

    return state;
}

QStringList QQmlAccessible::actionNames() const
{
    QStringList actions;
    switch (role()) {
    case QAccessible::PushButton:
        actions << QAccessibleActionInterface::pressAction();
        break;
    case QAccessible::RadioButton:
    case QAccessible::CheckBox:
        actions << QAccessibleActionInterface::toggleAction()
                << QAccessibleActionInterface::pressAction();
        break;
    case QAccessible::Slider:
    case QAccessible::SpinBox:
    case QAccessible::ScrollBar:
        actions << QAccessibleActionInterface::increaseAction()
                << QAccessibleActionInterface::decreaseAction();
        break;
    default:
        break;
    }
    return actions;
}

void QQmlAccessible::doAction(const QString &actionName)
{
    // Look for and call the accessible[actionName]Action() function on the item.
    // This allows for overriding the default action handling.
    const QByteArray functionName = QByteArrayLiteral("accessible") + actionName.toLatin1() + QByteArrayLiteral("Action");
    if (object()->metaObject()->indexOfMethod(QByteArray(functionName + QByteArrayLiteral("()"))) != -1) {
        QMetaObject::invokeMethod(object(), functionName);
        return;
    }

    // Role-specific default action handling follows. Items are expected to provide
    // properties according to role conventions. These will then be read and/or updated
    // by the accessibility system.
    //   Checkable roles   : checked
    //   Value-based roles : (via the value interface: value, minimumValue, maximumValue), stepSize
    switch (role()) {
    case QAccessible::RadioButton:
    case QAccessible::CheckBox: {
        QVariant checked = object()->property("checked");
        if (checked.isValid()) {
            if (actionName == QAccessibleActionInterface::toggleAction() ||
                actionName == QAccessibleActionInterface::pressAction()) {

                object()->setProperty("checked",  QVariant(!checked.toBool()));
            }
        }
        break;
    }
    case QAccessible::Slider:
    case QAccessible::SpinBox:
    case QAccessible::Dial:
    case QAccessible::ScrollBar: {
        if (actionName != QAccessibleActionInterface::increaseAction() &&
            actionName != QAccessibleActionInterface::decreaseAction())
            break;

        // Update the value using QAccessibleValueInterface, respecting
        // the minimum and maximum value (if set). Also check for and
        // use the "stepSize" property on the item
        if (QAccessibleValueInterface *valueIface = valueInterface()) {
            QVariant valueV = valueIface->currentValue();
            qreal newValue = valueV.toReal();

            QVariant stepSizeV = object()->property("stepSize");
            qreal stepSize = stepSizeV.isValid() ? stepSizeV.toReal() : qreal(1.0);
            if (actionName == QAccessibleActionInterface::increaseAction()) {
                newValue += stepSize;
            } else {
                newValue -= stepSize;
            }

            QVariant minimumValueV = valueIface->minimumValue();
            if (minimumValueV.isValid()) {
                newValue = qMax(newValue, minimumValueV.toReal());
            }
            QVariant maximumValueV = valueIface->maximumValue();
            if (maximumValueV.isValid()) {
                newValue = qMin(newValue, maximumValueV.toReal());
            }

            valueIface->setCurrentValue(QVariant(newValue));
        }
        break;
    }
    default:
        break;
    }
}

QStringList QQmlAccessible::keyBindingsForAction(const QString &actionName) const
{
    Q_UNUSED(actionName)
    return QStringList();
}

QT_END_NAMESPACE

#endif // QT_NO_ACCESSIBILITY
