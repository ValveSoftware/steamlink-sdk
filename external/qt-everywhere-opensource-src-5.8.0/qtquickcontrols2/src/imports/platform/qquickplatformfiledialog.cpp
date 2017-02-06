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

#include "qquickplatformfiledialog_p.h"

#include <QtCore/qvector.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype FileDialog
    \inherits Dialog
    \instantiates QQuickPlatformFileDialog
    \inqmlmodule Qt.labs.platform
    \since 5.8
    \brief A native file dialog.

    The FileDialog type provides a QML API for native platform file dialogs.

    \image qtlabsplatform-filedialog-gtk.png

    To show a file dialog, construct an instance of FileDialog, set the
    desired properties, and call \l {Dialog::}{open()}. The \l currentFile
    or \l currentFiles properties can be used to determine the currently
    selected file(s) in the dialog. The \l file and \l files properties
    are updated only after the final selection has been made by accepting
    the dialog.

    \code
    MenuItem {
        text: "Open..."
        onTriggered: fileDialog.open()
    }

    FileDialog {
        id: fileDialog
        currentFile: document.source
        folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
    }

    MyDocument {
        id: document
        source: fileDialog.file
    }
    \endcode

    \section2 Availability

    A native platform file dialog is currently available on the following platforms:

    \list
    \li iOS
    \li Linux (when running with the GTK+ platform theme)
    \li macOS
    \li Windows
    \li WinRT
    \endlist

    \input includes/widgets.qdocinc 1

    \labs

    \sa FolderDialog, StandardPaths
*/

QQuickPlatformFileDialog::QQuickPlatformFileDialog(QObject *parent)
    : QQuickPlatformDialog(QPlatformTheme::FileDialog, parent),
      m_fileMode(OpenFile),
      m_options(QFileDialogOptions::create()),
      m_selectedNameFilter(nullptr)
{
    m_options->setFileMode(QFileDialogOptions::ExistingFile);
    m_options->setAcceptMode(QFileDialogOptions::AcceptOpen);
}

/*!
    \qmlproperty enumeration Qt.labs.platform::FileDialog::fileMode

    This property holds the mode of the dialog.

    Available values:
    \value FileDialog.OpenFile The dialog is used to select an existing file (default).
    \value FileDialog.OpenFiles The dialog is used to select multiple existing files.
    \value FileDialog.SaveFile The dialog is used to select any file. The file does not have to exist.
*/
QQuickPlatformFileDialog::FileMode QQuickPlatformFileDialog::fileMode() const
{
    return m_fileMode;
}

void QQuickPlatformFileDialog::setFileMode(FileMode mode)
{
    if (mode == m_fileMode)
        return;

    switch (mode) {
    case OpenFile:
        m_options->setFileMode(QFileDialogOptions::ExistingFile);
        m_options->setAcceptMode(QFileDialogOptions::AcceptOpen);
        break;
    case OpenFiles:
        m_options->setFileMode(QFileDialogOptions::ExistingFiles);
        m_options->setAcceptMode(QFileDialogOptions::AcceptOpen);
        break;
    case SaveFile:
        m_options->setFileMode(QFileDialogOptions::AnyFile);
        m_options->setAcceptMode(QFileDialogOptions::AcceptSave);
        break;
    default:
        break;
    }

    m_fileMode = mode;
    emit fileModeChanged();
}

/*!
    \qmlproperty url Qt.labs.platform::FileDialog::file

    This property holds the final accepted file.

    Unlike the \l currentFile property, the \c file property is not updated
    while the user is selecting files in the dialog, but only after the final
    selection has been made. That is, when the user has clicked \uicontrol OK
    to accept a file. Alternatively, the \l {Dialog::}{accepted()} signal
    can be handled to get the final selection.

    \sa currentFile, {Dialog::}{accepted()}
*/
QUrl QQuickPlatformFileDialog::file() const
{
    return addDefaultSuffix(m_files.value(0));
}

void QQuickPlatformFileDialog::setFile(const QUrl &file)
{
    setFiles(QList<QUrl>() << file);
}

/*!
    \qmlproperty list<url> Qt.labs.platform::FileDialog::files

    This property holds the final accepted files.

    Unlike the \l currentFiles property, the \c files property is not updated
    while the user is selecting files in the dialog, but only after the final
    selection has been made. That is, when the user has clicked \uicontrol OK
    to accept files. Alternatively, the \l {Dialog::}{accepted()} signal
    can be handled to get the final selection.

    \sa currentFiles, {Dialog::}{accepted()}
*/
QList<QUrl> QQuickPlatformFileDialog::files() const
{
    return addDefaultSuffixes(m_files);
}

void QQuickPlatformFileDialog::setFiles(const QList<QUrl> &files)
{
    if (m_files == files)
        return;

    bool firstChanged = m_files.value(0) != files.value(0);
    m_files = files;
    if (firstChanged)
        emit fileChanged();
    emit filesChanged();
}

/*!
    \qmlproperty url Qt.labs.platform::FileDialog::currentFile

    This property holds the currently selected file in the dialog.

    Unlike the \l file property, the \c currentFile property is updated
    while the user is selecting files in the dialog, even before the final
    selection has been made.

    \sa file, currentFiles
*/
QUrl QQuickPlatformFileDialog::currentFile() const
{
    return currentFiles().value(0);
}

void QQuickPlatformFileDialog::setCurrentFile(const QUrl &file)
{
    setCurrentFiles(QList<QUrl>() << file);
}

/*!
    \qmlproperty list<url> Qt.labs.platform::FileDialog::currentFiles

    This property holds the currently selected files in the dialog.

    Unlike the \l files property, the \c currentFiles property is updated
    while the user is selecting files in the dialog, even before the final
    selection has been made.

    \sa files, currentFile
*/
QList<QUrl> QQuickPlatformFileDialog::currentFiles() const
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(handle()))
        return fileDialog->selectedFiles();
    return m_options->initiallySelectedFiles();
}

void QQuickPlatformFileDialog::setCurrentFiles(const QList<QUrl> &files)
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(handle())) {
        for (const QUrl &file : files)
            fileDialog->selectFile(file);
    }
    m_options->setInitiallySelectedFiles(files);
}

/*!
    \qmlproperty url Qt.labs.platform::FileDialog::folder

    This property holds the folder where files are selected.
    For selecting a folder, use FolderDialog instead.

    \sa FolderDialog
*/
QUrl QQuickPlatformFileDialog::folder() const
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(handle()))
        return fileDialog->directory();
    return m_options->initialDirectory();
}

void QQuickPlatformFileDialog::setFolder(const QUrl &folder)
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(handle()))
        fileDialog->setDirectory(folder);
    m_options->setInitialDirectory(folder);
}

/*!
    \qmlproperty flags Qt.labs.platform::FileDialog::options

    This property holds the various options that affect the look and feel of the dialog.

    By default, all options are disabled.

    Options should be set before showing the dialog. Setting them while the dialog is
    visible is not guaranteed to have an immediate effect on the dialog (depending on
    the option and on the platform).

    Available options:
    \value FileDialog.DontResolveSymlinks Don't resolve symlinks in the file dialog. By default symlinks are resolved.
    \value FileDialog.DontConfirmOverwrite Don't ask for confirmation if an existing file is selected. By default confirmation is requested.
    \value FileDialog.ReadOnly Indicates that the dialog doesn't allow creating directories.
    \value FileDialog.HideNameFilterDetails Indicates if the file name filter details are hidden or not.
*/
QFileDialogOptions::FileDialogOptions QQuickPlatformFileDialog::options() const
{
    return m_options->options();
}

void QQuickPlatformFileDialog::setOptions(QFileDialogOptions::FileDialogOptions options)
{
    if (options == m_options->options())
        return;

    m_options->setOptions(options);
    emit optionsChanged();
}

void QQuickPlatformFileDialog::resetOptions()
{
    setOptions(0);
}

/*!
    \qmlproperty list<string> Qt.labs.platform::FileDialog::nameFilters

    This property holds the filters that restrict the types of files that
    can be selected.

    \code
    FileDialog {
        nameFilters: ["Text files (*.txt)", "HTML files (*.html *.htm)"]
    }
    \endcode

    \note \b{*.*} is not a portable filter, because the historical assumption
    that the file extension determines the file type is not consistent on every
    operating system. It is possible to have a file with no dot in its name (for
    example, \c Makefile). In a native Windows file dialog, \b{*.*} will match
    such files, while in other types of file dialogs it may not. So it is better
    to use \b{*} if you mean to select any file.

    \sa selectedNameFilter
*/
QStringList QQuickPlatformFileDialog::nameFilters() const
{
    return m_options->nameFilters();
}

void QQuickPlatformFileDialog::setNameFilters(const QStringList &filters)
{
    if (filters == m_options->nameFilters())
        return;

    m_options->setNameFilters(filters);
    if (m_selectedNameFilter) {
        int index = m_selectedNameFilter->index();
        if (index < 0 || index >= filters.count())
            index = 0;
        m_selectedNameFilter->update(filters.value(index));
    }
    emit nameFiltersChanged();
}

void QQuickPlatformFileDialog::resetNameFilters()
{
    setNameFilters(QStringList());
}

/*!
    \qmlpropertygroup Qt.labs.platform::FileDialog::selectedNameFilter
    \qmlproperty int Qt.labs.platform::FileDialog::selectedNameFilter.index
    \qmlproperty string Qt.labs.platform::FileDialog::selectedNameFilter.name
    \qmlproperty list<string> Qt.labs.platform::FileDialog::selectedNameFilter.extensions

    These properties hold the currently selected name filter.

    \table
    \header
        \li Name
        \li Description
    \row
        \li \b index : int
        \li This property determines which \l {nameFilters}{name filter} is selected.
            The specified filter is selected when the dialog is opened. The value is
            updated when the user selects another filter.
    \row
        \li [read-only] \b name : string
        \li This property holds the name of the selected filter. In the
            example below, the name of the first filter is \c {"Text files"}
            and the second is \c {"HTML files"}.
    \row
        \li [read-only] \b extensions : list<string>
        \li This property holds the list of extensions of the selected filter.
            In the example below, the list of extensions of the first filter is
            \c {["txt"]} and the second is \c {["html", "htm"]}.
    \endtable

    \code
    FileDialog {
        id: fileDialog
        selectedNameFilter.index: 1
        nameFilters: ["Text files (*.txt)", "HTML files (*.html *.htm)"]
    }

    MyDocument {
        id: document
        fileType: fileDialog.selectedNameFilter.extensions[0]
    }
    \endcode

    \sa nameFilters
*/
QQuickPlatformFileNameFilter *QQuickPlatformFileDialog::selectedNameFilter() const
{
    if (!m_selectedNameFilter) {
        QQuickPlatformFileDialog *that = const_cast<QQuickPlatformFileDialog *>(this);
        m_selectedNameFilter = new QQuickPlatformFileNameFilter(that);
        m_selectedNameFilter->setOptions(m_options);
    }
    return m_selectedNameFilter;
}

/*!
    \qmlproperty string Qt.labs.platform::FileDialog::defaultSuffix

    This property holds a suffix that is added to selected files that have
    no suffix specified. The suffix is typically used to indicate the file
    type (e.g. "txt" indicates a text file).

    If the first character is a dot ('.'), it is removed.
*/
QString QQuickPlatformFileDialog::defaultSuffix() const
{
    return m_options->defaultSuffix();
}

void QQuickPlatformFileDialog::setDefaultSuffix(const QString &suffix)
{
    if (suffix == m_options->defaultSuffix())
        return;

    m_options->setDefaultSuffix(suffix);
    emit defaultSuffixChanged();
}

void QQuickPlatformFileDialog::resetDefaultSuffix()
{
    setDefaultSuffix(QString());
}

/*!
    \qmlproperty string Qt.labs.platform::FileDialog::acceptLabel

    This property holds the label text shown on the button that accepts the dialog.

    When set to an empty string, the default label of the underlying platform is used.
    The default label is typically \uicontrol Open or \uicontrol Save depending on which
    \l fileMode the dialog is used in.

    The default value is an empty string.

    \sa rejectLabel
*/
QString QQuickPlatformFileDialog::acceptLabel() const
{
    return m_options->labelText(QFileDialogOptions::Accept);
}

void QQuickPlatformFileDialog::setAcceptLabel(const QString &label)
{
    if (label == m_options->labelText(QFileDialogOptions::Accept))
        return;

    m_options->setLabelText(QFileDialogOptions::Accept, label);
    emit acceptLabelChanged();
}

void QQuickPlatformFileDialog::resetAcceptLabel()
{
    setAcceptLabel(QString());
}

/*!
    \qmlproperty string Qt.labs.platform::FileDialog::rejectLabel

    This property holds the label text shown on the button that rejects the dialog.

    When set to an empty string, the default label of the underlying platform is used.
    The default label is typically \uicontrol Cancel.

    The default value is an empty string.

    \sa acceptLabel
*/
QString QQuickPlatformFileDialog::rejectLabel() const
{
    return m_options->labelText(QFileDialogOptions::Reject);
}

void QQuickPlatformFileDialog::setRejectLabel(const QString &label)
{
    if (label == m_options->labelText(QFileDialogOptions::Reject))
        return;

    m_options->setLabelText(QFileDialogOptions::Reject, label);
    emit rejectLabelChanged();
}

void QQuickPlatformFileDialog::resetRejectLabel()
{
    setRejectLabel(QString());
}

bool QQuickPlatformFileDialog::useNativeDialog() const
{
    return QQuickPlatformDialog::useNativeDialog()
            && !m_options->testOption(QFileDialogOptions::DontUseNativeDialog);
}

void QQuickPlatformFileDialog::onCreate(QPlatformDialogHelper *dialog)
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(dialog)) {
        // TODO: emit currentFileChanged only when the first entry in currentFiles changes
        connect(fileDialog, &QPlatformFileDialogHelper::currentChanged, this, &QQuickPlatformFileDialog::currentFileChanged);
        connect(fileDialog, &QPlatformFileDialogHelper::currentChanged, this, &QQuickPlatformFileDialog::currentFilesChanged);
        connect(fileDialog, &QPlatformFileDialogHelper::directoryEntered, this, &QQuickPlatformFileDialog::folderChanged);
        fileDialog->setOptions(m_options);
    }
}

void QQuickPlatformFileDialog::onShow(QPlatformDialogHelper *dialog)
{
    m_options->setWindowTitle(title());
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(dialog)) {
        fileDialog->setOptions(m_options);
        if (m_selectedNameFilter) {
            const int index = m_selectedNameFilter->index();
            const QString filter = m_options->nameFilters().value(index);
            m_options->setInitiallySelectedNameFilter(filter);
            fileDialog->selectNameFilter(filter);
            connect(fileDialog, &QPlatformFileDialogHelper::filterSelected, m_selectedNameFilter, &QQuickPlatformFileNameFilter::update);
        }
    }
}

void QQuickPlatformFileDialog::onHide(QPlatformDialogHelper *dialog)
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(dialog)) {
        if (m_selectedNameFilter)
            disconnect(fileDialog, &QPlatformFileDialogHelper::filterSelected, m_selectedNameFilter, &QQuickPlatformFileNameFilter::update);
    }
}

void QQuickPlatformFileDialog::accept()
{
    if (QPlatformFileDialogHelper *fileDialog = qobject_cast<QPlatformFileDialogHelper *>(handle()))
        setFiles(fileDialog->selectedFiles());
    QQuickPlatformDialog::accept();
}

QUrl QQuickPlatformFileDialog::addDefaultSuffix(const QUrl &file) const
{
    QUrl url = file;
    const QString path = url.path();
    const QString suffix = m_options->defaultSuffix();
    if (!suffix.isEmpty() && !path.endsWith(QLatin1Char('/')) && path.lastIndexOf(QLatin1Char('.')) == -1)
        url.setPath(path + QLatin1Char('.') + suffix);
    return url;
}

QList<QUrl> QQuickPlatformFileDialog::addDefaultSuffixes(const QList<QUrl> &files) const
{
    QList<QUrl> urls;
    urls.reserve(files.size());
    for (const QUrl &file : files)
        urls += addDefaultSuffix(file);
    return urls;
}

QQuickPlatformFileNameFilter::QQuickPlatformFileNameFilter(QObject *parent)
    : QObject(parent), m_index(-1)
{
}

int QQuickPlatformFileNameFilter::index() const
{
    return m_index;
}

void QQuickPlatformFileNameFilter::setIndex(int index)
{
    if (m_index == index)
        return;

    m_index = index;
    emit indexChanged(index);
}

QString QQuickPlatformFileNameFilter::name() const
{
    return m_name;
}

QStringList QQuickPlatformFileNameFilter::extensions() const
{
    return m_extensions;
}

QSharedPointer<QFileDialogOptions> QQuickPlatformFileNameFilter::options() const
{
    return m_options;
}

void QQuickPlatformFileNameFilter::setOptions(const QSharedPointer<QFileDialogOptions> &options)
{
    m_options = options;
}

static QString extractName(const QString &filter)
{
    return filter.left(filter.indexOf(QLatin1Char('(')) - 1);
}

static QString extractExtension(const QString &filter)
{
    return filter.mid(filter.indexOf(QLatin1Char('.')) + 1);
}

static QStringList extractExtensions(const QString &filter)
{
    QStringList extensions;
    const int from = filter.indexOf(QLatin1Char('('));
    const int to = filter.lastIndexOf(QLatin1Char(')')) - 1;
    if (from >= 0 && from < to) {
        const QStringRef ref = filter.midRef(from + 1, to - from);
        const QVector<QStringRef> exts = ref.split(QLatin1Char(' '), QString::SkipEmptyParts);
        for (const QStringRef &ref : exts)
            extensions += extractExtension(ref.toString());
    }

    return extensions;
}

void QQuickPlatformFileNameFilter::update(const QString &filter)
{
    const QStringList filters = nameFilters();

    const int oldIndex = m_index;
    const QString oldName = m_name;
    const QStringList oldExtensions = m_extensions;

    m_index = filters.indexOf(filter);
    m_name = extractName(filter);
    m_extensions = extractExtensions(filter);

    if (oldIndex != m_index)
        emit indexChanged(m_index);
    if (oldName != m_name)
        emit nameChanged(m_name);
    if (oldExtensions != m_extensions)
        emit extensionsChanged(m_extensions);
}

QStringList QQuickPlatformFileNameFilter::nameFilters() const
{
    return m_options ? m_options->nameFilters() : QStringList();
}

QString QQuickPlatformFileNameFilter::nameFilter(int index) const
{
    return m_options ? m_options->nameFilters().value(index) : QString();
}

QT_END_NAMESPACE
