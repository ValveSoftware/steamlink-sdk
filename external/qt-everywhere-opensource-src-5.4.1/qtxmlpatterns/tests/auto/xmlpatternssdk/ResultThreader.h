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

#ifndef PatternistSDK_ResultThreader_H
#define PatternistSDK_ResultThreader_H

#include <QFile>
#include <QFileInfo>
#include <QThread>

#include "TestResultHandler.h"

QT_BEGIN_NAMESPACE

class QEventLoop;

namespace QPatternistSDK
{
    /**
     * @short Reads XML in the @c XQTSResult.xsd format, as a thread, allowing
     * multiple parses to be done simultaneously.
     *
     * @author Frans Englich <frans.englich@nokia.com>
     * @ingroup PatternistSDK
     */
    class Q_PATTERNISTSDK_EXPORT ResultThreader : public QThread
                                                , public TestResultHandler
    {
    public:
        enum Type
        {
            Baseline = 1,
            Result
        };

        /**
         * Creates a ResultThreader that will read @p file when run() is called.
         */
        ResultThreader(QEventLoop &ev,
                       QFile *file,
                       const Type type,
                       QObject *parent);

        /**
         * Parses the file passed in this class's constructor with this ResultHandlerTH::Item::LisT
         * as the QXmlContentHandler, and returns.
         */
        virtual void run();

        /**
         * @note Do not reimplement this function.
         * @returns whether this ResultThreader handles the baseline or the result.
         */
        Type type() const;

    private:
        Q_DISABLE_COPY(ResultThreader)

        QFile *const    m_file;
        const Type      m_type;
        QEventLoop &    m_eventLoop;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
