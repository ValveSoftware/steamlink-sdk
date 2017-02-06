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

#include "qdesigner_formbuilder_p.h"
#include "dynamicpropertysheet.h"
#include "qsimpleresource_p.h"
#include "widgetfactory_p.h"
#include "qdesigner_introspection_p.h"

#include <QtDesigner/private/ui4_p.h>
#include <QtDesigner/private/formbuilderextra_p.h>
// sdk
#include <QtDesigner/container.h>
#include <QtDesigner/propertysheet.h>
#include <QtDesigner/QExtensionManager>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerWidgetFactoryInterface>
#include <abstractdialoggui_p.h>

#include <QtUiPlugin/QDesignerCustomWidgetInterface>

// shared
#include <qdesigner_propertysheet_p.h>
#include <qdesigner_utils_p.h>
#include <formwindowbase_p.h>
#include <qtresourcemodel_p.h>

#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QStyle>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QMessageBox>
#include <QtGui/QPixmap>

#include <QtCore/QBuffer>
#include <QtCore/qdebug.h>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

QDesignerFormBuilder::QDesignerFormBuilder(QDesignerFormEditorInterface *core,
                                           const DeviceProfile &deviceProfile) :
    m_core(core),
    m_deviceProfile(deviceProfile),
    m_pixmapCache(0),
    m_iconCache(0),
    m_ignoreCreateResources(false),
    m_tempResourceSet(0),
    m_mainWidget(true)
{
    Q_ASSERT(m_core);
}

QString QDesignerFormBuilder::systemStyle() const
{
    return m_deviceProfile.isEmpty() ?
        QString::fromUtf8(QApplication::style()->metaObject()->className()) :
        m_deviceProfile.style();
}

QWidget *QDesignerFormBuilder::create(DomUI *ui, QWidget *parentWidget)
{
    m_mainWidget = true;
    QtResourceSet *resourceSet = core()->resourceModel()->currentResourceSet();

    // reload resource properties;
    createResources(ui->elementResources());
    core()->resourceModel()->setCurrentResourceSet(m_tempResourceSet);

    m_ignoreCreateResources = true;
    DesignerPixmapCache pixmapCache;
    DesignerIconCache iconCache(&pixmapCache);
    m_pixmapCache = &pixmapCache;
    m_iconCache = &iconCache;

    QWidget *widget = QFormBuilder::create(ui, parentWidget);

    core()->resourceModel()->setCurrentResourceSet(resourceSet);
    core()->resourceModel()->removeResourceSet(m_tempResourceSet);
    m_tempResourceSet = 0;
    m_ignoreCreateResources = false;
    m_pixmapCache = 0;
    m_iconCache = 0;

    m_customWidgetsWithScript.clear();
    return widget;
}

QWidget *QDesignerFormBuilder::createWidget(const QString &widgetName, QWidget *parentWidget, const QString &name)
{
    QWidget *widget = 0;

    if (widgetName == QStringLiteral("QToolBar")) {
        widget = new QToolBar(parentWidget);
    } else if (widgetName == QStringLiteral("QMenu")) {
        widget = new QMenu(parentWidget);
    } else if (widgetName == QStringLiteral("QMenuBar")) {
        widget = new QMenuBar(parentWidget);
    } else {
        widget = core()->widgetFactory()->createWidget(widgetName, parentWidget);
    }

    if (widget) {
        widget->setObjectName(name);
        if (QSimpleResource::hasCustomWidgetScript(m_core, widget))
            m_customWidgetsWithScript.insert(widget);
    }

    if (m_mainWidget) { // We need to apply the DPI here to take effect on size hints, etc.
        m_deviceProfile.apply(m_core, widget, DeviceProfile::ApplyPreview);
        m_mainWidget = false;
    }
    return widget;
}

bool QDesignerFormBuilder::addItem(DomWidget *ui_widget, QWidget *widget, QWidget *parentWidget)
{
    // Use container extension or rely on scripts unless main window.
    if (QFormBuilder::addItem(ui_widget, widget, parentWidget))
        return true;

    if (QDesignerContainerExtension *container = qt_extension<QDesignerContainerExtension*>(m_core->extensionManager(), parentWidget)) {
        container->addWidget(widget);
        return true;
    }
    return false;
}

bool QDesignerFormBuilder::addItem(DomLayoutItem *ui_item, QLayoutItem *item, QLayout *layout)
{
    return QFormBuilder::addItem(ui_item, item, layout);
}

QIcon QDesignerFormBuilder::nameToIcon(const QString &filePath, const QString &qrcPath)
{
    Q_UNUSED(filePath)
    Q_UNUSED(qrcPath)
    qWarning() << "QDesignerFormBuilder::nameToIcon() is obsoleted";
    return QIcon();
}

QPixmap QDesignerFormBuilder::nameToPixmap(const QString &filePath, const QString &qrcPath)
{
    Q_UNUSED(filePath)
    Q_UNUSED(qrcPath)
    qWarning() << "QDesignerFormBuilder::nameToPixmap() is obsoleted";
    return QPixmap();
}

/* If the property is a enum or flag value, retrieve
 * the existing enum/flag type via property sheet and use it to convert */

static bool readDomEnumerationValue(const DomProperty *p,
                                    const QDesignerPropertySheetExtension* sheet,
                                    QVariant &v)
{
    switch (p->kind()) {
    case DomProperty::Set: {
        const int index = sheet->indexOf(p->attributeName());
        if (index == -1)
            return false;
        const QVariant sheetValue = sheet->property(index);
        if (sheetValue.canConvert<PropertySheetFlagValue>()) {
            const PropertySheetFlagValue f = qvariant_cast<PropertySheetFlagValue>(sheetValue);
            bool ok = false;
            v = f.metaFlags.parseFlags(p->elementSet(), &ok);
            if (!ok)
                designerWarning(f.metaFlags.messageParseFailed(p->elementSet()));
            return true;
        }
    }
        break;
    case DomProperty::Enum: {
        const int index = sheet->indexOf(p->attributeName());
        if (index == -1)
            return false;
        const QVariant sheetValue = sheet->property(index);
        if (sheetValue.canConvert<PropertySheetEnumValue>()) {
            const PropertySheetEnumValue e = qvariant_cast<PropertySheetEnumValue>(sheetValue);
            bool ok = false;
            v = e.metaEnum.parseEnum(p->elementEnum(), &ok);
            if (!ok)
                designerWarning(e.metaEnum.messageParseFailed(p->elementEnum()));
            return true;
        }
    }
        break;
    default:
        break;
    }
    return false;
}

void QDesignerFormBuilder::applyProperties(QObject *o, const QList<DomProperty*> &properties)
{
    typedef QList<DomProperty*> DomPropertyList;

    if (properties.empty())
        return;

    const QDesignerPropertySheetExtension *sheet = qt_extension<QDesignerPropertySheetExtension*>(core()->extensionManager(), o);
    const QDesignerDynamicPropertySheetExtension *dynamicSheet = qt_extension<QDesignerDynamicPropertySheetExtension*>(core()->extensionManager(), o);
    const bool changingMetaObject = WidgetFactory::classNameOf(core(), o) == QStringLiteral("QAxWidget");
    const QDesignerMetaObjectInterface *meta = core()->introspection()->metaObject(o);
    const bool dynamicPropertiesAllowed = dynamicSheet && dynamicSheet->dynamicPropertiesAllowed();

    QDesignerPropertySheet *designerPropertySheet = qobject_cast<QDesignerPropertySheet *>(
                    core()->extensionManager()->extension(o, Q_TYPEID(QDesignerPropertySheetExtension)));

    if (designerPropertySheet) {
        if (designerPropertySheet->pixmapCache())
            designerPropertySheet->setPixmapCache(m_pixmapCache);
        if (designerPropertySheet->iconCache())
            designerPropertySheet->setIconCache(m_iconCache);
    }

    const DomPropertyList::const_iterator cend = properties.constEnd();
    for (DomPropertyList::const_iterator it = properties.constBegin(); it != cend; ++it) {
        DomProperty *p = *it;
        QVariant v;
        if (!readDomEnumerationValue(p, sheet, v))
            v = toVariant(o->metaObject(), p);

        if (v.isNull())
            continue;

        const QString attributeName = p->attributeName();
        if (d->applyPropertyInternally(o, attributeName, v))
            continue;

        // refuse fake properties like current tab name (weak test)
        if (!dynamicPropertiesAllowed) {
            if (changingMetaObject) // Changes after setting control of QAxWidget
                meta = core()->introspection()->metaObject(o);
            if (meta->indexOfProperty(attributeName) == -1)
                continue;
        }

        QObject *obj = o;
        QAbstractScrollArea *scroll = qobject_cast<QAbstractScrollArea *>(o);
        if (scroll && attributeName == QStringLiteral("cursor") && scroll->viewport())
            obj = scroll->viewport();

        // a real property
        obj->setProperty(attributeName.toUtf8(), v);
    }
}

DomWidget *QDesignerFormBuilder::createDom(QWidget *widget, DomWidget *ui_parentWidget, bool recursive)
{
    DomWidget *ui_widget = QFormBuilder::createDom(widget, ui_parentWidget, recursive);
    QSimpleResource::addExtensionDataToDOM(this, m_core, ui_widget, widget);
    return ui_widget;
}

QWidget *QDesignerFormBuilder::create(DomWidget *ui_widget, QWidget *parentWidget)
{
    QWidget *widget = QFormBuilder::create(ui_widget, parentWidget);
    // Do not apply state if scripts are to be run in preview mode
    QSimpleResource::applyExtensionDataFromDOM(this, m_core, ui_widget, widget);
    return widget;
}

void QDesignerFormBuilder::createResources(DomResources *resources)
{
    if (m_ignoreCreateResources)
        return;
    QStringList paths;
    if (resources != 0) {
        const QList<DomResource*> dom_include = resources->elementInclude();
        foreach (DomResource *res, dom_include) {
            QString path = QDir::cleanPath(workingDirectory().absoluteFilePath(res->attributeLocation()));
            paths << path;
        }
    }

    m_tempResourceSet = core()->resourceModel()->addResourceSet(paths);
}

QLayout *QDesignerFormBuilder::create(DomLayout *ui_layout, QLayout *layout, QWidget *parentWidget)
{
    return QFormBuilder::create(ui_layout, layout, parentWidget);
}

void QDesignerFormBuilder::loadExtraInfo(DomWidget *ui_widget, QWidget *widget, QWidget *parentWidget)
{
    QFormBuilder::loadExtraInfo(ui_widget, widget, parentWidget);
}

QWidget *QDesignerFormBuilder::createPreview(const QDesignerFormWindowInterface *fw,
                                             const QString &styleName,
                                             const QString &appStyleSheet,
                                             const DeviceProfile &deviceProfile,
                                             QString *errorMessage)
{
    // load
    QDesignerFormBuilder builder(fw->core(), deviceProfile);
    builder.setWorkingDirectory(fw->absoluteDir());

    QByteArray bytes = fw->contents().toUtf8();

    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);

    QWidget *widget = builder.load(&buffer, 0);
    if (!widget) { // Shouldn't happen
        *errorMessage = QCoreApplication::translate("QDesignerFormBuilder", "The preview failed to build.");
        return  0;
    }
    // Make sure palette is applied
    const QString styleToUse = styleName.isEmpty() ? builder.deviceProfile().style() : styleName;
    if (!styleToUse.isEmpty()) {
        if (WidgetFactory *wf = qobject_cast<qdesigner_internal::WidgetFactory *>(fw->core()->widgetFactory())) {
            if (styleToUse != wf->styleName())
                WidgetFactory::applyStyleToTopLevel(wf->getStyle(styleToUse), widget);
        }
    }
    // Fake application style sheet by prepending. (If this doesn't work, fake by nesting
    // into parent widget).
    if (!appStyleSheet.isEmpty()) {
        QString styleSheet = appStyleSheet;
        styleSheet += QLatin1Char('\n');
        styleSheet +=  widget->styleSheet();
        widget->setStyleSheet(styleSheet);
    }
    return widget;
}

QWidget *QDesignerFormBuilder::createPreview(const QDesignerFormWindowInterface *fw, const QString &styleName)
{
    return createPreview(fw, styleName, QString());
}

QWidget *QDesignerFormBuilder::createPreview(const QDesignerFormWindowInterface *fw,
                                             const QString &styleName,
                                             const QString &appStyleSheet,
                                             QString *errorMessage)
{
    return  createPreview(fw, styleName, appStyleSheet, DeviceProfile(), errorMessage);
}

QWidget *QDesignerFormBuilder::createPreview(const QDesignerFormWindowInterface *fw, const QString &styleName, const QString &appStyleSheet)
{
    QString errorMessage;
    QWidget *widget = createPreview(fw, styleName, appStyleSheet, DeviceProfile(), &errorMessage);
    if (!widget && !errorMessage.isEmpty()) {
        // Display Script errors or message box
        QWidget *dialogParent = fw->core()->topLevel();
        fw->core()->dialogGui()->message(dialogParent, QDesignerDialogGuiInterface::PreviewFailureMessage,
                                         QMessageBox::Warning, QCoreApplication::translate("QDesignerFormBuilder", "Designer"),
                                         errorMessage, QMessageBox::Ok);
        return 0;
    }
    return widget;
}

QPixmap QDesignerFormBuilder::createPreviewPixmap(const QDesignerFormWindowInterface *fw, const QString &styleName, const QString &appStyleSheet)
{
    QWidget *widget = createPreview(fw, styleName, appStyleSheet);
    if (!widget)
        return QPixmap();

    const QPixmap rc = widget->grab(QRect(0, 0, -1, -1));
    widget->deleteLater();
    return rc;
}

// ---------- NewFormWidgetFormBuilder

NewFormWidgetFormBuilder::NewFormWidgetFormBuilder(QDesignerFormEditorInterface *core,
                             const DeviceProfile &deviceProfile) :
    QDesignerFormBuilder(core, deviceProfile)
{
}

void NewFormWidgetFormBuilder::createCustomWidgets(DomCustomWidgets *dc)
{
    QSimpleResource::handleDomCustomWidgets(core(), dc);
}

} // namespace qdesigner_internal

QT_END_NAMESPACE
