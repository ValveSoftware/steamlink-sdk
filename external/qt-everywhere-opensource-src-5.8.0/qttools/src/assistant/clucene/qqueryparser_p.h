/****************************************************************************
**
** Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team.
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtTools module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUERYPARSER_P_H
#define QQUERYPARSER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists for the convenience
// of the help generator tools. This header file may change from version
// to version without notice, or even be removed.
//
// We mean it.
//

#include "qreader_p.h"
#include "qanalyzer_p.h"
#include "qclucene_global_p.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QSharedData>

CL_NS_DEF(queryParser)
    class QueryParser;
CL_NS_END
CL_NS_USE(queryParser)

QT_BEGIN_NAMESPACE

class QCLuceneQuery;
class QCLuceneMultiFieldQueryParser;

class Q_CLUCENE_EXPORT QCLuceneQueryParserPrivate : public QSharedData
{
public:
    QCLuceneQueryParserPrivate();
    QCLuceneQueryParserPrivate(const QCLuceneQueryParserPrivate &other);

    ~QCLuceneQueryParserPrivate();

    QueryParser *queryParser;
    bool deleteCLuceneQueryParser;

private:
    QCLuceneQueryParserPrivate &operator=(const QCLuceneQueryParserPrivate &other);
};

class Q_CLUCENE_EXPORT QCLuceneQueryParser
{
public:
    QCLuceneQueryParser(const QString &field, QCLuceneAnalyzer &analyzer);
    virtual ~QCLuceneQueryParser();

    QCLuceneQuery* parse(const QString &query);
    QCLuceneQuery* parse(QCLuceneReader &reader);
    static QCLuceneQuery* parse(const QString &query, const QString &field,
                                QCLuceneAnalyzer &analyzer);
    QCLuceneAnalyzer getAnalyzer();
    QString getField();

protected:
    friend class QCLuceneMultiFieldQueryParser;
    QSharedDataPointer<QCLuceneQueryParserPrivate> d;

private:
    QString field;
    QCLuceneAnalyzer analyzer;
};

class Q_CLUCENE_EXPORT QCLuceneMultiFieldQueryParser : public QCLuceneQueryParser
{
public:
    enum FieldFlags {
        NORMAL_FIELD = 0,
        REQUIRED_FIELD = 1,
        PROHIBITED_FIELD = 2
    };

    QCLuceneMultiFieldQueryParser(const QStringList &fieldList,
                                  QCLuceneAnalyzer &analyzer);
    ~QCLuceneMultiFieldQueryParser();

    static QCLuceneQuery *parse(const QString &query, const QStringList &fieldList,
                                QCLuceneAnalyzer &analyzer);
    static QCLuceneQuery *parse(const QString &query, const QStringList &fieldList,
                                QList<FieldFlags> flags, QCLuceneAnalyzer &analyzer);
};

QT_END_NAMESPACE

#endif  // QQUERYPARSER_P_H
