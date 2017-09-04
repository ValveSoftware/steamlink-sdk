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

#include "qquickwebenginedialogrequests_p.h"
#include "authentication_dialog_controller.h"
#include "javascript_dialog_controller.h"
#include "color_chooser_controller.h"
#include "file_picker_controller.h"
#include "web_contents_adapter_client.h"

QT_BEGIN_NAMESPACE

using namespace QtWebEngineCore;

ASSERT_ENUMS_MATCH(WebContentsAdapterClient::AlertDialog,
                   QQuickWebEngineJavaScriptDialogRequest::DialogTypeAlert)
ASSERT_ENUMS_MATCH(WebContentsAdapterClient::ConfirmDialog,
                   QQuickWebEngineJavaScriptDialogRequest::DialogTypeConfirm)
ASSERT_ENUMS_MATCH(WebContentsAdapterClient::PromptDialog,
                   QQuickWebEngineJavaScriptDialogRequest::DialogTypePrompt)
ASSERT_ENUMS_MATCH(WebContentsAdapterClient::UnloadDialog,
                   QQuickWebEngineJavaScriptDialogRequest::DialogTypeBeforeUnload)

ASSERT_ENUMS_MATCH(FilePickerController::Open,
                   QQuickWebEngineFileDialogRequest::FileModeOpen)
ASSERT_ENUMS_MATCH(FilePickerController::OpenMultiple,
                   QQuickWebEngineFileDialogRequest::FileModeOpenMultiple)
ASSERT_ENUMS_MATCH(FilePickerController::UploadFolder,
                   QQuickWebEngineFileDialogRequest::FileModeUploadFolder)
ASSERT_ENUMS_MATCH(FilePickerController::Save,
                   QQuickWebEngineFileDialogRequest::FileModeSave)

/*!
    \qmltype AuthenticationDialogRequest
    \instantiates QQuickWebEngineAuthenticationDialogRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.4

    \brief A request for providing authentication credentials required
    by proxies or HTTP servers.

    An AuthenticationDialogRequest is passed as an argument of the
    WebEngineView::authenticationDialogRequested signal. It is generated
    when basic HTTP or proxy authentication is required. The type
    of authentication can be checked with the \l type property.

    The \l accepted property of the request indicates whether the request
    is handled by the user code or the default dialog should be displayed.
    If you set the \l accepted property to \c true, make sure to call
    either \l dialogAccept() or \l dialogReject() afterwards.

    The following code uses a custom dialog to handle the request:
    \code

    WebEngineView {
        // ...
        onAuthenticationDialogRequested: function(request) {
            request.accepted = true;
            myDialog.request = request // keep the reference to the request
            myDialog.accept.connect(request.dialogAccept);
            myDialog.reject.connect(request.dialogReject);
            myDialog.visible = true;
        }
        // ...
    }

    \endcode
*/

QQuickWebEngineAuthenticationDialogRequest::QQuickWebEngineAuthenticationDialogRequest(
        QSharedPointer<AuthenticationDialogController> controller,
        QObject *parent):
    QObject(parent)
  , m_controller(controller.toWeakRef())
  , m_url(controller->url())
  , m_realm(controller->realm())
  , m_type(controller->isProxy() ? AuthenticationTypeProxy
                                 : AuthenticationTypeHTTP)
  , m_host(controller->host())
  , m_accepted(false)
{

}

QQuickWebEngineAuthenticationDialogRequest::~QQuickWebEngineAuthenticationDialogRequest()
{
}

/*!
    \qmlproperty url AuthenticationDialogRequest::url
    \readonly

    The URL of the HTTP request for which authentication was requested.
    In case of proxy authentication, this is a request URL which is proxied
    via host.

    \sa proxyHost
*/

QUrl QQuickWebEngineAuthenticationDialogRequest::url() const
{
    return m_url;
}

/*!
    \qmlproperty string AuthenticationDialogRequest::realm
    \readonly

    The HTTP authentication realm attribute value of the \c WWW-Authenticate
    header. Empty if \l type is AuthenticationTypeProxy.
*/

QString QQuickWebEngineAuthenticationDialogRequest::realm() const
{
    return m_realm;
}

/*!
    \qmlproperty string AuthenticationDialogRequest::proxyHost
    \readonly

    The hostname of the authentication proxy.
    Empty if \l type is AuthenticationTypeHTTP.
*/

QString QQuickWebEngineAuthenticationDialogRequest::proxyHost() const
{
    return m_host;
}

/*!
    \qmlproperty enumeration AuthenticationDialogRequest::type
    \readonly

    The type of the authentication request.

    \value  WebEngineAuthenticationDialogRequest.AuthenticationTypeHTTP
            HTTP authentication.
    \value  WebEngineAuthenticationDialogRequest.AuthenticationTypeProxy
            Proxy authentication.
*/

QQuickWebEngineAuthenticationDialogRequest::AuthenticationType
QQuickWebEngineAuthenticationDialogRequest::type() const
{
    return m_type;
}

/*!
    \qmlproperty bool AuthenticationDialogRequest::accepted

    Indicates whether the authentication dialog request has been
    accepted by the signal handler.

    If the property is \c false after any signal handlers
    for WebEngineView::authenticationDialogRequested have been executed,
    a default authentication dialog will be shown.
    To prevent this, set \c{request.accepted} to \c true.

    The default is \c false.
*/

bool QQuickWebEngineAuthenticationDialogRequest::isAccepted() const
{
    return m_accepted;
}

void QQuickWebEngineAuthenticationDialogRequest::setAccepted(bool accepted)
{
    m_accepted = accepted;
}

/*!
    \qmlmethod void AuthenticationDialogRequest::dialogAccept(string username, string password)

    This function notifies the engine that the user accepted the dialog,
    providing the \a username and the \a password required for authentication.
*/

void QQuickWebEngineAuthenticationDialogRequest::dialogAccept(const QString &user,
                                                              const QString &password)
{
    m_accepted = true;
    QSharedPointer<AuthenticationDialogController> controller
            = m_controller.toStrongRef();
    if (controller)
        controller->accept(user,password);
}

/*!
    \qmlmethod void AuthenticationDialogRequest::dialogReject()

    This function notifies the engine that the user rejected the dialog and the
    authentication shall not proceed.
*/

void QQuickWebEngineAuthenticationDialogRequest::dialogReject()
{
    m_accepted = true;
    QSharedPointer<AuthenticationDialogController> controller
            = m_controller.toStrongRef();
    if (controller)
        controller->reject();
}

///////////////////////////////////////////////////////////////////////////////

/*!
    \qmltype JavaScriptDialogRequest
    \instantiates QQuickWebEngineJavaScriptDialogRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.4

    \brief A request for showing an alert, a confirmation, or a prompt dialog
    from within JavaScript to the user.

    A JavaScriptDialogRequest is passed as an argument of the
    WebEngineView::javaScriptDialogRequested signal. The request is emitted
    if JavaScript on the page calls HTML5's
    \l{https://www.w3.org/TR/html5/webappapis.html#simple-dialogs}{Simple Dialogs}
    API, or in response to HTML5's
    \l {https://www.w3.org/TR/html5/browsers.html#beforeunloadevent}{BeforeUnloadEvent}.
    The type of a particular dialog can be checked with the \l type property.

    The \l accepted property of the request indicates whether the request
    is handled by the user code or the default dialog should be displayed.
    If you set the \l accepted property to \c true, make sure to call either
    \l dialogAccept() or \l dialogReject() afterwards. The JavaScript call
    causing the request will be blocked until then.

    The following code uses a custom dialog to handle the request:

    \code
    WebEngineView {
        // ...
        onJavaScriptDialogRequested: function(request) {
            request.accepted = true;
            myDialog.request = request // keep the reference to the request
            myDialog.accept.connect(request.dialogAccept);
            myDialog.reject.connect(request.dialogReject);
            myDialog.visible = true;
        }
        // ...
    }
    \endcode
*/

QQuickWebEngineJavaScriptDialogRequest::QQuickWebEngineJavaScriptDialogRequest(
        QSharedPointer<JavaScriptDialogController> controller, QObject *parent):
    QObject(parent)
  , m_controller(controller.toWeakRef())
  , m_message(controller->message())
  , m_defaultPrompt(controller->defaultPrompt())
  , m_title(controller->title())
  , m_type(static_cast<QQuickWebEngineJavaScriptDialogRequest::DialogType>(controller->type()))
  , m_securityOrigin(controller->securityOrigin())
  , m_accepted(false)
{
}

QQuickWebEngineJavaScriptDialogRequest::~QQuickWebEngineJavaScriptDialogRequest()
{

}

/*!
    \qmlproperty string JavaScriptDialogRequest::message
    \readonly

    The message to be shown to the user.
*/

QString QQuickWebEngineJavaScriptDialogRequest::message() const
{
    return m_message;
}

/*!
    \qmlproperty string JavaScriptDialogRequest::defaultText
    \readonly

    The default prompt text, if the requested dialog is a prompt.
*/


QString QQuickWebEngineJavaScriptDialogRequest::defaultText() const
{
    return m_defaultPrompt;
}

/*!
    \qmlproperty string JavaScriptDialogRequest::title
    \readonly

    A default title for the dialog.
*/

QString QQuickWebEngineJavaScriptDialogRequest::title() const
{
    return m_title;
}

/*!
    \qmlproperty enumeration JavaScriptDialogRequest::type
    \readonly

    Returns the type of the requested dialog box, see HTML5's

    \l{https://www.w3.org/TR/html5/webappapis.html#simple-dialogs}{Simple Dialogs}.

    \value  JavaScriptDialogRequest.DialogTypeAlert
            A JavaScript alert dialog.
    \value  JavaScriptDialogRequest.DialogTypeConfirm
            A JavaScript confirmation dialog.
    \value  JavaScriptDialogRequest.DialogTypePrompt
            A JavaScript prompt dialog.
    \value  JavaScriptDialogRequest.DialogTypeUnload
            The users should be asked if they want to leave the page.
*/

QQuickWebEngineJavaScriptDialogRequest::DialogType QQuickWebEngineJavaScriptDialogRequest::type() const
{
    return m_type;
}

/*!
    \qmlproperty url JavaScriptDialogRequest::securityOrigin
    \readonly

    The URL of the security origin.
*/

QUrl QQuickWebEngineJavaScriptDialogRequest::securityOrigin() const
{
    return m_securityOrigin;
}

/*!
    \qmlproperty bool JavaScriptDialogRequest::accepted

    Indicates whether the JavaScript dialog request has been
    accepted by the signal handler.

    If the property is \c false after any signal handlers
    for WebEngineView::javaScriptDialogRequested have been executed,
    a default dialog will be shown.
    To prevent this, set \c{request.accepted} to \c true.

    The default is \c false.
*/

bool QQuickWebEngineJavaScriptDialogRequest::isAccepted() const
{
    return m_accepted;
}

void QQuickWebEngineJavaScriptDialogRequest::setAccepted(bool accepted)
{
    m_accepted = accepted;
}

/*!
    \qmlmethod void JavaScriptDialogRequest::dialogAccept()

    This function notifies the engine that the user accepted the dialog.
*/

/*!
    \qmlmethod void JavaScriptDialogRequest::dialogAccept(string text)

    This function notifies the engine that the user accepted the dialog,
    providing the \a text in case of a prompt message box.
*/

void QQuickWebEngineJavaScriptDialogRequest::dialogAccept(const QString& text)
{
    m_accepted = true;
    QSharedPointer<JavaScriptDialogController> controller
            = m_controller.toStrongRef();
    if (controller) {
        controller->textProvided(text);
        controller->accept();
    }
}

/*!
    \qmlmethod void JavaScriptDialogRequest::dialogReject()

    This function notifies the engine that the user rejected the dialog.
*/

void QQuickWebEngineJavaScriptDialogRequest::dialogReject()
{
    m_accepted = true;
    QSharedPointer<JavaScriptDialogController> controller
            = m_controller.toStrongRef();
    if (controller)
        controller->reject();
}

///////////////////////////////////////////////////////////////////////////////

/*!
    \qmltype ColorDialogRequest
    \instantiates QQuickWebEngineColorDialogRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.4

    \brief A request for selecting a color by the user.

    A ColorDialogRequest is passed as an argument of the
    WebEngineView::colorDialogRequested signal. It is generated when
    a color picker dialog is requested. See
    \l { https://www.w3.org/TR/html5/forms.html#color-state-(type=color)}
    {HTML5 Color State}.

    The \l accepted property of the request indicates whether the request
    is handled by the user code or the default dialog should be displayed.
    If you set the \l accepted property to \c true, make sure to call either
    \l dialogAccept() or \l dialogReject() afterwards.

    The following code uses a custom dialog to handle the request:

    \code
    WebEngineView {
        // ...
        onColorDialogRequested: function(request) {
            request.accepted = true;
            myDialog.request = request // keep the reference to the request
            myDialog.accept.connect(request.dialogAccept);
            myDialog.reject.connect(request.dialogReject);
            myDialog.visible = true;
        }
        // ...
    }
    \endcode
*/

QQuickWebEngineColorDialogRequest::QQuickWebEngineColorDialogRequest(
        QSharedPointer<ColorChooserController> controller, QObject *parent):
    QObject(parent)
  , m_controller(controller.toWeakRef())
  , m_color(controller->initialColor())
  , m_accepted(false)
{

}

QQuickWebEngineColorDialogRequest::~QQuickWebEngineColorDialogRequest()
{

}

/*!
    \qmlproperty color ColorDialogRequest::color
    \readonly

    The default color to be selected in the dialog.
*/

QColor QQuickWebEngineColorDialogRequest::color() const
{
    return m_color;
}

/*!
    \qmlproperty bool ColorDialogRequest::accepted

    Indicates whether the color picker dialog request has been
    accepted by the signal handler.

    If the property is \c false after any signal handlers
    for WebEngineView::colorDialogRequested have been executed,
    a default color picker dialog will be shown.
    To prevent this, set \c{request.accepted} to \c true.

    The default is \c false.
*/

bool QQuickWebEngineColorDialogRequest::isAccepted() const
{
    return m_accepted;
}

void QQuickWebEngineColorDialogRequest::setAccepted(bool accepted)
{
    m_accepted = accepted;
}


/*!
    \qmlmethod void ColorDialogRequest::dialogAccept(color color)

    This function notifies the engine that the user accepted the dialog,
    providing the \a color.
*/

void QQuickWebEngineColorDialogRequest::dialogAccept(const QColor &color)
{
    m_accepted = true;
    QSharedPointer<ColorChooserController> controller = m_controller.toStrongRef();
    if (controller)
        controller->accept(color);
}

/*!
    \qmlmethod void ColorDialogRequest::dialogReject()

    This function notifies the engine that the user rejected the dialog.
*/

void QQuickWebEngineColorDialogRequest::dialogReject()
{
    m_accepted = true;
    QSharedPointer<ColorChooserController> controller = m_controller.toStrongRef();
    if (controller)
        controller->reject();
}

///////////////////////////////////////////////////////////////////////////////

/*!
    \qmltype FileDialogRequest
    \instantiates QQuickWebEngineFileDialogRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.4

    \brief A request for letting the user choose a (new or existing) file or
    directory.

    A FileDialogRequest is passed as an argument of the
    WebEngineView::fileDialogRequested signal. It is generated
    when the file dialog is requested by the input element.
    See \l {https://www.w3.org/TR/html5/forms.html#file-upload-state-(type=file)}{File Upload state}.

    The \l accepted property of the request indicates whether the request
    is handled by the user code or the default dialog should be displayed.
    If you set the \l accepted property to \c true, make sure to call either
    \l dialogAccept() or \l dialogReject() afterwards.

    The following code uses a custom dialog to handle the request:

    \code
    WebEngineView {
        // ...
        onFileDialogRequested: function(request) {
            request.accepted = true;
            myDialog.request = request // keep the reference to the request
            myDialog.accept.connect(request.dialogAccept);
            myDialog.reject.connect(request.dialogReject);
            myDialog.visible = true;
        }
        // ...
    }
    \endcode
*/

QQuickWebEngineFileDialogRequest::QQuickWebEngineFileDialogRequest(
        QSharedPointer<FilePickerController> controller, QObject *parent):
    QObject(parent)
  , m_controller(controller.toWeakRef())
  , m_filename(controller->defaultFileName())
  , m_acceptedMimeTypes(controller->acceptedMimeTypes())
  , m_mode(static_cast<QQuickWebEngineFileDialogRequest::FileMode>(controller->mode()))
  , m_accepted(false)
{

}

QQuickWebEngineFileDialogRequest::~QQuickWebEngineFileDialogRequest()
{

}

/*!
    \qmlproperty stringlist FileDialogRequest::acceptedMimeTypes
    \readonly

    A list of MIME types specified in the input element. The selection
    should be restricted to only these types of files.
*/

QStringList QQuickWebEngineFileDialogRequest::acceptedMimeTypes() const
{
    return m_acceptedMimeTypes;
}

/*!
    \qmlproperty string FileDialogRequest::defaultFileName
    \readonly

    The default name of the file to be selected in the dialog.
*/

QString QQuickWebEngineFileDialogRequest::defaultFileName() const
{
    return m_filename;
}

/*!
    \qmlproperty enumeration FileDialogRequest::mode
    \readonly

    The mode of the file dialog.

    \value  FileDialogRequest.FileModeOpen
            Allows users to specify a single existing file.
    \value  FileDialogRequest.FileModeOpenMultiple
            Allows users to specify multiple existing files.
    \value  FileDialogRequest.FileModeUploadFolder
            Allows users to specify a single existing folder for upload.
    \value  FileDialogRequest.FileModeSave
            Allows users to specify a non-existing file. If an existing file
            is selected, the users should be informed that the file is going
            to be overwritten.
*/

QQuickWebEngineFileDialogRequest::FileMode QQuickWebEngineFileDialogRequest::mode() const
{
    return m_mode;
}

/*!
    \qmlproperty bool FileDialogRequest::accepted

    Indicates whether the file picker dialog request has been
    handled by the signal handler.

    If the property is \c false after any signal handlers
    for WebEngineView::fileDialogRequested have been executed,
    a default file picker dialog will be shown.
    To prevent this, set \c{request.accepted} to \c true.

    The default is \c false.
*/

bool QQuickWebEngineFileDialogRequest::isAccepted() const
{
    return m_accepted;
}

void QQuickWebEngineFileDialogRequest::setAccepted(bool accepted)
{
    m_accepted = accepted;
}

/*!
    \qmlmethod void FileDialogRequest::dialogAccept(stringlist files)

    This function needs to be called when the user accepted the dialog with
    \a files.
*/

void QQuickWebEngineFileDialogRequest::dialogAccept(const QStringList &files)
{
    m_accepted = true;
    QSharedPointer<FilePickerController> controller = m_controller.toStrongRef();
    if (controller)
        controller->accepted(files);
}

/*!
    \qmlmethod void FileDialogRequest::dialogReject()

    This function needs to be called when the user did not accepted the dialog.
*/

void QQuickWebEngineFileDialogRequest::dialogReject()
{
    m_accepted = true;
    QSharedPointer<FilePickerController> controller = m_controller.toStrongRef();
    if (controller)
        controller->rejected();
}

///////////////////////////////////////////////////////////////////////////////

/*!
    \qmltype FormValidationMessageRequest
    \instantiates QQuickWebEngineFormValidationMessageRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.4

    \brief A request for showing a HTML5 form validation message to the user.

    A FormValidationMessageRequest is passed as an argument of the
    WebEngineView::formValidationMessageRequested signal. It is generated when
    the handling of the validation message is requested.

    The \l accepted property of the request indicates whether the request
    is handled by the user code or the default message should be displayed.

    The following code uses a custom message to handle the request:

    \code
    WebEngineView {
        // ...
        onFormValidationMessageRequested: function(request) {
            request.accepted = true;
            switch (request.type) {
                case FormValidationMessageRequest.Show:
                    validationMessage.text = request.text;
                    validationMessage.x = request.x;
                    validationMessage.y = request.y
                    validationMessage.visible = true;
                    break;
                 case FormValidationMessageRequest.Move:
                    break;
                 case FormValidationMessageRequest.Hide:
                    validationMessage.visible = false;
                    break;
                 }
        }
        // ...
    }
    \endcode
*/

QQuickWebEngineFormValidationMessageRequest::QQuickWebEngineFormValidationMessageRequest(
        QQuickWebEngineFormValidationMessageRequest::RequestType type, const QRect& anchor,
        const QString &mainText, const QString &subText, QObject *parent):
    QObject(parent)
  , m_anchor(anchor)
  , m_mainText(mainText)
  , m_subText(subText)
  , m_type(type)
  , m_accepted(false)
{

}

QQuickWebEngineFormValidationMessageRequest::~QQuickWebEngineFormValidationMessageRequest()
{

}

/*!
    \qmlproperty rectangle FormValidationMessageRequest::anchor
    \readonly

    An anchor of an element in the viewport for which the form
    validation message should be displayed.
*/

QRect QQuickWebEngineFormValidationMessageRequest::anchor() const
{
    return m_anchor;
}

/*!
    \qmlproperty bool FormValidationMessageRequest::text
    \readonly

    The text of the form validation message.
*/


QString QQuickWebEngineFormValidationMessageRequest::text() const
{
    return m_mainText;
}

/*!
    \qmlproperty bool FormValidationMessageRequest::subText
    \readonly

    The subtext of the form validation message.
*/


QString QQuickWebEngineFormValidationMessageRequest::subText() const
{
    return m_subText;
}

/*!
    \qmlproperty enumeration FormValidationMessageRequest::type
    \readonly

    The type of the form validation message request.

    \value  ValidationMessageRequest.Show
            The form validation message should be shown.
    \value  ValidationMessageRequest.Hide
            The form validation message should be hidden.
    \value  ValidationMessageRequest.Move
            The form validation message should be moved.
*/

QQuickWebEngineFormValidationMessageRequest::RequestType QQuickWebEngineFormValidationMessageRequest::type() const
{
    return m_type;
}

/*!
    \qmlproperty bool FormValidationMessageRequest::accepted

    Indicates whether the form validation request has been
    accepted by the signal handler.

    If the property is \c false after any signal handlers
    for WebEngineView::validationMessageRequested have been executed,
    a default file validation message will be shown.
    To prevent this, set \c {request.accepted} to \c true.

    The default is \c false.
*/

bool QQuickWebEngineFormValidationMessageRequest::isAccepted() const
{
    return m_accepted;
}

void QQuickWebEngineFormValidationMessageRequest::setAccepted(bool accepted)
{
    m_accepted = accepted;
}

QT_END_NAMESPACE
