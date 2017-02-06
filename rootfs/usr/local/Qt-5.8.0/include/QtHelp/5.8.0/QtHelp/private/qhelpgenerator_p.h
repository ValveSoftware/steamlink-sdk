/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#ifndef QHELPGENERATOR_H
#define QHELPGENERATOR_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists for the convenience
// of the help generator tools. This header file may change from version
// to version without notice, or even be removed.
//
// We mean it.
//

#include "qhelp_global.h"
#include "qhelpdatainterface_p.h"

#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

class QHelpGeneratorPrivate;

class QHELP_EXPORT QHelpGenerator : public QObject
{
    Q_OBJECT

public:
    QHelpGenerator(QObject *parent = 0);
    ~QHelpGenerator();

    bool generate(QHelpDataInterface *helpData,
        const QString &outputFileName);
    bool checkLinks(const QHelpDataInterface &helpData);
    QString error() const;

Q_SIGNALS:
    void statusChanged(const QString &msg);
    void progressChanged(double progress);
    void warning(const QString &msg);

private:
    struct FileNameTableData
    {
        QString name;
        int fileId;
        QString title;
    };

    void writeTree(QDataStream &s, QHelpDataContentItem *item, int depth);
    bool createTables();
    bool insertFileNotFoundFile();
    bool registerCustomFilter(const QString &filterName,
        const QStringList &filterAttribs, bool forceUpdate = false);
    bool registerVirtualFolder(const QString &folderName, const QString &ns);
    bool insertFilterAttributes(const QStringList &attributes);
    bool insertKeywords(const QList<QHelpDataIndexItem> &keywords,
        const QStringList &filterAttributes);
    bool insertFiles(const QStringList &files, const QString &rootPath,
        const QStringList &filterAttributes);
    bool insertContents(const QByteArray &ba,
        const QStringList &filterAttributes);
    bool insertMetaData(const QMap<QString, QVariant> &metaData);
    void cleanupDB();
    void setupProgress(QHelpDataInterface *helpData);
    void addProgress(double step);

    QHelpGeneratorPrivate *d;
};

QT_END_NAMESPACE

#endif
