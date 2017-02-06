/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
