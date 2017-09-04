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

#ifndef PatternistSDK_TestGroup_H
#define PatternistSDK_TestGroup_H

#include <QString>

#include "TestContainer.h"

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short Groups test groups and test cases into a group.
     *
     * TestGroup corresponds to the @c test-group element in XQTSCatalog.xsd.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TestGroup : public TestContainer
    {
    public:
        TestGroup(TreeItem *parent);

        /**
         * @returns the parent of this group. Is either another group, or
         * the TestSuite instance governing this tree.
         */
        virtual TreeItem *parent() const;

        virtual QVariant data(const Qt::ItemDataRole role, int column) const;

        QString note() const;

        void setName(const QString &name);
        void setNote(const QString &note);

    private:
        QString m_name;
        QString m_note;
        TreeItem *m_parent;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
