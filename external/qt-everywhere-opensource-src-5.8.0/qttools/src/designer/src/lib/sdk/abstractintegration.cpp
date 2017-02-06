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

#include "abstractintegration.h"
#include "abstractformwindow.h"
#include "abstractformwindowmanager.h"
#include "abstractformwindowcursor.h"
#include "abstractformeditor.h"
#include "abstractactioneditor.h"
#include "abstractwidgetbox.h"
#include "abstractresourcebrowser.h"
#include "propertysheet.h"
#include "abstractpropertyeditor.h"
#include "qextensionmanager.h"

#include <qtresourcemodel_p.h>

#include <qdesigner_propertycommand_p.h>
#include <qdesigner_propertyeditor_p.h>
#include <qdesigner_objectinspector_p.h>
#include <widgetdatabase_p.h>
#include <pluginmanager_p.h>
#include <widgetfactory_p.h>
#include <qdesigner_widgetbox_p.h>
#include <qtgradientmanager.h>
#include <qtgradientutils.h>
#include <qtresourcemodel_p.h>

#include <QtCore/QVariant>
#include <QtCore/QFile>
#include <QtCore/QDir>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

/*!
    \class QDesignerIntegrationInterface

    \brief The QDesignerIntegrationInterface glues together parts of \QD
    and allows for overwriting functionality for IDE integration.

    \internal

    \inmodule QtDesigner

    \sa QDesignerFormEditorInterface
*/

/*!
    \enum QDesignerIntegrationInterface::ResourceFileWatcherBehaviour

    \internal

    This enum describes if and how resource files are watched.

    \value NoResourceFileWatcher        Do not watch for changes
    \value ReloadResourceFileSilently   Reload files silently.
    \value PromptToReloadResourceFile   Prompt the user to reload.
*/

/*!
    \enum QDesignerIntegrationInterface::FeatureFlag

    \internal

    This enum describes the features that are available and can be
    controlled by the integration.

    \value ResourceEditorFeature       The resource editor is enabled.
    \value SlotNavigationFeature       Provide context menu entry offering 'Go to slot'.
    \value DefaultWidgetActionFeature  Trigger the preferred action of
                                       QDesignerTaskMenuExtension when widget is activated.
    \value DefaultFeature              Default for \QD

    \sa hasFeature(), features()
*/

/*!
    \property QDesignerIntegrationInterface::headerSuffix

    Returns the suffix of the header of promoted widgets.
*/

/*!
    \property QDesignerIntegrationInterface::headerLowercase

    Returns whether headers of promoted widgets should be lower-cased (as the user types the class name).
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::updateSelection()

    \brief Sets the selected widget of the form window in various components.

    \internal

    In IDE integrations, the method can be overwritten to move the selection handles
    of the form's main window, should it be selected.
*/

/*!
    \fn virtual QWidget *QDesignerIntegrationInterface::containerWindow(QWidget *widget) const

    \brief Returns the outer window containing a form for applying main container geometries.

    \internal

    Should be implemented by IDE integrations.
*/

/*!
    \fn virtual QDesignerResourceBrowserInterface *QDesignerIntegrationInterface::createResourceBrowser(QWidget *parent = 0)

    \brief Creates a resource browser depending on IDE integration.

    \internal

    \note Language integration takes precedence.
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::updateProperty(const QString &name, const QVariant &value, bool enableSubPropertyHandling)

    \brief Triggered by the property editor to update a property value.

    \internal

    If a different property editor is used, it should invoke this function.
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::updateProperty(const QString &name, const QVariant &value)

    \brief Triggered by the property editor to update a property value without subproperty handling.

    \internal

    If a different property editor is used, it should invoke this function.
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::resetProperty(const QString &name)

    \brief Triggered by the property editor to reset a property value.

    \internal

    If a different property editor is used, it should invoke this function.
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::addDynamicProperty(const QString &name, const QVariant &value)

    \brief Triggered by the property editor to add a dynamic property value.

    \internal

    If a different property editor is used, it should invoke this function.
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::removeDynamicProperty(const QString &name)

    \brief Triggered by the property editor to remove a dynamic property.

    \internal

    If a different property editor is used, it should invoke this function.
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::updateActiveFormWindow(QDesignerFormWindowInterface *formWindow)

    \brief Sets up the active form window.

    \internal
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::setupFormWindow(QDesignerFormWindowInterface *formWindow)

    \brief Sets up the new form window.

    \internal
*/

/*!
    \fn virtual void QDesignerIntegrationInterface::updateCustomWidgetPlugins()

    \brief Triggers a reload of the custom widget plugins.

    \internal
*/

class QDesignerIntegrationInterfacePrivate
{
public:
    QDesignerIntegrationInterfacePrivate(QDesignerFormEditorInterface *core) :
        m_core(core) {}

    QDesignerFormEditorInterface *m_core;
};

QDesignerIntegrationInterface::QDesignerIntegrationInterface(QDesignerFormEditorInterface *core, QObject *parent)
    : QObject(parent), d(new QDesignerIntegrationInterfacePrivate(core))
{
    core->setIntegration(this);
}

QDesignerIntegrationInterface::~QDesignerIntegrationInterface()
{
}

QDesignerFormEditorInterface *QDesignerIntegrationInterface::core() const
{
    return d->m_core;
}

bool QDesignerIntegrationInterface::hasFeature(Feature f) const
{
    return (features() & f) != 0;
}

void QDesignerIntegrationInterface::emitObjectNameChanged(QDesignerFormWindowInterface *formWindow, QObject *object, const QString &newName, const QString &oldName)
{
    emit objectNameChanged(formWindow, object, newName, oldName);
}

void QDesignerIntegrationInterface::emitNavigateToSlot(const QString &objectName,
                                              const QString &signalSignature,
                                              const QStringList &parameterNames)
{
     emit navigateToSlot(objectName, signalSignature, parameterNames);
}

void QDesignerIntegrationInterface::emitNavigateToSlot(const QString &slotSignature)
{
    emit navigateToSlot(slotSignature);
}

void QDesignerIntegrationInterface::emitHelpRequested(const QString &manual, const QString &document)
{
    emit helpRequested(manual, document);
}

/*!
    \class QDesignerIntegration

    \brief The QDesignerIntegration class is \QD's implementation of
    QDesignerIntegrationInterface.

    \internal

    \inmodule QtDesigner

    IDE integrations should register classes derived from QDesignerIntegration
    with QDesignerFormEditorInterface.
*/

namespace qdesigner_internal {

class QDesignerIntegrationPrivate {
public:
    explicit QDesignerIntegrationPrivate(QDesignerIntegration *qq);

    QWidget *containerWindow(QWidget *widget) const;

    // Load plugins into widget database and factory.
    static void initializePlugins(QDesignerFormEditorInterface *formEditor);

    QString contextHelpId() const;

    void updateProperty(const QString &name, const QVariant &value, bool enableSubPropertyHandling);
    void resetProperty(const QString &name);
    void addDynamicProperty(const QString &name, const QVariant &value);
    void removeDynamicProperty(const QString &name);

    void setupFormWindow(QDesignerFormWindowInterface *formWindow);
    void updateSelection();
    void updateCustomWidgetPlugins();

    void updatePropertyPrivate(const QString &name, const QVariant &value);

    void initialize();
    void getSelection(qdesigner_internal::Selection &s);
    QObject *propertyEditorObject();

    QDesignerIntegration *q;
    QString headerSuffix;
    bool headerLowercase;
    QDesignerIntegrationInterface::Feature m_features;
    QDesignerIntegrationInterface::ResourceFileWatcherBehaviour m_resourceFileWatcherBehaviour;
    QString m_gradientsPath;
    QtGradientManager *m_gradientManager;
};

QDesignerIntegrationPrivate::QDesignerIntegrationPrivate(QDesignerIntegration *qq) :
    q(qq),
    headerSuffix(QStringLiteral(".h")),
    headerLowercase(true),
    m_features(QDesignerIntegrationInterface::DefaultFeature),
    m_resourceFileWatcherBehaviour(QDesignerIntegrationInterface::PromptToReloadResourceFile),
    m_gradientManager(0)
{
}

void QDesignerIntegrationPrivate::initialize()
{
    typedef void (QDesignerIntegration::*QDesignerIntegrationUpdatePropertySlot3)(const QString &, const QVariant &, bool);

    //
    // integrate the `Form Editor component'
    //

    // Extensions
    QDesignerFormEditorInterface *core = q->core();
    if (QDesignerPropertyEditor *designerPropertyEditor= qobject_cast<QDesignerPropertyEditor *>(core->propertyEditor())) {
        QObject::connect(designerPropertyEditor, &QDesignerPropertyEditor::propertyValueChanged,
                         q, static_cast<QDesignerIntegrationUpdatePropertySlot3>(&QDesignerIntegration::updateProperty));
        QObject::connect(designerPropertyEditor, &QDesignerPropertyEditor::resetProperty,
                         q, &QDesignerIntegration::resetProperty);
        QObject::connect(designerPropertyEditor, &QDesignerPropertyEditor::addDynamicProperty,
                q, &QDesignerIntegration::addDynamicProperty);
        QObject::connect(designerPropertyEditor, &QDesignerPropertyEditor::removeDynamicProperty,
                q, &QDesignerIntegration::removeDynamicProperty);
    } else {
        QObject::connect(core->propertyEditor(), SIGNAL(propertyChanged(QString,QVariant)),
                q, SLOT(updatePropertyPrivate(QString,QVariant))); // ### fixme: VS Integration leftover?
    }

    QObject::connect(core->formWindowManager(), &QDesignerFormWindowManagerInterface::formWindowAdded,
            q, &QDesignerIntegrationInterface::setupFormWindow);

    QObject::connect(core->formWindowManager(), &QDesignerFormWindowManagerInterface::activeFormWindowChanged,
            q, &QDesignerIntegrationInterface::updateActiveFormWindow);

    m_gradientManager = new QtGradientManager(q);
    core->setGradientManager(m_gradientManager);

    QString designerFolder = QDir::homePath();
    designerFolder += QDir::separator();
    designerFolder += QStringLiteral(".designer");
    m_gradientsPath = designerFolder;
    m_gradientsPath += QDir::separator();
    m_gradientsPath += QStringLiteral("gradients.xml");

    QFile f(m_gradientsPath);
    if (f.open(QIODevice::ReadOnly)) {
        QtGradientUtils::restoreState(m_gradientManager, QString::fromLatin1(f.readAll()));
        f.close();
    } else {
        QFile defaultGradients(QStringLiteral(":/qt-project.org/designer/defaultgradients.xml"));
        if (defaultGradients.open(QIODevice::ReadOnly)) {
            QtGradientUtils::restoreState(m_gradientManager, QString::fromLatin1(defaultGradients.readAll()));
            defaultGradients.close();
        }
    }

    if (WidgetDataBase *widgetDataBase = qobject_cast<WidgetDataBase*>(core->widgetDataBase()))
        widgetDataBase->grabStandardWidgetBoxIcons();
}

void QDesignerIntegrationPrivate::updateProperty(const QString &name, const QVariant &value, bool enableSubPropertyHandling)
{
    QDesignerFormWindowInterface *formWindow = q->core()->formWindowManager()->activeFormWindow();
    if (!formWindow)
        return;

    Selection selection;
    getSelection(selection);
    if (selection.empty())
        return;

    SetPropertyCommand *cmd = new SetPropertyCommand(formWindow);
    // find a reference object to compare to and to find the right group
    if (cmd->init(selection.selection(), name, value, propertyEditorObject(), enableSubPropertyHandling)) {
        formWindow->commandHistory()->push(cmd);
    } else {
        delete cmd;
        qDebug() << "Unable to set  property " << name << '.';
    }
}

void QDesignerIntegrationPrivate::resetProperty(const QString &name)
{
    QDesignerFormWindowInterface *formWindow = q->core()->formWindowManager()->activeFormWindow();
    if (!formWindow)
        return;

    Selection selection;
    getSelection(selection);
    if (selection.empty())
        return;

    ResetPropertyCommand *cmd = new ResetPropertyCommand(formWindow);
    // find a reference object to find the right group
    if (cmd->init(selection.selection(), name, propertyEditorObject())) {
        formWindow->commandHistory()->push(cmd);
    } else {
        delete cmd;
        qDebug() << "** WARNING Unable to reset property " << name << '.';
    }
}

void QDesignerIntegrationPrivate::addDynamicProperty(const QString &name, const QVariant &value)
{
    QDesignerFormWindowInterface *formWindow = q->core()->formWindowManager()->activeFormWindow();
    if (!formWindow)
        return;

    Selection selection;
    getSelection(selection);
    if (selection.empty())
        return;

    AddDynamicPropertyCommand *cmd = new AddDynamicPropertyCommand(formWindow);
    if (cmd->init(selection.selection(), propertyEditorObject(), name, value)) {
        formWindow->commandHistory()->push(cmd);
    } else {
        delete cmd;
        qDebug() <<  "** WARNING Unable to add dynamic property " << name << '.';
    }
}

void QDesignerIntegrationPrivate::removeDynamicProperty(const QString &name)
{
    QDesignerFormWindowInterface *formWindow = q->core()->formWindowManager()->activeFormWindow();
    if (!formWindow)
        return;

    Selection selection;
    getSelection(selection);
    if (selection.empty())
        return;

    RemoveDynamicPropertyCommand *cmd = new RemoveDynamicPropertyCommand(formWindow);
    if (cmd->init(selection.selection(), propertyEditorObject(), name)) {
        formWindow->commandHistory()->push(cmd);
    } else {
        delete cmd;
        qDebug() << "** WARNING Unable to remove dynamic property " << name << '.';
    }

}

void QDesignerIntegrationPrivate::setupFormWindow(QDesignerFormWindowInterface *formWindow)
{
    QObject::connect(formWindow, &QDesignerFormWindowInterface::selectionChanged,
                     q, &QDesignerIntegrationInterface::updateSelection);
}

void QDesignerIntegrationPrivate::updateSelection()
{
    QDesignerFormEditorInterface *core = q->core();
    QDesignerFormWindowInterface *formWindow = core->formWindowManager()->activeFormWindow();
    QWidget *selection = 0;

    if (formWindow) {
        selection = formWindow->cursor()->current();
    }

    if (QDesignerActionEditorInterface *actionEditor = core->actionEditor())
        actionEditor->setFormWindow(formWindow);

    if (QDesignerPropertyEditorInterface *propertyEditor = core->propertyEditor())
        propertyEditor->setObject(selection);

    if (QDesignerObjectInspectorInterface *objectInspector = core->objectInspector())
        objectInspector->setFormWindow(formWindow);

}

QWidget *QDesignerIntegrationPrivate::containerWindow(QWidget *widget) const
{
    // Find the parent window to apply a geometry to.
    while (widget) {
        if (widget->isWindow())
            break;
        if (!qstrcmp(widget->metaObject()->className(), "QMdiSubWindow"))
            break;

        widget = widget->parentWidget();
    }

    return widget;
}

void QDesignerIntegrationPrivate::getSelection(Selection &s)
{
    QDesignerFormEditorInterface *core = q->core();
    // Get multiselection from object inspector
    if (QDesignerObjectInspector *designerObjectInspector = qobject_cast<QDesignerObjectInspector *>(core->objectInspector())) {
        designerObjectInspector->getSelection(s);
        // Action editor puts actions that are not on the form yet
        // into the property editor only.
        if (s.empty())
            if (QObject *object = core->propertyEditor()->object())
                s.objects.push_back(object);

    } else {
        // Just in case someone plugs in an old-style object inspector: Emulate selection
        s.clear();
        QDesignerFormWindowInterface *formWindow = core->formWindowManager()->activeFormWindow();
        if (!formWindow)
            return;

        QObject *object = core->propertyEditor()->object();
        if (object->isWidgetType()) {
            QWidget *widget = static_cast<QWidget*>(object);
            QDesignerFormWindowCursorInterface *cursor = formWindow->cursor();
            if (cursor->isWidgetSelected(widget)) {
                s.managed.push_back(widget);
            } else {
                s.unmanaged.push_back(widget);
            }
        } else {
            s.objects.push_back(object);
        }
    }
}

QObject *QDesignerIntegrationPrivate::propertyEditorObject()
{
    if (QDesignerPropertyEditorInterface *propertyEditor = q->core()->propertyEditor())
        return propertyEditor->object();
    return 0;
}

// Load plugins into widget database and factory.
void QDesignerIntegrationPrivate::initializePlugins(QDesignerFormEditorInterface *formEditor)
{
    // load the plugins
    qdesigner_internal::WidgetDataBase *widgetDataBase = qobject_cast<qdesigner_internal::WidgetDataBase*>(formEditor->widgetDataBase());
    if (widgetDataBase) {
        widgetDataBase->loadPlugins();
    }

    if (qdesigner_internal::WidgetFactory *widgetFactory = qobject_cast<qdesigner_internal::WidgetFactory*>(formEditor->widgetFactory())) {
        widgetFactory->loadPlugins();
    }

    if (widgetDataBase) {
        widgetDataBase->grabDefaultPropertyValues();
    }
}

void QDesignerIntegrationPrivate::updateCustomWidgetPlugins()
{
    QDesignerFormEditorInterface *formEditor = q->core();
    if (QDesignerPluginManager *pm = formEditor->pluginManager())
        pm->registerNewPlugins();

    initializePlugins(formEditor);

    // Do not just reload the last file as the WidgetBox merges the compiled-in resources
    // and $HOME/.designer/widgetbox.xml. This would also double the scratchpad.
    if (QDesignerWidgetBox *wb = qobject_cast<QDesignerWidgetBox*>(formEditor->widgetBox())) {
        const QDesignerWidgetBox::LoadMode oldLoadMode = wb->loadMode();
        wb->setLoadMode(QDesignerWidgetBox::LoadCustomWidgetsOnly);
        wb->load();
        wb->setLoadMode(oldLoadMode);
    }
}

static QString fixHelpClassName(const QString &className)
{
    // ### generalize using the Widget Data Base
    if (className == QStringLiteral("Line"))
        return QStringLiteral("QFrame");
    if (className == QStringLiteral("Spacer"))
        return QStringLiteral("QSpacerItem");
    if (className == QStringLiteral("QLayoutWidget"))
        return QStringLiteral("QLayout");
    return className;
}

// Return class in which the property is defined
static QString classForProperty(QDesignerFormEditorInterface *core,
                                QObject *object,
                                const QString &property)
{
    if (const QDesignerPropertySheetExtension *ps = qt_extension<QDesignerPropertySheetExtension *>(core->extensionManager(), object)) {
        const int index = ps->indexOf(property);
        if (index >= 0)
            return ps->propertyGroup(index);
    }
    return QString();
}

QString QDesignerIntegrationPrivate::contextHelpId() const
{
    QDesignerFormEditorInterface *core = q->core();
    QObject *currentObject = core->propertyEditor()->object();
    if (!currentObject)
        return QString();
    // Return a help index id consisting of "class::property"
    QString className;
    QString currentPropertyName = core->propertyEditor()->currentPropertyName();
    if (!currentPropertyName.isEmpty())
        className = classForProperty(core, currentObject, currentPropertyName);
    if (className.isEmpty()) {
        currentPropertyName.clear(); // We hit on some fake property.
        className = qdesigner_internal::WidgetFactory::classNameOf(core, currentObject);
    }
    QString helpId = fixHelpClassName(className);
    if (!currentPropertyName.isEmpty()) {
        helpId += QStringLiteral("::");
        helpId += currentPropertyName;
    }
    return helpId;
}

} // namespace qdesigner_internal

// -------------- QDesignerIntegration
// As of 4.4, the header will be distributed with the Eclipse plugin.

QDesignerIntegration::QDesignerIntegration(QDesignerFormEditorInterface *core, QObject *parent) :
    QDesignerIntegrationInterface(core, parent),
    d(new qdesigner_internal::QDesignerIntegrationPrivate(this))
{
    d->initialize();
}

QDesignerIntegration::~QDesignerIntegration()
{
    QFile f(d->m_gradientsPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QtGradientUtils::saveState(d->m_gradientManager).toUtf8());
        f.close();
    }
}

QString QDesignerIntegration::headerSuffix() const
{
    return d->headerSuffix;
}

void QDesignerIntegration::setHeaderSuffix(const QString &headerSuffix)
{
    d->headerSuffix = headerSuffix;
}

bool QDesignerIntegration::isHeaderLowercase() const
{
    return d->headerLowercase;
}

void QDesignerIntegration::setHeaderLowercase(bool headerLowercase)
{
    d->headerLowercase = headerLowercase;
}

QDesignerIntegrationInterface::Feature QDesignerIntegration::features() const
{
    return d->m_features;
}

void QDesignerIntegration::setFeatures(Feature f)
{
    d->m_features = f;
}

QDesignerIntegrationInterface::ResourceFileWatcherBehaviour QDesignerIntegration::resourceFileWatcherBehaviour() const
{
    return d->m_resourceFileWatcherBehaviour;
}

void QDesignerIntegration::setResourceFileWatcherBehaviour(ResourceFileWatcherBehaviour behaviour)
{
    if (d->m_resourceFileWatcherBehaviour != behaviour) {
        d->m_resourceFileWatcherBehaviour = behaviour;
        core()->resourceModel()->setWatcherEnabled(behaviour != QDesignerIntegrationInterface::NoResourceFileWatcher);
    }
}

void QDesignerIntegration::updateProperty(const QString &name, const QVariant &value, bool enableSubPropertyHandling)
{
    d->updateProperty(name, value, enableSubPropertyHandling);
    emit propertyChanged(core()->formWindowManager()->activeFormWindow(), name, value);
}

void QDesignerIntegration::updateProperty(const QString &name, const QVariant &value)
{
    updateProperty(name, value, true);
}

void QDesignerIntegration::resetProperty(const QString &name)
{
    d->resetProperty(name);
}

void QDesignerIntegration::addDynamicProperty(const QString &name, const QVariant &value)
{
    d->addDynamicProperty(name, value);
}

void QDesignerIntegration::removeDynamicProperty(const QString &name)
{
    d->removeDynamicProperty(name);
}

void QDesignerIntegration::updateActiveFormWindow(QDesignerFormWindowInterface *)
{
    d->updateSelection();
}

void QDesignerIntegration::setupFormWindow(QDesignerFormWindowInterface *formWindow)
{
    d->setupFormWindow(formWindow);
    connect(formWindow, &QDesignerFormWindowInterface::selectionChanged,
            this, &QDesignerIntegrationInterface::updateSelection);
}

void QDesignerIntegration::updateSelection()
{
    d->updateSelection();
}

QWidget *QDesignerIntegration::containerWindow(QWidget *widget) const
{
    return d->containerWindow(widget);
}

// Load plugins into widget database and factory.
void QDesignerIntegration::initializePlugins(QDesignerFormEditorInterface *formEditor)
{
    qdesigner_internal::QDesignerIntegrationPrivate::initializePlugins(formEditor);
}

void QDesignerIntegration::updateCustomWidgetPlugins()
{
    d->updateCustomWidgetPlugins();
}

QDesignerResourceBrowserInterface *QDesignerIntegration::createResourceBrowser(QWidget *)
{
    return 0;
}

QString QDesignerIntegration::contextHelpId() const
{
    return d->contextHelpId();
}

QT_END_NAMESPACE
