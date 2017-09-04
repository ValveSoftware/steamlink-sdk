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

#ifndef PatternistSDK_TestContainer_H
#define PatternistSDK_TestContainer_H

#include "Global.h"
#include "TestItem.h"

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short an abstract base class, containing
     * an appropriate implementation of TestItem for sub-classes
     * which can contain other TestItem instances.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT TestContainer : public TestItem
    {
    public:
        virtual ~TestContainer();
        virtual void appendChild(TreeItem *item);
        virtual TreeItem *child(const unsigned int row) const;
        virtual unsigned int childCount() const;

        /**
         * @returns the TestResults of this TestContainer's children.
         */
        virtual TestResult::List execute(const ExecutionStage stage,
                                         TestSuite *ts);

        QString title() const;
        void setTitle(const QString &title);

        virtual TreeItem::List children() const;

        /**
         * @return always 2
         */
        virtual int columnCount() const;

        virtual bool isFinalNode() const;

        virtual ResultSummary resultSummary() const;
        virtual QString description() const;
        virtual void setDescription(const QString &desc);

        /**
         * Determines whether TestContainer will delete its children upon
         * destruction. By default, it will.
         */
        void setDeleteChildren(const bool val);

        /**
         * Removes the last appended child.
         */
        void removeLast();

    protected:
        /**
         * Constructor, protected. TestContainer is an abstract class,
         * and is not ment to be instantiated, but sub classed.
         */
        TestContainer();

    private:
        TreeItem::List  m_children;
        QString         m_title;
        QString         m_description;
        bool            m_deleteChildren;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
