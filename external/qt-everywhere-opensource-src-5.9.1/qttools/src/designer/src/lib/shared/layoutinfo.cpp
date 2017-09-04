/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "layoutinfo_p.h"

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerContainerExtension>
#include <QtDesigner/QDesignerMetaDataBaseInterface>
#include <QtDesigner/QExtensionManager>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QSplitter>
#include <QtCore/QDebug>
#include <QtCore/QHash>
#include <QtCore/QRect>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {
/*!
  \overload
*/
LayoutInfo::Type LayoutInfo::layoutType(const QDesignerFormEditorInterface *core, const QLayout *layout)
{
    Q_UNUSED(core)
    if (!layout)
        return NoLayout;
    else if (qobject_cast<const QHBoxLayout*>(layout))
        return HBox;
    else if (qobject_cast<const QVBoxLayout*>(layout))
        return VBox;
    else if (qobject_cast<const QGridLayout*>(layout))
        return Grid;
    else if (qobject_cast<const QFormLayout*>(layout))
       return Form;
    return UnknownLayout;
}

static const QHash<QString, LayoutInfo::Type> &layoutNameTypeMap()
{
    static QHash<QString, LayoutInfo::Type> nameTypeMap;
    if (nameTypeMap.empty()) {
        nameTypeMap.insert(QStringLiteral("QVBoxLayout"), LayoutInfo::VBox);
        nameTypeMap.insert(QStringLiteral("QHBoxLayout"), LayoutInfo::HBox);
        nameTypeMap.insert(QStringLiteral("QGridLayout"), LayoutInfo::Grid);
        nameTypeMap.insert(QStringLiteral("QFormLayout"), LayoutInfo::Form);
    }
    return nameTypeMap;
}

LayoutInfo::Type LayoutInfo::layoutType(const QString &typeName)
{
    return layoutNameTypeMap().value(typeName, NoLayout);
}

QString LayoutInfo::layoutName(Type t)
{
    return layoutNameTypeMap().key(t);
}

/*!
  \overload
*/
LayoutInfo::Type LayoutInfo::layoutType(const QDesignerFormEditorInterface *core, const QWidget *w)
{
    if (const QSplitter *splitter = qobject_cast<const QSplitter *>(w))
        return  splitter->orientation() == Qt::Horizontal ? HSplitter : VSplitter;
    return layoutType(core, w->layout());
}

LayoutInfo::Type LayoutInfo::managedLayoutType(const QDesignerFormEditorInterface *core,
                                               const QWidget *w,
                                               QLayout **ptrToLayout)
{
    if (ptrToLayout)
        *ptrToLayout = 0;
    if (const QSplitter *splitter = qobject_cast<const QSplitter *>(w))
        return  splitter->orientation() == Qt::Horizontal ? HSplitter : VSplitter;
    QLayout *layout = managedLayout(core, w);
    if (!layout)
        return NoLayout;
    if (ptrToLayout)
        *ptrToLayout = layout;
    return layoutType(core, layout);
}

QWidget *LayoutInfo::layoutParent(const QDesignerFormEditorInterface *core, QLayout *layout)
{
    Q_UNUSED(core)

    QObject *o = layout;
    while (o) {
        if (QWidget *widget = qobject_cast<QWidget*>(o))
            return widget;

        o = o->parent();
    }
    return 0;
}

void LayoutInfo::deleteLayout(const QDesignerFormEditorInterface *core, QWidget *widget)
{
    if (QDesignerContainerExtension *container = qt_extension<QDesignerContainerExtension*>(core->extensionManager(), widget))
        widget = container->widget(container->currentIndex());

    Q_ASSERT(widget != 0);

    QLayout *layout = managedLayout(core, widget);

    if (layout == 0 || core->metaDataBase()->item(layout) != 0) {
        delete layout;
        widget->updateGeometry();
        return;
    }

    qDebug() << "trying to delete an unmanaged layout:" << "widget:" << widget << "layout:" << layout;
}

LayoutInfo::Type LayoutInfo::laidoutWidgetType(const QDesignerFormEditorInterface *core,
                                               QWidget *widget,
                                               bool *isManaged,
                                               QLayout **ptrToLayout)
{
    if (isManaged)
        *isManaged = false;
    if (ptrToLayout)
        *ptrToLayout = 0;

    QWidget *parent = widget->parentWidget();
    if (!parent)
        return NoLayout;

    // 1) Splitter
    if (QSplitter *splitter  = qobject_cast<QSplitter*>(parent)) {
        if (isManaged)
            *isManaged = core->metaDataBase()->item(splitter);
        return  splitter->orientation() == Qt::Horizontal ? HSplitter : VSplitter;
    }

    // 2) Layout of parent
    QLayout *parentLayout = parent->layout();
    if (!parentLayout)
        return NoLayout;

    if (parentLayout->indexOf(widget) != -1) {
        if (isManaged)
            *isManaged = core->metaDataBase()->item(parentLayout);
        if (ptrToLayout)
            *ptrToLayout = parentLayout;
        return layoutType(core, parentLayout);
    }

    // 3) Some child layout (see below comment about Q3GroupBox)
    const QList<QLayout*> childLayouts = parentLayout->findChildren<QLayout*>();
    if (childLayouts.empty())
        return NoLayout;
    const QList<QLayout*>::const_iterator lcend = childLayouts.constEnd();
    for (QList<QLayout*>::const_iterator it = childLayouts.constBegin(); it != lcend; ++it) {
        QLayout *layout = *it;
        if (layout->indexOf(widget) != -1) {
            if (isManaged)
                *isManaged = core->metaDataBase()->item(layout);
            if (ptrToLayout)
                *ptrToLayout = layout;
            return layoutType(core, layout);
        }
    }

    return NoLayout;
}

QLayout *LayoutInfo::internalLayout(const QWidget *widget)
{
    return widget->layout();
}


QLayout *LayoutInfo::managedLayout(const QDesignerFormEditorInterface *core, const QWidget *widget)
{
    if (widget == 0)
        return 0;

    QLayout *layout = widget->layout();
    if (!layout)
        return 0;

    return managedLayout(core, layout);
}

QLayout *LayoutInfo::managedLayout(const QDesignerFormEditorInterface *core, QLayout *layout)
{
    QDesignerMetaDataBaseInterface *metaDataBase = core->metaDataBase();

    if (!metaDataBase)
        return layout;
    /* This code exists mainly for the Q3GroupBox class, for which
     * widget->layout() returns an internal VBoxLayout. */
    const QDesignerMetaDataBaseItemInterface *item = metaDataBase->item(layout);
    if (item == 0) {
        layout = layout->findChild<QLayout*>();
        item = metaDataBase->item(layout);
    }
    if (!item)
        return 0;
    return layout;
}

// Is it a a dummy grid placeholder created by Designer?
bool LayoutInfo::isEmptyItem(QLayoutItem *item)
{
    if (item == 0) {
        qDebug() << "** WARNING Zero-item passed on to isEmptyItem(). This indicates a layout inconsistency.";
        return true;
    }
    return item->spacerItem() != 0;
}

QDESIGNER_SHARED_EXPORT void getFormLayoutItemPosition(const QFormLayout *formLayout, int index, int *rowPtr, int *columnPtr, int *rowspanPtr, int *colspanPtr)
{
    int row;
    QFormLayout::ItemRole role;
    formLayout->getItemPosition(index, &row, &role);
    const int columnspan = role == QFormLayout::SpanningRole ? 2 : 1;
    const int column = (columnspan > 1 || role == QFormLayout::LabelRole) ? 0 : 1;
    if (rowPtr)
        *rowPtr = row;
    if (columnPtr)
        *columnPtr = column;
    if (rowspanPtr)
        *rowspanPtr = 1;
    if (colspanPtr)
        *colspanPtr = columnspan;
}

static inline QFormLayout::ItemRole formLayoutRole(int column, int colspan)
{
    if (colspan > 1)
        return QFormLayout::SpanningRole;
    return column == 0 ? QFormLayout::LabelRole : QFormLayout::FieldRole;
}

QDESIGNER_SHARED_EXPORT void formLayoutAddWidget(QFormLayout *formLayout, QWidget *w, const QRect &r, bool insert)
{
    // Consistent API galore...
    if (insert) {
        const bool spanning = r.width() > 1;
        if (spanning) {
            formLayout->insertRow(r.y(), w);
        } else {
            QWidget *label = 0, *field = 0;
            if (r.x() == 0) {
                label = w;
            } else {
                field = w;
            }
            formLayout->insertRow(r.y(), label, field);
        }
    } else {
        formLayout->setWidget(r.y(), formLayoutRole(r.x(), r.width()), w);
    }
}

} // namespace qdesigner_internal

QT_END_NAMESPACE
