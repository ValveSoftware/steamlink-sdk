/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "formeditor.h"
#include "formeditor_optionspage.h"
#include "embeddedoptionspage.h"
#include "templateoptionspage.h"
#include "metadatabase_p.h"
#include "widgetdatabase_p.h"
#include "widgetfactory_p.h"
#include "formwindowmanager.h"
#include "qmainwindow_container.h"
#include "qmdiarea_container.h"
#include "qwizard_container.h"
#include "default_container.h"
#include "default_layoutdecoration.h"
#include "default_actionprovider.h"
#include "qlayoutwidget_propertysheet.h"
#include "spacer_propertysheet.h"
#include "line_propertysheet.h"
#include "layout_propertysheet.h"
#include "qdesigner_stackedbox_p.h"
#include "qdesigner_toolbox_p.h"
#include "qdesigner_tabwidget_p.h"
#include "qtresourcemodel_p.h"
#include "itemview_propertysheet.h"

// sdk
#include <QtDesigner/QExtensionManager>
#include <QtDesigner/QDesignerIntegrationInterface>
// shared
#include <pluginmanager_p.h>
#include <qdesigner_taskmenu_p.h>
#include <qdesigner_membersheet_p.h>
#include <qdesigner_promotion_p.h>
#include <dialoggui_p.h>
#include <qdesigner_introspection_p.h>
#include <qdesigner_qsettings_p.h>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

FormEditor::FormEditor(QObject *parent)
    : QDesignerFormEditorInterface(parent)
{
    setIntrospection(new QDesignerIntrospection);
    setDialogGui(new DialogGui);
    QDesignerPluginManager *pluginManager = new QDesignerPluginManager(this);
    setPluginManager(pluginManager);

    WidgetDataBase *widgetDatabase = new WidgetDataBase(this, this);
    setWidgetDataBase(widgetDatabase);

    MetaDataBase *metaDataBase = new MetaDataBase(this, this);
    setMetaDataBase(metaDataBase);

    WidgetFactory *widgetFactory = new WidgetFactory(this, this);
    setWidgetFactory(widgetFactory);

    FormWindowManager *formWindowManager = new FormWindowManager(this, this);
    setFormManager(formWindowManager);
    connect(formWindowManager, SIGNAL(formWindowAdded(QDesignerFormWindowInterface*)), widgetFactory, SLOT(formWindowAdded(QDesignerFormWindowInterface*)));
    connect(formWindowManager, SIGNAL(activeFormWindowChanged(QDesignerFormWindowInterface*)), widgetFactory, SLOT(activeFormWindowChanged(QDesignerFormWindowInterface*)));

    QExtensionManager *mgr = new QExtensionManager(this);
    const QString containerExtensionId = Q_TYPEID(QDesignerContainerExtension);

    QDesignerStackedWidgetContainerFactory::registerExtension(mgr, containerExtensionId);
    QDesignerTabWidgetContainerFactory::registerExtension(mgr, containerExtensionId);
    QDesignerToolBoxContainerFactory::registerExtension(mgr, containerExtensionId);
    QMainWindowContainerFactory::registerExtension(mgr, containerExtensionId);
    QDockWidgetContainerFactory::registerExtension(mgr, containerExtensionId);
    QScrollAreaContainerFactory::registerExtension(mgr, containerExtensionId);
    QMdiAreaContainerFactory::registerExtension(mgr, containerExtensionId);
    QWizardContainerFactory::registerExtension(mgr, containerExtensionId);

    mgr->registerExtensions(new QDesignerLayoutDecorationFactory(mgr),
                            Q_TYPEID(QDesignerLayoutDecorationExtension));

    const QString actionProviderExtensionId = Q_TYPEID(QDesignerActionProviderExtension);
    QToolBarActionProviderFactory::registerExtension(mgr, actionProviderExtensionId);
    QMenuBarActionProviderFactory::registerExtension(mgr, actionProviderExtensionId);
    QMenuActionProviderFactory::registerExtension(mgr, actionProviderExtensionId);

    QDesignerDefaultPropertySheetFactory::registerExtension(mgr);
    QLayoutWidgetPropertySheetFactory::registerExtension(mgr);
    SpacerPropertySheetFactory::registerExtension(mgr);
    LinePropertySheetFactory::registerExtension(mgr);
    LayoutPropertySheetFactory::registerExtension(mgr);
    QStackedWidgetPropertySheetFactory::registerExtension(mgr);
    QToolBoxWidgetPropertySheetFactory::registerExtension(mgr);
    QTabWidgetPropertySheetFactory::registerExtension(mgr);
    QMdiAreaPropertySheetFactory::registerExtension(mgr);
    QWizardPagePropertySheetFactory::registerExtension(mgr);
    QWizardPropertySheetFactory::registerExtension(mgr);

    QTreeViewPropertySheetFactory::registerExtension(mgr);
    QTableViewPropertySheetFactory::registerExtension(mgr);

    const QString internalTaskMenuId = QStringLiteral("QDesignerInternalTaskMenuExtension");
    QDesignerTaskMenuFactory::registerExtension(mgr, internalTaskMenuId);

    mgr->registerExtensions(new QDesignerMemberSheetFactory(mgr),
                            Q_TYPEID(QDesignerMemberSheetExtension));

    setExtensionManager(mgr);

    setPromotion(new QDesignerPromotion(this));

    QtResourceModel *resourceModel = new QtResourceModel(this);
    setResourceModel(resourceModel);
    connect(resourceModel, SIGNAL(qrcFileModifiedExternally(QString)),
            this, SLOT(slotQrcFileChangedExternally(QString)));

    QList<QDesignerOptionsPageInterface*> optionsPages;
    optionsPages << new TemplateOptionsPage(this) << new FormEditorOptionsPage(this) << new EmbeddedOptionsPage(this);
    setOptionsPages(optionsPages);

    setSettingsManager(new QDesignerQSettings());
}

FormEditor::~FormEditor()
{
}

void FormEditor::slotQrcFileChangedExternally(const QString &path)
{
    if (!integration())
        return;

    QDesignerIntegration::ResourceFileWatcherBehaviour behaviour = integration()->resourceFileWatcherBehaviour();
    if (behaviour == QDesignerIntegration::NoResourceFileWatcher) {
        return;
    } else if (behaviour == QDesignerIntegration::PromptToReloadResourceFile) {
        QMessageBox::StandardButton button = dialogGui()->message(topLevel(), QDesignerDialogGuiInterface::FileChangedMessage, QMessageBox::Warning,
                tr("Resource File Changed"),
                tr("The file \"%1\" has changed outside Designer. Do you want to reload it?").arg(path),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (button != QMessageBox::Yes)
            return;
    }

    resourceModel()->reload(path);
}

}

QT_END_NAMESPACE
