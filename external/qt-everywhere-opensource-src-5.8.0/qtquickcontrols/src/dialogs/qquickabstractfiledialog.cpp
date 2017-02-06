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

#include "qquickabstractfiledialog_p.h"
#include "qquickitem.h"

#include <private/qguiapplication_p.h>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QWindow>

QT_BEGIN_NAMESPACE

QQuickAbstractFileDialog::QQuickAbstractFileDialog(QObject *parent)
    : QQuickAbstractDialog(parent)
    , m_dlgHelper(0)
    , m_options(QFileDialogOptions::create())
    , m_selectExisting(true)
    , m_selectMultiple(false)
    , m_selectFolder(false)
    , m_sidebarVisible(true)
{
    updateModes();
    connect(this, SIGNAL(accepted()), this, SIGNAL(selectionAccepted()));
}

QQuickAbstractFileDialog::~QQuickAbstractFileDialog()
{
}

void QQuickAbstractFileDialog::setVisible(bool v)
{
    if (helper() && v) {
        m_dlgHelper->setOptions(m_options);
        m_dlgHelper->setFilter();
        emit filterSelected();
    }
    QQuickAbstractDialog::setVisible(v);
}

QString QQuickAbstractFileDialog::title() const
{
    return m_options->windowTitle();
}

void QQuickAbstractFileDialog::setTitle(const QString &t)
{
    if (m_options->windowTitle() == t) return;
    m_options->setWindowTitle(t);
    emit titleChanged();
}

void QQuickAbstractFileDialog::setSelectExisting(bool selectExisting)
{
    if (selectExisting == m_selectExisting) return;
    m_selectExisting = selectExisting;
    updateModes();
}

void QQuickAbstractFileDialog::setSelectMultiple(bool selectMultiple)
{
    if (selectMultiple == m_selectMultiple) return;
    m_selectMultiple = selectMultiple;
    updateModes();
}

void QQuickAbstractFileDialog::setSelectFolder(bool selectFolder)
{
    if (selectFolder == m_selectFolder) return;
    m_selectFolder = selectFolder;
    updateModes();
}

QUrl QQuickAbstractFileDialog::folder() const
{
    if (m_dlgHelper && !m_dlgHelper->directory().isEmpty())
        return m_dlgHelper->directory();
    return m_options->initialDirectory();
}

static QUrl fixupFolder(const QUrl &f)
{
    QString lf = f.toLocalFile();
    while (lf.startsWith("//"))
        lf.remove(0, 1);
    if (lf.isEmpty())
        lf = QDir::currentPath();
    return QUrl::fromLocalFile(lf);
}

void QQuickAbstractFileDialog::setFolder(const QUrl &f)
{
    QUrl u = fixupFolder(f);
    if (m_dlgHelper)
        m_dlgHelper->setDirectory(u);
    m_options->setInitialDirectory(u);
    emit folderChanged();
}

void QQuickAbstractFileDialog::updateFolder(const QUrl &f)
{
    QUrl u = fixupFolder(f);
    m_options->setInitialDirectory(u);
    emit folderChanged();
}

void QQuickAbstractFileDialog::setNameFilters(const QStringList &f)
{
    m_options->setNameFilters(f);
    if (f.isEmpty())
        selectNameFilter(QString());
    else if (!f.contains(selectedNameFilter(), Qt::CaseInsensitive))
        selectNameFilter(f.first());
    emit nameFiltersChanged();
}

QString QQuickAbstractFileDialog::selectedNameFilter() const
{
    QString ret;
    if (m_dlgHelper)
        ret = m_dlgHelper->selectedNameFilter();
    if (ret.isEmpty())
        return m_options->initiallySelectedNameFilter();
    return ret;
}

void QQuickAbstractFileDialog::selectNameFilter(const QString &f)
{
    // This should work whether the dialog is currently being shown already, or ahead of time.
    m_options->setInitiallySelectedNameFilter(f);
    if (m_dlgHelper)
        m_dlgHelper->selectNameFilter(f);
    emit filterSelected();
}

void QQuickAbstractFileDialog::setSelectedNameFilterIndex(int idx)
{
    selectNameFilter(nameFilters().at(idx));
}

void QQuickAbstractFileDialog::setSidebarVisible(bool s)
{
    if (s == m_sidebarVisible) return;
    m_sidebarVisible = s;
    emit sidebarVisibleChanged();
}

QStringList QQuickAbstractFileDialog::selectedNameFilterExtensions() const
{
    QString filterRaw = selectedNameFilter();
    QStringList ret;
    if (filterRaw.isEmpty()) {
        ret << "*";
        return ret;
    }
    QRegularExpression re("(\\*\\.?\\w*)");
    QRegularExpressionMatchIterator i = re.globalMatch(filterRaw);
    while (i.hasNext())
        ret << i.next().captured(1);
    if (ret.isEmpty())
        ret << filterRaw;
    return ret;
}

int QQuickAbstractFileDialog::selectedNameFilterIndex() const
{
    return nameFilters().indexOf(selectedNameFilter());
}

QUrl QQuickAbstractFileDialog::fileUrl() const
{
    QList<QUrl> urls = fileUrls();
    return (urls.count() == 1) ? urls[0] : QUrl();
}

void QQuickAbstractFileDialog::updateModes()
{
    // The 4 possible modes are AnyFile, ExistingFile, Directory, ExistingFiles
    // Assume AnyFile until we find a reason to the contrary
    QFileDialogOptions::FileMode mode = QFileDialogOptions::AnyFile;

    if (m_selectFolder) {
        mode = QFileDialogOptions::Directory;
        m_options->setOption(QFileDialogOptions::ShowDirsOnly);
        m_selectMultiple = false;
        m_selectExisting = true;
        setNameFilters(QStringList());
    } else if (m_selectExisting) {
        mode = m_selectMultiple ?
            QFileDialogOptions::ExistingFiles : QFileDialogOptions::ExistingFile;
        m_options->setOption(QFileDialogOptions::ShowDirsOnly, false);
    } else if (m_selectMultiple) {
        m_selectExisting = true;
    }
    if (!m_selectExisting)
        m_selectMultiple = false;
    m_options->setFileMode(mode);
    m_options->setAcceptMode(m_selectExisting ?
                           QFileDialogOptions::AcceptOpen : QFileDialogOptions::AcceptSave);
    emit fileModeChanged();
}

void QQuickAbstractFileDialog::addShortcut(const QString &name, const QString &visibleName, const QString &path)
{
    QQmlEngine *engine = qmlEngine(this);
    QUrl url = QUrl::fromLocalFile(path);

    // Since the app can have bindings to the shortcut, we always add it
    // to the public API, even if the directory doesn't (yet) exist.
    m_shortcuts.setProperty(name, url.toString());

    // ...but we are more strict about showing it as a clickable link inside the dialog
    if (visibleName.isEmpty() || !QDir(path).exists())
        return;

    QJSValue o = engine->newObject();
    o.setProperty("name", visibleName);
    // TODO maybe some day QJSValue could directly store a QUrl
    o.setProperty("url", url.toString());

    int length = m_shortcutDetails.property(QLatin1String("length")).toInt();
    m_shortcutDetails.setProperty(length, o);
}

void QQuickAbstractFileDialog::addShortcutFromStandardLocation(const QString &name, QStandardPaths::StandardLocation loc, bool local)
{
    if (m_selectExisting) {
        QStringList readPaths = QStandardPaths::standardLocations(loc);
        QString path = readPaths.isEmpty() ? QString() : local ? readPaths.first() : readPaths.last();
        addShortcut(name, QStandardPaths::displayName(loc), path);
    } else {
        QString path = QStandardPaths::writableLocation(loc);
        addShortcut(name, QStandardPaths::displayName(loc), path);
    }
}

void QQuickAbstractFileDialog::populateShortcuts()
{
    QJSEngine *engine = qmlEngine(this);
    m_shortcutDetails = engine->newArray();
    m_shortcuts = engine->newObject();

    addShortcutFromStandardLocation(QLatin1String("desktop"), QStandardPaths::DesktopLocation);
    addShortcutFromStandardLocation(QLatin1String("documents"), QStandardPaths::DocumentsLocation);
    addShortcutFromStandardLocation(QLatin1String("music"), QStandardPaths::MusicLocation);
    addShortcutFromStandardLocation(QLatin1String("movies"), QStandardPaths::MoviesLocation);
    addShortcutFromStandardLocation(QLatin1String("home"), QStandardPaths::HomeLocation);

#ifndef Q_OS_IOS
    addShortcutFromStandardLocation(QLatin1String("pictures"), QStandardPaths::PicturesLocation);
#else
    // On iOS we point pictures to the system picture folder when loading
    addShortcutFromStandardLocation(QLatin1String("pictures"), QStandardPaths::PicturesLocation, !m_selectExisting);
#endif

#ifndef Q_OS_IOS
    // on iOS, this returns only "/", which is never a useful path to read or write anything
    const QFileInfoList drives = QDir::drives();
    for (const QFileInfo &fi : drives)
        addShortcut(fi.absoluteFilePath(), fi.absoluteFilePath(), fi.absoluteFilePath());
#endif

    emit shortcutsChanged();
}

QJSValue QQuickAbstractFileDialog::shortcuts()
{
    if (m_shortcuts.isUndefined())
        populateShortcuts();

    return m_shortcuts;
}

QJSValue QQuickAbstractFileDialog::__shortcuts()
{
    if (m_shortcutDetails.isUndefined())
        populateShortcuts();

    return m_shortcutDetails;
}

QT_END_NAMESPACE
