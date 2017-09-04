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

#include "javascript_dialog_controller.h"
#include "javascript_dialog_controller_p.h"

#include"javascript_dialog_manager_qt.h"
#include "type_conversion.h"

namespace QtWebEngineCore {

void JavaScriptDialogControllerPrivate::dialogFinished(bool accepted, const base::string16 &promptValue)
{
    // Clear the queue first as this could result in the engine asking us to run another dialog,
    // but hold a shared pointer so the dialog does not get deleted prematurely when running in-process.
    QSharedPointer<JavaScriptDialogController> dialog = JavaScriptDialogManagerQt::GetInstance()->takeDialogForContents(contents);

    callback.Run(accepted, promptValue);
}

JavaScriptDialogControllerPrivate::JavaScriptDialogControllerPrivate(WebContentsAdapterClient::JavascriptDialogType t, const QString &msg, const QString &prompt
                                                                     , const QString &title, const QUrl &securityOrigin
                                                                     , const content::JavaScriptDialogManager::DialogClosedCallback &cb, content::WebContents *c)
    : type(t)
    , message(msg)
    , defaultPrompt(prompt)
    , securityOrigin(securityOrigin)
    , title(title)
    , callback(cb)
    , contents(c)
{
}

JavaScriptDialogController::~JavaScriptDialogController()
{
}

QString JavaScriptDialogController::message() const
{
    return d->message;
}

QString JavaScriptDialogController::defaultPrompt() const
{
    return d->defaultPrompt;
}

QString JavaScriptDialogController::title() const
{
    return d->title;
}

WebContentsAdapterClient::JavascriptDialogType JavaScriptDialogController::type() const
{
    return d->type;
}

QUrl JavaScriptDialogController::securityOrigin() const
{
    return d->securityOrigin;
}

void JavaScriptDialogController::textProvided(const QString &text)
{
    d->userInput = text;
}

void JavaScriptDialogController::accept()
{
    d->dialogFinished(true, toString16(d->userInput));
}

void JavaScriptDialogController::reject()
{
    d->dialogFinished(false, toString16(d->defaultPrompt));
}

JavaScriptDialogController::JavaScriptDialogController(JavaScriptDialogControllerPrivate *dd)
{
    Q_ASSERT(dd);
    d.reset(dd);
}

} // namespace QtWebEngineCore
