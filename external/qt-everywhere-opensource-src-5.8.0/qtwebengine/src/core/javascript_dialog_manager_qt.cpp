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

#include "javascript_dialog_manager_qt.h"

#include "javascript_dialog_controller.h"
#include "javascript_dialog_controller_p.h"
#include "web_contents_view_qt.h"
#include "type_conversion.h"

#include "base/memory/singleton.h"

namespace QtWebEngineCore {

Q_STATIC_ASSERT_X(static_cast<int>(content::JAVASCRIPT_MESSAGE_TYPE_PROMPT) == static_cast<int>(WebContentsAdapterClient::PromptDialog), "These enums should be in sync.");

JavaScriptDialogManagerQt *JavaScriptDialogManagerQt::GetInstance()
{
    return base::Singleton<JavaScriptDialogManagerQt>::get();
}

void JavaScriptDialogManagerQt::RunJavaScriptDialog(content::WebContents *webContents, const GURL &originUrl, content::JavaScriptMessageType javascriptMessageType, const base::string16 &messageText, const base::string16 &defaultPromptText, const content::JavaScriptDialogManager::DialogClosedCallback &callback, bool *didSuppressMessage)
{
    WebContentsAdapterClient *client = WebContentsViewQt::from(static_cast<content::WebContentsImpl*>(webContents)->GetView())->client();
    if (!client) {
        if (didSuppressMessage)
            *didSuppressMessage = true;
        return;
    }

    WebContentsAdapterClient::JavascriptDialogType dialogType = static_cast<WebContentsAdapterClient::JavascriptDialogType>(javascriptMessageType);
    runDialogForContents(webContents, dialogType, toQt(messageText).toHtmlEscaped(), toQt(defaultPromptText).toHtmlEscaped(), toQt(originUrl.GetOrigin()), callback);
}

void JavaScriptDialogManagerQt::RunBeforeUnloadDialog(content::WebContents *webContents, bool isReload,
                                                      const content::JavaScriptDialogManager::DialogClosedCallback &callback) {
    Q_UNUSED(isReload);
    runDialogForContents(webContents, WebContentsAdapterClient::UnloadDialog, QString()/*toQt(messageText).toHtmlEscaped()*/, QString() , QUrl(), callback);
}

bool JavaScriptDialogManagerQt::HandleJavaScriptDialog(content::WebContents *contents, bool accept, const base::string16 *promptOverride)
{
    QSharedPointer<JavaScriptDialogController> dialog = m_activeDialogs.value(contents);
    if (!dialog)
        return false;
    dialog->d->dialogFinished(accept, promptOverride ? *promptOverride : base::string16());
    takeDialogForContents(contents);
    return true;
}

void JavaScriptDialogManagerQt::runDialogForContents(content::WebContents *webContents, WebContentsAdapterClient::JavascriptDialogType type
                                                     , const QString &messageText, const QString &defaultPrompt, const QUrl &origin
                                                     , const content::JavaScriptDialogManager::DialogClosedCallback &callback, const QString &title)
{
    WebContentsAdapterClient *client = WebContentsViewQt::from(static_cast<content::WebContentsImpl*>(webContents)->GetView())->client();
    if (!client)
        return;

    JavaScriptDialogControllerPrivate *dialogData = new JavaScriptDialogControllerPrivate(type, messageText, defaultPrompt, title, origin, callback, webContents);
    QSharedPointer<JavaScriptDialogController> dialog(new JavaScriptDialogController(dialogData));

    // We shouldn't get new dialogs for a given WebContents until we gave back a result.
    Q_ASSERT(!m_activeDialogs.contains(webContents));
    m_activeDialogs.insert(webContents, dialog);

    client->javascriptDialog(dialog);

}

QSharedPointer<JavaScriptDialogController> JavaScriptDialogManagerQt::takeDialogForContents(content::WebContents *contents)
{
    QSharedPointer<JavaScriptDialogController> dialog = m_activeDialogs.take(contents);
    if (dialog)
        Q_EMIT dialog->dialogCloseRequested();
    return dialog;
}

} // namespace QtWebEngineCore
