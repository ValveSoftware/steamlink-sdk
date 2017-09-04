/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "servicesproxymodel.h"

ServicesProxyModel::ServicesProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

QVariant ServicesProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section != 0)
        return QVariant();

    return tr("Services");
}


bool ServicesProxyModel::lessThan(const QModelIndex &left,
                                  const QModelIndex &right) const
{
    QString s1 = sourceModel()->data(left).toString();
    QString s2 = sourceModel()->data(right).toString();

    const bool isNumber1 = s1.startsWith(QLatin1String(":1."));
    const bool isNumber2 = s2.startsWith(QLatin1String(":1."));
    if (isNumber1 == isNumber2) {
        if (isNumber1) {
            int number1 = s1.midRef(3).toInt();
            int number2 = s2.midRef(3).toInt();
            return number1 < number2;
        } else {
            return s1.compare(s2, Qt::CaseInsensitive) < 0;
        }
    } else {
        return isNumber2;
    }
}
