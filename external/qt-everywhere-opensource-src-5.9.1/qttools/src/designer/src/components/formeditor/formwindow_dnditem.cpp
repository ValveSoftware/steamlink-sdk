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

#include "formwindow_dnditem.h"
#include "formwindow.h"

#include <private/ui4_p.h>
#include <qdesigner_resource.h>
#include <qtresourcemodel_p.h>

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/private/ui4_p.h>

#include <QtWidgets/QLabel>
#include <QtGui/QPixmap>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

static QWidget *decorationFromWidget(QWidget *w)
{
    QLabel *label = new QLabel(0, Qt::ToolTip);
    QPixmap pm = w->grab(QRect(0, 0, -1, -1));
    label->setPixmap(pm);
    label->resize((QSizeF(pm.size()) / pm.devicePixelRatio()).toSize());

    return label;
}

static DomUI *widgetToDom(QWidget *widget, FormWindow *form)
{
    QDesignerResource builder(form);
    builder.setSaveRelative(false);
    return builder.copy(FormBuilderClipboard(widget));
}

FormWindowDnDItem::FormWindowDnDItem(QDesignerDnDItemInterface::DropType type, FormWindow *form,
                                        QWidget *widget, const QPoint &global_mouse_pos)
    : QDesignerDnDItem(type, form)
{
    QWidget *decoration = decorationFromWidget(widget);
    QPoint pos = widget->mapToGlobal(QPoint(0, 0));
    decoration->move(pos);

    init(0, widget, decoration, global_mouse_pos);
}

DomUI *FormWindowDnDItem::domUi() const
{
    DomUI *result = QDesignerDnDItem::domUi();
    if (result != 0)
        return result;
    FormWindow *form = qobject_cast<FormWindow*>(source());
    if (widget() == 0 || form == 0)
        return 0;

    QtResourceModel *resourceModel = form->core()->resourceModel();
    QtResourceSet *currentResourceSet = resourceModel->currentResourceSet();
    /* Short:
     *   We need to activate the original resourceSet associated with a form
     *   to properly generate the dom resource includes.
     * Long:
     *   widgetToDom() calls copy() on QDesignerResource. It generates the
     *   Dom structure. In order to create DomResources properly we need to
     *   have the associated ResourceSet active (QDesignerResource::saveResources()
     *   queries the resource model for a qrc path for the given resource file:
     *      qrcFile = m_core->resourceModel()->qrcPath(ri->text());
     *   This works only when the resource file comes from the active
     *   resourceSet */
    resourceModel->setCurrentResourceSet(form->resourceSet());

    result = widgetToDom(widget(), form);
    const_cast<FormWindowDnDItem*>(this)->setDomUi(result);
    resourceModel->setCurrentResourceSet(currentResourceSet);
    return result;
}

QT_END_NAMESPACE
