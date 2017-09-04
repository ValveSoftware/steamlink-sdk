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

#include "qaxwidgetplugin.h"
#include "qaxwidgetextrainfo.h"
#include "qdesigneraxwidget.h"
#include "qaxwidgetpropertysheet.h"
#include "qaxwidgettaskmenu.h"

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerFormWindowInterface>

#include <QtCore/qplugin.h>
#include <QtGui/QIcon>
#include <ActiveQt/QAxWidget>

QT_BEGIN_NAMESPACE

QAxWidgetPlugin::QAxWidgetPlugin(QObject *parent) :
    QObject(parent),
    m_core(0)
{
}

QString QAxWidgetPlugin::name() const
{
    return QStringLiteral("QAxWidget");
}

QString QAxWidgetPlugin::group() const
{
    return QStringLiteral("Containers");
}

QString QAxWidgetPlugin::toolTip() const
{
    return tr("ActiveX control");
}

QString QAxWidgetPlugin::whatsThis() const
{
    return tr("ActiveX control widget");
}

QString QAxWidgetPlugin::includeFile() const
{
    return QStringLiteral("qaxwidget.h");
}

QIcon QAxWidgetPlugin::icon() const
{
    return QIcon(QDesignerAxWidget::widgetIcon());
}

bool QAxWidgetPlugin::isContainer() const
{
    return false;
}

QWidget *QAxWidgetPlugin::createWidget(QWidget *parent)
{
    // Construction from Widget box or on a form?
    const bool isFormEditor = parent != 0 && QDesignerFormWindowInterface::findFormWindow(parent) != 0;
    QDesignerAxWidget *rc = new QDesignerAxPluginWidget(parent);
    if (!isFormEditor)
        rc->setDrawFlags(QDesignerAxWidget::DrawFrame|QDesignerAxWidget::DrawControl);
    return rc;
}

bool QAxWidgetPlugin::isInitialized() const
{
    return m_core != 0;
}

void QAxWidgetPlugin::initialize(QDesignerFormEditorInterface *core)
{
    if (m_core != 0)
        return;

    m_core = core;

    QExtensionManager *mgr = core->extensionManager();
    ActiveXPropertySheetFactory::registerExtension(mgr);
    ActiveXTaskMenuFactory::registerExtension(mgr, Q_TYPEID(QDesignerTaskMenuExtension));
    QAxWidgetExtraInfoFactory *extraInfoFactory = new QAxWidgetExtraInfoFactory(core, mgr);
    mgr->registerExtensions(extraInfoFactory, Q_TYPEID(QDesignerExtraInfoExtension));
}

QString QAxWidgetPlugin::domXml() const
{
    return QStringLiteral("\
<ui language=\"c++\">\
    <widget class=\"QAxWidget\" name=\"axWidget\">\
        <property name=\"geometry\">\
            <rect>\
                <x>0</x>\
                <y>0</y>\
                <width>80</width>\
                <height>70</height>\
            </rect>\
        </property>\
    </widget>\
</ui>");
}

QT_END_NAMESPACE
