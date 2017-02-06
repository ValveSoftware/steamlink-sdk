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

#ifndef QHPWRITER_H
#define QHPWRITER_H

#include <QtCore/QXmlStreamWriter>
#include "filterpage.h"

QT_BEGIN_NAMESPACE

class AdpReader;

class QhpWriter : public QXmlStreamWriter
{
public:
    enum IdentifierPrefix {SkipAll, FilePrefix, GlobalPrefix};
    QhpWriter(const QString &namespaceName,
        const QString &virtualFolder);
    void setAdpReader(AdpReader *reader);
    void setFilterAttributes(const QStringList &attributes);
    void setCustomFilters(const QList<CustomFilter> filters);
    void setFiles(const QStringList &files);
    void generateIdentifiers(IdentifierPrefix prefix,
        const QString prefixString = QString());
    bool writeFile(const QString &fileName);

private:
    void writeCustomFilters();
    void writeFilterSection();
    void writeToc();
    void writeKeywords();
    void writeFiles();

    QString m_namespaceName;
    QString m_virtualFolder;
    AdpReader *m_adpReader;
    QStringList m_filterAttributes;
    QList<CustomFilter> m_customFilters;
    QStringList m_files;
    IdentifierPrefix m_prefix;
    QString m_prefixString;
};

QT_END_NAMESPACE

#endif
