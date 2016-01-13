/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef PatternistSDK_TreeModel_H
#define PatternistSDK_TreeModel_H

#include <QAbstractItemModel>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include "Global.h"

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    class TreeItem;

    /**
     * @short TreeItem is a generic QAbstractItemModel tailored for
     * representing hierarchial data.
     *
     * TreeModel is an item model in Qt's model/view framework. Its
     * data consists of TreeItem instances.
     *
     * @see TreeItem
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TreeModel : public QAbstractItemModel
    {
        Q_OBJECT
    public:
        TreeModel(const QStringList columnData, QObject *parent);
        virtual ~TreeModel();

        virtual QVariant data(const QModelIndex &index, int role) const;
        virtual Qt::ItemFlags flags(const QModelIndex &index) const;
        virtual QVariant headerData(int section,
                                    Qt::Orientation orientation,
                                    int role = Qt::DisplayRole) const;
        virtual QModelIndex index(int row,
                                  int column,
                                  const QModelIndex &parent = QModelIndex()) const;
        virtual QModelIndex parent(const QModelIndex &index) const;
        virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
        virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;

        TreeItem *root() const;
        /**
         * Sets @p root to the new root, and deletes the old.
         */
        void setRoot(TreeItem *root);

    protected Q_SLOTS:
        void childChanged(TreeItem *child);

    private:
        QPointer<TreeItem> m_root;
        const QStringList m_columnData;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
