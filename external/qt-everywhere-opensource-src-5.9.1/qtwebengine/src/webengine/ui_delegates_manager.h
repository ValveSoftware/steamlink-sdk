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

#ifndef UI_DELEGATES_MANAGER_H
#define UI_DELEGATES_MANAGER_H

#include "qglobal.h"
#include "web_contents_adapter.h"
#include "web_contents_adapter_client.h"

#include <QCoreApplication>
#include <QExplicitlySharedDataPointer>
#include <QPoint>
#include <QSharedPointer>

#define FOR_EACH_COMPONENT_TYPE(F, SEPARATOR) \
    F(Menu, menu) SEPARATOR \
    F(MenuItem, menuItem) SEPARATOR \
    F(MenuSeparator, menuSeparator) SEPARATOR \
    F(AlertDialog, alertDialog) SEPARATOR \
    F(ColorDialog, colorDialog) SEPARATOR \
    F(ConfirmDialog, confirmDialog) SEPARATOR \
    F(PromptDialog, promptDialog) SEPARATOR \
    F(FilePicker, filePicker) SEPARATOR \
    F(MessageBubble, messageBubble) SEPARATOR \
    F(AuthenticationDialog, authenticationDialog) SEPARATOR \
    F(ToolTip, toolTip) SEPARATOR \

#define COMMA_SEPARATOR ,
#define SEMICOLON_SEPARATOR ;
#define ENUM_DECLARATION(TYPE, COMPONENT) \
    TYPE
#define MEMBER_DECLARATION(TYPE, COMPONENT) \
    QQmlComponent *COMPONENT##Component

QT_BEGIN_NAMESPACE
class QQmlContext;
class QQmlComponent;
class QQuickItem;
class QQuickWebEngineView;
class QQmlEngine;
QT_END_NAMESPACE

namespace QtWebEngineCore {
class AuthenticationDialogController;
class JavaScriptDialogController;
class FilePickerController;

const char *defaultPropertyName(QObject *obj);

class MenuItemHandler : public QObject {
Q_OBJECT
public:
    MenuItemHandler(QObject *parent);

Q_SIGNALS:
    void triggered();
};

class UIDelegatesManager
{
    Q_DECLARE_TR_FUNCTIONS(UIDelegatesManager)
public:
    enum ComponentType {
        Invalid = -1,
        FOR_EACH_COMPONENT_TYPE(ENUM_DECLARATION, COMMA_SEPARATOR)
        ComponentTypeCount
    };

    UIDelegatesManager(QQuickWebEngineView *);
    virtual ~UIDelegatesManager();

    virtual bool initializeImportDirs(QStringList &dirs, QQmlEngine *engine);
    virtual void addMenuItem(MenuItemHandler *menuItemHandler, const QString &text,
                             const QString &iconName = QString(),
                             bool enabled = true,
                             bool checkable = false, bool checked = true);
    void addMenuSeparator(QObject *menu);
    virtual QObject *addMenu(QObject *parentMenu, const QString &title,
                             const QPoint &pos = QPoint());
    QQmlContext *creationContextForComponent(QQmlComponent *);
    void showColorDialog(QSharedPointer<ColorChooserController>);
    void showDialog(QSharedPointer<JavaScriptDialogController>);
    void showDialog(QSharedPointer<AuthenticationDialogController>);
    void showFilePicker(QSharedPointer<FilePickerController>);
    virtual void showMenu(QObject *menu);
    void showMessageBubble(const QRect &anchor, const QString &mainText,
                           const QString &subText);
    void hideMessageBubble();
    void moveMessageBubble(const QRect &anchor);
    void showToolTip(const QString &text);

protected:
    bool ensureComponentLoaded(ComponentType);

    QQuickWebEngineView *m_view;
    QScopedPointer<QQuickItem> m_messageBubbleItem;
    QScopedPointer<QObject> m_toolTip;
    QStringList m_importDirs;

    FOR_EACH_COMPONENT_TYPE(MEMBER_DECLARATION, SEMICOLON_SEPARATOR)

    Q_DISABLE_COPY(UIDelegatesManager)

};

// delegate manager for qtquickcontrols2 with fallback to qtquickcontrols1

class UI2DelegatesManager : public UIDelegatesManager
{
public:
    UI2DelegatesManager(QQuickWebEngineView *);
    bool initializeImportDirs(QStringList &dirs, QQmlEngine *engine) override;
    QObject *addMenu(QObject *parentMenu, const QString &title,
                     const QPoint &pos = QPoint()) override;
    void addMenuItem(MenuItemHandler *menuItemHandler, const QString &text,
                     const QString &iconName = QString(),
                     bool enabled = true,
                     bool checkable = false, bool checked = false) override;
    void showMenu(QObject *menu) override;
    Q_DISABLE_COPY(UI2DelegatesManager)

};

} // namespace QtWebEngineCore

#endif // UI_DELEGATES_MANAGER_H
