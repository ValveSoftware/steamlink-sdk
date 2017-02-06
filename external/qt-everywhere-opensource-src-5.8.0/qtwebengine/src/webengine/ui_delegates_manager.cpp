/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "ui_delegates_manager.h"

#include "api/qquickwebengineview_p.h"
#include <authentication_dialog_controller.h>
#include <color_chooser_controller.h>
#include <file_picker_controller.h>
#include <javascript_dialog_controller.h>
#include <web_contents_adapter_client.h>

#include <QFileInfo>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QCursor>
#include <QList>
#include <QScreen>
#include <QGuiApplication>

// Uncomment for QML debugging
//#define UI_DELEGATES_DEBUG

namespace QtWebEngineCore {

#define NO_SEPARATOR
#if defined(Q_OS_WIN)
#define FILE_NAME_CASE_STATEMENT(TYPE, COMPONENT) \
    case UIDelegatesManager::TYPE:\
        return QString::fromLatin1(#TYPE ##".qml");
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

static QPoint calculateToolTipPosition(QPoint &position, QSize &toolTip) {
    QRect screen;
    QList<QScreen *> screens = QGuiApplication::screens();
    Q_FOREACH (const QScreen *src, screens)
        if (src->availableGeometry().contains(position))
            screen = src->availableGeometry();

    position += QPoint(2, 16);

    if (position.x() + toolTip.width() > screen.x() + screen.width())
        position.rx() -= 4 + toolTip.width();
    if (position.y() + toolTip.height() > screen.y() + screen.height())
        position.ry() -= 24 + toolTip.height();
    if (position.y() < screen.y())
        position.setY(screen.y());
    if (position.x() + toolTip.width() > screen.x() + screen.width())
        position.setX(screen.x() + screen.width() - toolTip.width());
    if (position.x() < screen.x())
        position.setX(screen.x());
    if (position.y() + toolTip.height() > screen.y() + screen.height())
        position.setY(screen.y() + screen.height() - toolTip.height());

    return position;
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

#define COMPONENT_MEMBER_INIT(TYPE, COMPONENT) \
    , COMPONENT##Component(0)

UIDelegatesManager::UIDelegatesManager(QQuickWebEngineView *view)
    : m_view(view)
    , m_messageBubbleItem(0)
    , m_toolTip(nullptr)
    FOR_EACH_COMPONENT_TYPE(COMPONENT_MEMBER_INIT, NO_SEPARATOR)
{
}

UIDelegatesManager::~UIDelegatesManager()
{
}

#define COMPONENT_MEMBER_CASE_STATEMENT(TYPE, COMPONENT) \
    case TYPE: \
        component = &COMPONENT##Component; \
        break;

bool UIDelegatesManager::initializeImportDirs(QStringList &dirs, QQmlEngine *engine) {
    foreach (const QString &path, engine->importPathList()) {
        QFileInfo fi(path % QLatin1String("/QtWebEngine/Controls1Delegates/"));
        if (fi.exists()) {
            dirs << fi.absolutePath();
            return true;
        }
    }
    return false;
}

bool UIDelegatesManager::ensureComponentLoaded(ComponentType type)
{
    QQmlEngine* engine = qmlEngine(m_view);
    if (m_importDirs.isEmpty() && !initializeImportDirs(m_importDirs, engine))
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

    foreach (const QString &importDir, m_importDirs) {
        QFileInfo fi(importDir % QLatin1Char('/') % fileName);
        if (!fi.exists())
            continue;
        // FIXME: handle async loading
        *component = (new QQmlComponent(engine, QUrl::fromLocalFile(fi.absoluteFilePath()),
                                        QQmlComponent::PreferSynchronous, m_view));

        if ((*component)->status() != QQmlComponent::Ready) {
            foreach (const QQmlError &err, (*component)->errors())
                qWarning("QtWebEngine: component error: %s\n", qPrintable(err.toString()));
            delete *component;
            *component = nullptr;
            return false;
        }
        return true;
    }
    return false;
}

#define CHECK_QML_SIGNAL_PROPERTY(prop, location) \
    if (!prop.isSignalProperty()) \
        qWarning("%s is missing %s signal property.\n", qPrintable(location.toString()), qPrintable(prop.name()));

void UIDelegatesManager::addMenuItem(MenuItemHandler *menuItemHandler, const QString &text, const QString &iconName, bool enabled,
                                     bool checkable, bool checked)
{
    Q_ASSERT(menuItemHandler);
    if (!ensureComponentLoaded(MenuItem))
        return;
    QObject *it = menuItemComponent->beginCreate(qmlContext(m_view));

    QQmlProperty(it, QStringLiteral("text")).write(text);
    QQmlProperty(it, QStringLiteral("iconName")).write(iconName);
    QQmlProperty(it, QStringLiteral("enabled")).write(enabled);
    QQmlProperty(it, QStringLiteral("checkable")).write(checkable);
    QQmlProperty(it, QStringLiteral("checked")).write(checked);

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
    Q_ASSERT(parentMenu);
    if (!ensureComponentLoaded(Menu))
        return nullptr;
    QQmlContext *context = qmlContext(m_view);
    QObject *menu = menuComponent->beginCreate(context);
    // set visual parent for non-Window-based menus
    if (QQuickItem *item = qobject_cast<QQuickItem*>(menu))
        item->setParentItem(m_view);

    if (!title.isEmpty())
        QQmlProperty(menu, QStringLiteral("title")).write(title);
    if (!pos.isNull())
        QQmlProperty(menu, QStringLiteral("pos")).write(pos);

    menu->setParent(parentMenu);

    QQmlProperty doneSignal(menu, QStringLiteral("onDone"));
    static int deleteLaterIndex = menu->metaObject()->indexOfSlot("deleteLater()");
    CHECK_QML_SIGNAL_PROPERTY(doneSignal, menuComponent->url());
    QObject::connect(menu, doneSignal.method(), menu, menu->metaObject()->method(deleteLaterIndex));

    QQmlListReference entries(parentMenu, defaultPropertyName(parentMenu), qmlEngine(m_view));
    if (entries.isValid())
        entries.append(menu);

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
        title = tr("Javascript Alert - %1").arg(m_view->url().toString());
        break;
    case WebContentsAdapterClient::ConfirmDialog:
        dialogComponentType = ConfirmDialog;
        title = tr("Javascript Confirm - %1").arg(m_view->url().toString());
        break;
    case WebContentsAdapterClient::PromptDialog:
        dialogComponentType = PromptDialog;
        title = tr("Javascript Prompt - %1").arg(m_view->url().toString());
        break;
    case WebContentsAdapterClient::UnloadDialog:
        dialogComponentType = ConfirmDialog;
        title = tr("Are you sure you want to leave this page?");
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
    // set visual parent for non-Window-based dialogs
    if (QQuickItem *item = qobject_cast<QQuickItem*>(dialog))
        item->setParentItem(m_view);
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
    }

    dialogComponent->completeCreate();

    QObject::connect(dialogController.data(), &JavaScriptDialogController::dialogCloseRequested, dialog, &QObject::deleteLater);

    QMetaObject::invokeMethod(dialog, "open");
}

void UIDelegatesManager::showColorDialog(QSharedPointer<ColorChooserController> controller)
{
    if (!ensureComponentLoaded(ColorDialog)) {
        // Let the controller know it couldn't be loaded
        qWarning("Failed to load dialog, rejecting.");
        controller->reject();
        return;
    }

    QQmlContext *context = qmlContext(m_view);
    QObject *colorDialog = colorDialogComponent->beginCreate(context);
    if (QQuickItem *item = qobject_cast<QQuickItem*>(colorDialog))
        item->setParentItem(m_view);
    colorDialog->setParent(m_view);

    if (controller->initialColor().isValid())
        colorDialog->setProperty("color", controller->initialColor());

    QQmlProperty selectedColorSignal(colorDialog, QStringLiteral("onSelectedColor"));
    CHECK_QML_SIGNAL_PROPERTY(selectedColorSignal, colorDialogComponent->url());
    QQmlProperty rejectedSignal(colorDialog, QStringLiteral("onRejected"));
    CHECK_QML_SIGNAL_PROPERTY(rejectedSignal, colorDialogComponent->url());

    static int acceptIndex = controller->metaObject()->indexOfSlot("accept(QVariant)");
    QObject::connect(colorDialog, selectedColorSignal.method(), controller.data(), controller->metaObject()->method(acceptIndex));
    static int rejectIndex = controller->metaObject()->indexOfSlot("reject()");
    QObject::connect(colorDialog, rejectedSignal.method(), controller.data(), controller->metaObject()->method(rejectIndex));

     // delete later
     static int deleteLaterIndex = colorDialog->metaObject()->indexOfSlot("deleteLater()");
     QObject::connect(colorDialog, selectedColorSignal.method(), colorDialog, colorDialog->metaObject()->method(deleteLaterIndex));
     QObject::connect(colorDialog, rejectedSignal.method(), colorDialog, colorDialog->metaObject()->method(deleteLaterIndex));

    colorDialogComponent->completeCreate();
    QMetaObject::invokeMethod(colorDialog, "open");
}

void UIDelegatesManager::showDialog(QSharedPointer<AuthenticationDialogController> dialogController)
{
    Q_ASSERT(!dialogController.isNull());

    if (!ensureComponentLoaded(AuthenticationDialog)) {
        // Let the controller know it couldn't be loaded
        qWarning("Failed to load authentication dialog, rejecting.");
        dialogController->reject();
        return;
    }

    QQmlContext *context = qmlContext(m_view);
    QObject *authenticationDialog = authenticationDialogComponent->beginCreate(context);
    // set visual parent for non-Window-based dialogs
    if (QQuickItem *item = qobject_cast<QQuickItem*>(authenticationDialog))
        item->setParentItem(m_view);
    authenticationDialog->setParent(m_view);

    QString introMessage;
    if (dialogController->isProxy()) {
        introMessage = tr("Connect to proxy \"%1\" using:");
        introMessage = introMessage.arg(dialogController->host().toHtmlEscaped());
    } else {
        const QUrl url = dialogController->url();
        introMessage = tr("Enter username and password for \"%1\" at %2://%3");
        introMessage = introMessage.arg(dialogController->realm(), url.scheme(), url.host());
    }
    QQmlProperty textProp(authenticationDialog, QStringLiteral("text"));
    textProp.write(introMessage);

    QQmlProperty acceptSignal(authenticationDialog, QStringLiteral("onAccepted"));
    QQmlProperty rejectSignal(authenticationDialog, QStringLiteral("onRejected"));
    CHECK_QML_SIGNAL_PROPERTY(acceptSignal, authenticationDialogComponent->url());
    CHECK_QML_SIGNAL_PROPERTY(rejectSignal, authenticationDialogComponent->url());

    static int acceptIndex = dialogController->metaObject()->indexOfSlot("accept(QString,QString)");
    static int deleteLaterIndex = authenticationDialog->metaObject()->indexOfSlot("deleteLater()");
    QObject::connect(authenticationDialog, acceptSignal.method(), dialogController.data(), dialogController->metaObject()->method(acceptIndex));
    QObject::connect(authenticationDialog, acceptSignal.method(), authenticationDialog, authenticationDialog->metaObject()->method(deleteLaterIndex));
    static int rejectIndex = dialogController->metaObject()->indexOfSlot("reject()");
    QObject::connect(authenticationDialog, rejectSignal.method(), dialogController.data(), dialogController->metaObject()->method(rejectIndex));
    QObject::connect(authenticationDialog, rejectSignal.method(), authenticationDialog, authenticationDialog->metaObject()->method(deleteLaterIndex));

    authenticationDialogComponent->completeCreate();
    QMetaObject::invokeMethod(authenticationDialog, "open");
}

void UIDelegatesManager::showFilePicker(QSharedPointer<FilePickerController> controller)
{

    if (!ensureComponentLoaded(FilePicker))
        return;

    QQmlContext *context = qmlContext(m_view);
    QObject *filePicker = filePickerComponent->beginCreate(context);
    if (QQuickItem *item = qobject_cast<QQuickItem*>(filePicker))
        item->setParentItem(m_view);
    filePicker->setParent(m_view);
    filePickerComponent->completeCreate();

    // Fine-tune some properties depending on the mode.
    switch (controller->mode()) {
    case FilePickerController::Open:
        break;
    case FilePickerController::Save:
        filePicker->setProperty("selectExisting", false);
        break;
    case FilePickerController::OpenMultiple:
        filePicker->setProperty("selectMultiple", true);
        break;
    case FilePickerController::UploadFolder:
        filePicker->setProperty("selectFolder", true);
        break;
    default:
        Q_UNREACHABLE();
    }

    QQmlProperty filesPickedSignal(filePicker, QStringLiteral("onFilesSelected"));
    CHECK_QML_SIGNAL_PROPERTY(filesPickedSignal, filePickerComponent->url());
    QQmlProperty rejectSignal(filePicker, QStringLiteral("onRejected"));
    CHECK_QML_SIGNAL_PROPERTY(rejectSignal, filePickerComponent->url());
    static int acceptedIndex = controller->metaObject()->indexOfSlot("accepted(QVariant)");
    QObject::connect(filePicker, filesPickedSignal.method(), controller.data(), controller->metaObject()->method(acceptedIndex));
    static int rejectedIndex = controller->metaObject()->indexOfSlot("rejected()");
    QObject::connect(filePicker, rejectSignal.method(), controller.data(), controller->metaObject()->method(rejectedIndex));

    // delete when done.
    static int deleteLaterIndex = filePicker->metaObject()->indexOfSlot("deleteLater()");
    QObject::connect(filePicker, filesPickedSignal.method(), filePicker, filePicker->metaObject()->method(deleteLaterIndex));
    QObject::connect(filePicker, rejectSignal.method(), filePicker, filePicker->metaObject()->method(deleteLaterIndex));

    QMetaObject::invokeMethod(filePicker, "open");
}

void UIDelegatesManager::showMenu(QObject *menu)
{
     QMetaObject::invokeMethod(menu, "popup");
}

void UIDelegatesManager::showMessageBubble(const QRect &anchor, const QString &mainText, const QString &subText)
{
    if (!ensureComponentLoaded(MessageBubble))
        return;

    Q_ASSERT(m_messageBubbleItem.isNull());

    QQmlContext *context = qmlContext(m_view);
    m_messageBubbleItem.reset(qobject_cast<QQuickItem *>(messageBubbleComponent->beginCreate(context)));
    m_messageBubbleItem->setParentItem(m_view);
    messageBubbleComponent->completeCreate();

    QQmlProperty(m_messageBubbleItem.data(), QStringLiteral("maxWidth")).write(anchor.size().width());
    QQmlProperty(m_messageBubbleItem.data(), QStringLiteral("mainText")).write(mainText);
    QQmlProperty(m_messageBubbleItem.data(), QStringLiteral("subText")).write(subText);
    QQmlProperty(m_messageBubbleItem.data(), QStringLiteral("x")).write(anchor.x());
    QQmlProperty(m_messageBubbleItem.data(), QStringLiteral("y")).write(anchor.y() + anchor.size().height());
}

void UIDelegatesManager::hideMessageBubble()
{
    m_messageBubbleItem.reset();
}

void UIDelegatesManager::moveMessageBubble(const QRect &anchor)
{
    if (m_messageBubbleItem.isNull())
        return;

    QQmlProperty(m_messageBubbleItem.data(), QStringLiteral("x")).write(anchor.x());
    QQmlProperty(m_messageBubbleItem.data(), QStringLiteral("y")).write(anchor.y() + anchor.size().height());
}

void UIDelegatesManager::showToolTip(const QString &text)
{
    if (!ensureComponentLoaded(ToolTip))
        return;

    if (text.isEmpty()) {
        m_toolTip.reset();
        return;
    }

    if (!m_toolTip.isNull())
        return;

    QQmlContext *context = qmlContext(m_view);
    m_toolTip.reset(toolTipComponent->beginCreate(context));
    if (QQuickItem *item = qobject_cast<QQuickItem *>(m_toolTip.data()))
        item->setParentItem(m_view);
    m_toolTip->setParent(m_view);
    toolTipComponent->completeCreate();

    QQmlProperty(m_toolTip.data(), QStringLiteral("text")).write(text);

    int height = QQmlProperty(m_toolTip.data(), QStringLiteral("height")).read().toInt();
    int width = QQmlProperty(m_toolTip.data(), QStringLiteral("width")).read().toInt();
    QSize toolTipSize(width, height);
    QPoint position = m_view->cursor().pos();
    position = m_view->mapFromGlobal(calculateToolTipPosition(position, toolTipSize)).toPoint();

    QQmlProperty(m_toolTip.data(), QStringLiteral("x")).write(position.x());
    QQmlProperty(m_toolTip.data(), QStringLiteral("y")).write(position.y());

    QMetaObject::invokeMethod(m_toolTip.data(), "open");
}

UI2DelegatesManager::UI2DelegatesManager(QQuickWebEngineView *view) : UIDelegatesManager(view)
{

}

bool UI2DelegatesManager::initializeImportDirs(QStringList &dirs, QQmlEngine *engine)
{
    foreach (const QString &path, engine->importPathList()) {
        QFileInfo fi1(path % QLatin1String("/QtWebEngine/Controls1Delegates/"));
        QFileInfo fi2(path % QLatin1String("/QtWebEngine/Controls2Delegates/"));
        if (fi1.exists() && fi2.exists()) {
            dirs << fi2.absolutePath() << fi1.absolutePath();
            return true;
        }
    }
    return false;
}

QObject *UI2DelegatesManager::addMenu(QObject *parentMenu, const QString &title, const QPoint &pos)
{
    Q_ASSERT(parentMenu);
    if (!ensureComponentLoaded(Menu))
        return nullptr;
    QQmlContext *context = qmlContext(m_view);
    QObject *menu = menuComponent->beginCreate(context);

    // set visual parent for non-Window-based menus
    if (QQuickItem *item = qobject_cast<QQuickItem*>(menu))
        item->setParentItem(m_view);

    if (!title.isEmpty())
        menu->setProperty("title", title);
    if (!pos.isNull()) {
        menu->setProperty("x", pos.x());
        menu->setProperty("y", pos.y());
    }

    menu->setParent(parentMenu);
    QQmlProperty doneSignal(menu, QStringLiteral("onDone"));
    CHECK_QML_SIGNAL_PROPERTY(doneSignal, menuComponent->url())
    static int deleteLaterIndex = menu->metaObject()->indexOfSlot("deleteLater()");
    QObject::connect(menu, doneSignal.method(), menu, menu->metaObject()->method(deleteLaterIndex));
    menuComponent->completeCreate();
    return menu;
}

void UI2DelegatesManager::addMenuItem(MenuItemHandler *menuItemHandler, const QString &text,
                                      const QString &/*iconName*/, bool enabled,
                                      bool checkable, bool checked)
{
    Q_ASSERT(menuItemHandler);
    if (!ensureComponentLoaded(MenuItem))
        return;

    QObject *it = menuItemComponent->beginCreate(qmlContext(m_view));

    it->setProperty("text", text);
    it->setProperty("enabled", enabled);
    it->setProperty("checked", checked);
    it->setProperty("checkable", checkable);

    QQmlProperty signal(it, QStringLiteral("onTriggered"));
    CHECK_QML_SIGNAL_PROPERTY(signal, menuItemComponent->url());
    QObject::connect(it, signal.method(), menuItemHandler,
                     QMetaMethod::fromSignal(&MenuItemHandler::triggered));
    menuItemComponent->completeCreate();

    QObject *menu = menuItemHandler->parent();
    it->setParent(menu);

    QQmlListReference entries(menu, defaultPropertyName(menu), qmlEngine(m_view));
    if (entries.isValid())
        entries.append(it);
}

void UI2DelegatesManager::showMenu(QObject *menu)
{
    QMetaObject::invokeMethod(menu, "open");
}

} // namespace QtWebEngineCore
