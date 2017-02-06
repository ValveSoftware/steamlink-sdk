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

#include "TestContainer.h"

#include "TreeItem.h"

using namespace QPatternistSDK;

int TreeItem::row() const
{
    const TreeItem *const p = parent();

    if(p)
    {
        /* The const_cast makes it possible for QPointer's constructor
         * to implicitly kick in. */
        return p->children().indexOf(const_cast<TreeItem *>(this));
    }
    else
        return -1;
}

QPair<int, int> TreeItem::executeRange = qMakePair<int,int>(0,INT_MAX);
int TreeItem::executions = 0;


// vim: et:ts=4:sw=4:sts=4
