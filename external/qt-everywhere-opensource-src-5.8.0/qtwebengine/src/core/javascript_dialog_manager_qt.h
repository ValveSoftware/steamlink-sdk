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
#ifndef JAVASCRIPT_DIALOG_MANAGER_QT_H
#define JAVASCRIPT_DIALOG_MANAGER_QT_H

#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/common/javascript_message_type.h"

#include "web_contents_adapter_client.h"

#include "qglobal.h"
#include <QMap>
#include <QSharedPointer>

namespace content {
class WebContents;
}

namespace QtWebEngineCore {
class JavaScriptDialogController;

class JavaScriptDialogManagerQt : public content::JavaScriptDialogManager
{
public:
    // For use with the Singleton helper class from chromium
    static JavaScriptDialogManagerQt *GetInstance();

    virtual void RunJavaScriptDialog(content::WebContents *, const GURL &, content::JavaScriptMessageType javascriptMessageType,
                                     const base::string16 &messageText, const base::string16 &defaultPromptText,
                                     const content::JavaScriptDialogManager::DialogClosedCallback &callback, bool *didSuppressMessage) Q_DECL_OVERRIDE;

    virtual void RunBeforeUnloadDialog(content::WebContents *, bool isReload,
                                       const content::JavaScriptDialogManager::DialogClosedCallback &callback) Q_DECL_OVERRIDE;
    virtual bool HandleJavaScriptDialog(content::WebContents *, bool accept, const base::string16 *promptOverride) Q_DECL_OVERRIDE;
    virtual void CancelActiveAndPendingDialogs(content::WebContents *contents) Q_DECL_OVERRIDE { takeDialogForContents(contents); }
    virtual void ResetDialogState(content::WebContents *contents) Q_DECL_OVERRIDE { takeDialogForContents(contents); }

    void runDialogForContents(content::WebContents *, WebContentsAdapterClient::JavascriptDialogType, const QString &messageText, const QString &defaultPrompt
                              , const QUrl &,const content::JavaScriptDialogManager::DialogClosedCallback &callback, const QString &title = QString());
    QSharedPointer<JavaScriptDialogController> takeDialogForContents(content::WebContents *);

private:
    QMap<content::WebContents *, QSharedPointer<JavaScriptDialogController> > m_activeDialogs;

};

} // namespace QtWebEngineCore

#endif // JAVASCRIPT_DIALOG_MANAGER_QT_H

