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

#ifndef PatternistSDK_UserTestCase_H
#define PatternistSDK_UserTestCase_H

#include <QString>

#include "TestCase.h"

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short Displays a test case entered manually by the user.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class UserTestCase : public TestCase
    {
        Q_OBJECT
    public:
        UserTestCase();
        virtual QString creator() const;
        virtual QString description() const;
        virtual QDate lastModified() const;

        /**
         * @returns the query the user has entered in the editor.
         * @param ok is always set to @c true
         */
        virtual QString sourceCode(bool &ok) const;
        virtual bool isXPath() const;
        virtual QVariant data(const Qt::ItemDataRole role, int column) const;
        virtual QString title() const;
        virtual QString name() const;
        /**
         * Performs an assert crash.
         */
        virtual QUrl testCasePath() const;
        virtual TreeItem *parent() const;
        virtual int columnCount() const;

        virtual Scenario scenario() const;

        /**
         * @returns always a default constructed QUrl.
         */
        virtual QUrl contextItemSource() const;

        /**
         * @return an empty list.
         */
        virtual TestBaseLine::List baseLines() const;

        void setLanguage(const QXmlQuery::QueryLanguage lang)
        {
            m_lang = lang;
        }

        virtual QPatternist::ExternalVariableLoader::Ptr externalVariableLoader() const;

        virtual QXmlQuery::QueryLanguage language() const
        {
            return m_lang;
        }

    public Q_SLOTS:
        void setSourceCode(const QString &code);
        void focusDocumentChanged(const QString &code);

    private:
        QString                     m_sourceCode;
        QXmlQuery::QueryLanguage    m_lang;
        QUrl                        m_contextSource;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
