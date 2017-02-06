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

#ifndef PatternistSDK_Worker_H
#define PatternistSDK_Worker_H

#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QStringList>

#include "ResultThreader.h"

QT_BEGIN_NAMESPACE

class QEventLoop;

namespace QPatternistSDK
{
    /**
     * @short Gets notified when the ResultThreader threads are
     * finished, and output summaries and adjusts a baseline.
     *
     * @author Frans Englich <frans.englich@nokia.com>
     * @ingroup PatternistSDK
     */
    class Q_PATTERNISTSDK_EXPORT Worker : public QObject
    {
        Q_OBJECT
    public:
        Worker(QEventLoop &e,
               const QFileInfo &baseline,
               const QFileInfo &result);

    public Q_SLOTS:
        void threadFinished();

    private:
        static inline void list(QTextStream &out, const QString &msg, QStringList &list);

        qint8                       m_finishedCount;
        const QFileInfo             m_baselineFile;
        const QFileInfo             m_resultFile;
        ResultThreader::Hash        m_result;
        ResultThreader::Hash        m_baseline;
        ResultThreader::Hash        m_summary;
        QStringList                 m_unexpectedPasses;
        QStringList                 m_unexpectedFailures;
        QStringList                 m_notTested;
        QEventLoop &                m_eventLoop;
        static const char *const    m_indent;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
