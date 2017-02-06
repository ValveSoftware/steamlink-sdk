/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Platform module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickplatformmessagedialog_p.h"

#include <QtQml/qqmlinfo.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MessageDialog
    \inherits Dialog
    \instantiates QQuickPlatformMessageDialog
    \inqmlmodule Qt.labs.platform
    \since 5.8
    \brief A native message dialog.

    The MessageDialog type provides a QML API for native platform message dialogs.

    \image qtlabsplatform-messagedialog-android.png

    A message dialog is used to inform the user, or ask the user a question.
    A message dialog displays a primary \l text to alert the user to a situation,
    an \l {informativeText}{informative text} to further explain the alert or to
    ask the user a question, and an optional \l {detailedText}{detailed text} to
    provide even more data if the user requests it. A message box can also display
    a configurable set of \l buttons for accepting a user response.

    To show a message dialog, construct an instance of MessageDialog, set the
    desired properties, and call \l {Dialog::}{open()}.

    \code
    MessageDialog {
        buttons: MessageDialog.Ok
        text: "The document has been modified."
    }
    \endcode

    The user must click the \uicontrol OK button to dismiss the message dialog.
    A modal message dialog blocks the rest of the GUI until the message is
    dismissed.

    A more elaborate approach than just alerting the user to an event is to
    also ask the user what to do about it. Store the question in the
    \l {informativeText}{informative text} property, and specify the \l buttons
    property to the set of buttons you want as the set of user responses. The
    buttons are specified by combining values using the bitwise OR operator. The
    display order for the buttons is platform dependent.

    \code
    MessageDialog {
        text: "The document has been modified."
        informativeText: "Do you want to save your changes?"
        buttons: MessageDialog.Ok | MessageDialog.Cancel

        onAccepted: document.save()
    }
    \endcode

    \image qtlabsplatform-messagedialog-informative-android.png

    The \l clicked() signal passes the information of which button was clicked.

    A native platform message dialog is currently available on the following platforms:

    \list
    \li iOS
    \li Android
    \li WinRT
    \endlist

    \input includes/widgets.qdocinc 1

    \labs
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::clicked(button)

    This signal is emitted when a dialog \a button is clicked.

    \sa buttons
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::okClicked()

    This signal is emitted when \uicontrol Ok is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::saveClicked()

    This signal is emitted when \uicontrol Save is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::saveAllClicked()

    This signal is emitted when \uicontrol {Save All} is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::openClicked()

    This signal is emitted when \uicontrol Open is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::yesClicked()

    This signal is emitted when \uicontrol Yes is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::yesToAllClicked()

    This signal is emitted when \uicontrol {Yes To All} is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::noClicked()

    This signal is emitted when \uicontrol No is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::noToAllClicked()

    This signal is emitted when \uicontrol {No To All} is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::abortClicked()

    This signal is emitted when \uicontrol Abort is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::retryClicked()

    This signal is emitted when \uicontrol Retry is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::ignoreClicked()

    This signal is emitted when \uicontrol Ignore is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::closeClicked()

    This signal is emitted when \uicontrol Close is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::cancelClicked()

    This signal is emitted when \uicontrol Cancel is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::discardClicked()

    This signal is emitted when \uicontrol Discard is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::helpClicked()

    This signal is emitted when \uicontrol Help is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::applyClicked()

    This signal is emitted when \uicontrol Apply is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::resetClicked()

    This signal is emitted when \uicontrol Reset is clicked.
*/

/*!
    \qmlsignal Qt.labs.platform::MessageDialog::restoreDefaultsClicked()

    This signal is emitted when \uicontrol {Restore Defaults} is clicked.
*/

QQuickPlatformMessageDialog::QQuickPlatformMessageDialog(QObject *parent)
    : QQuickPlatformDialog(QPlatformTheme::MessageDialog, parent),
      m_options(QMessageDialogOptions::create())
{
}

/*!
    \qmlproperty string Qt.labs.platform::MessageDialog::text

    This property holds the text to be displayed on the message dialog.

    \sa informativeText, detailedText
*/
QString QQuickPlatformMessageDialog::text() const
{
    return m_options->text();
}

void QQuickPlatformMessageDialog::setText(const QString &text)
{
    if (m_options->text() == text)
        return;

    m_options->setText(text);
    emit textChanged();
}

/*!
    \qmlproperty string Qt.labs.platform::MessageDialog::informativeText

    This property holds the informative text that provides a fuller description for the message.

    Informative text can be used to expand upon the \l text to give more information to the user.

    \sa text, detailedText
*/
QString QQuickPlatformMessageDialog::informativeText() const
{
    return m_options->informativeText();
}

void QQuickPlatformMessageDialog::setInformativeText(const QString &text)
{
    if (m_options->informativeText() == text)
        return;

    m_options->setInformativeText(text);
    emit informativeTextChanged();
}

/*!
    \qmlproperty string Qt.labs.platform::MessageDialog::detailedText

    This property holds the text to be displayed in the details area.

    \sa text, informativeText
*/
QString QQuickPlatformMessageDialog::detailedText() const
{
    return m_options->detailedText();
}

void QQuickPlatformMessageDialog::setDetailedText(const QString &text)
{
    if (m_options->detailedText() == text)
        return;

    m_options->setDetailedText(text);
    emit detailedTextChanged();
}

/*!
    \qmlproperty flags Qt.labs.platform::MessageDialog::buttons

    This property holds a combination of buttons that are used by the message dialog.
    The default value is \c MessageDialog.NoButton.

    Possible flags:
    \value MessageDialog.Ok An "OK" button defined with the \c AcceptRole.
    \value MessageDialog.Open An "Open" button defined with the \c AcceptRole.
    \value MessageDialog.Save A "Save" button defined with the \c AcceptRole.
    \value MessageDialog.Cancel A "Cancel" button defined with the \c RejectRole.
    \value MessageDialog.Close A "Close" button defined with the \c RejectRole.
    \value MessageDialog.Discard A "Discard" or "Don't Save" button, depending on the platform, defined with the \c DestructiveRole.
    \value MessageDialog.Apply An "Apply" button defined with the \c ApplyRole.
    \value MessageDialog.Reset A "Reset" button defined with the \c ResetRole.
    \value MessageDialog.RestoreDefaults A "Restore Defaults" button defined with the \c ResetRole.
    \value MessageDialog.Help A "Help" button defined with the \c HelpRole.
    \value MessageDialog.SaveAll A "Save All" button defined with the \c AcceptRole.
    \value MessageDialog.Yes A "Yes" button defined with the \c YesRole.
    \value MessageDialog.YesToAll A "Yes to All" button defined with the \c YesRole.
    \value MessageDialog.No A "No" button defined with the \c NoRole.
    \value MessageDialog.NoToAll A "No to All" button defined with the \c NoRole.
    \value MessageDialog.Abort An "Abort" button defined with the \c RejectRole.
    \value MessageDialog.Retry A "Retry" button defined with the \c AcceptRole.
    \value MessageDialog.Ignore An "Ignore" button defined with the \c AcceptRole.
    \value MessageDialog.NoButton The dialog has no buttons.

    \sa clicked()
*/
QPlatformDialogHelper::StandardButtons QQuickPlatformMessageDialog::buttons() const
{
    return m_options->standardButtons();
}

void QQuickPlatformMessageDialog::setButtons(QPlatformDialogHelper::StandardButtons buttons)
{
    if (m_options->standardButtons() == buttons)
        return;

    m_options->setStandardButtons(buttons);
    emit buttonsChanged();
}

void QQuickPlatformMessageDialog::onCreate(QPlatformDialogHelper *dialog)
{
    if (QPlatformMessageDialogHelper *messageDialog = qobject_cast<QPlatformMessageDialogHelper *>(dialog)) {
        connect(messageDialog, &QPlatformMessageDialogHelper::clicked, this, &QQuickPlatformMessageDialog::handleClick);
        messageDialog->setOptions(m_options);
    }
}

void QQuickPlatformMessageDialog::onShow(QPlatformDialogHelper *dialog)
{
    m_options->setWindowTitle(title());
    if (QPlatformMessageDialogHelper *messageDialog = qobject_cast<QPlatformMessageDialogHelper *>(dialog))
        messageDialog->setOptions(m_options);
}

void QQuickPlatformMessageDialog::handleClick(QPlatformDialogHelper::StandardButton button)
{
    done(button);
    emit clicked(button);

    switch (button) {
    case QPlatformDialogHelper::Ok: emit okClicked(); break;
    case QPlatformDialogHelper::Save: emit saveClicked(); break;
    case QPlatformDialogHelper::SaveAll: emit saveAllClicked(); break;
    case QPlatformDialogHelper::Open: emit openClicked(); break;
    case QPlatformDialogHelper::Yes: emit yesClicked(); break;
    case QPlatformDialogHelper::YesToAll: emit yesToAllClicked(); break;
    case QPlatformDialogHelper::No: emit noClicked(); break;
    case QPlatformDialogHelper::NoToAll: emit noToAllClicked(); break;
    case QPlatformDialogHelper::Abort: emit abortClicked(); break;
    case QPlatformDialogHelper::Retry: emit retryClicked(); break;
    case QPlatformDialogHelper::Ignore: emit ignoreClicked(); break;
    case QPlatformDialogHelper::Close: emit closeClicked(); break;
    case QPlatformDialogHelper::Cancel: emit cancelClicked(); break;
    case QPlatformDialogHelper::Discard: emit discardClicked(); break;
    case QPlatformDialogHelper::Help: emit helpClicked(); break;
    case QPlatformDialogHelper::Apply: emit applyClicked(); break;
    case QPlatformDialogHelper::Reset: emit resetClicked(); break;
    case QPlatformDialogHelper::RestoreDefaults: emit restoreDefaultsClicked(); break;
    default: qmlInfo(this) << "unknown button" << button; break;
    }
}

QT_END_NAMESPACE
