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

#include "qquickplatformfolderdialog_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype FolderDialog
    \inherits Dialog
    \instantiates QQuickPlatformFolderDialog
    \inqmlmodule Qt.labs.platform
    \since 5.8
    \brief A native folder dialog.

    The FolderDialog type provides a QML API for native platform folder dialogs.

    \image qtlabsplatform-folderdialog-gtk.png

    To show a folder dialog, construct an instance of FolderDialog, set the
    desired properties, and call \l {Dialog::}{open()}. The \l currentFolder
    property can be used to determine the currently selected folder in the
    dialog. The \l folder property is updated only after the final selection
    has been made by accepting the dialog.

    \code
    MenuItem {
        text: "Open..."
        onTriggered: folderDialog.open()
    }

    FolderDialog {
        id: folderDialog
        currentFolder: viewer.folder
        folder: StandardPaths.standardLocations(StandardPaths.PicturesLocation)[0]
    }

    MyViewer {
        id: viewer
        folder: folderDialog.folder
    }
    \endcode

    \section2 Availability

    A native platform folder dialog is currently available on the following platforms:

    \list
    \li iOS
    \li Linux (when running with the GTK+ platform theme)
    \li macOS
    \li Windows
    \li WinRT
    \endlist

    \input includes/widgets.qdocinc 1

    \labs

    \sa FileDialog, StandardPaths
*/

QQuickPlatformFolderDialog::QQuickPlatformFolderDialog(QObject *parent)
    : QQuickPlatformDialog(QPlatformTheme::FileDialog, parent),
      m_options(QFileDialogOptions::create())
{
    m_options->setFileMode(QFileDialogOptions::Directory);
    m_options->setAcceptMode(QFileDialogOptions::AcceptOpen);
}

/*!
    \qmlproperty url Qt.labs.platform::FolderDialog::folder

    This property holds the final accepted folder.

    Unlike the \l currentFolder property, the \c folder property is not updated
    while the user is selecting folders in the dialog, but only after the final
    selection has been made. That is, when the user has clicked \uicontrol OK
    to accept a folder. Alternatively, the \l {Dialog::}{accepted()} signal
    can be handled to get the final selection.

    \sa currentFolder, {Dialog::}{accepted()}
*/
QUrl QQuickPlatformFolderDialog::folder() const
{
    return m_folder;
}

void QQuickPlatformFolderDialog::setFolder(const QUrl &folder)
{
    if (m_folder == folder)
        return;

    m_folder = folder;
    setCurrentFolder(folder);
    emit folderChanged();
}

/*!
    \qmlproperty url Qt.labs.platform::FolderDialog::currentFolder

    This property holds the currently selected folder in the dialog.

    Unlike the \l folder property, the \c currentFolder property is updated
    while the user is selecting folders in the dialog, even before the final
    selection has been made.

    \sa folder
*/
QUrl QQuickPlatformFolderDialog::currentFolder() const
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(handle()))
        return fileDialog->directory();
    return m_options->initialDirectory();
}

void QQuickPlatformFolderDialog::setCurrentFolder(const QUrl &folder)
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(handle()))
        fileDialog->setDirectory(folder);
    m_options->setInitialDirectory(folder);
}

/*!
    \qmlproperty flags Qt.labs.platform::FolderDialog::options

    This property holds the various options that affect the look and feel of the dialog.

    By default, all options are disabled.

    Options should be set before showing the dialog. Setting them while the dialog is
    visible is not guaranteed to have an immediate effect on the dialog (depending on
    the option and on the platform).

    Available options:
    \value FolderDialog.ShowDirsOnly Only show directories in the folder dialog. By default both folders and directories are shown.
    \value FolderDialog.DontResolveSymlinks Don't resolve symlinks in the folder dialog. By default symlinks are resolved.
    \value FolderDialog.ReadOnly Indicates that the dialog doesn't allow creating directories.
*/
QFileDialogOptions::FileDialogOptions QQuickPlatformFolderDialog::options() const
{
    return m_options->options();
}

void QQuickPlatformFolderDialog::setOptions(QFileDialogOptions::FileDialogOptions options)
{
    if (options == m_options->options())
        return;

    m_options->setOptions(options);
    emit optionsChanged();
}

void QQuickPlatformFolderDialog::resetOptions()
{
    setOptions(0);
}

/*!
    \qmlproperty string Qt.labs.platform::FolderDialog::acceptLabel

    This property holds the label text shown on the button that accepts the dialog.

    When set to an empty string, the default label of the underlying platform is used.
    The default label is typically \uicontrol Open.

    The default value is an empty string.

    \sa rejectLabel
*/
QString QQuickPlatformFolderDialog::acceptLabel() const
{
    return m_options->labelText(QFileDialogOptions::Accept);
}

void QQuickPlatformFolderDialog::setAcceptLabel(const QString &label)
{
    if (label == m_options->labelText(QFileDialogOptions::Accept))
        return;

    m_options->setLabelText(QFileDialogOptions::Accept, label);
    emit acceptLabelChanged();
}

void QQuickPlatformFolderDialog::resetAcceptLabel()
{
    setAcceptLabel(QString());
}

/*!
    \qmlproperty string Qt.labs.platform::FolderDialog::rejectLabel

    This property holds the label text shown on the button that rejects the dialog.

    When set to an empty string, the default label of the underlying platform is used.
    The default label is typically \uicontrol Cancel.

    The default value is an empty string.

    \sa acceptLabel
*/
QString QQuickPlatformFolderDialog::rejectLabel() const
{
    return m_options->labelText(QFileDialogOptions::Reject);
}

void QQuickPlatformFolderDialog::setRejectLabel(const QString &label)
{
    if (label == m_options->labelText(QFileDialogOptions::Reject))
        return;

    m_options->setLabelText(QFileDialogOptions::Reject, label);
    emit rejectLabelChanged();
}

void QQuickPlatformFolderDialog::resetRejectLabel()
{
    setRejectLabel(QString());
}

bool QQuickPlatformFolderDialog::useNativeDialog() const
{
    return QQuickPlatformDialog::useNativeDialog()
            && !m_options->testOption(QFileDialogOptions::DontUseNativeDialog);
}

void QQuickPlatformFolderDialog::onCreate(QPlatformDialogHelper *dialog)
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(dialog)) {
        connect(fileDialog, &QPlatformFileDialogHelper::directoryEntered, this, &QQuickPlatformFolderDialog::currentFolderChanged);
        fileDialog->setOptions(m_options);
    }
}

void QQuickPlatformFolderDialog::onShow(QPlatformDialogHelper *dialog)
{
    m_options->setWindowTitle(title());
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(dialog))
        fileDialog->setOptions(m_options);
}

void QQuickPlatformFolderDialog::accept()
{
    setFolder(currentFolder());
    QQuickPlatformDialog::accept();
}

QT_END_NAMESPACE
