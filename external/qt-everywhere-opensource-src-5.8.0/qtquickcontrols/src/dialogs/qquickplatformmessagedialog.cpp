/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
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

#include "qquickplatformmessagedialog_p.h"
#include "qquickitem.h"

#include <private/qguiapplication_p.h>
#include <QWindow>
#include <QQuickView>
#include <QQuickWindow>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MessageDialog
    \instantiates QQuickPlatformMessageDialog
    \inqmlmodule QtQuick.Dialogs
    \ingroup qtquickdialogs
    \brief Dialog component for displaying popup messages.
    \since 5.2

    The most basic use case for a MessageDialog is a popup alert. It also
    allows the user to respond in various ways depending on which buttons are
    enabled. The dialog is initially invisible. You need to set the properties
    as desired first, then set \l visible to \c true or call \l open().

    Here is a minimal example to show an alert and exit after the user
    responds:

    \qml
    import QtQuick 2.2
    import QtQuick.Dialogs 1.1

    MessageDialog {
        id: messageDialog
        title: "May I have your attention please"
        text: "It's so cool that you are using Qt Quick."
        onAccepted: {
            console.log("And of course you could only agree.")
            Qt.quit()
        }
        Component.onCompleted: visible = true
    }
    \endqml

    There are several possible handlers depending on which \l standardButtons
    the dialog has and the \l {QMessageBox::ButtonRole} {ButtonRole} of each.
    For example, the \l {rejected} {onRejected} handler will be called if the
    user presses a \gui Cancel, \gui Close or \gui Abort button.

    A MessageDialog window is automatically transient for its parent window. So
    whether you declare the dialog inside an \l Item or inside a \l Window, the
    dialog will appear centered over the window containing the item, or over
    the Window that you declared.

    The implementation of MessageDialog will be a platform message dialog if
    possible. If that isn't possible, then it will try to instantiate a
    \l QMessageBox. If that also isn't possible, then it will fall back to a QML
    implementation, \c DefaultMessageDialog.qml. In that case you can customize
    the appearance by editing this file. \c DefaultMessageDialog.qml contains a
    \l Rectangle to hold the dialog's contents, because certain embedded systems
    do not support multiple top-level windows. When the dialog becomes visible,
    it will automatically be wrapped in a \l Window if possible, or simply
    reparented on top of the main window if there can only be one window.
*/

/*!
    \qmlsignal MessageDialog::accepted()

    This signal is emitted when the user has pressed any button which has the
    \l {QMessageBox::AcceptRole} {AcceptRole}: \gui OK, \gui Open, \gui Save,
    \gui {Save All}, \gui Retry or \gui Ignore.

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal MessageDialog::rejected()

    This signal is emitted when the user has dismissed the dialog, by closing
    the dialog window, by pressing a \gui Cancel, \gui Close or \gui Abort
    button on the dialog, or by pressing the back button or the escape key.

    The corresponding handler is \c onRejected.
*/

/*!
    \qmlsignal MessageDialog::discard()

    This signal is emitted when the user has pressed the \gui Discard button.

    The corresponding handler is \c onDiscard.
*/

/*!
    \qmlsignal MessageDialog::help()

    This signal is emitted when the user has pressed the \gui Help button.
    Depending on platform, the dialog may not be automatically dismissed
    because the help that your application provides may need to be relevant to
    the text shown in this dialog in order to assist the user in making a
    decision. However on other platforms it's not possible to show a dialog and
    a help window at the same time. If you want to be sure that the dialog will
    close, you can set \l visible to \c false in your handler.

    The corresponding handler is \c onHelp.
*/

/*!
    \qmlsignal MessageDialog::yes()

    This signal is emitted when the user has pressed any button which has
    the \l {QMessageBox::YesRole} {YesRole}: \gui Yes or \gui {Yes to All}.

    The corresponding handler is \c onYes.
*/

/*!
    \qmlsignal MessageDialog::no()

    This signal is emitted when the user has pressed any button which has
    the \l {QMessageBox::NoRole} {NoRole}: \gui No or \gui {No to All}.

    The corresponding handler is \c onNo.
*/

/*!
    \qmlsignal MessageDialog::apply()

    This signal is emitted when the user has pressed the \gui Apply button.

    The corresponding handler is \c onApply.
*/

/*!
    \qmlsignal MessageDialog::reset()

    This signal is emitted when the user has pressed any button which has
    the \l {QMessageBox::ResetRole} {ResetRole}: \gui Reset or \gui {Restore Defaults}.

    The corresponding handler is \c onReset.
*/

/*! \qmlproperty StandardButton MessageDialog::clickedButton

    This property holds the button pressed by the user. Its value is
    one of the flags set for the standardButtons property.
*/

/*!
    \class QQuickPlatformMessageDialog
    \inmodule QtQuick.Dialogs
    \internal

    \brief The QQuickPlatformMessageDialog class provides a message dialog

    The dialog is implemented via the QPlatformMessageDialogHelper when possible;
    otherwise it falls back to a QMessageBox or a QML implementation.

    \since 5.2
*/

/*!
    Constructs a file dialog with parent window \a parent.
*/
QQuickPlatformMessageDialog::QQuickPlatformMessageDialog(QObject *parent) :
    QQuickAbstractMessageDialog(parent)
{
}

/*!
    Destroys the file dialog.
*/
QQuickPlatformMessageDialog::~QQuickPlatformMessageDialog()
{
    if (m_dlgHelper)
        m_dlgHelper->hide();
    delete m_dlgHelper;
}

QPlatformMessageDialogHelper *QQuickPlatformMessageDialog::helper()
{
    QQuickItem *parentItem = qobject_cast<QQuickItem *>(parent());
    if (parentItem)
        m_parentWindow = parentItem->window();

    if ( !m_dlgHelper && QGuiApplicationPrivate::platformTheme()->
            usePlatformNativeDialog(QPlatformTheme::MessageDialog) ) {
        m_dlgHelper = static_cast<QPlatformMessageDialogHelper *>(QGuiApplicationPrivate::platformTheme()
           ->createPlatformDialogHelper(QPlatformTheme::MessageDialog));
        if (!m_dlgHelper)
            return m_dlgHelper;
        // accept() shouldn't be emitted.  reject() happens only if the dialog is
        // dismissed by closing the window rather than by one of its button widgets.
        connect(m_dlgHelper, SIGNAL(accept()), this, SLOT(accept()));
        connect(m_dlgHelper, SIGNAL(reject()), this, SLOT(reject()));
        connect(m_dlgHelper, SIGNAL(clicked(QPlatformDialogHelper::StandardButton,QPlatformDialogHelper::ButtonRole)),
            this, SLOT(click(QPlatformDialogHelper::StandardButton,QPlatformDialogHelper::ButtonRole)));
    }

    return m_dlgHelper;
}

/*!
    \qmlproperty bool MessageDialog::visible

    This property holds whether the dialog is visible. By default this is
    \c false.

    \sa modality
*/

/*!
    \qmlproperty Qt::WindowModality MessageDialog::modality

    Whether the dialog should be shown modal with respect to the window
    containing the dialog's parent Item, modal with respect to the whole
    application, or non-modal.

    By default it is \c Qt.WindowModal.

    Modality does not mean that there are any blocking calls to wait for the
    dialog to be accepted or rejected; it's only that the user will be
    prevented from interacting with the parent window and/or the application
    windows until the dialog is dismissed.
*/

/*!
    \qmlmethod void MessageDialog::open()

    Shows the dialog to the user. It is equivalent to setting \l visible to
    \c true.
*/

/*!
    \qmlmethod void MessageDialog::close()

    Closes the dialog.
*/

/*!
    \qmlproperty string MessageDialog::title

    The title of the dialog window.
*/

/*!
    \qmlproperty string MessageDialog::text

    The primary text to be displayed.
*/

/*!
    \qmlproperty string MessageDialog::informativeText

    The informative text that provides a fuller description for the message.

    Informative text can be used to supplement the \c text to give more
    information to the user. Depending on the platform, it may appear in a
    smaller font below the text, or simply appended to the text.

    \sa {MessageDialog::text}{text}
*/

/*!
    \qmlproperty string MessageDialog::detailedText

    The text to be displayed in the details area, which is hidden by default.
    The user will then be able to press the \gui {Show Details...} button to
    make it visible.

    \sa {MessageDialog::text}{text}
*/

/*!
    \qmlproperty QQuickStandardIcon::Icon MessageDialog::icon

    The icon of the message box can be specified with one of these values:

    \table
    \row
    \li no icon
    \li \c StandardIcon.NoIcon
    \li For an unadorned text alert.
    \row
    \li \inlineimage ../images/question.png "Question icon"
    \li \c StandardIcon.Question
    \li For asking a question during normal operations.
    \row
    \li \image information.png
    \li \c StandardIcon.Information
    \li For reporting information about normal operations.
    \row
    \li \image warning.png
    \li \c StandardIcon.Warning
    \li For reporting non-critical errors.
    \row
    \li \image critical.png
    \li \c StandardIcon.Critical
    \li For reporting critical errors.
    \endtable

    The default is \c StandardIcon.NoIcon.

    The enum values are the same as in \l QMessageBox::Icon.
*/

/*!
    \qmlproperty StandardButtons MessageDialog::standardButtons

    The MessageDialog has a row of buttons along the bottom, each of which has
    a \l {QMessageBox::ButtonRole} {ButtonRole} that determines which signal
    will be emitted when the button is pressed. You can also find out which
    specific button was pressed after the fact via the \l clickedButton
    property. You can control which buttons are available by setting
    standardButtons to a bitwise-or combination of the following flags:

    \value StandardButton.Ok
           An \gui OK button defined with the
           \l {QMessageBox::}{AcceptRole}.
    \value StandardButton.Open
           An \gui Open button defined with the
           \l {QMessageBox::}{AcceptRole}.
    \value StandardButton.Save
           A \gui Save button defined with the
           \l {QMessageBox::}{AcceptRole}.
    \value StandardButton.Cancel
           A \gui Cancel button defined with the
           \l {QMessageBox::}{RejectRole}.
    \value StandardButton.Close
           A \gui Close button defined with the
           \l {QMessageBox::}{RejectRole}.
    \value StandardButton.Discard
           A \gui Discard or \gui {Don't Save} button,
           depending on the platform, defined with the
           \l {QMessageBox::}{DestructiveRole}.
    \value StandardButton.Apply
           An \gui Apply button defined with the
           \l {QMessageBox::}{ApplyRole}.
    \value StandardButton.Reset
           A \gui Reset button defined with the
           \l {QMessageBox::}{ResetRole}.
    \value StandardButton.RestoreDefaults
           A \gui {Restore Defaults} button defined with the
           \l {QMessageBox::}{ResetRole}.
    \value StandardButton.Help
           A \gui Help button defined with the
           \l {QMessageBox::}{HelpRole}.
    \value StandardButton.SaveAll
           A \gui {Save All} button defined with the
           \l {QMessageBox::}{AcceptRole}.
    \value StandardButton.Yes
           A \gui Yes button defined with the
           \l {QMessageBox::}{YesRole}.
    \value StandardButton.YesToAll
           A \gui {Yes to All} button defined with the
           \l {QMessageBox::}{YesRole}.
    \value StandardButton.No
           A \gui No button defined with the
           \l {QMessageBox::}{NoRole}.
    \value StandardButton.NoToAll
           A \gui {No to All} button defined with the
           \l {QMessageBox::}{NoRole}.
    \value StandardButton.Abort
           An \gui Abort button defined with the
           \l {QMessageBox::}{RejectRole}.
    \value StandardButton.Retry
           A \gui Retry button defined with the
           \l {QMessageBox::}{AcceptRole}.
    \value StandardButton.Ignore
           An \gui Ignore button defined with the
           \l {QMessageBox::}{AcceptRole}.

    For example the following dialog will ask a question with 5 possible answers:

    \qml
    import QtQuick 2.2
    import QtQuick.Dialogs 1.1

    MessageDialog {
        title: "Overwrite?"
        icon: StandardIcon.Question
        text: "file.txt already exists.  Replace?"
        detailedText: "To replace a file means that its existing contents will be lost. " +
            "The file that you are copying now will be copied over it instead."
        standardButtons: StandardButton.Yes | StandardButton.YesToAll |
            StandardButton.No | StandardButton.NoToAll | StandardButton.Abort
        Component.onCompleted: visible = true
        onYes: console.log("copied")
        onNo: console.log("didn't copy")
        onRejected: console.log("aborted")
    }
    \endqml

    \image replacefile.png

    The default is \c StandardButton.Ok.

    The enum values are the same as in \l QMessageBox::StandardButtons.
*/

QT_END_NAMESPACE
