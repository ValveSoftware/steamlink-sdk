/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Assistant of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
