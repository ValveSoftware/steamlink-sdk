/****************************************************************************
**
** Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team.
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qqueryparser_p.h"
#include "qquery_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/queryParser/QueryParser.h>

QT_BEGIN_NAMESPACE

QCLuceneQueryParserPrivate::QCLuceneQueryParserPrivate()
    : QSharedData()
{
    queryParser = 0;
    deleteCLuceneQueryParser = true;
}

QCLuceneQueryParserPrivate::QCLuceneQueryParserPrivate(const QCLuceneQueryParserPrivate &other)
    : QSharedData()
{
    queryParser = _CL_POINTER(other.queryParser);
    deleteCLuceneQueryParser = other.deleteCLuceneQueryParser;
}

QCLuceneQueryParserPrivate::~QCLuceneQueryParserPrivate()
{
    if (deleteCLuceneQueryParser)
        _CLDECDELETE(queryParser);
}


QCLuceneQueryParser::QCLuceneQueryParser(const QString &field,
                                         QCLuceneAnalyzer &analyzer)
    : d(new QCLuceneQueryParserPrivate())
    , field(field)
    , analyzer(analyzer)
{
    TCHAR *fieldName = QStringToTChar(field);

    d->queryParser = new lucene::queryParser::QueryParser(fieldName,
        analyzer.d->analyzer);

    delete [] fieldName;
}

QCLuceneQueryParser::~QCLuceneQueryParser()
{
    // nothing todo
}

QCLuceneQuery* QCLuceneQueryParser::parse(const QString &query)
{
    TCHAR *string = QStringToTChar(query);

    QCLuceneQuery *retValue = 0;
    lucene::search::Query* q = d->queryParser->parse(string);
    if (q) {
        retValue = new QCLuceneQuery();
        retValue->d->query = q;
    }

    delete [] string;
    return retValue;
}

QCLuceneQuery* QCLuceneQueryParser::parse(QCLuceneReader &reader)
{
    QCLuceneQuery *retValue = 0;
    lucene::search::Query* q = d->queryParser->parse(reader.d->reader);
    if (q) {
        retValue = new QCLuceneQuery();
        retValue->d->query = q;
    }

    return retValue;
}

QCLuceneQuery* QCLuceneQueryParser::parse(const QString &query, const QString &field,
                                          QCLuceneAnalyzer &analyzer)
{
    QCLuceneQueryParser parser(field, analyzer);
    return parser.parse(query);
}

QCLuceneAnalyzer QCLuceneQueryParser::getAnalyzer()
{
    return analyzer;
}

QString QCLuceneQueryParser::getField()
{
    return field;
}


QCLuceneMultiFieldQueryParser::QCLuceneMultiFieldQueryParser(
    const QStringList &fieldList, QCLuceneAnalyzer &analyzer)
    : QCLuceneQueryParser(QLatin1String(""), analyzer)
{
    Q_UNUSED(fieldList)
}

QCLuceneMultiFieldQueryParser::~QCLuceneMultiFieldQueryParser()
{
    // nothing todo
}

QCLuceneQuery* QCLuceneMultiFieldQueryParser::parse(const QString &query,
                                                    const QStringList &fieldList,
                                                    QCLuceneAnalyzer &analyzer)
{
    QCLuceneBooleanQuery *retValue = new QCLuceneBooleanQuery();
    foreach (const QString &field, fieldList) {
        QCLuceneQuery *q = QCLuceneQueryParser::parse(query, field, analyzer);
        if (!q) {
            delete retValue;
            retValue = 0; break;
        } else {
            retValue->add(q, true, false, false);
        }
    }

    return retValue;
}

QCLuceneQuery* QCLuceneMultiFieldQueryParser::parse(const QString &query,
                                                    const QStringList &fieldList,
                                                    QList<FieldFlags> flags,
                                                    QCLuceneAnalyzer &analyzer)
{
    QCLuceneBooleanQuery *retValue = new QCLuceneBooleanQuery();
    qint32 i = 0;
    foreach (const QString &field, fieldList) {
        QCLuceneQuery *q = QCLuceneQueryParser::parse(query, field, analyzer);
        if (q) {
            qint32 flag = flags.at(i);
            switch (flag) {
                case QCLuceneMultiFieldQueryParser::REQUIRED_FIELD: {
                    retValue->add(q, true, true, false);
                }   break;

                case QCLuceneMultiFieldQueryParser::PROHIBITED_FIELD: {
                    retValue->add(q, true, false, true);
                }   break;

                default: {
                    retValue->add(q, true, false, false);
                }   break;
            }

            ++i;
        } else {
            delete retValue;
            retValue = 0; break;
        }
    }
    return retValue;
}

QT_END_NAMESPACE
