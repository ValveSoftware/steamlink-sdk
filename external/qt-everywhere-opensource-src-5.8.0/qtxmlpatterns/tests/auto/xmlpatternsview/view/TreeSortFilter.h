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

#ifndef PatternistSDK_TreeSortFilter_H
#define PatternistSDK_TreeSortFilter_H

#include <QSortFilterProxyModel>

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short A sort & filter model for hierarchical item models.
     *
     * Features:
     * - When sorting, numbers are treated as a whole instead of on a
     *   character-per-character basis. For example, @c myFile-10 is sorted after @c myFile-9.
     * - When filtering, it behaves as usually is expected when the item model is hierarchical. That is,
     *   an item is shown if it matches or any of its children matches.
     *
     * @image html TreeSortFilter.png "TreeSortFilter in action on a QTreeView."
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class TreeSortFilter : public QSortFilterProxyModel
    {
    public:
        /**
         * Creates a TreeSortFilter.
         *
         * @param parent the parent. Must not be @c null.
         */
        TreeSortFilter(QObject *parent);

    protected:
        /**
         * Compares @p left and @p right. They are treated as QStrings.
         */
        virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

        /**
         * Overridden to implement filtering.
         */
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

    private:
        inline bool numericLessThan(const QString &l, const QString &r) const;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
