/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlxmllistmodel_p.h"

#include <qqmlcontext.h>
#include <private/qqmlengine_p.h>
#include <private/qv8engine_p.h>
#include <private/qv4value_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4object_p.h>

#include <QDebug>
#include <QStringList>
#include <QMap>
#include <QThread>
#include <QXmlQuery>
#include <QXmlResultItems>
#include <QXmlNodeModelIndex>
#include <QBuffer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QMutex>

#include <private/qabstractitemmodel_p.h>

Q_DECLARE_METATYPE(QQuickXmlQueryResult)

QT_BEGIN_NAMESPACE

using namespace QV4;

typedef QPair<int, int> QQuickXmlListRange;

#define XMLLISTMODEL_CLEAR_ID 0

/*!
    \qmlmodule QtQuick.XmlListModel 2
    \title Qt Quick XmlListModel QML Types
    \ingroup qmlmodules
    \brief Provides QML types for creating models from XML data

    This QML module contains types for creating models from XML data.

    To use the types in this module, import the module with the following line:

    \code
    import QtQuick.XmlListModel 2.0
    \endcode
*/

/*!
    \qmltype XmlRole
    \instantiates QQuickXmlListModelRole
    \inqmlmodule QtQuick.XmlListModel
    \brief For specifying a role to an XmlListModel
    \ingroup qtquick-models

    \sa {Qt QML}
*/

/*!
    \qmlproperty string QtQuick.XmlListModel::XmlRole::name

    The name for the role. This name is used to access the model data for this role.

    For example, the following model has a role named "title", which can be accessed
    from the view's delegate:

    \qml
    XmlListModel {
        id: xmlModel
        // ...
        XmlRole {
            name: "title"
            query: "title/string()"
        }
    }
    \endqml

    \qml
    ListView {
        model: xmlModel
        delegate: Text { text: title }
    }
    \endqml
*/

/*!
    \qmlproperty string QtQuick.XmlListModel::XmlRole::query
    The relative XPath expression query for this role. The query must be relative; it cannot start
    with a '/'.

    For example, if there is an XML document like this:

    \quotefile qml/xmlrole.xml
    Here are some valid XPath expressions for XmlRole queries on this document:

    \snippet qml/xmlrole.qml 0
    \dots 4
    \snippet qml/xmlrole.qml 1

    Accessing the model data for the above roles from a delegate:

    \snippet qml/xmlrole.qml 2

    See the \l{http://www.w3.org/TR/xpath20/}{W3C XPath 2.0 specification} for more information.
*/

/*!
    \qmlproperty bool QtQuick.XmlListModel::XmlRole::isKey
    Defines whether this is a key role.
    Key roles are used to determine whether a set of values should
    be updated or added to the XML list model when XmlListModel::reload()
    is called.

    \sa XmlListModel
*/

struct XmlQueryJob
{
    int queryId;
    QByteArray data;
    QString query;
    QString namespaces;
    QStringList roleQueries;
    QList<void*> roleQueryErrorId; // the ptr to send back if there is an error
    QStringList keyRoleQueries;
    QStringList keyRoleResultsCache;
    QString prefix;
};


class QQuickXmlQueryEngine;
class QQuickXmlQueryThreadObject : public QObject
{
    Q_OBJECT
public:
    QQuickXmlQueryThreadObject(QQuickXmlQueryEngine *);

    void processJobs();
    virtual bool event(QEvent *e);

private:
    QQuickXmlQueryEngine *m_queryEngine;
};


class QQuickXmlQueryEngine : public QThread
{
    Q_OBJECT
public:
    QQuickXmlQueryEngine(QQmlEngine *eng);
    ~QQuickXmlQueryEngine();

    int doQuery(QString query, QString namespaces, QByteArray data, QList<QQuickXmlListModelRole *>* roleObjects, QStringList keyRoleResultsCache);
    void abort(int id);

    void processJobs();

    static QQuickXmlQueryEngine *instance(QQmlEngine *engine);

signals:
    void queryCompleted(const QQuickXmlQueryResult &);
    void error(void*, const QString&);

protected:
    void run();

private:
    void processQuery(XmlQueryJob *job);
    void doQueryJob(XmlQueryJob *job, QQuickXmlQueryResult *currentResult);
    void doSubQueryJob(XmlQueryJob *job, QQuickXmlQueryResult *currentResult);
    void getValuesOfKeyRoles(const XmlQueryJob& currentJob, QStringList *values, QXmlQuery *query) const;
    void addIndexToRangeList(QList<QQuickXmlListRange> *ranges, int index) const;

    QMutex m_mutex;
    QQuickXmlQueryThreadObject *m_threadObject;
    QList<XmlQueryJob> m_jobs;
    QSet<int> m_cancelledJobs;
    QAtomicInt m_queryIds;

    QQmlEngine *m_engine;
    QObject *m_eventLoopQuitHack;

    static QHash<QQmlEngine *,QQuickXmlQueryEngine*> queryEngines;
    static QMutex queryEnginesMutex;
};
QHash<QQmlEngine *,QQuickXmlQueryEngine*> QQuickXmlQueryEngine::queryEngines;
QMutex QQuickXmlQueryEngine::queryEnginesMutex;


QQuickXmlQueryThreadObject::QQuickXmlQueryThreadObject(QQuickXmlQueryEngine *e)
    : m_queryEngine(e)
{
}

void QQuickXmlQueryThreadObject::processJobs()
{
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

bool QQuickXmlQueryThreadObject::event(QEvent *e)
{
    if (e->type() == QEvent::User) {
        m_queryEngine->processJobs();
        return true;
    } else {
        return QObject::event(e);
    }
}



QQuickXmlQueryEngine::QQuickXmlQueryEngine(QQmlEngine *eng)
: QThread(eng), m_threadObject(0), m_queryIds(XMLLISTMODEL_CLEAR_ID + 1), m_engine(eng), m_eventLoopQuitHack(0)
{
    qRegisterMetaType<QQuickXmlQueryResult>("QQuickXmlQueryResult");

    m_eventLoopQuitHack = new QObject;
    m_eventLoopQuitHack->moveToThread(this);
    connect(m_eventLoopQuitHack, SIGNAL(destroyed(QObject*)), SLOT(quit()), Qt::DirectConnection);
    start(QThread::IdlePriority);
}

QQuickXmlQueryEngine::~QQuickXmlQueryEngine()
{
    queryEnginesMutex.lock();
    queryEngines.remove(m_engine);
    queryEnginesMutex.unlock();

    m_eventLoopQuitHack->deleteLater();
    wait();
}

int QQuickXmlQueryEngine::doQuery(QString query, QString namespaces, QByteArray data, QList<QQuickXmlListModelRole *>* roleObjects, QStringList keyRoleResultsCache) {
    {
        QMutexLocker m1(&m_mutex);
        m_queryIds.ref();
        if (m_queryIds.load() <= 0)
            m_queryIds.store(1);
    }

    XmlQueryJob job;
    job.queryId = m_queryIds.load();
    job.data = data;
    job.query = QLatin1String("doc($src)") + query;
    job.namespaces = namespaces;
    job.keyRoleResultsCache = keyRoleResultsCache;

    for (int i=0; i<roleObjects->count(); i++) {
        if (!roleObjects->at(i)->isValid()) {
            job.roleQueries << QString();
            continue;
        }
        job.roleQueries << roleObjects->at(i)->query();
        job.roleQueryErrorId << static_cast<void*>(roleObjects->at(i));
        if (roleObjects->at(i)->isKey())
            job.keyRoleQueries << job.roleQueries.last();
    }

    {
        QMutexLocker ml(&m_mutex);
        m_jobs.append(job);
        if (m_threadObject)
            m_threadObject->processJobs();
    }

    return job.queryId;
}

void QQuickXmlQueryEngine::abort(int id)
{
    QMutexLocker ml(&m_mutex);
    if (id != -1)
        m_cancelledJobs.insert(id);
}

void QQuickXmlQueryEngine::run()
{
    m_mutex.lock();
    m_threadObject = new QQuickXmlQueryThreadObject(this);
    m_mutex.unlock();

    processJobs();
    exec();

    delete m_threadObject;
    m_threadObject = 0;
}

void QQuickXmlQueryEngine::processJobs()
{
    QMutexLocker locker(&m_mutex);

    while (true) {
        if (m_jobs.isEmpty())
            return;

        XmlQueryJob currentJob = m_jobs.takeLast();
        while (m_cancelledJobs.remove(currentJob.queryId)) {
            if (m_jobs.isEmpty())
              return;
            currentJob = m_jobs.takeLast();
        }

        locker.unlock();
        processQuery(&currentJob);
        locker.relock();
    }
}

QQuickXmlQueryEngine *QQuickXmlQueryEngine::instance(QQmlEngine *engine)
{
    queryEnginesMutex.lock();
    QQuickXmlQueryEngine *queryEng = queryEngines.value(engine);
    if (!queryEng) {
        queryEng = new QQuickXmlQueryEngine(engine);
        queryEngines.insert(engine, queryEng);
    }
    queryEnginesMutex.unlock();

    return queryEng;
}

void QQuickXmlQueryEngine::processQuery(XmlQueryJob *job)
{
    QQuickXmlQueryResult result;
    result.queryId = job->queryId;
    doQueryJob(job, &result);
    doSubQueryJob(job, &result);

    {
        QMutexLocker ml(&m_mutex);
        if (m_cancelledJobs.contains(job->queryId)) {
            m_cancelledJobs.remove(job->queryId);
        } else {
            emit queryCompleted(result);
        }
    }
}

void QQuickXmlQueryEngine::doQueryJob(XmlQueryJob *currentJob, QQuickXmlQueryResult *currentResult)
{
    Q_ASSERT(currentJob->queryId != -1);

    QString r;
    QXmlQuery query;
    QBuffer buffer(&currentJob->data);
    buffer.open(QIODevice::ReadOnly);
    query.bindVariable(QLatin1String("src"), &buffer);
    query.setQuery(currentJob->namespaces + currentJob->query);
    query.evaluateTo(&r);

    //always need a single root element
    QByteArray xml = "<dummy:items xmlns:dummy=\"http://qtsotware.com/dummy\">\n" + r.toUtf8() + "</dummy:items>";
    QBuffer b(&xml);
    b.open(QIODevice::ReadOnly);

    QString namespaces = QLatin1String("declare namespace dummy=\"http://qtsotware.com/dummy\";\n") + currentJob->namespaces;
    QString prefix = QLatin1String("doc($inputDocument)/dummy:items/*");

    //figure out how many items we are dealing with
    int count = -1;
    {
        QXmlResultItems result;
        QXmlQuery countquery;
        countquery.bindVariable(QLatin1String("inputDocument"), &b);
        countquery.setQuery(namespaces + QLatin1String("count(") + prefix + QLatin1Char(')'));
        countquery.evaluateTo(&result);
        QXmlItem item(result.next());
        if (item.isAtomicValue())
            count = item.toAtomicValue().toInt();
    }

    currentJob->data = xml;
    currentJob->prefix = namespaces + prefix + QLatin1Char('/');
    currentResult->size = (count > 0 ? count : 0);
}

void QQuickXmlQueryEngine::getValuesOfKeyRoles(const XmlQueryJob& currentJob, QStringList *values, QXmlQuery *query) const
{
    const QStringList &keysQueries = currentJob.keyRoleQueries;
    QString keysQuery;
    if (keysQueries.count() == 1)
        keysQuery = currentJob.prefix + keysQueries[0];
    else if (keysQueries.count() > 1)
        keysQuery = currentJob.prefix + QLatin1String("concat(") + keysQueries.join(QLatin1Char(',')) + QLatin1Char(')');

    if (!keysQuery.isEmpty()) {
        query->setQuery(keysQuery);
        QXmlResultItems resultItems;
        query->evaluateTo(&resultItems);
        QXmlItem item(resultItems.next());
        while (!item.isNull()) {
            values->append(item.toAtomicValue().toString());
            item = resultItems.next();
        }
    }
}

void QQuickXmlQueryEngine::addIndexToRangeList(QList<QQuickXmlListRange> *ranges, int index) const {
    if (ranges->isEmpty())
        ranges->append(qMakePair(index, 1));
    else if (ranges->last().first + ranges->last().second == index)
        ranges->last().second += 1;
    else
        ranges->append(qMakePair(index, 1));
}

void QQuickXmlQueryEngine::doSubQueryJob(XmlQueryJob *currentJob, QQuickXmlQueryResult *currentResult)
{
    Q_ASSERT(currentJob->queryId != -1);

    QBuffer b(&currentJob->data);
    b.open(QIODevice::ReadOnly);

    QXmlQuery subquery;
    subquery.bindVariable(QLatin1String("inputDocument"), &b);

    QStringList keyRoleResults;
    getValuesOfKeyRoles(*currentJob, &keyRoleResults, &subquery);

    // See if any values of key roles have been inserted or removed.

    if (currentJob->keyRoleResultsCache.isEmpty()) {
        currentResult->inserted << qMakePair(0, currentResult->size);
    } else {
        if (keyRoleResults != currentJob->keyRoleResultsCache) {
            QStringList temp;
            for (int i=0; i<currentJob->keyRoleResultsCache.count(); i++) {
                if (!keyRoleResults.contains(currentJob->keyRoleResultsCache[i]))
                    addIndexToRangeList(&currentResult->removed, i);
                else
                    temp << currentJob->keyRoleResultsCache[i];
            }
            for (int i=0; i<keyRoleResults.count(); i++) {
                if (temp.count() == i || keyRoleResults[i] != temp[i]) {
                    temp.insert(i, keyRoleResults[i]);
                    addIndexToRangeList(&currentResult->inserted, i);
                }
            }
        }
    }
    currentResult->keyRoleResultsCache = keyRoleResults;

    // Get the new values for each role.
    //### we might be able to condense even further (query for everything in one go)
    const QStringList &queries = currentJob->roleQueries;
    for (int i = 0; i < queries.size(); ++i) {
        QList<QVariant> resultList;
        if (!queries[i].isEmpty()) {
            subquery.setQuery(currentJob->prefix + QLatin1String("(let $v := string(") + queries[i] + QLatin1String(") return if ($v) then ") + queries[i] + QLatin1String(" else \"\")"));
            if (subquery.isValid()) {
                QXmlResultItems resultItems;
                subquery.evaluateTo(&resultItems);
                QXmlItem item(resultItems.next());
                while (!item.isNull()) {
                    resultList << item.toAtomicValue(); //### we used to trim strings
                    item = resultItems.next();
                }
            } else {
                emit error(currentJob->roleQueryErrorId.at(i), queries[i]);
            }
        }
        //### should warn here if things have gone wrong.
        while (resultList.count() < currentResult->size)
            resultList << QVariant();
        currentResult->data << resultList;
        b.seek(0);
    }

    //this method is much slower, but works better for incremental loading
    /*for (int j = 0; j < m_size; ++j) {
        QList<QVariant> resultList;
        for (int i = 0; i < m_roleObjects->size(); ++i) {
            QQuickXmlListModelRole *role = m_roleObjects->at(i);
            subquery.setQuery(m_prefix.arg(j+1) + role->query());
            if (role->isStringList()) {
                QStringList data;
                subquery.evaluateTo(&data);
                resultList << QVariant(data);
                //qDebug() << data;
            } else {
                QString s;
                subquery.evaluateTo(&s);
                if (role->isCData()) {
                    //un-escape
                    s.replace(QLatin1String("&lt;"), QLatin1String("<"));
                    s.replace(QLatin1String("&gt;"), QLatin1String(">"));
                    s.replace(QLatin1String("&amp;"), QLatin1String("&"));
                }
                resultList << s.trimmed();
                //qDebug() << s;
            }
            b.seek(0);
        }
        m_modelData << resultList;
    }*/
}

class QQuickXmlListModelPrivate : public QAbstractItemModelPrivate
{
    Q_DECLARE_PUBLIC(QQuickXmlListModel)
public:
    QQuickXmlListModelPrivate()
        : isComponentComplete(true), size(0), highestRole(Qt::UserRole)
        , reply(0), status(QQuickXmlListModel::Null), progress(0.0)
        , queryId(-1), roleObjects(), redirectCount(0) {}


    void notifyQueryStarted(bool remoteSource) {
        Q_Q(QQuickXmlListModel);
        progress = remoteSource ? 0.0 : 1.0;
        status = QQuickXmlListModel::Loading;
        errorString.clear();
        emit q->progressChanged(progress);
        emit q->statusChanged(status);
    }

    void deleteReply() {
        Q_Q(QQuickXmlListModel);
        if (reply) {
            QObject::disconnect(reply, 0, q, 0);
            reply->deleteLater();
            reply = 0;
        }
    }

    bool isComponentComplete;
    QUrl src;
    QString xml;
    QString query;
    QString namespaces;
    int size;
    QList<int> roles;
    QStringList roleNames;
    int highestRole;

    QNetworkReply *reply;
    QQuickXmlListModel::Status status;
    QString errorString;
    qreal progress;
    int queryId;
    QStringList keyRoleResultsCache;
    QList<QQuickXmlListModelRole *> roleObjects;

    static void append_role(QQmlListProperty<QQuickXmlListModelRole> *list, QQuickXmlListModelRole *role);
    static void clear_role(QQmlListProperty<QQuickXmlListModelRole> *list);
    QList<QList<QVariant> > data;
    int redirectCount;
};


void QQuickXmlListModelPrivate::append_role(QQmlListProperty<QQuickXmlListModelRole> *list, QQuickXmlListModelRole *role)
{
    QQuickXmlListModel *_this = qobject_cast<QQuickXmlListModel *>(list->object);
    if (_this && role) {
        int i = _this->d_func()->roleObjects.count();
        _this->d_func()->roleObjects.append(role);
        if (_this->d_func()->roleNames.contains(role->name())) {
            qmlInfo(role) << QQuickXmlListModel::tr("\"%1\" duplicates a previous role name and will be disabled.").arg(role->name());
            return;
        }
        _this->d_func()->roles.insert(i, _this->d_func()->highestRole);
        _this->d_func()->roleNames.insert(i, role->name());
        ++_this->d_func()->highestRole;
    }
}

//### clear needs to invalidate any cached data (in data table) as well
//    (and the model should emit the appropriate signals)
void QQuickXmlListModelPrivate::clear_role(QQmlListProperty<QQuickXmlListModelRole> *list)
{
    QQuickXmlListModel *_this = static_cast<QQuickXmlListModel *>(list->object);
    _this->d_func()->roles.clear();
    _this->d_func()->roleNames.clear();
    _this->d_func()->roleObjects.clear();
}

/*!
    \qmltype XmlListModel
    \instantiates QQuickXmlListModel
    \inqmlmodule QtQuick.XmlListModel
    \brief For specifying a read-only model using XPath expressions
    \ingroup qtquick-models


    To use this element, you will need to import the module with the following line:
    \code
    import QtQuick.XmlListModel 2.0
    \endcode

    XmlListModel is used to create a read-only model from XML data. It can be used as a data source
    for view elements (such as ListView, PathView, GridView) and other elements that interact with model
    data (such as \l Repeater).

    For example, if there is a XML document at http://www.mysite.com/feed.xml like this:

    \code
    <?xml version="1.0" encoding="utf-8"?>
    <rss version="2.0">
        ...
        <channel>
            <item>
                <title>A blog post</title>
                <pubDate>Sat, 07 Sep 2010 10:00:01 GMT</pubDate>
            </item>
            <item>
                <title>Another blog post</title>
                <pubDate>Sat, 07 Sep 2010 15:35:01 GMT</pubDate>
            </item>
        </channel>
    </rss>
    \endcode

    A XmlListModel could create a model from this data, like this:

    \qml
    import QtQuick 2.0
    import QtQuick.XmlListModel 2.0

    XmlListModel {
        id: xmlModel
        source: "http://www.mysite.com/feed.xml"
        query: "/rss/channel/item"

        XmlRole { name: "title"; query: "title/string()" }
        XmlRole { name: "pubDate"; query: "pubDate/string()" }
    }
    \endqml

    The \l {XmlListModel::query}{query} value of "/rss/channel/item" specifies that the XmlListModel should generate
    a model item for each \c <item> in the XML document.

    The XmlRole objects define the
    model item attributes. Here, each model item will have \c title and \c pubDate
    attributes that match the \c title and \c pubDate values of its corresponding \c <item>.
    (See \l XmlRole::query for more examples of valid XPath expressions for XmlRole.)

    The model could be used in a ListView, like this:

    \qml
    ListView {
        width: 180; height: 300
        model: xmlModel
        delegate: Text { text: title + ": " + pubDate }
    }
    \endqml

    \image qml-xmllistmodel-example.png

    The XmlListModel data is loaded asynchronously, and \l status
    is set to \c XmlListModel.Ready when loading is complete.
    Note this means when XmlListModel is used for a view, the view is not
    populated until the model is loaded.


    \section2 Using key XML roles

    You can define certain roles as "keys" so that when reload() is called,
    the model will only add and refresh data that contains new values for
    these keys.

    For example, if above role for "pubDate" was defined like this instead:

    \qml
        XmlRole { name: "pubDate"; query: "pubDate/string()"; isKey: true }
    \endqml

    Then when reload() is called, the model will only add and reload
    items with a "pubDate" value that is not already
    present in the model.

    This is useful when displaying the contents of XML documents that
    are incrementally updated (such as RSS feeds) to avoid repainting the
    entire contents of a model in a view.

    If multiple key roles are specified, the model only adds and reload items
    with a combined value of all key roles that is not already present in
    the model.

    \sa {Qt Quick Demo - RSS News}
*/

QQuickXmlListModel::QQuickXmlListModel(QObject *parent)
    : QAbstractListModel(*(new QQuickXmlListModelPrivate), parent)
{
}

QQuickXmlListModel::~QQuickXmlListModel()
{
}

/*!
    \qmlproperty list<XmlRole> QtQuick.XmlListModel::XmlListModel::roles

    The roles to make available for this model.
*/
QQmlListProperty<QQuickXmlListModelRole> QQuickXmlListModel::roleObjects()
{
    Q_D(QQuickXmlListModel);
    QQmlListProperty<QQuickXmlListModelRole> list(this, d->roleObjects);
    list.append = &QQuickXmlListModelPrivate::append_role;
    list.clear = &QQuickXmlListModelPrivate::clear_role;
    return list;
}

QModelIndex QQuickXmlListModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_D(const QQuickXmlListModel);
    return !parent.isValid() && column == 0 && row >= 0 && row < d->size
            ? createIndex(row, column)
            : QModelIndex();
}

int QQuickXmlListModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const QQuickXmlListModel);
    return !parent.isValid() ? d->size : 0;
}

QVariant QQuickXmlListModel::data(const QModelIndex &index, int role) const
{
    Q_D(const QQuickXmlListModel);
    const int roleIndex = d->roles.indexOf(role);
    return (roleIndex == -1 || !index.isValid())
            ? QVariant()
            : d->data.value(roleIndex).value(index.row());
}

QHash<int, QByteArray> QQuickXmlListModel::roleNames() const
{
    Q_D(const QQuickXmlListModel);
    QHash<int,QByteArray> roleNames;
    for (int i = 0; i < d->roles.count(); ++i)
        roleNames.insert(d->roles.at(i), d->roleNames.at(i).toUtf8());
    return roleNames;
}

/*!
    \qmlproperty int QtQuick.XmlListModel::XmlListModel::count
    The number of data entries in the model.
*/
int QQuickXmlListModel::count() const
{
    Q_D(const QQuickXmlListModel);
    return d->size;
}

/*!
    \qmlproperty url QtQuick.XmlListModel::XmlListModel::source
    The location of the XML data source.

    If both \c source and \l xml are set, \l xml is used.
*/
QUrl QQuickXmlListModel::source() const
{
    Q_D(const QQuickXmlListModel);
    return d->src;
}

void QQuickXmlListModel::setSource(const QUrl &src)
{
    Q_D(QQuickXmlListModel);
    if (d->src != src) {
        d->src = src;
        if (d->xml.isEmpty())   // src is only used if d->xml is not set
            reload();
        emit sourceChanged();
   }
}

/*!
    \qmlproperty string QtQuick.XmlListModel::XmlListModel::xml
    This property holds the XML data for this model, if set.

    The text is assumed to be UTF-8 encoded.

    If both \l source and \c xml are set, \c xml is used.
*/
QString QQuickXmlListModel::xml() const
{
    Q_D(const QQuickXmlListModel);
    return d->xml;
}

void QQuickXmlListModel::setXml(const QString &xml)
{
    Q_D(QQuickXmlListModel);
    if (d->xml != xml) {
        d->xml = xml;
        reload();
        emit xmlChanged();
    }
}

/*!
    \qmlproperty string QtQuick.XmlListModel::XmlListModel::query
    An absolute XPath query representing the base query for creating model items
    from this model's XmlRole objects. The query should start with '/' or '//'.
*/
QString QQuickXmlListModel::query() const
{
    Q_D(const QQuickXmlListModel);
    return d->query;
}

void QQuickXmlListModel::setQuery(const QString &query)
{
    Q_D(QQuickXmlListModel);
    if (!query.startsWith(QLatin1Char('/'))) {
        qmlInfo(this) << QCoreApplication::translate("QQuickXmlRoleList", "An XmlListModel query must start with '/' or \"//\"");
        return;
    }

    if (d->query != query) {
        d->query = query;
        reload();
        emit queryChanged();
    }
}

/*!
    \qmlproperty string QtQuick.XmlListModel::XmlListModel::namespaceDeclarations
    The namespace declarations to be used in the XPath queries.

    The namespaces should be declared as in XQuery. For example, if a requested document
    at http://mysite.com/feed.xml uses the namespace "http://www.w3.org/2005/Atom",
    this can be declared as the default namespace:

    \qml
    XmlListModel {
        source: "http://mysite.com/feed.xml"
        query: "/feed/entry"
        namespaceDeclarations: "declare default element namespace 'http://www.w3.org/2005/Atom';"

        XmlRole { name: "title"; query: "title/string()" }
    }
    \endqml
*/
QString QQuickXmlListModel::namespaceDeclarations() const
{
    Q_D(const QQuickXmlListModel);
    return d->namespaces;
}

void QQuickXmlListModel::setNamespaceDeclarations(const QString &declarations)
{
    Q_D(QQuickXmlListModel);
    if (d->namespaces != declarations) {
        d->namespaces = declarations;
        reload();
        emit namespaceDeclarationsChanged();
    }
}

/*!
    \qmlmethod object QtQuick.XmlListModel::XmlListModel::get(int index)

    Returns the item at \a index in the model.

    For example, for a model like this:

    \qml
    XmlListModel {
        id: model
        source: "http://mysite.com/feed.xml"
        query: "/feed/entry"
        XmlRole { name: "title"; query: "title/string()" }
    }
    \endqml

    This will access the \c title value for the first item in the model:

    \js
    var title = model.get(0).title;
    \endjs
*/
QQmlV4Handle QQuickXmlListModel::get(int index) const
{
    // Must be called with a context and handle scope
    Q_D(const QQuickXmlListModel);

    if (index < 0 || index >= count())
        return QQmlV4Handle(Encode::undefined());

    QQmlEngine *engine = qmlContext(this)->engine();
    QV8Engine *v8engine = QQmlEnginePrivate::getV8Engine(engine);
    ExecutionEngine *v4engine = QV8Engine::getV4(v8engine);
    Scope scope(v4engine);
    Scoped<Object> o(scope, v4engine->newObject());
    ScopedString name(scope);
    ScopedValue value(scope);
    for (int ii = 0; ii < d->roleObjects.count(); ++ii) {
        name = v4engine->newIdentifier(d->roleObjects[ii]->name());
        value = v4engine->fromVariant(d->data.value(ii).value(index));
        o->insertMember(name.getPointer(), value);
    }

    return QQmlV4Handle(o);
}

/*!
    \qmlproperty enumeration QtQuick.XmlListModel::XmlListModel::status
    Specifies the model loading status, which can be one of the following:

    \list
    \li XmlListModel.Null - No XML data has been set for this model.
    \li XmlListModel.Ready - The XML data has been loaded into the model.
    \li XmlListModel.Loading - The model is in the process of reading and loading XML data.
    \li XmlListModel.Error - An error occurred while the model was loading. See errorString() for details
       about the error.
    \endlist

    \sa progress

*/
QQuickXmlListModel::Status QQuickXmlListModel::status() const
{
    Q_D(const QQuickXmlListModel);
    return d->status;
}

/*!
    \qmlproperty real QtQuick.XmlListModel::XmlListModel::progress

    This indicates the current progress of the downloading of the XML data
    source. This value ranges from 0.0 (no data downloaded) to
    1.0 (all data downloaded). If the XML data is not from a remote source,
    the progress becomes 1.0 as soon as the data is read.

    Note that when the progress is 1.0, the XML data has been downloaded, but
    it is yet to be loaded into the model at this point. Use the status
    property to find out when the XML data has been read and loaded into
    the model.

    \sa status, source
*/
qreal QQuickXmlListModel::progress() const
{
    Q_D(const QQuickXmlListModel);
    return d->progress;
}

/*!
    \qmlmethod QtQuick.XmlListModel::XmlListModel::errorString()

    Returns a string description of the last error that occurred
    if \l status is XmlListModel::Error.
*/
QString QQuickXmlListModel::errorString() const
{
    Q_D(const QQuickXmlListModel);
    return d->errorString;
}

void QQuickXmlListModel::classBegin()
{
    Q_D(QQuickXmlListModel);
    d->isComponentComplete = false;

    QQuickXmlQueryEngine *queryEngine = QQuickXmlQueryEngine::instance(qmlEngine(this));
    connect(queryEngine, SIGNAL(queryCompleted(QQuickXmlQueryResult)),
            SLOT(queryCompleted(QQuickXmlQueryResult)));
    connect(queryEngine, SIGNAL(error(void*,QString)),
            SLOT(queryError(void*,QString)));
}

void QQuickXmlListModel::componentComplete()
{
    Q_D(QQuickXmlListModel);
    d->isComponentComplete = true;
    reload();
}

/*!
    \qmlmethod QtQuick.XmlListModel::XmlListModel::reload()

    Reloads the model.

    If no key roles have been specified, all existing model
    data is removed, and the model is rebuilt from scratch.

    Otherwise, items are only added if the model does not already
    contain items with matching key role values.

    \sa {Using key XML roles}, XmlRole::isKey
*/
void QQuickXmlListModel::reload()
{
    Q_D(QQuickXmlListModel);

    if (!d->isComponentComplete)
        return;

    QQuickXmlQueryEngine::instance(qmlEngine(this))->abort(d->queryId);
    d->queryId = -1;

    if (d->size < 0)
        d->size = 0;

    if (d->reply) {
        d->reply->abort();
        d->deleteReply();
    }

    if (!d->xml.isEmpty()) {
        d->queryId = QQuickXmlQueryEngine::instance(qmlEngine(this))->doQuery(d->query, d->namespaces, d->xml.toUtf8(), &d->roleObjects, d->keyRoleResultsCache);
        d->notifyQueryStarted(false);

    } else if (d->src.isEmpty()) {
        d->queryId = XMLLISTMODEL_CLEAR_ID;
        d->notifyQueryStarted(false);
        QTimer::singleShot(0, this, SLOT(dataCleared()));

    } else {
        d->notifyQueryStarted(true);
        QNetworkRequest req(d->src);
        req.setRawHeader("Accept", "application/xml,*/*");
        d->reply = qmlContext(this)->engine()->networkAccessManager()->get(req);
        QObject::connect(d->reply, SIGNAL(finished()), this, SLOT(requestFinished()));
        QObject::connect(d->reply, SIGNAL(downloadProgress(qint64,qint64)),
                         this, SLOT(requestProgress(qint64,qint64)));
    }
}

#define XMLLISTMODEL_MAX_REDIRECT 16

void QQuickXmlListModel::requestFinished()
{
    Q_D(QQuickXmlListModel);

    d->redirectCount++;
    if (d->redirectCount < XMLLISTMODEL_MAX_REDIRECT) {
        QVariant redirect = d->reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid()) {
            QUrl url = d->reply->url().resolved(redirect.toUrl());
            d->deleteReply();
            setSource(url);
            return;
        }
    }
    d->redirectCount = 0;

    if (d->reply->error() != QNetworkReply::NoError) {
        d->errorString = d->reply->errorString();
        d->deleteReply();

        if (d->size > 0) {
            beginRemoveRows(QModelIndex(), 0, d->size - 1);
            d->data.clear();
            d->size = 0;
            endRemoveRows();
            emit countChanged();
        }

        d->status = Error;
        d->queryId = -1;
        emit statusChanged(d->status);
    } else {
        QByteArray data = d->reply->readAll();
        if (data.isEmpty()) {
            d->queryId = XMLLISTMODEL_CLEAR_ID;
            QTimer::singleShot(0, this, SLOT(dataCleared()));
        } else {
            d->queryId = QQuickXmlQueryEngine::instance(qmlEngine(this))->doQuery(d->query, d->namespaces, data, &d->roleObjects, d->keyRoleResultsCache);
        }
        d->deleteReply();

        d->progress = 1.0;
        emit progressChanged(d->progress);
    }
}

void QQuickXmlListModel::requestProgress(qint64 received, qint64 total)
{
    Q_D(QQuickXmlListModel);
    if (d->status == Loading && total > 0) {
        d->progress = qreal(received)/total;
        emit progressChanged(d->progress);
    }
}

void QQuickXmlListModel::dataCleared()
{
    Q_D(QQuickXmlListModel);
    QQuickXmlQueryResult r;
    r.queryId = XMLLISTMODEL_CLEAR_ID;
    r.size = 0;
    r.removed << qMakePair(0, count());
    r.keyRoleResultsCache = d->keyRoleResultsCache;
    queryCompleted(r);
}

void QQuickXmlListModel::queryError(void* object, const QString& error)
{
    // Be extra careful, object may no longer exist, it's just an ID.
    Q_D(QQuickXmlListModel);
    for (int i=0; i<d->roleObjects.count(); i++) {
        if (d->roleObjects.at(i) == static_cast<QQuickXmlListModelRole*>(object)) {
            qmlInfo(d->roleObjects.at(i)) << QQuickXmlListModel::tr("invalid query: \"%1\"").arg(error);
            return;
        }
    }
    qmlInfo(this) << QQuickXmlListModel::tr("invalid query: \"%1\"").arg(error);
}

void QQuickXmlListModel::queryCompleted(const QQuickXmlQueryResult &result)
{
    Q_D(QQuickXmlListModel);
    if (result.queryId != d->queryId)
        return;

    int origCount = d->size;
    bool sizeChanged = result.size != d->size;

    d->size = result.size;
    d->data = result.data;
    d->keyRoleResultsCache = result.keyRoleResultsCache;
    if (d->src.isEmpty() && d->xml.isEmpty())
        d->status = Null;
    else
        d->status = Ready;
    d->errorString.clear();
    d->queryId = -1;

    bool hasKeys = false;
    for (int i=0; i<d->roleObjects.count(); i++) {
        if (d->roleObjects[i]->isKey()) {
            hasKeys = true;
            break;
        }
    }
    if (!hasKeys) {
        if (origCount > 0) {
            beginRemoveRows(QModelIndex(), 0, origCount - 1);
            endRemoveRows();
        }
        if (d->size > 0) {
            beginInsertRows(QModelIndex(), 0, d->size - 1);
            endInsertRows();
        }
    } else {
        for (int i=0; i<result.removed.count(); i++) {
            const int index = result.removed[i].first;
            const int count = result.removed[i].second;
            if (count > 0) {
                beginRemoveRows(QModelIndex(), index, index + count - 1);
                endRemoveRows();
            }
        }
        for (int i=0; i<result.inserted.count(); i++) {
            const int index = result.inserted[i].first;
            const int count = result.inserted[i].second;
            if (count > 0) {
                beginInsertRows(QModelIndex(), index, index + count - 1);
                endInsertRows();
            }
        }
    }
    if (sizeChanged)
        emit countChanged();

    emit statusChanged(d->status);
}

QT_END_NAMESPACE

#include <qqmlxmllistmodel.moc>
