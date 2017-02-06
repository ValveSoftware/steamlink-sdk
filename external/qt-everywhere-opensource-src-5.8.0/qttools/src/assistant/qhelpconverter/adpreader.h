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

#ifndef ADPREADER_H
#define ADPREADER_H

#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QXmlStreamReader>

QT_BEGIN_NAMESPACE

struct ContentItem {
    ContentItem(const QString &t, const QString &r, int d)
       : title(t), reference(r), depth(d) {}
    QString title;
    QString reference;
    int depth;
};

struct KeywordItem {
    KeywordItem(const QString &k, const QString &r)
       : keyword(k), reference(r) {}
    QString keyword;
    QString reference;
};

class AdpReader : public QXmlStreamReader
{
public:
    void readData(const QByteArray &contents);
    QList<ContentItem> contents() const;
    QList<KeywordItem> keywords() const;
    QSet<QString> files() const;

    QMap<QString, QString> properties() const;

private:
    void readProject();
    void readProfile();
    void readDCF();
    void addFile(const QString &file);

    QMap<QString, QString> m_properties;
    QList<ContentItem> m_contents;
    QList<KeywordItem> m_keywords;
    QSet<QString> m_files;
};

QT_END_NAMESPACE

#endif
