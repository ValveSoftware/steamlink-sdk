/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "ui_delegates_manager.h"

#include "api/qquickwebengineview_p.h"
#include "javascript_dialog_controller.h"

#include <QAbstractListModel>
#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QStringBuilder>

// Uncomment for QML debugging
//#define UI_DELEGATES_DEBUG

#define NO_SEPARATOR
#if defined(Q_OS_WIN)
#define FILE_NAME_CASE_STATEMENT(TYPE, COMPONENT) \
    case UIDelegatesManager::TYPE:\
        return QStringLiteral(#TYPE L ##".qml");
#else
#define FILE_NAME_CASE_STATEMENT(TYPE, COMPONENT) \
    case UIDelegatesManager::TYPE:\
        return QStringLiteral(#TYPE".qml");
#endif

static QString fileNameForComponent(UIDelegatesManager::ComponentType type)
{
    switch (type) {
    FOR_EACH_COMPONENT_TYPE(FILE_NAME_CASE_STATEMENT, NO_SEPARATOR)
    default:
        Q_UNREACHABLE();
    }
    return QString();
}

static QString getUIDelegatesImportDir(QQmlEngine *engine) {
    static QString importDir;
    static bool initialized = false;
    if (initialized)
        return importDir;
    Q_FOREACH (const QString &path, engine->importPathList()) {
        QFileInfo fi(path % QLatin1String("/QtWebEngine/UIDelegates/"));
        if (fi.exists()) {
            importDir = fi.absolutePath();
            break;
        }
    }
    initialized = true;
    return importDir;
}

const char *defaultPropertyName(QObject *obj)
{
    const QMetaObject *metaObject = obj->metaObject();

    int idx = metaObject->indexOfClassInfo("DefaultProperty");
    if (-1 == idx)
        return 0;

    QMetaClassInfo info = metaObject->classInfo(idx);
    return info.value();
}

MenuItemHandler::MenuItemHandler(QObject *parent)
    : QObject(parent)
{
}


CopyMenuItem::CopyMenuItem(QObject *parent, const QString &textToCopy)
    : MenuItemHandler(parent)
    , m_textToCopy(textToCopy)
{
    connect(this, &MenuItemHandler::triggered, this, &CopyMenuItem::onTriggered);
}

void CopyMenuItem::onTriggered()
{
    qApp->clipboard()->setText(m_textToCopy);
}

NavigateMenuItem::NavigateMenuItem(QObject *parent, const QExplicitlySharedDataPointer<WebContentsAdapter> &adapter, const QUrl &targetUrl)
    : MenuItemHandler(parent)
    , m_adapter(adapter)
    , m_targetUrl(targetUrl)
{
    connect(this, &MenuItemHandler::triggered, this, &NavigateMenuItem::onTriggered);
}

void NavigateMenuItem::onTriggered()
{
    m_adapter->load(m_targetUrl);
}

#define COMPONENT_MEMBER_INIT(TYPE, COMPONENT) \
    , COMPONENT##Component(0)

UIDelegatesManager::UIDelegatesManager(QQuickWebEngineView *view)
    : m_view(view)
    FOR_EACH_COMPONENT_TYPE(COMPONENT_MEMBER_INIT, NO_SEPARATOR)
{
}

#define COMPONENT_MEMBER_CASE_STATEMENT(TYPE, COMPONENT) \
    case TYPE: \
        component = &COMPONENT##Component; \
        break;

bool UIDelegatesManager::ensureComponentLoaded(ComponentType type)
{
    QQmlEngine* engine = qmlEngine(m_view);
    if (getUIDelegatesImportDir(engine).isNull())
        return false;
    QQmlComponent **component;
    switch (type) {
    FOR_EACH_COMPONENT_TYPE(COMPONENT_MEMBER_CASE_STATEMENT, NO_SEPARATOR)
    default:
        Q_UNREACHABLE();
        return false;
    }
    QString fileName(fileNameForComponent(type));
#ifndef UI_DELEGATES_DEBUG
    if (*component)
        return true;
#else // Unconditionally reload the components each time.
    fprintf(stderr, "%s: %s\n", Q_FUNC_INFO, qPrintable(fileName));
#endif
    if (!engine)
        return false;
    QFileInfo fi(getUIDelegatesImportDir(engine) % QLatin1Char('/') % fileName);
    if (!fi.exists())
        return false;
    // FIXME: handle async loading
    *component = (new QQmlComponent(engine, QUrl::fromLocalFile(fi.absoluteFilePath()), QQmlComponent::PreferSynchronous, m_view));

    if ((*component)->status() != QQmlComponent::Ready) {
        Q_FOREACH (const QQmlError& err, (*component)->errors())
            qWarning("QtWebEngine: component error: %s\n", qPrintable(err.toString()));
        delete *component;
        *component = 0;
        return false;
    }
    return true;
}

#define CHECK_QML_SIGNAL_PROPERTY(prop, location) \
    if (!prop.isSignalProperty()) \
        qWarning("%s is missing %s signal property.\n", qPrintable(location.toString()), qPrintable(prop.name()));

void UIDelegatesManager::addMenuItem(MenuItemHandler *menuItemHandler, const QString &text, const QString &iconName, bool enabled)
{
    Q_ASSERT(menuItemHandler);
    if (!ensureComponentLoaded(MenuItem))
        return;
    QObject *it = menuItemComponent->beginCreate(qmlContext(m_view));

    QQmlProperty(it, QStringLiteral("text")).write(text);
    QQmlProperty(it, QStringLiteral("iconName")).write(iconName);
    QQmlProperty(it, QStringLiteral("enabled")).write(enabled);

    QQmlProperty signal(it, QStringLiteral("onTriggered"));
    CHECK_QML_SIGNAL_PROPERTY(signal, menuItemComponent->url());
    QObject::connect(it, signal.method(), menuItemHandler, QMetaMethod::fromSignal(&MenuItemHandler::triggered));
    menuItemComponent->completeCreate();

    QObject *menu = menuItemHandler->parent();
    it->setParent(menu);

    QQmlListReference entries(menu, defaultPropertyName(menu), qmlEngine(m_view));
    if (entries.isValid())
        entries.append(it);
}

void UIDelegatesManager::addMenuSeparator(QObject *menu)
{
    if (!ensureComponentLoaded(MenuSeparator))
        return;

    QQmlContext *itemContext = qmlContext(m_view);
    QObject *sep = menuSeparatorComponent->create(itemContext);
    sep->setParent(menu);

    QQmlListReference entries(menu, defaultPropertyName(menu), qmlEngine(m_view));
    if (entries.isValid())
        entries.append(sep);
}

QObject *UIDelegatesManager::addMenu(QObject *parentMenu, const QString &title, const QPoint& pos)
{

    if (!ensureComponentLoaded(Menu))
        return 0;
    QQmlContext *context = qmlContext(m_view);
    QObject *menu = menuComponent->beginCreate(context);
    // Useful when not using Qt Quick Controls' Menu
    if (QQuickItem* item = qobject_cast<QQuickItem*>(menu))
        item->setParentItem(m_view);

    if (!title.isEmpty())
        QQmlProperty(menu, QStringLiteral("title")).write(title);
    if (!pos.isNull())
        QQmlProperty(menu, QStringLiteral("pos")).write(pos);
    if (!parentMenu) {
        QQmlProperty doneSignal(menu, QStringLiteral("onDone"));
        static int deleteLaterIndex = menu->metaObject()->indexOfSlot("deleteLater()");
        if (doneSignal.isSignalProperty())
            QObject::connect(menu, doneSignal.method(), menu, menu->metaObject()->method(deleteLaterIndex));
    } else {
        menu->setParent(parentMenu);

        QQmlListReference entries(parentMenu, defaultPropertyName(parentMenu), qmlEngine(m_view));
        if (entries.isValid())
            entries.append(menu);
    }
    menuComponent->completeCreate();
    return menu;
}

#define ASSIGN_DIALOG_COMPONENT_DATA_CASE_STATEMENT(TYPE, COMPONENT) \
    case TYPE:\
        dialogComponent = COMPONENT##Component; \
        break;


void UIDelegatesManager::showDialog(QSharedPointer<JavaScriptDialogController> dialogController)
{
    Q_ASSERT(!dialogController.isNull());
    ComponentType dialogComponentType = Invalid;
    QString title;
    switch (dialogController->type()) {
    case WebContentsAdapterClient::AlertDialog:
        dialogComponentType = AlertDialog;
        title = QObject::tr("Javascript Alert - %1").arg(m_view->url().toString());
        break;
    case WebContentsAdapterClient::ConfirmDialog:
        dialogComponentType = ConfirmDialog;
        title = QObject::tr("Javascript Confirm - %1").arg(m_view->url().toString());
        break;
    case WebContentsAdapterClient::PromptDialog:
        dialogComponentType = PromptDialog;
        title = QObject::tr("Javascript Prompt - %1").arg(m_view->url().toString());
        break;
    case WebContentsAdapterClient::InternalAuthorizationDialog:
        dialogComponentType = ConfirmDialog;
        title = dialogController->title();
        break;
    default:
        Q_UNREACHABLE();
    }

    if (!ensureComponentLoaded(dialogComponentType)) {
        // Let the controller know it couldn't be loaded
        qWarning("Failed to load dialog, rejecting.");
        dialogController->reject();
        return;
    }

    QQmlComponent *dialogComponent = Q_NULLPTR;
    switch (dialogComponentType) {
    FOR_EACH_COMPONENT_TYPE(ASSIGN_DIALOG_COMPONENT_DATA_CASE_STATEMENT, NO_SEPARATOR)
    default:
        Q_UNREACHABLE();
    }

    QQmlContext *context = qmlContext(m_view);
    QObject *dialog = dialogComponent->beginCreate(context);
    dialog->setParent(m_view);
    QQmlProperty textProp(dialog, QStringLiteral("text"));
    textProp.write(dialogController->message());

    QQmlProperty titleProp(dialog, QStringLiteral("title"));
    titleProp.write(title);

    QQmlProperty acceptSignal(dialog, QStringLiteral("onAccepted"));
    QQmlProperty rejectSignal(dialog, QStringLiteral("onRejected"));
    CHECK_QML_SIGNAL_PROPERTY(acceptSignal, dialogComponent->url());
    CHECK_QML_SIGNAL_PROPERTY(rejectSignal, dialogComponent->url());

    static int acceptIndex = dialogController->metaObject()->indexOfSlot("accept()");
    QObject::connect(dialog, acceptSignal.method(), dialogController.data(), dialogController->metaObject()->method(acceptIndex));
    static int rejectIndex = dialogController->metaObject()->indexOfSlot("reject()");
    QObject::connect(dialog, rejectSignal.method(), dialogController.data(), dialogController->metaObject()->method(rejectIndex));

    if (dialogComponentType == PromptDialog) {
        QQmlProperty promptProp(dialog, QStringLiteral("prompt"));
        promptProp.write(dialogController->defaultPrompt());
        QQmlProperty inputSignal(dialog, QStringLiteral("onInput"));
        CHECK_QML_SIGNAL_PROPERTY(inputSignal, dialogComponent->url());
        static int setTextIndex = dialogController->metaObject()->indexOfSlot("textProvided(QString)");
        QObject::connect(dialog, inputSignal.method(), dialogController.data(), dialogController->metaObject()->method(setTextIndex));
        QQmlProperty closingSignal(dialog, QStringLiteral("onClosing"));
        QObject::connect(dialog, closingSignal.method(), dialogController.data(), dialogController->metaObject()->method(rejectIndex));
    }

    dialogComponent->completeCreate();

    QObject::connect(dialogController.data(), &JavaScriptDialogController::dialogCloseRequested, dialog, &QObject::deleteLater);

    QMetaObject::invokeMethod(dialog, "open");
}

namespace {
class FilePickerController : public QObject {
    Q_OBJECT
public:
    FilePickerController(WebContentsAdapterClient::FileChooserMode, const QExplicitlySharedDataPointer<WebContentsAdapter> &, QObject * = 0);

public Q_SLOTS:
    void accepted(const QJSValue &files);
    void rejected();

private:
    QExplicitlySharedDataPointer<WebContentsAdapter> m_adapter;
    WebContentsAdapterClient::FileChooserMode m_mode;

};


FilePickerController::FilePickerController(WebContentsAdapterClient::FileChooserMode mode, const QExplicitlySharedDataPointer<WebContentsAdapter> &adapter, QObject *parent)
    : QObject(parent)
    , m_adapter(adapter)
    , m_mode(mode)
{
}

void FilePickerController::accepted(const QJSValue &filesValue)
{
    QStringList stringList;
    int length = filesValue.property(QStringLiteral("length")).toInt();
    for (int i = 0; i < length; i++) {
        stringList.append(QUrl(filesValue.property(i).toString()).toLocalFile());
    }

    m_adapter->filesSelectedInChooser(stringList, m_mode);
}

void FilePickerController::rejected()
{
    m_adapter->filesSelectedInChooser(QStringList(), m_mode);
}

} // namespace


void UIDelegatesManager::showFilePicker(WebContentsAdapterClient::FileChooserMode mode, const QString &defaultFileName, const QStringList &acceptedMimeTypes, const QExplicitlySharedDataPointer<WebContentsAdapter> &adapter)
{
    Q_UNUSED(defaultFileName);
    Q_UNUSED(acceptedMimeTypes);

    if (!ensureComponentLoaded(FilePicker))
        return;

    QQmlContext *context = qmlContext(m_view);
    QObject *filePicker = filePickerComponent->beginCreate(context);
    if (QQuickItem* item = qobject_cast<QQuickItem*>(filePicker))
        item->setParentItem(m_view);
    filePicker->setParent(m_view);
    filePickerComponent->completeCreate();

    // Fine-tune some properties depending on the mode.
    switch (mode) {
    case WebContentsAdapterClient::Open:
        break;
    case WebContentsAdapterClient::Save:
        filePicker->setProperty("selectExisting", false);
        break;
    case WebContentsAdapterClient::OpenMultiple:
        filePicker->setProperty("selectMultiple", true);
        break;
    case WebContentsAdapterClient::UploadFolder:
        filePicker->setProperty("selectFolder", true);
        break;
    default:
        Q_UNREACHABLE();
    }

    FilePickerController *controller = new FilePickerController(mode, adapter, filePicker);
    QQmlProperty filesPickedSignal(filePicker, QStringLiteral("onFilesSelected"));
    CHECK_QML_SIGNAL_PROPERTY(filesPickedSignal, filePickerComponent->url());
    QQmlProperty rejectSignal(filePicker, QStringLiteral("onRejected"));
    CHECK_QML_SIGNAL_PROPERTY(rejectSignal, filePickerComponent->url());
    static int acceptedIndex = controller->metaObject()->indexOfSlot("accepted(QJSValue)");
    QObject::connect(filePicker, filesPickedSignal.method(), controller, controller->metaObject()->method(acceptedIndex));
    static int rejectedIndex = controller->metaObject()->indexOfSlot("rejected()");
    QObject::connect(filePicker, rejectSignal.method(), controller, controller->metaObject()->method(rejectedIndex));

    // delete when done.
    static int deleteLaterIndex = filePicker->metaObject()->indexOfSlot("deleteLater()");
    QObject::connect(filePicker, filesPickedSignal.method(), filePicker, filePicker->metaObject()->method(deleteLaterIndex));
    QObject::connect(filePicker, rejectSignal.method(), filePicker, filePicker->metaObject()->method(deleteLaterIndex));

    QMetaObject::invokeMethod(filePicker, "open");
}

#include "ui_delegates_manager.moc"
