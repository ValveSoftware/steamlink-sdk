/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#include "qhelpenginecore.h"
#include "qhelpsearchengine.h"
#include "qhelpsearchquerywidget.h"
#include "qhelpsearchresultwidget.h"

#include "qhelpsearchindexreader_p.h"
#if defined(QT_CLUCENE_SUPPORT)
#   include "qhelpsearchindexreader_clucene_p.h"
#   include "qhelpsearchindexwriter_clucene_p.h"
#else
#   include "qhelpsearchindexreader_default_p.h"
#   include "qhelpsearchindexwriter_default_p.h"
#endif

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QVariant>
#include <QtCore/QThread>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

#if defined(QT_CLUCENE_SUPPORT)
    using namespace fulltextsearch::clucene;
#else
    using namespace fulltextsearch::qt;
#endif

class QHelpSearchEnginePrivate : public QObject
{
    Q_OBJECT

signals:
    void indexingStarted();
    void indexingFinished();

    void searchingStarted();
    void searchingFinished(int hits);

private:
    QHelpSearchEnginePrivate(QHelpEngineCore *helpEngine)
        : queryWidget(0)
        , resultWidget(0)
        , helpEngine(helpEngine)
    {
        indexReader = 0;
        indexWriter = 0;
    }

    ~QHelpSearchEnginePrivate()
    {
        delete indexReader;
        delete indexWriter;
    }

    int hitCount() const
    {
        int count = 0;
        if (indexReader)
            count = indexReader->hitCount();

        return count;
    }

    QList<QHelpSearchEngine::SearchHit> hits(int start, int end) const
    {
        return indexReader ?
                indexReader->hits(start, end) :
                QList<QHelpSearchEngine::SearchHit>();
    }

    void updateIndex(bool reindex = false)
    {
        if (helpEngine.isNull())
            return;

        if (!QFile::exists(QFileInfo(helpEngine->collectionFile()).path()))
            return;

        if (!indexWriter) {
            indexWriter = new QHelpSearchIndexWriter();

            connect(indexWriter, SIGNAL(indexingStarted()), this, SIGNAL(indexingStarted()));
            connect(indexWriter, SIGNAL(indexingFinished()), this, SIGNAL(indexingFinished()));
            connect(indexWriter, SIGNAL(indexingFinished()), this, SLOT(optimizeIndex()));
        }

        indexWriter->cancelIndexing();
        indexWriter->updateIndex(helpEngine->collectionFile(),
                                 indexFilesFolder(), reindex);
    }

    void cancelIndexing()
    {
        if (indexWriter)
            indexWriter->cancelIndexing();
    }

    void search(const QList<QHelpSearchQuery> &queryList)
    {
        if (helpEngine.isNull())
            return;

        if (!QFile::exists(QFileInfo(helpEngine->collectionFile()).path()))
            return;

        if (!indexReader) {
#if defined(QT_CLUCENE_SUPPORT)
            indexReader = new QHelpSearchIndexReaderClucene();
#else
            indexReader = new QHelpSearchIndexReaderDefault();
#endif // QT_CLUCENE_SUPPORT
            connect(indexReader, SIGNAL(searchingStarted()), this, SIGNAL(searchingStarted()));
            connect(indexReader, SIGNAL(searchingFinished(int)), this, SIGNAL(searchingFinished(int)));
        }

        m_queryList = queryList;
        indexReader->cancelSearching();
        indexReader->search(helpEngine->collectionFile(), indexFilesFolder(), queryList);
    }

    void cancelSearching()
    {
        if (indexReader)
            indexReader->cancelSearching();
    }

    QString indexFilesFolder() const
    {
        QString indexFilesFolder = QLatin1String(".fulltextsearch");
        if (helpEngine && !helpEngine->collectionFile().isEmpty()) {
            QFileInfo fi(helpEngine->collectionFile());
            indexFilesFolder = fi.absolutePath() + QDir::separator()
                + QLatin1Char('.')
                + fi.fileName().left(fi.fileName().lastIndexOf(QLatin1String(".qhc")));
        }
        return indexFilesFolder;
    }

private slots:
    void optimizeIndex()
    {
#if defined(QT_CLUCENE_SUPPORT)
        if (indexWriter && !helpEngine.isNull()) {
            indexWriter->optimizeIndex();
        }
#endif
    }

private:
    friend class QHelpSearchEngine;

    QHelpSearchQueryWidget *queryWidget;
    QHelpSearchResultWidget *resultWidget;

    fulltextsearch::QHelpSearchIndexReader *indexReader;
    QHelpSearchIndexWriter *indexWriter;

    QPointer<QHelpEngineCore> helpEngine;

    QList<QHelpSearchQuery> m_queryList;
};

#include "qhelpsearchengine.moc"


/*!
    \class QHelpSearchQuery
    \since 4.4
    \inmodule QtHelp
    \brief The QHelpSearchQuery class contains the field name and the associated
    search term

    The QHelpSearchQuery class contains the field name and the associated search
    term. Depending on the field the search term might get split up into separate
    terms to be parsed differently by the search engine.

    \sa QHelpSearchQueryWidget
*/

/*!
    \fn QHelpSearchQuery::QHelpSearchQuery()

    Constructs a new empty QHelpSearchQuery.
*/

/*!
    \fn QHelpSearchQuery::QHelpSearchQuery(FieldName field, const QStringList &wordList)

    Constructs a new QHelpSearchQuery and initializes it with the given \a field and \a wordList.
*/

/*!
    \enum QHelpSearchQuery::FieldName
    This enum type specifies the field names that are handled by the search engine.

    \value DEFAULT  the default field provided by the search widget, several terms should be
                    split and stored in the word list except search terms enclosed in quotes.
    \value FUZZY    a field only provided in use with clucene. Terms should be split in separate
                    words and passed to the search engine.
    \value WITHOUT  a field only provided in use with clucene. Terms should be split in separate
                    words and passed to the search engine.
    \value PHRASE   a field only provided in use with clucene. Terms should not be split in separate
                    words.
    \value ALL      a field only provided in use with clucene. Terms should be split in separate
                    words and passed to the search engine
    \value ATLEAST  a field only provided in use with clucene. Terms should be split in separate
                    words and passed to the search engine
*/

/*!
    \class QHelpSearchEngine
    \since 4.4
    \inmodule QtHelp
    \brief The QHelpSearchEngine class provides access to widgets reusable
    to integrate fulltext search as well as to index and search documentation.

    Before the search engine can be used, one has to instantiate at least a
    QHelpEngineCore object that needs to be passed to the search engines constructor.
    This is required as the search engine needs to be connected to the help
    engines setupFinished() signal to know when it can start to index documentation.

    After starting the indexing process the signal indexingStarted() is emitted and
    on the end of the indexing process the indexingFinished() is emitted. To stop
    the indexing one can call cancelIndexing().

    While the indexing process has finished, the search engine can now be used to search
    thru its index for a given term. To do this one may use the possibility of creating the
    QHelpSearchQuery list by self or reuse the QHelpSearchQueryWidget which has the inbuild
    functionality to set up a proper search queries list that get's passed to the search engines
    search() function.

    After the list of querys has been passed to the search engine, the signal searchingStarted()
    is emitted and after the search has finished the searchingFinished() signal is emitted. The
    search process can be stopped by calling cancelSearching().

    If the search succeeds, the searchingFinished() will be called with the search hits count,
    which can be reused to fetch the search hits from the search engine. Calling the hits()
    function with the range of hits you would like to get will return a list of the requested
    SearchHits. They basically constist at the moment of a pair of strings where the values
    of that pair are the documentation file path and the page title.

    To display the given hits use the QHelpSearchResultWidget or build up your own one if you need
    more advanced functionality. Note that the QHelpSearchResultWidget can not be instantiated
    directly, you must retrieve the widget from the search engine in use as all connections will be
    established for you by the widget itself.
*/

/*!
    \fn void QHelpSearchEngine::indexingStarted()

    This signal is emitted when indexing process is started.
*/

/*!
    \fn void QHelpSearchEngine::indexingFinished()

    This signal is emitted when the indexing process is complete.
*/

/*!
    \fn void QHelpSearchEngine::searchingStarted()

    This signal is emitted when the search process is started.
*/

/*!
    \fn void QHelpSearchEngine::searchingFinished(int hits)

    This signal is emitted when the search process is complete.
    The hit count is stored in \a hits.
*/

/*!
    Constructs a new search engine with the given \a parent. The search engine
    uses the given \a helpEngine to access the documentation that needs to be indexed.
    The QHelpEngine's setupFinished() signal is automatically connected to the
    QHelpSearchEngine's indexing function, so that new documentation will be indexed
    after the signal is emitted.
*/
QHelpSearchEngine::QHelpSearchEngine(QHelpEngineCore *helpEngine, QObject *parent)
    : QObject(parent)
{
    d = new QHelpSearchEnginePrivate(helpEngine);

    connect(helpEngine, SIGNAL(setupFinished()), this, SLOT(indexDocumentation()));

    connect(d, SIGNAL(indexingStarted()), this, SIGNAL(indexingStarted()));
    connect(d, SIGNAL(indexingFinished()), this, SIGNAL(indexingFinished()));
    connect(d, SIGNAL(searchingStarted()), this, SIGNAL(searchingStarted()));
    connect(d, SIGNAL(searchingFinished(int)), this, SIGNAL(searchingFinished(int)));
}

/*!
    Destructs the search engine.
*/
QHelpSearchEngine::~QHelpSearchEngine()
{
    delete d;
}

/*!
    Returns a widget to use as input widget. Depending on your search engine
    configuration you will get a different widget with more or less subwidgets.
*/
QHelpSearchQueryWidget* QHelpSearchEngine::queryWidget()
{
    if (!d->queryWidget)
        d->queryWidget = new QHelpSearchQueryWidget();

    return d->queryWidget;
}

/*!
    Returns a widget that can hold and display the search results.
*/
QHelpSearchResultWidget* QHelpSearchEngine::resultWidget()
{
    if (!d->resultWidget)
        d->resultWidget = new QHelpSearchResultWidget(this);

    return d->resultWidget;
}

/*!
    \obsolete
    Returns the amount of hits the search engine found.
    \sa hitCount()
*/
int QHelpSearchEngine::hitsCount() const
{
    return d->hitCount();
}

/*!
    \since 4.6
    Returns the amount of hits the search engine found.
*/
int QHelpSearchEngine::hitCount() const
{
    return d->hitCount();
}

/*!
    \typedef QHelpSearchEngine::SearchHit

    Typedef for QPair<QString, QString>.
    The values of that pair are the documentation file path and the page title.

    \sa hits()
*/

/*!
    Returns a list of search hits within the range of \a start \a end.
*/
QList<QHelpSearchEngine::SearchHit> QHelpSearchEngine::hits(int start, int end) const
{
   return d->hits(start, end);
}

/*!
    Returns the list of queries last searched for.
    \since 4.5
*/
QList<QHelpSearchQuery> QHelpSearchEngine::query() const
{
    return d->m_queryList;
}

/*!
    Forces the search engine to reindex all documentation files.
*/
void QHelpSearchEngine::reindexDocumentation()
{
    d->updateIndex(true);
}

/*!
    Stops the indexing process.
*/
void QHelpSearchEngine::cancelIndexing()
{
    d->cancelIndexing();
}

/*!
    Stops the search process.
*/
void QHelpSearchEngine::cancelSearching()
{
    d->cancelSearching();
}

/*!
    Starts the search process using the given list of queries \a queryList
    build by the search field name and the values to search for.
*/
void QHelpSearchEngine::search(const QList<QHelpSearchQuery> &queryList)
{
    d->search(queryList);
}

void QHelpSearchEngine::indexDocumentation()
{
    d->updateIndex();
}

QT_END_NAMESPACE
