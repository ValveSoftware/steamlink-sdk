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

#include "qaccessiblequickitem_p.h"

#include <QtGui/qtextdocument.h>

#include "QtQuick/private/qquickitem_p.h"
#include "QtQuick/private/qquicktext_p.h"
#include "QtQuick/private/qquickaccessibleattached_p.h"
#include "QtQuick/qquicktextdocument.h"
QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

QAccessibleQuickItem::QAccessibleQuickItem(QQuickItem *item)
    : QAccessibleObject(item), m_doc(textDocument())
{
}

QWindow *QAccessibleQuickItem::window() const
{
    return item()->window();
}

int QAccessibleQuickItem::childCount() const
{
    return childItems().count();
}

QRect QAccessibleQuickItem::rect() const
{
    const QRect r = itemScreenRect(item());
    return r;
}

QRect QAccessibleQuickItem::viewRect() const
{
    // ### no window in some cases.
    if (!item()->window()) {
        return QRect();
    }

    QQuickWindow *window = item()->window();
    QPoint screenPos = window->mapToGlobal(QPoint(0,0));
    return QRect(screenPos, window->size());
}


bool QAccessibleQuickItem::clipsChildren() const
{
    return static_cast<QQuickItem *>(item())->clip();
}

QAccessibleInterface *QAccessibleQuickItem::childAt(int x, int y) const
{
    if (item()->clip()) {
        if (!rect().contains(x, y))
            return 0;
    }

    const QList<QQuickItem*> kids = accessibleUnignoredChildren(item(), true);
    for (int i = kids.count() - 1; i >= 0; --i) {
        QAccessibleInterface *childIface = QAccessible::queryAccessibleInterface(kids.at(i));
        if (QAccessibleInterface *childChild = childIface->childAt(x, y))
            return childChild;
        if (childIface && !childIface->state().invisible) {
            if (childIface->rect().contains(x, y))
                return childIface;
        }
    }

    return 0;
}

QAccessibleInterface *QAccessibleQuickItem::parent() const
{
    QQuickItem *parent = item()->parentItem();
    QQuickWindow *window = item()->window();
    QQuickItem *ci = window ? window->contentItem() : 0;
    while (parent && !QQuickItemPrivate::get(parent)->isAccessible && parent != ci)
        parent = parent->parentItem();

    if (parent) {
        if (parent == ci) {
            // Jump out to the scene widget if the parent is the root item.
            // There are two root items, QQuickWindow::rootItem and
            // QQuickView::declarativeRoot. The former is the true root item,
            // but is not a part of the accessibility tree. Check if we hit
            // it here and return an interface for the scene instead.
            return QAccessible::queryAccessibleInterface(window);
        } else {
            while (parent && !parent->d_func()->isAccessible)
                parent = parent->parentItem();
            return QAccessible::queryAccessibleInterface(parent);
        }
    }
    return 0;
}

QAccessibleInterface *QAccessibleQuickItem::child(int index) const
{
    QList<QQuickItem *> children = childItems();

    if (index < 0 || index >= children.count())
        return 0;

    QQuickItem *child = children.at(index);
    if (!child) // FIXME can this happen?
        return 0;

    return QAccessible::queryAccessibleInterface(child);
}

int QAccessibleQuickItem::indexOfChild(const QAccessibleInterface *iface) const
{
    QList<QQuickItem*> kids = childItems();
    return kids.indexOf(static_cast<QQuickItem*>(iface->object()));
}

static void unignoredChildren(QQuickItem *item, QList<QQuickItem *> *items, bool paintOrder)
{
    const QList<QQuickItem*> childItems = paintOrder ? QQuickItemPrivate::get(item)->paintOrderChildItems()
                                               : item->childItems();
    for (QQuickItem *child : childItems) {
        if (QQuickItemPrivate::get(child)->isAccessible) {
            items->append(child);
        } else {
            unignoredChildren(child, items, paintOrder);
        }
    }
}

QList<QQuickItem *> accessibleUnignoredChildren(QQuickItem *item, bool paintOrder)
{
    QList<QQuickItem *> items;
    unignoredChildren(item, &items, paintOrder);
    return items;
}

QList<QQuickItem *> QAccessibleQuickItem::childItems() const
{
    return accessibleUnignoredChildren(item());
}

QAccessible::State QAccessibleQuickItem::state() const
{
    QQuickAccessibleAttached *attached = QQuickAccessibleAttached::attachedProperties(item());
    if (!attached)
        return QAccessible::State();

    QAccessible::State state = attached->state();

    QRect viewRect_ = viewRect();
    QRect itemRect = rect();

    if (viewRect_.isNull() || itemRect.isNull() || !item()->window() || !item()->window()->isVisible() ||!item()->isVisible() || qFuzzyIsNull(item()->opacity()))
        state.invisible = true;
    if (!viewRect_.intersects(itemRect))
        state.offscreen = true;
    if ((role() == QAccessible::CheckBox || role() == QAccessible::RadioButton) && object()->property("checked").toBool())
        state.checked = true;
    if (item()->activeFocusOnTab() || role() == QAccessible::EditableText)
        state.focusable = true;
    if (item()->hasActiveFocus())
        state.focused = true;
    return state;
}

QAccessible::Role QAccessibleQuickItem::role() const
{
    // Workaround for setAccessibleRole() not working for
    // Text items. Text items are special since they are defined
    // entirely from C++ (setting the role from QML works.)
    if (qobject_cast<QQuickText*>(const_cast<QQuickItem *>(item())))
        return QAccessible::StaticText;

    QAccessible::Role role = QAccessible::NoRole;
    if (item())
        role = QQuickItemPrivate::get(item())->accessibleRole();
    if (role == QAccessible::NoRole)
        role = QAccessible::Client;

    return role;
}

bool QAccessibleQuickItem::isAccessible() const
{
    return item()->d_func()->isAccessible;
}

QStringList QAccessibleQuickItem::actionNames() const
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
    if (state().focusable)
        actions.append(QAccessibleActionInterface::setFocusAction());

    // ### The following can lead to duplicate action names.
    if (QQuickAccessibleAttached *attached = QQuickAccessibleAttached::attachedProperties(item()))
        attached->availableActions(&actions);
    return actions;
}

void QAccessibleQuickItem::doAction(const QString &actionName)
{
    bool accepted = false;
    if (actionName == QAccessibleActionInterface::setFocusAction()) {
        item()->forceActiveFocus();
        accepted = true;
    }
    if (QQuickAccessibleAttached *attached = QQuickAccessibleAttached::attachedProperties(item()))
        accepted = attached->doAction(actionName);

    if (accepted)
        return;
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

QStringList QAccessibleQuickItem::keyBindingsForAction(const QString &actionName) const
{
    Q_UNUSED(actionName)
    return QStringList();
}

QString QAccessibleQuickItem::text(QAccessible::Text textType) const
{
    // handles generic behavior not specific to an item
    switch (textType) {
    case QAccessible::Name: {
        QVariant accessibleName = QQuickAccessibleAttached::property(object(), "name");
        if (!accessibleName.isNull())
            return accessibleName.toString();
        break;}
    case QAccessible::Description: {
        QVariant accessibleDecription = QQuickAccessibleAttached::property(object(), "description");
        if (!accessibleDecription.isNull())
            return accessibleDecription.toString();
        break;}
#ifdef Q_ACCESSIBLE_QUICK_ITEM_ENABLE_DEBUG_DESCRIPTION
    case QAccessible::DebugDescription: {
        QString debugString;
        debugString = QString::fromLatin1(object()->metaObject()->className()) + QLatin1Char(' ');
        debugString += isAccessible() ? QLatin1String("enabled") : QLatin1String("disabled");
        return debugString;
        break; }
#endif
    case QAccessible::Value:
    case QAccessible::Help:
    case QAccessible::Accelerator:
    default:
        break;
    }

    // the following block handles item-specific behavior
    if (role() == QAccessible::EditableText) {
        if (textType == QAccessible::Value) {
            if (QTextDocument *doc = textDocument()) {
                return doc->toPlainText();
            }
            QVariant text = object()->property("text");
            return text.toString();
        }
    }

    return QString();
}

void *QAccessibleQuickItem::interface_cast(QAccessible::InterfaceType t)
{
    QAccessible::Role r = role();
    if (t == QAccessible::ActionInterface)
        return static_cast<QAccessibleActionInterface*>(this);
    if (t == QAccessible::ValueInterface &&
           (r == QAccessible::Slider ||
            r == QAccessible::SpinBox ||
            r == QAccessible::Dial ||
            r == QAccessible::ScrollBar))
       return static_cast<QAccessibleValueInterface*>(this);

    if (t == QAccessible::TextInterface &&
            (r == QAccessible::EditableText))
        return static_cast<QAccessibleTextInterface*>(this);

    return QAccessibleObject::interface_cast(t);
}

QVariant QAccessibleQuickItem::currentValue() const
{
    return item()->property("value");
}

void QAccessibleQuickItem::setCurrentValue(const QVariant &value)
{
    item()->setProperty("value", value);
}

QVariant QAccessibleQuickItem::maximumValue() const
{
    return item()->property("maximumValue");
}

QVariant QAccessibleQuickItem::minimumValue() const
{
    return item()->property("minimumValue");
}

QVariant QAccessibleQuickItem::minimumStepSize() const
{
    return item()->property("stepSize");
}

/*!
  \internal
  Shared between QAccessibleQuickItem and QAccessibleQuickView
*/
QRect itemScreenRect(QQuickItem *item)
{
    // ### no window in some cases.
    // ### Should we really check for 0 opacity?
    if (!item->window() ||!item->isVisible() || qFuzzyIsNull(item->opacity())) {
        return QRect();
    }

    QSize itemSize((int)item->width(), (int)item->height());
    // ### If the bounding rect fails, we first try the implicit size, then we go for the
    // parent size. WE MIGHT HAVE TO REVISIT THESE FALLBACKS.
    if (itemSize.isEmpty()) {
        itemSize = QSize((int)item->implicitWidth(), (int)item->implicitHeight());
        if (itemSize.isEmpty() && item->parentItem())
            // ### Seems that the above fallback is not enough, fallback to use the parent size...
            itemSize = QSize((int)item->parentItem()->width(), (int)item->parentItem()->height());
    }

    QPointF scenePoint = item->mapToScene(QPointF(0, 0));
    QPoint screenPos = item->window()->mapToGlobal(scenePoint.toPoint());
    return QRect(screenPos, itemSize);
}

QTextDocument *QAccessibleQuickItem::textDocument() const
{
    QVariant docVariant = item()->property("textDocument");
    if (docVariant.canConvert<QQuickTextDocument*>()) {
        QQuickTextDocument *qqdoc = docVariant.value<QQuickTextDocument*>();
        return qqdoc->textDocument();
    }
    return 0;
}

int QAccessibleQuickItem::characterCount() const
{
    if (m_doc) {
        QTextCursor cursor = QTextCursor(m_doc);
        cursor.movePosition(QTextCursor::End);
        return cursor.position();
    }
    return text(QAccessible::Value).size();
}

int QAccessibleQuickItem::cursorPosition() const
{
    QVariant pos = item()->property("cursorPosition");
    return pos.toInt();
}

void QAccessibleQuickItem::setCursorPosition(int position)
{
    item()->setProperty("cursorPosition", position);
}

QString QAccessibleQuickItem::text(int startOffset, int endOffset) const
{
    if (m_doc) {
        QTextCursor cursor = QTextCursor(m_doc);
        cursor.setPosition(startOffset);
        cursor.setPosition(endOffset, QTextCursor::KeepAnchor);
        return cursor.selectedText();
    }
    return text(QAccessible::Value).mid(startOffset, endOffset - startOffset);
}

QString QAccessibleQuickItem::textBeforeOffset(int offset, QAccessible::TextBoundaryType boundaryType,
                                 int *startOffset, int *endOffset) const
{
    Q_ASSERT(startOffset);
    Q_ASSERT(endOffset);

    if (m_doc) {
        QTextCursor cursor = QTextCursor(m_doc);
        cursor.setPosition(offset);
        QPair<int, int> boundaries = QAccessible::qAccessibleTextBoundaryHelper(cursor, boundaryType);
        cursor.setPosition(boundaries.first - 1);
        boundaries = QAccessible::qAccessibleTextBoundaryHelper(cursor, boundaryType);

        *startOffset = boundaries.first;
        *endOffset = boundaries.second;

        return text(boundaries.first, boundaries.second);
    } else {
        return QAccessibleTextInterface::textBeforeOffset(offset, boundaryType, startOffset, endOffset);
    }
}

QString QAccessibleQuickItem::textAfterOffset(int offset, QAccessible::TextBoundaryType boundaryType,
                                int *startOffset, int *endOffset) const
{
    Q_ASSERT(startOffset);
    Q_ASSERT(endOffset);

    if (m_doc) {
        QTextCursor cursor = QTextCursor(m_doc);
        cursor.setPosition(offset);
        QPair<int, int> boundaries = QAccessible::qAccessibleTextBoundaryHelper(cursor, boundaryType);
        cursor.setPosition(boundaries.second);
        boundaries = QAccessible::qAccessibleTextBoundaryHelper(cursor, boundaryType);

        *startOffset = boundaries.first;
        *endOffset = boundaries.second;

        return text(boundaries.first, boundaries.second);
    } else {
        return QAccessibleTextInterface::textAfterOffset(offset, boundaryType, startOffset, endOffset);
    }
}

QString QAccessibleQuickItem::textAtOffset(int offset, QAccessible::TextBoundaryType boundaryType,
                             int *startOffset, int *endOffset) const
{
    Q_ASSERT(startOffset);
    Q_ASSERT(endOffset);

    if (m_doc) {
        QTextCursor cursor = QTextCursor(m_doc);
        cursor.setPosition(offset);
        QPair<int, int> boundaries = QAccessible::qAccessibleTextBoundaryHelper(cursor, boundaryType);

        *startOffset = boundaries.first;
        *endOffset = boundaries.second;
        return text(boundaries.first, boundaries.second);
    } else {
        return QAccessibleTextInterface::textAtOffset(offset, boundaryType, startOffset, endOffset);
    }
}

void QAccessibleQuickItem::selection(int selectionIndex, int *startOffset, int *endOffset) const
{
    if (selectionIndex == 0) {
        *startOffset = item()->property("selectionStart").toInt();
        *endOffset = item()->property("selectionEnd").toInt();
    } else {
        *startOffset = 0;
        *endOffset = 0;
    }
}

int QAccessibleQuickItem::selectionCount() const
{
    if (item()->property("selectionStart").toInt() != item()->property("selectionEnd").toInt())
        return 1;
    return 0;
}

void QAccessibleQuickItem::addSelection(int /* startOffset */, int /* endOffset */)
{

}
void QAccessibleQuickItem::removeSelection(int /* selectionIndex */)
{

}
void QAccessibleQuickItem::setSelection(int /* selectionIndex */, int /* startOffset */, int /* endOffset */)
{

}


#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE
