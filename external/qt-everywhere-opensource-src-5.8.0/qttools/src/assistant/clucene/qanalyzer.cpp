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

#include "qanalyzer_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/analysis/AnalysisHeader.h>

QT_BEGIN_NAMESPACE

QCLuceneAnalyzerPrivate::QCLuceneAnalyzerPrivate()
    : QSharedData()
{
    analyzer = 0;
    deleteCLuceneAnalyzer = true;
}

QCLuceneAnalyzerPrivate::QCLuceneAnalyzerPrivate(const QCLuceneAnalyzerPrivate &other)
    : QSharedData()
{
    analyzer = _CL_POINTER(other.analyzer);
    deleteCLuceneAnalyzer = other.deleteCLuceneAnalyzer;
}

QCLuceneAnalyzerPrivate::~QCLuceneAnalyzerPrivate()
{
    if (deleteCLuceneAnalyzer)
        _CLDECDELETE(analyzer);
}


QCLuceneAnalyzer::QCLuceneAnalyzer()
    : d(new QCLuceneAnalyzerPrivate())
{
    //nothing todo, private
}

QCLuceneAnalyzer::~QCLuceneAnalyzer()
{
    // nothing todo
}

qint32 QCLuceneAnalyzer::positionIncrementGap(const QString &fieldName) const
{
    Q_UNUSED(fieldName);
    return 0;
}

QCLuceneTokenStream QCLuceneAnalyzer::tokenStream(const QString &fieldName,
                                                  const QCLuceneReader &reader) const
{
    TCHAR *fName = QStringToTChar(fieldName);
    QCLuceneTokenStream tokenStream;
    tokenStream.d->tokenStream = d->analyzer->tokenStream(fName, reader.d->reader);
    delete [] fName;

    return tokenStream;
}


QCLuceneStandardAnalyzer::QCLuceneStandardAnalyzer()
    : QCLuceneAnalyzer()
{
    d->analyzer = new lucene::analysis::standard::StandardAnalyzer();
}

QCLuceneStandardAnalyzer::~QCLuceneStandardAnalyzer()
{
    // nothing todo
}

QCLuceneStandardAnalyzer::QCLuceneStandardAnalyzer(const QStringList &stopWords)
{
    const TCHAR **tArray = new const TCHAR*[stopWords.count() +1];

    for(int i = 0; i < stopWords.count(); ++i) {
        TCHAR *stopWord = QStringToTChar(stopWords.at(i));
        tArray[i] = STRDUP_TtoT(stopWord);
        delete [] stopWord;
    }
    tArray[stopWords.count()] = 0;

    d->analyzer = new lucene::analysis::standard::StandardAnalyzer(tArray);

    for (int i = 0; i < stopWords.count(); ++i)
        delete [] tArray[i];

    delete [] tArray;
}


QCLuceneWhitespaceAnalyzer::QCLuceneWhitespaceAnalyzer()
    : QCLuceneAnalyzer()
{
    d->analyzer = new lucene::analysis::WhitespaceAnalyzer();
}

QCLuceneWhitespaceAnalyzer::~QCLuceneWhitespaceAnalyzer()
{
    // nothing todo
}


QCLuceneSimpleAnalyzer::QCLuceneSimpleAnalyzer()
    : QCLuceneAnalyzer()
{
    d->analyzer = new lucene::analysis::SimpleAnalyzer();
}

QCLuceneSimpleAnalyzer::~QCLuceneSimpleAnalyzer()
{
    // nothing todo
}


QCLuceneStopAnalyzer::QCLuceneStopAnalyzer()
    : QCLuceneAnalyzer()
{
    d->analyzer = new lucene::analysis::StopAnalyzer();
}

QCLuceneStopAnalyzer::~QCLuceneStopAnalyzer()
{
    // nothing todo
}

QCLuceneStopAnalyzer::QCLuceneStopAnalyzer(const QStringList &stopWords)
    : QCLuceneAnalyzer()
{
    const TCHAR **tArray = new const TCHAR*[stopWords.count() +1];

    for(int i = 0; i < stopWords.count(); ++i) {
        TCHAR *stopWord = QStringToTChar(stopWords.at(i));
        tArray[i] = STRDUP_TtoT(stopWord);
        delete [] stopWord;
    }
    tArray[stopWords.count()] = 0;

    d->analyzer = new lucene::analysis::StopAnalyzer(tArray);

    for (int i = 0; i < stopWords.count(); ++i)
        delete [] tArray[i];

    delete [] tArray;
}

QStringList QCLuceneStopAnalyzer::englishStopWords() const
{
    QStringList stopWordList;

    const TCHAR** stopWords = lucene::analysis::StopAnalyzer::ENGLISH_STOP_WORDS;
    for (qint32 i = 0; stopWords[i] != 0; ++i)
        stopWordList.append(TCharToQString(stopWords[i]));

    return stopWordList;
}


QCLuceneKeywordAnalyzer::QCLuceneKeywordAnalyzer()
    : QCLuceneAnalyzer()
{
    d->analyzer = new lucene::analysis::KeywordAnalyzer();
}

QCLuceneKeywordAnalyzer::~QCLuceneKeywordAnalyzer()
{
    // nothing todo
}


QCLucenePerFieldAnalyzerWrapper::QCLucenePerFieldAnalyzerWrapper(
    QCLuceneAnalyzer *defaultAnalyzer)
    : QCLuceneAnalyzer()
{
    d->analyzer = new
        lucene::analysis::PerFieldAnalyzerWrapper(defaultAnalyzer->d->analyzer);

    analyzers.append(defaultAnalyzer);
    defaultAnalyzer->d->deleteCLuceneAnalyzer = false;
}

QCLucenePerFieldAnalyzerWrapper::~QCLucenePerFieldAnalyzerWrapper()
{
    qDeleteAll(analyzers);
}

void QCLucenePerFieldAnalyzerWrapper::addAnalyzer(const QString &fieldName,
                                                  QCLuceneAnalyzer *analyzer)
{
    lucene::analysis::PerFieldAnalyzerWrapper *analyzerWrapper =
        static_cast<lucene::analysis::PerFieldAnalyzerWrapper*> (d->analyzer);

    if (analyzerWrapper == 0)
        return;

    analyzers.append(analyzer);
    analyzer->d->deleteCLuceneAnalyzer = false;

    TCHAR *fName = QStringToTChar(fieldName);
    analyzerWrapper->addAnalyzer(fName, analyzer->d->analyzer);
    delete [] fName;
}

QT_END_NAMESPACE
