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
#include "qhelpsearchindexreader_default_p.h"
#include "qhelpsearchindexwriter_default_p.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QVariant>
#include <QtCore/QThread>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE

using namespace fulltextsearch::qt;

class QHelpSearchResultData : public QSharedData
{
public:
    QUrl m_url;
    QString m_title;
    QString m_snippet;
};

/*!
    \class QHelpSearchResult
    \since 5.9
    \inmodule QtHelp
    \brief The QHelpSearchResult class provides the data associated with the
    search result.

    The QHelpSearchResult object is a data object that describes a single search result.
    The vector of search result objects is returned by QHelpSearchEngine::searchResults().
    The description of the search result contains the document title and URL
    that the search input matched. It also contains the snippet from
    the document content containing the best match of the search input.
    \sa QHelpSearchEngine
*/

/*!
    Constructs a new empty QHelpSearchResult.
*/
QHelpSearchResult::QHelpSearchResult()
    : d(new QHelpSearchResultData)
{
}

/*!
    Constructs a copy of \a other.
*/
QHelpSearchResult::QHelpSearchResult(const QHelpSearchResult &other)
    : d(other.d)
{
}

/*!
    Constructs the search result containing \a url, \a title and \a snippet
    as the description of the result.
*/
QHelpSearchResult::QHelpSearchResult(const QUrl &url, const QString &title, const QString &snippet)
    : d(new QHelpSearchResultData)
{
    d->m_url = url;
    d->m_title = title;
    d->m_snippet = snippet;
}

/*!
    Destroys the search result.
*/
QHelpSearchResult::~QHelpSearchResult()
{
}

/*!
    Assigns \a other to this search result and returns a reference to this search result.
*/
QHelpSearchResult &QHelpSearchResult::operator=(const QHelpSearchResult &other)
{
    d = other.d;
    return *this;
}

/*!
    Returns the document title of the search result.
*/
QString QHelpSearchResult::title() const
{
    return d->m_title;
}

/*!
    Returns the document URL of the search result.
*/
QUrl QHelpSearchResult::url() const
{
    return d->m_url;
}

/*!
    Returns the document snippet containing the search phrase of the search result.
*/
QString QHelpSearchResult::snippet() const
{
    return d->m_snippet;
}


class QHelpSearchEnginePrivate : public QObject
{
    Q_OBJECT

signals:
    void indexingStarted();
    void indexingFinished();

    void searchingStarted();
    void searchingFinished(int searchResultCount);

private:
    QHelpSearchEnginePrivate(QHelpEngineCore *helpEngine)
        : helpEngine(helpEngine)
    {
    }

    ~QHelpSearchEnginePrivate()
    {
        delete indexReader;
        delete indexWriter;
    }

    int searchResultCount() const
    {
        return indexReader ? indexReader->searchResultCount() : 0;
    }

    QVector<QHelpSearchResult> searchResults(int start, int end) const
    {
        return indexReader ?
               indexReader->searchResults(start, end) :
               QVector<QHelpSearchResult>();
    }

    void updateIndex(bool reindex = false)
    {
        if (helpEngine.isNull())
            return;

        if (!QFile::exists(QFileInfo(helpEngine->collectionFile()).path()))
            return;

        if (!indexWriter) {
            indexWriter = new QHelpSearchIndexWriter();

            connect(indexWriter, &QHelpSearchIndexWriter::indexingStarted,
                    this, &QHelpSearchEnginePrivate::indexingStarted);
            connect(indexWriter, &QHelpSearchIndexWriter::indexingFinished,
                    this, &QHelpSearchEnginePrivate::indexingFinished);
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

    void search(const QString &searchInput)
    {
        if (helpEngine.isNull())
            return;

        if (!QFile::exists(QFileInfo(helpEngine->collectionFile()).path()))
            return;

        if (!indexReader) {
            indexReader = new QHelpSearchIndexReaderDefault();
            connect(indexReader, &fulltextsearch::QHelpSearchIndexReader::searchingStarted,
                    this, &QHelpSearchEnginePrivate::searchingStarted);
            connect(indexReader, &fulltextsearch::QHelpSearchIndexReader::searchingFinished,
                    this, &QHelpSearchEnginePrivate::searchingFinished);
        }

        m_searchInput = searchInput;
        indexReader->cancelSearching();
        indexReader->search(helpEngine->collectionFile(), indexFilesFolder(), searchInput);
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

private:
    friend class QHelpSearchEngine;

    bool m_isIndexingScheduled = false;

    QHelpSearchQueryWidget *queryWidget = nullptr;
    QHelpSearchResultWidget *resultWidget = nullptr;

    fulltextsearch::QHelpSearchIndexReader *indexReader = nullptr;
    QHelpSearchIndexWriter *indexWriter = nullptr;

    QPointer<QHelpEngineCore> helpEngine;

    QString m_searchInput;
};

#include "qhelpsearchengine.moc"

/*!
    \class QHelpSearchQuery
    \obsolete
    \since 4.4
    \inmodule QtHelp
    \brief The QHelpSearchQuery class contains the field name and the associated
    search term

    The QHelpSearchQuery class contains the field name and the associated search
    term. Depending on the field the search term might get split up into separate
    terms to be parsed differently by the search engine.

    \note This class has been deprecated in favor of QString.

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
    \value FUZZY    \obsolete Terms should be split in separate
                    words and passed to the search engine.
    \value WITHOUT  \obsolete  Terms should be split in separate
                    words and passed to the search engine.
    \value PHRASE   \obsolete  Terms should not be split in separate words.
    \value ALL      \obsolete  Terms should be split in separate
                    words and passed to the search engine
    \value ATLEAST  \obsolete  Terms should be split in separate
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

    When the indexing process has finished, the search engine can be used to
    search through the index for a given term using the search() function. When
    the search input is passed to the search engine, the searchingStarted()
    signal is emitted. When the search finishes, the searchingFinished() signal
    is emitted. The search process can be stopped by calling cancelSearching().

    If the search succeeds, searchingFinished() is called with the search result
    count to fetch the search results from the search engine. Calling the
    searchResults() function with a range returns a list of QHelpSearchResult
    objects within the range. The results consist of the document title and URL,
    as well as a snippet from the document that contains the best match for the
    search input.

    To display the given search results use the QHelpSearchResultWidget or build up your own one if you need
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
    \fn void QHelpSearchEngine::searchingFinished(int searchResultCount)

    This signal is emitted when the search process is complete.
    The search result count is stored in \a searchResultCount.
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

    connect(helpEngine, &QHelpEngineCore::setupFinished,
            this, &QHelpSearchEngine::scheduleIndexDocumentation);

    connect(d, &QHelpSearchEnginePrivate::indexingStarted,
            this, &QHelpSearchEngine::indexingStarted);
    connect(d, &QHelpSearchEnginePrivate::indexingFinished,
            this, &QHelpSearchEngine::indexingFinished);
    connect(d, &QHelpSearchEnginePrivate::searchingStarted,
            this, &QHelpSearchEngine::searchingStarted);
    connect(d, &QHelpSearchEnginePrivate::searchingFinished,
            this, &QHelpSearchEngine::searchingFinished);
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
    Use searchResultCount() instead.
*/
int QHelpSearchEngine::hitsCount() const
{
    return d->searchResultCount();
}

/*!
    \since 4.6
    \obsolete
    Use searchResultCount() instead.
*/
int QHelpSearchEngine::hitCount() const
{
    return d->searchResultCount();
}

/*!
    \since 5.9
    Returns the number of results the search engine found.
*/
int QHelpSearchEngine::searchResultCount() const
{
    return d->searchResultCount();
}

/*!
    \typedef QHelpSearchEngine::SearchHit
    \obsolete

    Use QHelpSearchResult instead.

    Typedef for QPair<QString, QString>.
    The values of that pair are the documentation file path and the page title.

    \sa hits()
*/

/*!
    \obsolete
    Use searchResults() instead.
*/
QList<QHelpSearchEngine::SearchHit> QHelpSearchEngine::hits(int start, int end) const
{
    QList<QHelpSearchEngine::SearchHit> hits;
    for (const QHelpSearchResult &result : searchResults(start, end))
        hits.append(qMakePair(result.url().toString(), result.title()));
    return hits;
}

/*!
    \since 5.9
    Returns a list of search results within the range from the index
    specified by \a start to the index specified by \a end.
*/
QVector<QHelpSearchResult> QHelpSearchEngine::searchResults(int start, int end) const
{
    return d->searchResults(start, end);
}

/*!
    \since 5.9
    Returns the phrase that was last searched for.
*/
QString QHelpSearchEngine::searchInput() const
{
    return d->m_searchInput;
}

/*!
    \obsolete
    \since 4.5
    Use searchInput() instead.
*/
QList<QHelpSearchQuery> QHelpSearchEngine::query() const
{
    return QList<QHelpSearchQuery>() << QHelpSearchQuery(QHelpSearchQuery::DEFAULT,
           d->m_searchInput.split(QChar::Space));
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
    \since 5.9
    Starts the search process using the given search phrase \a searchInput.

    The phrase may consist of several words. By default, the search engine returns
    the list of documents that contain all the specified words.
    The phrase may contain any combination of the logical operators AND, OR, and
    NOT. The operator must be written in all capital letters, otherwise it will
    be considered a part of the search phrase.

    If double quotation marks are used to group the words,
    the search engine will search for an exact match of the quoted phrase.

    For more information about the text query syntax,
    see \l {https://sqlite.org/fts5.html#full_text_query_syntax}
    {SQLite FTS5 Extension}.
*/
void QHelpSearchEngine::search(const QString &searchInput)
{
    d->search(searchInput);
}

/*!
    \obsolete
    Use search(const QString &searchInput) instead.
*/
void QHelpSearchEngine::search(const QList<QHelpSearchQuery> &queryList)
{
    if (queryList.isEmpty())
        return;

    d->search(queryList.first().wordList.join(QChar::Space));
}

/*!
    \internal
*/
void QHelpSearchEngine::scheduleIndexDocumentation()
{
    if (d->m_isIndexingScheduled)
        return;

    d->m_isIndexingScheduled = true;
    QTimer::singleShot(0, this, &QHelpSearchEngine::indexDocumentation);
}

void QHelpSearchEngine::indexDocumentation()
{
    d->m_isIndexingScheduled = false;
    d->updateIndex();
}

QT_END_NAMESPACE
