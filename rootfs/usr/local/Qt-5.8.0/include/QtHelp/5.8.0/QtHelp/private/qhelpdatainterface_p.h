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

#ifndef QHELPDATAINTERFACE_H
#define QHELPDATAINTERFACE_H

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

#include <QtCore/QStringList>
#include <QtCore/QSharedData>

QT_BEGIN_NAMESPACE

class QHELP_EXPORT QHelpDataContentItem
{
public:
    QHelpDataContentItem(QHelpDataContentItem *parent, const QString &title,
        const QString &reference);
    ~QHelpDataContentItem();

    QString title() const;
    QString reference() const;
    QList<QHelpDataContentItem*> children() const;

private:
    QString m_title;
    QString m_reference;
    QList<QHelpDataContentItem*> m_children;
};

struct QHELP_EXPORT QHelpDataIndexItem {
    QHelpDataIndexItem() {}
    QHelpDataIndexItem(const QString &n, const QString &id, const QString &r)
        : name(n), identifier(id), reference(r) {}

    QString name;
    QString identifier;
    QString reference;

    bool operator==(const QHelpDataIndexItem & other) const;
};

class QHelpDataFilterSectionData : public QSharedData
{
public:
    ~QHelpDataFilterSectionData()
    {
        qDeleteAll(contents);
    }

    QStringList filterAttributes;
    QList<QHelpDataIndexItem> indices;
    QList<QHelpDataContentItem*> contents;
    QStringList files;
};

class QHELP_EXPORT QHelpDataFilterSection
{
public:
    QHelpDataFilterSection();

    void addFilterAttribute(const QString &filter);
    QStringList filterAttributes() const;

    void addIndex(const QHelpDataIndexItem &index);
    void setIndices(const QList<QHelpDataIndexItem> &indices);
    QList<QHelpDataIndexItem> indices() const;

    void addContent(QHelpDataContentItem *content);
    void setContents(const QList<QHelpDataContentItem*> &contents);
    QList<QHelpDataContentItem*> contents() const;

    void addFile(const QString &file);
    void setFiles(const QStringList &files);
    QStringList files() const;

private:
    QSharedDataPointer<QHelpDataFilterSectionData> d;
};

struct QHELP_EXPORT QHelpDataCustomFilter {
    QStringList filterAttributes;
    QString name;
};

class QHELP_EXPORT QHelpDataInterface
{
public:
    QHelpDataInterface() {}
    virtual ~QHelpDataInterface() {}

    virtual QString namespaceName() const = 0;
    virtual QString virtualFolder() const = 0;
    virtual QList<QHelpDataCustomFilter> customFilters() const = 0;
    virtual QList<QHelpDataFilterSection> filterSections() const = 0;
    virtual QMap<QString, QVariant> metaData() const = 0;
    virtual QString rootPath() const = 0;
};

QT_END_NAMESPACE

#endif // QHELPDATAINTERFACE_H
