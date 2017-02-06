/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "tracer.h"

#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QDateTime>
#include <QtCore/QFileSystemWatcher>
#include <QtHelp/QHelpEngineCore>
#include "helpenginewrapper.h"
#include "qtdocinstaller.h"

QT_BEGIN_NAMESPACE

QtDocInstaller::QtDocInstaller(const QList<DocInfo> &docInfos)
    : m_abort(false), m_docInfos(docInfos)
{
    TRACE_OBJ
}

QtDocInstaller::~QtDocInstaller()
{
    TRACE_OBJ
    if (!isRunning())
        return;
    m_mutex.lock();
    m_abort = true;
    m_mutex.unlock();
    wait();
}

void QtDocInstaller::installDocs()
{
    TRACE_OBJ
    start(LowPriority);
}

void QtDocInstaller::run()
{
    TRACE_OBJ
    m_qchDir = QLibraryInfo::location(QLibraryInfo::DocumentationPath);
    m_qchFiles = m_qchDir.entryList(QStringList() << QLatin1String("*.qch"));

    bool changes = false;
    foreach (const DocInfo &docInfo, m_docInfos) {
        changes |= installDoc(docInfo);
        m_mutex.lock();
        if (m_abort) {
            m_mutex.unlock();
            return;
        }
        m_mutex.unlock();
    }
    emit docsInstalled(changes);
}

bool QtDocInstaller::installDoc(const DocInfo &docInfo)
{
    TRACE_OBJ
    const QString &component = docInfo.first;
    const QStringList &info = docInfo.second;
    QDateTime dt;
    if (!info.isEmpty() && !info.first().isEmpty())
        dt = QDateTime::fromString(info.first(), Qt::ISODate);

    QString qchFile;
    if (info.count() == 2)
        qchFile = info.last();

    if (m_qchFiles.isEmpty()) {
        emit qchFileNotFound(component);
        return false;
    }
    foreach (const QString &f, m_qchFiles) {
        if (f.startsWith(component)) {
            QFileInfo fi(m_qchDir.absolutePath() + QDir::separator() + f);
            if (dt.isValid() && fi.lastModified().toSecsSinceEpoch() == dt.toSecsSinceEpoch()
                && qchFile == fi.absoluteFilePath())
                return false;
            emit registerDocumentation(component, fi.absoluteFilePath());
            return true;
        }
    }

    emit qchFileNotFound(component);
    return false;
}

QT_END_NAMESPACE
