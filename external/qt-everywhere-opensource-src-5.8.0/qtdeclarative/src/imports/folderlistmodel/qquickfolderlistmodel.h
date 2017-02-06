/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#ifndef QQUICKFOLDERLISTMODEL_H
#define QQUICKFOLDERLISTMODEL_H

#include <qqml.h>
#include <QStringList>
#include <QUrl>
#include <QAbstractListModel>

QT_BEGIN_NAMESPACE


class QQmlContext;
class QModelIndex;

class QQuickFolderListModelPrivate;

//![class begin]
class QQuickFolderListModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
//![class begin]

//![class props]
    Q_PROPERTY(QUrl folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(QUrl rootFolder READ rootFolder WRITE setRootFolder)
    Q_PROPERTY(QUrl parentFolder READ parentFolder NOTIFY folderChanged)
    Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters)
    Q_PROPERTY(SortField sortField READ sortField WRITE setSortField)
    Q_PROPERTY(bool sortReversed READ sortReversed WRITE setSortReversed)
    Q_PROPERTY(bool showFiles READ showFiles WRITE setShowFiles REVISION 1)
    Q_PROPERTY(bool showDirs READ showDirs WRITE setShowDirs)
    Q_PROPERTY(bool showDirsFirst READ showDirsFirst WRITE setShowDirsFirst)
    Q_PROPERTY(bool showDotAndDotDot READ showDotAndDotDot WRITE setShowDotAndDotDot)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden REVISION 1)
    Q_PROPERTY(bool showOnlyReadable READ showOnlyReadable WRITE setShowOnlyReadable)
    Q_PROPERTY(bool caseSensitive READ caseSensitive WRITE setCaseSensitive REVISION 2)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
//![class props]

//![abslistmodel]
public:
    QQuickFolderListModel(QObject *parent = 0);
    ~QQuickFolderListModel();

    enum Roles {
        FileNameRole = Qt::UserRole + 1,
        FilePathRole = Qt::UserRole + 2,
        FileBaseNameRole = Qt::UserRole + 3,
        FileSuffixRole = Qt::UserRole + 4,
        FileSizeRole = Qt::UserRole + 5,
        FileLastModifiedRole = Qt::UserRole + 6,
        FileLastReadRole = Qt::UserRole +7,
        FileIsDirRole = Qt::UserRole + 8,
        FileUrlRole = Qt::UserRole + 9
    };

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual QHash<int, QByteArray> roleNames() const;
//![abslistmodel]

//![count]
    int count() const { return rowCount(QModelIndex()); }
//![count]

//![prop funcs]
    QUrl folder() const;
    void setFolder(const QUrl &folder);
    QUrl rootFolder() const;
    void setRootFolder(const QUrl &path);

    QUrl parentFolder() const;

    QStringList nameFilters() const;
    void setNameFilters(const QStringList &filters);

    enum SortField { Unsorted, Name, Time, Size, Type };
    Q_ENUM(SortField)
    SortField sortField() const;
    void setSortField(SortField field);

    bool sortReversed() const;
    void setSortReversed(bool rev);

    bool showFiles() const;
    void setShowFiles(bool showFiles);
    bool showDirs() const;
    void setShowDirs(bool showDirs);
    bool showDirsFirst() const;
    void setShowDirsFirst(bool showDirsFirst);
    bool showDotAndDotDot() const;
    void setShowDotAndDotDot(bool on);
    bool showHidden() const;
    void setShowHidden(bool on);
    bool showOnlyReadable() const;
    void setShowOnlyReadable(bool on);
    bool caseSensitive() const;
    void setCaseSensitive(bool on);
//![prop funcs]

    Q_INVOKABLE bool isFolder(int index) const;
    Q_INVOKABLE QVariant get(int idx, const QString &property) const;
    Q_INVOKABLE int indexOf(const QUrl &file) const;

//![parserstatus]
    virtual void classBegin();
    virtual void componentComplete();
//![parserstatus]

    int roleFromString(const QString &roleName) const;

//![notifier]
Q_SIGNALS:
    void folderChanged();
    void rowCountChanged() const;
    Q_REVISION(1) void countChanged() const;
//![notifier]

//![class end]


private:
    Q_DISABLE_COPY(QQuickFolderListModel)
    Q_DECLARE_PRIVATE(QQuickFolderListModel)
    QScopedPointer<QQuickFolderListModelPrivate> d_ptr;

    Q_PRIVATE_SLOT(d_func(), void _q_directoryChanged(const QString &directory, const QList<FileProperty> &list))
    Q_PRIVATE_SLOT(d_func(), void _q_directoryUpdated(const QString &directory, const QList<FileProperty> &list, int fromIndex, int toIndex))
    Q_PRIVATE_SLOT(d_func(), void _q_sortFinished(const QList<FileProperty> &list))
};
//![class end]

QT_END_NAMESPACE

#endif // QQUICKFOLDERLISTMODEL_H
