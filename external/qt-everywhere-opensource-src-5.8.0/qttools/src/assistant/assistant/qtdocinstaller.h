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

#ifndef QTDOCINSTALLER
#define QTDOCINSTALLER

#include <QtCore/QDir>
#include <QtCore/QMutex>
#include <QtCore/QPair>
#include <QtCore/QStringList>
#include <QtCore/QThread>

QT_BEGIN_NAMESPACE

class HelpEngineWrapper;

class QtDocInstaller : public QThread
{
    Q_OBJECT

public:
    typedef QPair<QString, QStringList> DocInfo;
    QtDocInstaller(const QList<DocInfo> &docInfos);
    ~QtDocInstaller();
    void installDocs();

signals:
    void qchFileNotFound(const QString &component);
    void registerDocumentation(const QString &component,
                               const QString &absFileName);
    void docsInstalled(bool newDocsInstalled);

private:
    void run();
    bool installDoc(const DocInfo &docInfo);

    bool m_abort;
    QMutex m_mutex;
    QStringList m_qchFiles;
    QDir m_qchDir;
    QList<DocInfo> m_docInfos;
};

QT_END_NAMESPACE

#endif
