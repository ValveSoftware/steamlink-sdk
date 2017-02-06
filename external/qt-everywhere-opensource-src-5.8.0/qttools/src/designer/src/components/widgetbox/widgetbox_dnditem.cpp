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

#include "widgetbox_dnditem.h"

#include <widgetfactory_p.h>
#include <spacer_widget_p.h>
#include <qdesigner_formbuilder_p.h>
#include <qtresourcemodel_p.h>
#include <formwindowbase_p.h>
#include <qdesigner_utils_p.h>
#include <qdesigner_dockwidget_p.h>
#include <qsimpleresource_p.h>

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerFormWindowManagerInterface>

#include <QtDesigner/private/ui4_p.h>

#include <QtWidgets/QStyle>
#include <QtWidgets/QApplication>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {
/*******************************************************************************
** WidgetBoxResource
*/

static inline DeviceProfile currentDeviceProfile(const QDesignerFormEditorInterface *core)
{
    if (QDesignerFormWindowInterface *cfw = core->formWindowManager()->activeFormWindow())
        if (const FormWindowBase *fwb = qobject_cast<const FormWindowBase *>(cfw))
            return fwb->deviceProfile();
    return DeviceProfile();
}

class WidgetBoxResource : public QDesignerFormBuilder
{
public:
    WidgetBoxResource(QDesignerFormEditorInterface *core);

    // protected->public
   QWidget *createUI(DomUI *ui, QWidget *parents) { return QDesignerFormBuilder::create(ui, parents); }

protected:

    QWidget *create(DomWidget *ui_widget, QWidget *parents) Q_DECL_OVERRIDE;
    QWidget *createWidget(const QString &widgetName, QWidget *parentWidget, const QString &name) Q_DECL_OVERRIDE;
    void createCustomWidgets(DomCustomWidgets *) Q_DECL_OVERRIDE;
};

WidgetBoxResource::WidgetBoxResource(QDesignerFormEditorInterface *core) :
    QDesignerFormBuilder(core, currentDeviceProfile(core))
{
}


QWidget *WidgetBoxResource::createWidget(const QString &widgetName, QWidget *parentWidget, const QString &name)
{
    if (widgetName == QStringLiteral("Spacer")) {
        Spacer *spacer = new Spacer(parentWidget);
        spacer->setObjectName(name);
        return spacer;
    }

    return QDesignerFormBuilder::createWidget(widgetName, parentWidget, name);
}

QWidget *WidgetBoxResource::create(DomWidget *ui_widget, QWidget *parent)
{
    QWidget *result = QDesignerFormBuilder::create(ui_widget, parent);
    // It is possible to have a syntax error or something in custom
    // widget XML, so, try to recover here by creating an artificial
    // top level + widget.
    if (!result) {
        const QString msg = QApplication::translate("qdesigner_internal::WidgetBox", "Warning: Widget creation failed in the widget box. This could be caused by invalid custom widget XML.");
        qdesigner_internal::designerWarning(msg);
        result = new QWidget(parent);
        new QWidget(result);
    }
    result->setFocusPolicy(Qt::NoFocus);
    result->setObjectName(ui_widget->attributeName());
    return result;
}

void WidgetBoxResource::createCustomWidgets(DomCustomWidgets *dc)
{
    // Make a promotion entry in  case someone has a promoted widget
    // in the scratchpad.
    QSimpleResource::handleDomCustomWidgets(core(), dc);

}

/*******************************************************************************
** WidgetBoxResource
*/

static QSize geometryProp(const DomWidget *dw)
{
    const QList<DomProperty*> prop_list = dw->elementProperty();
    const QString geometry = QStringLiteral("geometry");
    foreach (DomProperty *prop, prop_list) {
        if (prop->attributeName() !=  geometry)
            continue;
        DomRect *dr = prop->elementRect();
        if (dr == 0)
            continue;
        return QSize(dr->elementWidth(), dr->elementHeight());
    }
    return QSize();
}

static QSize domWidgetSize(const DomWidget *dw)
{
    QSize size = geometryProp(dw);
    if (size.isValid())
        return size;

    foreach (const DomWidget *child, dw->elementWidget()) {
        size = geometryProp(child);
        if (size.isValid())
            return size;
    }

    foreach (const DomLayout *dl, dw->elementLayout()) {
        foreach (DomLayoutItem *item, dl->elementItem()) {
            const DomWidget *child = item->elementWidget();
            if (child == 0)
                continue;
            size = geometryProp(child);
            if (size.isValid())
                return size;
        }
    }

    return QSize();
}

static QWidget *decorationFromDomWidget(DomUI *dom_ui, QDesignerFormEditorInterface *core)
{
    WidgetBoxResource builder(core);
    // We have the builder create the articial QWidget fake top level as a tooltip
    // because the size algorithm works better at weird DPI settings
    // if the actual widget is created as a child of a container
    QWidget *fakeTopLevel = builder.createUI(dom_ui, static_cast<QWidget*>(0));
    fakeTopLevel->setParent(0, Qt::ToolTip); // Container
    // Actual widget
    const DomWidget *domW = dom_ui->elementWidget()->elementWidget().front();
    QWidget *w = fakeTopLevel->findChildren<QWidget*>().front();
    Q_ASSERT(w);
    // hack begin;
    // We set _q_dockDrag dynamic property which will be detected in drag enter event of form window.
    // Dock drop is handled in special way (highlight goes to central widget of main window)
    if (qobject_cast<QDesignerDockWidget *>(w))
        fakeTopLevel->setProperty("_q_dockDrag", QVariant(true));
    // hack end;
    w->setAutoFillBackground(true); // Different style for embedded
    QSize size = domWidgetSize(domW);
    const QSize minimumSize = w->minimumSizeHint();
    if (!size.isValid())
        size = w->sizeHint();
    if (size.width() < minimumSize.width())
        size.setWidth(minimumSize.width());
    if (size.height() < minimumSize.height())
        size.setHeight(minimumSize.height());
    // A QWidget might have size -1,-1 if no geometry property is specified in the widget box.
    if (size.isEmpty())
        size = size.expandedTo(QSize(16, 16));
    w->setGeometry(QRect(QPoint(0, 0), size));
    fakeTopLevel->resize(size);
    return fakeTopLevel;
}

WidgetBoxDnDItem::WidgetBoxDnDItem(QDesignerFormEditorInterface *core,
                                   DomUI *dom_ui,
                                   const QPoint &global_mouse_pos) :
    QDesignerDnDItem(CopyDrop)
{
    QWidget *decoration = decorationFromDomWidget(dom_ui, core);
    decoration->move(global_mouse_pos - QPoint(5, 5));

    init(dom_ui, 0, decoration, global_mouse_pos);
}
}

QT_END_NAMESPACE
