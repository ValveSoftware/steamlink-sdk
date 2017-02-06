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

#include "qhelpcontentwidget.h"
#include "qhelpenginecore.h"
#include "qhelpengine_p.h"
#include "qhelpdbreader_p.h"

#include <QDir>
#include <QtCore/QStack>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtWidgets/QHeaderView>

QT_BEGIN_NAMESPACE

class QHelpContentItemPrivate
{
public:
    QHelpContentItemPrivate(const QString &t, const QString &l,
                            QHelpDBReader *r, QHelpContentItem *p)
    {
        parent = p;
        title = t;
        link = l;
        helpDBReader = r;
    }

    QList<QHelpContentItem*> childItems;
    QHelpContentItem *parent;
    QString title;
    QString link;
    QHelpDBReader *helpDBReader;
};

class QHelpContentProvider : public QThread
{
    Q_OBJECT
public:
    QHelpContentProvider(QHelpEnginePrivate *helpEngine);
    ~QHelpContentProvider();
    void collectContents(const QString &customFilterName);
    void stopCollecting();
    QHelpContentItem *rootItem();

signals:
    void finishedSuccessFully();

private:
    void run();

    QHelpEnginePrivate *m_helpEngine;
    QStringList m_filterAttributes;
    QQueue<QHelpContentItem*> m_rootItems;
    QMutex m_mutex;
    bool m_abort;
};

class QHelpContentModelPrivate
{
public:
    QHelpContentItem *rootItem;
    QHelpContentProvider *qhelpContentProvider;
};



/*!
    \class QHelpContentItem
    \inmodule QtHelp
    \brief The QHelpContentItem class provides an item for use with QHelpContentModel.
    \since 4.4
*/

QHelpContentItem::QHelpContentItem(const QString &name, const QString &link,
                                   QHelpDBReader *reader, QHelpContentItem *parent)
{
    d = new QHelpContentItemPrivate(name, link, reader, parent);
}

/*!
    Destroys the help content item.
*/
QHelpContentItem::~QHelpContentItem()
{
    qDeleteAll(d->childItems);
    delete d;
}

void QHelpContentItem::appendChild(QHelpContentItem *item)
{
    d->childItems.append(item);
}

/*!
    Returns the child of the content item in the give \a row.

    \sa parent()
*/
QHelpContentItem *QHelpContentItem::child(int row) const
{
    if (row >= childCount())
        return 0;
    return d->childItems.value(row);
}

/*!
    Returns the number of child items.
*/
int QHelpContentItem::childCount() const
{
    return d->childItems.count();
}

/*!
    Returns the row of this item from its parents view.
*/
int QHelpContentItem::row() const
{
    if (d->parent)
        return d->parent->d->childItems.indexOf(const_cast<QHelpContentItem*>(this));
    return 0;
}

/*!
    Returns the title of the content item.
*/
QString QHelpContentItem::title() const
{
    return d->title;
}

/*!
    Returns the URL of this content item.
*/
QUrl QHelpContentItem::url() const
{
    return d->helpDBReader->urlOfPath(d->link);
}

/*!
    Returns the parent content item.
*/
QHelpContentItem *QHelpContentItem::parent() const
{
    return d->parent;
}

/*!
    Returns the position of a given \a child.
*/
int QHelpContentItem::childPosition(QHelpContentItem *child) const
{
    return d->childItems.indexOf(child);
}



QHelpContentProvider::QHelpContentProvider(QHelpEnginePrivate *helpEngine)
    : QThread(helpEngine)
{
    m_helpEngine = helpEngine;
    m_abort = false;
}

QHelpContentProvider::~QHelpContentProvider()
{
    stopCollecting();
}

void QHelpContentProvider::collectContents(const QString &customFilterName)
{
    m_mutex.lock();
    m_filterAttributes = m_helpEngine->q->filterAttributes(customFilterName);
    m_mutex.unlock();
    if (!isRunning()) {
        start(LowPriority);
    } else {
        stopCollecting();
        start(LowPriority);
    }
}

void QHelpContentProvider::stopCollecting()
{
    if (isRunning()) {
        m_mutex.lock();
        m_abort = true;
        m_mutex.unlock();
        wait();
        // we need to force-set m_abort to false, because the thread might either have
        // finished between the isRunning() check and the "m_abort = true" above, or the
        // isRunning() check might already happen after the "m_abort = false" in the run() method,
        // either way never resetting m_abort to false from within the run() method
        m_abort = false;
    }
    qDeleteAll(m_rootItems);
    m_rootItems.clear();
}

QHelpContentItem *QHelpContentProvider::rootItem()
{
    QMutexLocker locker(&m_mutex);
    if (m_rootItems.isEmpty())
        return 0;
    return m_rootItems.dequeue();
}

void QHelpContentProvider::run()
{
    QString title;
    QString link;
    int depth = 0;
    QHelpContentItem *item = 0;

    m_mutex.lock();
    QHelpContentItem * const rootItem = new QHelpContentItem(QString(), QString(), 0);
    QStringList atts = m_filterAttributes;
    const QStringList fileNames = m_helpEngine->orderedFileNameList;
    m_mutex.unlock();

    foreach (const QString &dbFileName, fileNames) {
        m_mutex.lock();
        if (m_abort) {
            delete rootItem;
            m_abort = false;
            m_mutex.unlock();
            return;
        }
        m_mutex.unlock();
        QHelpDBReader reader(dbFileName,
            QHelpGlobal::uniquifyConnectionName(dbFileName +
            QLatin1String("FromQHelpContentProvider"),
            QThread::currentThread()), 0);
        if (!reader.init())
            continue;
        foreach (const QByteArray& ba, reader.contentsForFilter(atts)) {
            if (ba.size() < 1)
                continue;

            int _depth = 0;
            bool _root = false;
            QStack<QHelpContentItem*> stack;

            QDataStream s(ba);
            for (;;) {
                s >> depth;
                s >> link;
                s >> title;
                if (title.isEmpty())
                    break;
CHECK_DEPTH:
                if (depth == 0) {
                    m_mutex.lock();
                    item = new QHelpContentItem(title, link,
                        m_helpEngine->fileNameReaderMap.value(dbFileName), rootItem);
                    rootItem->appendChild(item);
                    m_mutex.unlock();
                    stack.push(item);
                    _depth = 1;
                    _root = true;
                } else {
                    if (depth > _depth && _root) {
                        _depth = depth;
                        stack.push(item);
                    }
                    if (depth == _depth) {
                        item = new QHelpContentItem(title, link,
                            m_helpEngine->fileNameReaderMap.value(dbFileName), stack.top());
                        stack.top()->appendChild(item);
                    } else if (depth < _depth) {
                        stack.pop();
                        --_depth;
                        goto CHECK_DEPTH;
                    }
                }
            }
        }
    }
    m_mutex.lock();
    m_rootItems.enqueue(rootItem);
    m_abort = false;
    m_mutex.unlock();
    emit finishedSuccessFully();
}



/*!
    \class QHelpContentModel
    \inmodule QtHelp
    \brief The QHelpContentModel class provides a model that supplies content to views.
    \since 4.4
*/

/*!
    \fn void QHelpContentModel::contentsCreationStarted()

    This signal is emitted when the creation of the contents has
    started. The current contents are invalid from this point on
    until the signal contentsCreated() is emitted.

    \sa isCreatingContents()
*/

/*!
    \fn void QHelpContentModel::contentsCreated()

    This signal is emitted when the contents have been created.
*/

QHelpContentModel::QHelpContentModel(QHelpEnginePrivate *helpEngine)
    : QAbstractItemModel(helpEngine)
{
    d = new QHelpContentModelPrivate();
    d->rootItem = 0;
    d->qhelpContentProvider = new QHelpContentProvider(helpEngine);

    connect(d->qhelpContentProvider, SIGNAL(finishedSuccessFully()),
        this, SLOT(insertContents()), Qt::QueuedConnection);
    connect(helpEngine->q, SIGNAL(readersAboutToBeInvalidated()), this, SLOT(invalidateContents()));
}

/*!
    Destroys the help content model.
*/
QHelpContentModel::~QHelpContentModel()
{
    delete d->rootItem;
    delete d;
}

void QHelpContentModel::invalidateContents(bool onShutDown)
{
    if (onShutDown) {
        disconnect(this, SLOT(insertContents()));
    } else {
        beginResetModel();
    }
    d->qhelpContentProvider->stopCollecting();
    if (d->rootItem) {
        delete d->rootItem;
        d->rootItem = 0;
    }
    if (!onShutDown)
        endResetModel();
}

/*!
    Creates new contents by querying the help system
    for contents specified for the \a customFilterName.
*/
void QHelpContentModel::createContents(const QString &customFilterName)
{
    d->qhelpContentProvider->collectContents(customFilterName);
    emit contentsCreationStarted();
}

void QHelpContentModel::insertContents()
{
    QHelpContentItem * const newRootItem = d->qhelpContentProvider->rootItem();
    if (!newRootItem)
        return;
    beginResetModel();
    if (d->rootItem)
        delete d->rootItem;
    d->rootItem = newRootItem;
    endResetModel();
    emit contentsCreated();
}

/*!
    Returns true if the contents are currently rebuilt, otherwise
    false.
*/
bool QHelpContentModel::isCreatingContents() const
{
    return d->qhelpContentProvider->isRunning();
}

/*!
    Returns the help content item at the model index position
    \a index.
*/
QHelpContentItem *QHelpContentModel::contentItemAt(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<QHelpContentItem*>(index.internalPointer());
    else
        return d->rootItem;
}

/*!
    Returns the index of the item in the model specified by
    the given \a row, \a column and \a parent index.
*/
QModelIndex QHelpContentModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!d->rootItem)
        return QModelIndex();

    QHelpContentItem *parentItem = contentItemAt(parent);
    QHelpContentItem *item = parentItem->child(row);
    if (!item)
        return QModelIndex();
    return createIndex(row, column, item);
}

/*!
    Returns the parent of the model item with the given
    \a index, or QModelIndex() if it has no parent.
*/
QModelIndex QHelpContentModel::parent(const QModelIndex &index) const
{
    QHelpContentItem *item = contentItemAt(index);
    if (!item)
        return QModelIndex();

    QHelpContentItem *parentItem = static_cast<QHelpContentItem*>(item->parent());
    if (!parentItem)
        return QModelIndex();

    QHelpContentItem *grandparentItem = static_cast<QHelpContentItem*>(parentItem->parent());
    if (!grandparentItem)
        return QModelIndex();

    int row = grandparentItem->childPosition(parentItem);
    return createIndex(row, index.column(), parentItem);
}

/*!
    Returns the number of rows under the given \a parent.
*/
int QHelpContentModel::rowCount(const QModelIndex &parent) const
{
    QHelpContentItem *parentItem = contentItemAt(parent);
    if (!parentItem)
        return 0;
    return parentItem->childCount();
}

/*!
    Returns the number of columns under the given \a parent. Currently returns always 1.
*/
int QHelpContentModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return 1;
}

/*!
    Returns the data stored under the given \a role for
    the item referred to by the \a index.
*/
QVariant QHelpContentModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    QHelpContentItem *item = contentItemAt(index);
    if (!item)
        return QVariant();
    return item->title();
}



/*!
    \class QHelpContentWidget
    \inmodule QtHelp
    \brief The QHelpContentWidget class provides a tree view for displaying help content model items.
    \since 4.4
*/

/*!
    \fn void QHelpContentWidget::linkActivated(const QUrl &link)

    This signal is emitted when a content item is activated and
    its associated \a link should be shown.
*/

QHelpContentWidget::QHelpContentWidget()
    : QTreeView(0)
{
    header()->hide();
    setUniformRowHeights(true);
    connect(this, SIGNAL(activated(QModelIndex)),
        this, SLOT(showLink(QModelIndex)));
}

/*!
    Returns the index of the content item with the \a link.
    An invalid index is returned if no such an item exists.
*/
QModelIndex QHelpContentWidget::indexOf(const QUrl &link)
{
    QHelpContentModel *contentModel = qobject_cast<QHelpContentModel*>(model());
    if (!contentModel || link.scheme() != QLatin1String("qthelp"))
        return QModelIndex();

    m_syncIndex = QModelIndex();
    for (int i = 0; i < contentModel->rowCount(); ++i) {
        QHelpContentItem *itm = contentModel->contentItemAt(contentModel->index(i, 0));
        if (itm && itm->url().host() == link.host()) {
            if (searchContentItem(contentModel, contentModel->index(i, 0), QDir::cleanPath(link.path())))
                return m_syncIndex;
        }
    }
    return QModelIndex();
}

bool QHelpContentWidget::searchContentItem(QHelpContentModel *model, const QModelIndex &parent,
    const QString &cleanPath)
{
    QHelpContentItem *parentItem = model->contentItemAt(parent);
    if (!parentItem)
        return false;

    if (QDir::cleanPath(parentItem->url().path()) == cleanPath) {
        m_syncIndex = parent;
        return true;
    }

    for (int i=0; i<parentItem->childCount(); ++i) {
        if (searchContentItem(model, model->index(i, 0, parent), cleanPath))
            return true;
    }
    return false;
}

void QHelpContentWidget::showLink(const QModelIndex &index)
{
    QHelpContentModel *contentModel = qobject_cast<QHelpContentModel*>(model());
    if (!contentModel)
        return;

    QHelpContentItem *item = contentModel->contentItemAt(index);
    if (!item)
        return;
    QUrl url = item->url();
    if (url.isValid())
        emit linkActivated(url);
}

QT_END_NAMESPACE

#include "qhelpcontentwidget.moc"
