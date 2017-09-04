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

#include "file_picker_controller.h"
#include "type_conversion.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

#include <QFileInfo>
#include <QDir>
#include <QVariant>
#include <QStringList>

namespace QtWebEngineCore {

FilePickerController::FilePickerController(FileChooserMode mode, content::RenderFrameHost *frameHost, const QString &defaultFileName, const QStringList &acceptedMimeTypes, QObject *parent)
    : QObject(parent)
    , m_defaultFileName(defaultFileName)
    , m_acceptedMimeTypes(acceptedMimeTypes)
    , m_frameHost(frameHost)
    , m_mode(mode)
{
}

void FilePickerController::accepted(const QStringList &files)
{
    FilePickerController::filesSelectedInChooser(files, m_frameHost);
}

void FilePickerController::accepted(const QVariant &files)
{
    QStringList stringList;

    if (files.canConvert(QVariant::StringList)) {
        stringList = files.toStringList();
    } else if (files.canConvert<QList<QUrl> >()) {
        Q_FOREACH (const QUrl &url, files.value<QList<QUrl> >())
            stringList.append(url.toLocalFile());
    } else {
        qWarning("An unhandled type '%s' was provided in FilePickerController::accepted(QVariant)", files.typeName());
    }

    FilePickerController::filesSelectedInChooser(stringList, m_frameHost);
}

void FilePickerController::rejected()
{
    FilePickerController::filesSelectedInChooser(QStringList(), m_frameHost);
}

static QStringList listRecursively(const QDir &dir)
{
    QStringList ret;
    QFileInfoList infoList(dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden));
    Q_FOREACH (const QFileInfo &fileInfo, infoList) {
        if (fileInfo.isDir()) {
            ret.append(fileInfo.absolutePath() + QStringLiteral("/.")); // Match chromium's behavior. See chrome/browser/file_select_helper.cc
            ret.append(listRecursively(QDir(fileInfo.absoluteFilePath())));
        } else
            ret.append(fileInfo.absoluteFilePath());
    }
    return ret;
}

ASSERT_ENUMS_MATCH(FilePickerController::Open, content::FileChooserParams::Open)
ASSERT_ENUMS_MATCH(FilePickerController::OpenMultiple, content::FileChooserParams::OpenMultiple)
ASSERT_ENUMS_MATCH(FilePickerController::UploadFolder, content::FileChooserParams::UploadFolder)
ASSERT_ENUMS_MATCH(FilePickerController::Save, content::FileChooserParams::Save)

void FilePickerController::filesSelectedInChooser(const QStringList &filesList, content::RenderFrameHost *frameHost)
{
    Q_ASSERT(frameHost);
    QStringList files(filesList);
    if (this->m_mode == UploadFolder && !filesList.isEmpty()
            && QFileInfo(filesList.first()).isDir()) // Enumerate the directory
        files = listRecursively(QDir(filesList.first()));
    frameHost->FilesSelectedInChooser(toVector<content::FileChooserFileInfo>(files), static_cast<content::FileChooserParams::Mode>(this->m_mode));
}

QStringList FilePickerController::acceptedMimeTypes() const
{
    return m_acceptedMimeTypes;
}

FilePickerController::FileChooserMode FilePickerController::mode() const
{
    return m_mode;
}

QString FilePickerController::defaultFileName() const
{
    return m_defaultFileName;
}

} // namespace
