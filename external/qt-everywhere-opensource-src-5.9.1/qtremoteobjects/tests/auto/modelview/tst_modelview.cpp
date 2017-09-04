/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "modeltest.h"

#include <QtTest/QtTest>
#include <QMetaType>
#include <QRemoteObjectReplica>
#include <QRemoteObjectNode>
#include <QAbstractItemModelReplica>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QEventLoop>

#include <cstdlib>

namespace {

template <class Storage>
bool waitForSignal(QVector<Storage> *storage, QSignalSpy *spy)
{
    if (!storage || !spy)
        return false;
    const int maxRuns = 10;
    int runs = 0;
    const int storageSize = storage->size();
    QVector<Storage> rowsToRemove;
    while (runs < maxRuns) {
        ++runs;
        if (spy->wait() && !spy->isEmpty()){

            foreach (const Storage &row, *storage) {
                for (int i = 0; i < spy->size(); ++i) {
                    const QList<QVariant> &signal = spy->at(i);
                    if (row.match(signal)) {
                        rowsToRemove.append(row);
                        break;
                    }
                }
            }
            foreach (const Storage &row, rowsToRemove)
                storage->removeAll(row);
            if (storage->isEmpty())
                break;
        }
    }
    return storage->isEmpty() && spy->size() == storageSize;
}

inline bool compareIndices(const QModelIndex &lhs, const QModelIndex &rhs)
{
    QModelIndex left = lhs;
    QModelIndex right = rhs;
    while (left.row() == right.row() && left.column() == right.column() && left.isValid() && right.isValid()) {
        left = left.parent();
        right = right.parent();
    }
    if (left.isValid() || right.isValid())
        return false;
    return true;
}

QList<QStandardItem*> createInsertionChildren(int num, const QString& name, const QColor &background)
{
    QList<QStandardItem*> children;
    for (int i = 0; i < num; ++i) {
        QScopedPointer<QStandardItem> item(new QStandardItem(QStringLiteral("%1 %2").arg(name).arg(i+1)));
        item->setBackground(background);
        children.append(item.take());
    }
    return children;
}

struct InsertedRow
{
    InsertedRow(const QModelIndex &index = QModelIndex(), int start = -1, int end = -1)
        : m_index(index)
        , m_start(start)
        , m_end(end){}
    InsertedRow(const InsertedRow &other)
        : m_index(other.m_index)
        , m_start(other.m_start)
        , m_end(other.m_end){}
    bool match(const QList<QVariant> &signal) const
    {
        if (signal.size() != 3)
            return false;
        const bool matchingTypes = signal[0].type() == QVariant::nameToType("QModelIndex")
                                   && signal[1].type() == QVariant::nameToType("int")
                                   && signal[2].type() == QVariant::nameToType("int");
        if (!matchingTypes)
            return false;
        const QModelIndex otherIndex = signal[0].value<QModelIndex>();
        const int otherStart = signal[1].value<int>();
        const int otherEnd = signal[2].value<int>();
        return compareIndices(m_index, otherIndex) && (m_start == otherStart) && (m_end == otherEnd);
    }

    bool operator==(const QList<QVariant> &signal) const
    {
        return match(signal);
    }

    bool operator==(const InsertedRow &other) const
    {
        return m_index == other.m_index && m_start == other.m_start && m_end == other.m_end;
    }

    QModelIndex m_index;
    int m_start;
    int m_end;
};

struct WaitForDataChanged
{
    struct IndexPair
    {
        QModelIndex topLeft;
        QModelIndex bottomRight;
    };

    WaitForDataChanged(const QVector<QModelIndex> &pending, QSignalSpy *spy) : m_pending(pending), m_spy(spy){}
    bool wait()
    {
        Q_ASSERT(m_spy);
        const int maxRuns = std::min(m_pending.size(), 100);
        int runs = 0;
        bool cancel = false;
        while (!cancel) {
            const int numSignals = m_spy->size();
            for (int i = 0; i < numSignals; ++i) {
                const QList<QVariant> &signal = m_spy->at(i);
                IndexPair pair = extractPair(signal);
                checkAndRemoveRange(pair.topLeft, pair.bottomRight);
                cancel = m_pending.isEmpty();
            }
            if (!cancel)
                m_spy->wait(50);
            ++runs;
            if (runs >= maxRuns)
                cancel = true;
        }
        return runs < maxRuns;
    }

    static IndexPair extractPair(const QList<QVariant> &signal)
    {
        IndexPair pair;
        if (signal.size() != 3)
            return pair;
        const bool matchingTypes = signal[0].type() == QVariant::nameToType("QModelIndex")
                                   && signal[1].type() == QVariant::nameToType("QModelIndex")
                                   && signal[2].type() == QVariant::nameToType("QVector<int>");
        if (!matchingTypes)
            return pair;
        const QModelIndex topLeft = signal[0].value<QModelIndex>();
        const QModelIndex bottomRight = signal[1].value<QModelIndex>();
        pair.topLeft = topLeft;
        pair.bottomRight = bottomRight;
        return pair;
    }

    void checkAndRemoveRange(const QModelIndex &topLeft, const QModelIndex &bottomRight)
    {
        QVERIFY(topLeft.parent() == bottomRight.parent());
        QVector<QModelIndex>  toRemove;
        for (int i = 0; i < m_pending.size(); ++i) {
            const QModelIndex &pending = m_pending.at(i);
            if (pending.isValid()  && compareIndices(pending.parent(), topLeft.parent())) {
                const bool fitLeft = topLeft.column() <= pending.column();
                const bool fitRight = bottomRight.column() >= pending.column();
                const bool fitTop = topLeft.row() <= pending.row();
                const bool fitBottom = bottomRight.row() >= pending.row();
                if (fitLeft && fitRight && fitTop && fitBottom)
                    toRemove.append(pending);
            }
        }
        foreach (const QModelIndex &index, toRemove) {
            const int ind = m_pending.indexOf(index);
            m_pending.remove(ind);
        }
    }

    QVector<QModelIndex> m_pending;
    QSignalSpy *m_spy;
};

QTextStream cout(stdout, QIODevice::WriteOnly);

//Keep in case we need detailed debugging in the future...
#if 0
inline QDebug operator <<(QDebug dbg, const InsertedRow &row)
{
    return dbg.nospace() << "Index(" << row.m_index << ", " << row.m_start << ", " << row.m_end << ")";
}

inline void dumpModel(const QAbstractItemModel *model, const QModelIndex &parent = QModelIndex())
{
    int level = 0;
    for (QModelIndex idx = parent; idx.isValid(); idx = model->parent(idx), ++level);
    const QHash<int,QByteArray> roles = model->roleNames();
    for (int i = 0; i < model->rowCount(parent); ++i) {
        for (int j = 0; j < model->columnCount(parent); ++j) {
            const QModelIndex index = model->index(i, j);
            Q_ASSERT(index.isValid());
            QString s;
            s.fill(QChar(' '), level*2);
            cout << qPrintable(s);
            cout << QString(QLatin1String("%1:%2")).arg(i).arg(j);
            foreach (int role, roles.keys()) {
                const QVariant v = model->data(index, role);
                if (!v.isValid()) continue;
                QString t;
                QDebug(&t) << v;
                cout << " " << QString::fromUtf8(roles[role]) << "=" << t.trimmed();
            }

            {
                QString t;
                QDebug(&t) << model->flags(index);
                cout << " flags=" << t;
            }

            cout << "\n";
            cout.flush();
            dumpModel(model, index); // recursive
        }
    }
}
#endif

void compareData(const QAbstractItemModel *sourceModel, const QAbstractItemModelReplica *replica)
{
    QVERIFY(sourceModel);
    QVERIFY(replica);

    QCOMPARE(replica->rowCount(), sourceModel->rowCount());
    QCOMPARE(replica->columnCount(), sourceModel->columnCount());
    QCOMPARE(replica->roleNames(), sourceModel->roleNames());

    for (int i = 0; i < sourceModel->rowCount(); ++i) {
        for (int j = 0; j < sourceModel->columnCount(); ++j) {
            foreach (int role, replica->availableRoles()) {
                QCOMPARE(replica->index(i, j).data(role), sourceModel->index(i, j).data(role));
            }
        }
    }
}

void compareIndex(const QModelIndex &sourceIndex, const QModelIndex &replicaIndex,
                  const QVector<int> &roles)
{
    QVERIFY(sourceIndex.isValid());
    QVERIFY(replicaIndex.isValid());
    foreach (int role, roles) {
        QCOMPARE(replicaIndex.data(role), sourceIndex.data(role));
    }
    const QAbstractItemModel *sourceModel = sourceIndex.model();
    const QAbstractItemModel *replicaModel = replicaIndex.model();
    const int sourceRowCount = sourceModel->rowCount(sourceIndex);
    const int replicaRowCount = replicaModel->rowCount(replicaIndex);
    QCOMPARE(replicaRowCount, sourceRowCount);
    const int sourceColumnCount = sourceModel->columnCount(sourceIndex);
    const int replicaColumnCount = replicaModel->columnCount(replicaIndex);
    // only test the column count if the row count is larger than zero, because we
    // assume the column count is constant over a tree model and it doesn't make a
    // difference in the view.
    if (sourceRowCount)
        QCOMPARE(replicaColumnCount, sourceColumnCount);
    for (int i = 0; i < sourceRowCount; ++i) {
        for (int j = 0; j < sourceColumnCount; ++j) {
            compareIndex(sourceIndex.child(i, j), replicaIndex.child(i, j), roles);
        }
    }
}

void compareTreeData(const QAbstractItemModel *sourceModel, const QAbstractItemModelReplica *replica)
{
    QVERIFY(sourceModel);
    QVERIFY(replica);

    QCOMPARE(replica->rowCount(), sourceModel->rowCount());
    QCOMPARE(replica->columnCount(), sourceModel->columnCount());

    for (int i = 0; i < sourceModel->rowCount(); ++i) {
        for (int j = 0; j < sourceModel->columnCount(); ++j) {
            const QModelIndex replicaIndex = replica->index(i, j);
            const QModelIndex sourceIndex = sourceModel->index(i, j);
            compareIndex(sourceIndex, replicaIndex, replica->availableRoles());
        }
    }
}

void compareTreeData(const QAbstractItemModel *sourceModel, const QAbstractItemModel *replica, const QVector<int> &roles)
{
    QVERIFY(sourceModel);
    QVERIFY(replica);

    QCOMPARE(replica->rowCount(), sourceModel->rowCount());
    QCOMPARE(replica->columnCount(), sourceModel->columnCount());

    for (int i = 0; i < sourceModel->rowCount(); ++i) {
        for (int j = 0; j < sourceModel->columnCount(); ++j) {
            const QModelIndex replicaIndex = replica->index(i, j);
            const QModelIndex sourceIndex = sourceModel->index(i, j);
            compareIndex(sourceIndex, replicaIndex, roles);
        }
    }
}

void compareFlags(const QAbstractItemModel *sourceModel, const QAbstractItemModelReplica *replica)
{
    QVERIFY(sourceModel);
    QVERIFY(replica);

    QCOMPARE(replica->rowCount(), sourceModel->rowCount());
    QCOMPARE(replica->columnCount(), sourceModel->columnCount());

    for (int i = 0; i < sourceModel->rowCount(); ++i) {
        for (int j = 0; j < sourceModel->columnCount(); ++j) {
            if (replica->index(i, j).flags() != sourceModel->index(i, j).flags())
                qWarning() << sourceModel->index(i, j).flags() << replica->index(i, j).flags() << i << j;
            QCOMPARE(replica->index(i, j).flags(), sourceModel->index(i, j).flags());
        }
    }
}

// class to test cutom role names
class RolenamesListModel : public QAbstractListModel
{
public:
    explicit RolenamesListModel(QObject *parent = 0) : QAbstractListModel(parent) { }
    int rowCount(const QModelIndex &) const override{return m_list.length(); }
    QVariant data(const QModelIndex &index, int role) const override
    {
       if (role == Qt::UserRole)
           return m_list.at(index.row()).second;
       else if (role == Qt::UserRole+1)
           return m_list.at(index.row()).first;
       else
           return QVariant();
    }
    QHash<int, QByteArray> roleNames() const override
    {
        QHash<int, QByteArray> roles;
        roles[Qt::UserRole] = "name";
        roles[Qt::UserRole+1] = "pid";
        return roles;
    }
    void addPair(const QVariant pid,const QVariant name) {
        m_list.append(qMakePair(pid, name));
    }
    void clearList() {
        m_list.clear();
    }
private:
    QVector<QPair<QVariant, QVariant> > m_list;
};

QList<QStandardItem*> addChild(int numChilds, int nestingLevel)
{
    QList<QStandardItem*> result;
    if (nestingLevel == 0)
        return result;
    for (int i = 0; i < numChilds; ++i) {
        QStandardItem *child = new QStandardItem(QStringLiteral("Child num %1, nesting Level %2").arg(i+1).arg(nestingLevel));
        if (i == 0) {
            QList<QStandardItem*> res = addChild(numChilds, nestingLevel -1);
            if (res.size() > 0)
                child->appendRow(res);
        }
        result.push_back(child);
    }
    return result;
}

int getRandomNumber(int min, int max)
{
    int res = std::rand();
    const int diff = (max - min);
    res = res % diff;
    res += min;
    return res;
}

class FetchData : public QObject
{
    Q_OBJECT
public:
    FetchData(const QAbstractItemModelReplica *replica) : QObject(), m_replica(replica), isFinished(false) {
        if (!m_replica->isInitialized()) {
            QEventLoop l;
            connect(m_replica, SIGNAL(initialized()), &l, SLOT(quit()));
            l.exec();
        }

        connect(m_replica, &QAbstractItemModelReplica::dataChanged, this, &FetchData::dataChanged);
        connect(m_replica, &QAbstractItemModelReplica::rowsInserted, this, &FetchData::rowsInserted);
    }

    void addData(const QModelIndex &index, const QVector<int> &roles)
    {
        foreach (int role, roles) {
            const bool cached = m_replica->hasData(index, role);
            if (cached)
                continue;
            if (!m_pending.contains(index))
                m_pending[index] = QVector<int>() << role;
            else {
                if (!m_pending[index].contains(role))
                    m_pending[index].append(role);
            }
        }
    }

    void addIndex(const QModelIndex &parent, const QVector<int> &roles)
    {
        if (parent.isValid())
            addData(parent, roles);
        for (int i = 0; i < m_replica->rowCount(parent); ++i) {
            for (int j = 0; j < m_replica->columnCount(parent); ++j) {
                const QModelIndex index = m_replica->index(i, j, parent);
                Q_ASSERT(index.isValid());
                addIndex(index, roles);
            }
        }
    }

    void addAll()
    {
        addIndex(QModelIndex(), m_replica->availableRoles());
    }

    void fetch()
    {
        isFinished = m_pending.isEmpty() && m_waitForInsertion.isEmpty();
        if (isFinished) {
            emitFetched();
            return;
        }
        QHash<QPersistentModelIndex, QVector<int> > pending(m_pending);
        pending.detach();
        QHash<QPersistentModelIndex, QVector<int> >::ConstIterator it(pending.constBegin()), end(pending.constEnd());
        for (; it != end; ++it) {
            foreach (int role, it.value()) {
                QVariant v = m_replica->data(it.key(), role);
                Q_UNUSED(v);
            }
        }
    }

    bool fetchAndWait(int timeout = 3000)
    {
        QEventLoop l;
        QTimer::singleShot(timeout, &l, &QEventLoop::quit);
        connect(this, &FetchData::fetched, &l, &QEventLoop::quit);
        fetch();
        l.exec();
        return isFinished;
    }

signals:
    void fetched();

private:
    const QAbstractItemModelReplica *m_replica;
    QHash<QPersistentModelIndex, QVector<int> > m_pending;
    QSet<QPersistentModelIndex> m_waitForInsertion;
    bool isFinished;

    void emitFetched()
    {
        QTimer::singleShot(0, this, SIGNAL(fetched()));
    }

    void rowsInserted(const QModelIndex &parent, int first, int last)
    {
        static QVector<int> rolesV;
        if (rolesV.isEmpty())
            rolesV << Qt::DisplayRole << Qt::BackgroundRole;
        m_waitForInsertion.remove(parent);
        const int columnCount = m_replica->columnCount(parent);
        if (!(m_replica->hasChildren(parent) && columnCount > 0 && m_replica->rowCount(parent) > 0))
            qWarning() << m_replica->hasChildren(parent) << columnCount << m_replica->rowCount(parent) << parent.data();
        QVERIFY(m_replica->hasChildren(parent) && columnCount > 0 && m_replica->rowCount(parent) > 0);
        for (int i = first; i <= last; ++i) {
            for (int j = 0; j < columnCount; ++j) {
                const QModelIndex index = m_replica->index(i, j, parent);
                const int childRowCount = m_replica->rowCount(index);

                QVERIFY(index.isValid());
                if (m_replica->hasChildren(index) && childRowCount == 0) {
                    if (index.column() == 0)
                        m_waitForInsertion.insert(index);
                }
                addIndex(index, rolesV);
            }
        }
        if (m_replica->hasChildren(parent))
            fetch();
    }

    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>())
    {
        Q_ASSERT(topLeft.isValid());
        Q_ASSERT(bottomRight.isValid());
        Q_ASSERT(topLeft.parent() == bottomRight.parent());

        const QModelIndex parent = topLeft.parent();
        for (int i = topLeft.row(); i <= bottomRight.row(); ++i) {
            for (int j = topLeft.column(); j <= bottomRight.column(); ++j) {
                const QModelIndex index = m_replica->index(i, j, parent);
                Q_ASSERT(index.isValid());
                QHash<QPersistentModelIndex, QVector<int> >::Iterator it = m_pending.find(index);
                if (it == m_pending.end())
                    continue;

#if 0
                QVector<int> itroles = it.value();
                itroles.detach();
                if (roles.isEmpty()) {
                    itroles.clear();
                } else {
                    foreach (int r, roles) {
                        itroles.removeAll(r);
                    }
                }
                if (itroles.isEmpty()) {
                    m_pending.erase(it);
                } else {
                    m_pending[index] = itroles;
                }
#else
                Q_UNUSED(roles);
                m_pending.erase(it);
                if (m_replica->hasChildren(index)) {
                    // ask for the row count to get an update
                    const int rowCount = m_replica->rowCount(index);
                    for (int i = 0; i < rowCount; ++i) {
                        const QModelIndex cIndex = m_replica->index(i, 0, index);
                        Q_ASSERT(cIndex.isValid());
                        addIndex(cIndex, m_replica->availableRoles());
                    }
                    if (rowCount)
                        fetch();
                    else if (index.column() == 0)
                        m_waitForInsertion.insert(index);
                }
#endif
            }
        }

        isFinished = m_pending.isEmpty() && m_waitForInsertion.isEmpty();
        if (isFinished) {
            emitFetched();
        }
    }
};

} // namespace

class TestModelView: public QObject
{
    Q_OBJECT

    QRemoteObjectHost m_basicServer;
    QRemoteObjectNode m_client;
    QRemoteObjectRegistryHost m_registryServer;
    QStandardItemModel m_sourceModel;
    RolenamesListModel m_listModel;

private slots:
    void initTestCase();

    void testEmptyModel();
    void testInitialData();
    void testInitialDataTree();
    void testHeaderData();
    void testFlags();
    void testDataChanged();
    void testDataChangedTree();
    void testDataInsertion();
    void testDataInsertionTree();
    void testSetData();
    void testSetDataTree();
    void testDataRemoval();
    void testDataRemovalTree();
    void testServerInsertDataTree();

    void testRoleNames();

    void testModelTest();
    void testSortFilterModel();

    void testSelectionFromReplica();
    void testSelectionFromSource();

    void cleanup();
};

void TestModelView::initTestCase()
{
    QLoggingCategory::setFilterRules("qt.remoteobjects.warning=false");

    //Setup registry
    //Registry needs to be created first until we get the retry mechanism implemented
    m_registryServer.setRegistryUrl(QUrl(QStringLiteral("tcp://127.0.0.1:65212")));

    m_basicServer.setHostUrl(QUrl(QStringLiteral("tcp://127.0.0.1:65211")));
    m_basicServer.setRegistryUrl(QUrl(QStringLiteral("tcp://127.0.0.1:65212")));

    static const int modelSize = 20;

    QVector<int> roles = QVector<int>() << Qt::DisplayRole << Qt::BackgroundRole;

    QHash<int,QByteArray> roleNames;
    roleNames[Qt::DisplayRole] = "text";
    roleNames[Qt::BackgroundRole] = "background";
    m_sourceModel.setItemRoleNames(roleNames);

    QStringList list;
    list.reserve(modelSize);

    QStringList hHeaderList;
    hHeaderList << QStringLiteral("First Column with spacing") << QStringLiteral("Second Column with spacing");
    m_sourceModel.setHorizontalHeaderLabels(hHeaderList);

    std::srand(0);
    for (int i = 0; i < modelSize; ++i) {
        QStandardItem *firstItem = new QStandardItem(QStringLiteral("FancyTextNumber %1").arg(i));
        QStandardItem *secondItem = new QStandardItem(QStringLiteral("FancyRow2TextNumber %1").arg(i));
        if (i % 2 == 0)
            firstItem->setBackground(Qt::red);
        firstItem->appendRow(addChild(2,getRandomNumber(1, 4)));
        QList<QStandardItem*> row;
        row << firstItem << secondItem;
        m_sourceModel.appendRow(row);
        list << QStringLiteral("FancyTextNumber %1").arg(i);
    }
    m_basicServer.enableRemoting(&m_sourceModel, "test", roles);

    const int numElements = 1000;
    for (int i = 0; i < numElements; ++i) {
        QString name = QString("Data %1").arg(i);
        QString pid = QString("%1").arg(i);
        m_listModel.addPair(name, pid);
    }
    roles.clear();
    roles << Qt::UserRole << Qt::UserRole+1;
    m_basicServer.enableRemoting(&m_listModel, "testRoleNames", roles);

    m_client.setRegistryUrl(QUrl(QStringLiteral("tcp://127.0.0.1:65212")));
}

void TestModelView::testEmptyModel()
{
    QVector<int> roles = QVector<int>() << Qt::DisplayRole << Qt::BackgroundRole;
    QStandardItemModel emptyModel;
    m_basicServer.enableRemoting(&emptyModel, "emptyModel", roles);

    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("emptyModel"));
    model->setRootCacheSize(1000);

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    compareData(&emptyModel, model.data());
}

void TestModelView::testInitialData()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    compareData(&m_sourceModel, model.data());
}

void TestModelView::testInitialDataTree()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    compareTreeData(&m_sourceModel, model.data());
}

void TestModelView::testHeaderData()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    // ask for all Data members first, so we don't have to wait for update signals
    QSignalSpy spyHeader(model.data(), SIGNAL(headerDataChanged(Qt::Orientation,int,int)));
    for (int i = 0; i < m_sourceModel.rowCount(); ++i)
        model->headerData(i, Qt::Vertical, Qt::DisplayRole);
    for (int i = 0; i < m_sourceModel.columnCount(); ++i)
        model->headerData(i, Qt::Horizontal, Qt::DisplayRole);
    spyHeader.wait();

    for (int i = 0; i < m_sourceModel.rowCount(); ++i)
        QCOMPARE(model->headerData(i, Qt::Vertical, Qt::DisplayRole), m_sourceModel.headerData(i, Qt::Vertical, Qt::DisplayRole));
    for (int i = 0; i < m_sourceModel.columnCount(); ++i)
        QCOMPARE(model->headerData(i, Qt::Horizontal, Qt::DisplayRole), m_sourceModel.headerData(i, Qt::Horizontal, Qt::DisplayRole));
}

void TestModelView::testDataChangedTree()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    compareTreeData(&m_sourceModel, model.data());
    QSignalSpy dataChangedSpy(model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    for (int i = 10; i < 20; ++i) {
        const QModelIndex parent = m_sourceModel.index(i,0);
        const int rowCount = m_sourceModel.rowCount(parent);
        const int colCount = m_sourceModel.columnCount(parent);
        for (int row = 0; row < rowCount; ++row) {
            for (int col = 0; col < colCount; ++col) {
                if (col % 2 == 0)
                    m_sourceModel.setData(m_sourceModel.index(row, col, parent), QColor(Qt::gray), Qt::BackgroundRole);
                else
                    m_sourceModel.setData(m_sourceModel.index(row, col, parent), QColor(Qt::cyan), Qt::BackgroundRole);
            }
        }
        m_sourceModel.setData(m_sourceModel.index(i, 1), QColor(Qt::magenta), Qt::BackgroundRole);
    }

    bool signalsReceived = false;
    int runs = 0;
    const int maxRuns = 10;
    while (runs < maxRuns) {
        if (dataChangedSpy.wait() &&!dataChangedSpy.isEmpty()) {
            signalsReceived = true;
            if (dataChangedSpy.takeFirst().at(1).value<QModelIndex>().row() == 19)
                break;
        }
        ++runs;
    }
    QVERIFY(signalsReceived);
    compareTreeData(&m_sourceModel, model.data());
}

void TestModelView::testFlags()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    QSignalSpy dataChangedSpy(model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    for (int i = 10; i < 20; ++i) {
        QStandardItem* firstItem = m_sourceModel.item(i, 0);
        QStandardItem* secondItem = m_sourceModel.item(i, 1);
        firstItem->setFlags(firstItem->flags() | Qt::ItemIsEnabled | Qt::ItemIsTristate);
        secondItem->setFlags(firstItem->flags() | Qt::ItemIsEnabled);
    }
    bool signalsReceived = false;
    while (dataChangedSpy.wait()) {
        signalsReceived = true;
        if (dataChangedSpy.takeLast().at(1).value<QModelIndex>().row() == 19)
            break;
    }
    QVERIFY(signalsReceived);
    compareFlags(&m_sourceModel, model.data());
}

void TestModelView::testDataChanged()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    QSignalSpy dataChangedSpy(model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    for (int i = 10; i < 20; ++i)
        m_sourceModel.setData(m_sourceModel.index(i, 1), QColor(Qt::blue), Qt::BackgroundRole);

    bool signalsReceived = false;
    while (dataChangedSpy.wait()) {
        signalsReceived = true;
        if (dataChangedSpy.takeLast().at(1).value<QModelIndex>().row() == 19)
            break;
    }
    QVERIFY(signalsReceived);
    compareData(&m_sourceModel, model.data());
}

void TestModelView::testDataInsertion()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    QVector<QModelIndex> pending;

    QSignalSpy dataChangedSpy(model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    QVector<InsertedRow> insertedRows;
    QSignalSpy rowSpy(model.data(), SIGNAL(rowsInserted(QModelIndex,int,int)));
    m_sourceModel.insertRows(2, 9);
    insertedRows.append(InsertedRow(QModelIndex(), 2, 10));
    const int maxRuns = 10;
    int runs = 0;
    QVector<InsertedRow> rowsToRemove;
    while (runs < maxRuns) {
        ++runs;
        if (rowSpy.wait() && !rowSpy.isEmpty()){

            foreach (const InsertedRow &irow, insertedRows) {
                for (int i = 0; i < rowSpy.size(); ++i) {
                    const QList<QVariant> &signal = rowSpy.at(i);
                    if (irow.match(signal)) {
                        //fetch the data of the inserted index
                        const QModelIndex &parent = signal.at(0).value<QModelIndex>();
                        const int start = signal.at(1).value<int>();
                        const int end = signal.at(2).value<int>();
                        const int columnCount = model->columnCount(parent);
                        for (int row = start; row <= end; ++row)
                            for (int column = 0; column < columnCount; ++column) {
                                model->data(model->index(row, column, parent), Qt::DisplayRole);
                                model->data(model->index(row, column, parent), Qt::BackgroundRole);
                                pending.append(model->index(row, column, parent));
                            }
                        rowsToRemove.append(irow);
                        break;
                    }
                }
            }
            foreach (const InsertedRow &irow, rowsToRemove)
                insertedRows.removeAll(irow);
            if (insertedRows.isEmpty())
                break;
        }

    }
    QCOMPARE(rowSpy.count(), 1);
    QCOMPARE(m_sourceModel.rowCount(), model->rowCount());

    // change one row to check for inconsistencies
    m_sourceModel.setData(m_sourceModel.index(0, 1), QColor(Qt::green), Qt::BackgroundRole);
    m_sourceModel.setData(m_sourceModel.index(0, 1), QLatin1String("foo"), Qt::DisplayRole);
    pending.append(model->index(0, 1));
    WaitForDataChanged w(pending, &dataChangedSpy);


    QVERIFY(w.wait());
    compareData(&m_sourceModel, model.data());
}

void TestModelView::testDataInsertionTree()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    const QVector<int> roles = model->availableRoles();

    QVector<InsertedRow> insertedRows;
    QSignalSpy rowSpy(model.data(), SIGNAL(rowsInserted(QModelIndex,int,int)));

    QSignalSpy dataChangedSpy(model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    QVector<QModelIndex> pending;

    for (int i = 0; i < 9; ++ i) {
        insertedRows.append(InsertedRow(QModelIndex(), 2 + i, 2 + i));
        m_sourceModel.insertRow(2 + i, createInsertionChildren(2, QStringLiteral("insertedintree"), Qt::darkRed));
        const QModelIndex childIndex = m_sourceModel.index(2 + i, 0);
        const QModelIndex childIndex2 = m_sourceModel.index(2 + i, 1);
        pending.append(childIndex);
        pending.append(childIndex2);

    }
    const QModelIndex parent = m_sourceModel.index(10, 0);
    QStandardItem* parentItem = m_sourceModel.item(10, 0);
    for (int i = 0; i < 4; ++ i) {
        insertedRows.append(InsertedRow(parent, i, i));
        parentItem->insertRow(i, createInsertionChildren(2, QStringLiteral("insertedintreedeep"), Qt::darkCyan));
        const QModelIndex childIndex = m_sourceModel.index(0, 0, parent);
        const QModelIndex childIndex2 = m_sourceModel.index(0, 1, parent);
        Q_ASSERT(childIndex.isValid());
        Q_ASSERT(childIndex2.isValid());
        pending.append(childIndex);
        pending.append(childIndex2);
    }

    const int maxRuns = 10;
    int runs = 0;
    QVector<InsertedRow> rowsToRemove;
    while (runs < maxRuns) {
        ++runs;
        if (rowSpy.wait() && !rowSpy.isEmpty()){

            foreach (const InsertedRow &irow, insertedRows) {
                for (int i = 0; i < rowSpy.size(); ++i) {
                    const QList<QVariant> &signal = rowSpy.at(i);
                    if (irow.match(signal)) {
                        //fetch the data of the inserted index
                        const QModelIndex &parent = signal.at(0).value<QModelIndex>();
                        const int start = signal.at(1).value<int>();
                        const int end = signal.at(2).value<int>();
                        const int columnCount = model->columnCount(parent);
                        for (int row = start; row <= end; ++row)
                            for (int column = 0; column < columnCount; ++column) {
                                model->data(model->index(row, column, parent), Qt::DisplayRole);
                                model->data(model->index(row, column, parent), Qt::BackgroundRole);
                                pending.append(model->index(row, column, parent));
                            }
                        rowsToRemove.append(irow);
                        break;
                    }
                }
            }
            foreach (const InsertedRow &irow, rowsToRemove)
                insertedRows.removeAll(irow);
            if (insertedRows.isEmpty())
                break;
        }

    }
    QVERIFY(rowSpy.count() == 13);
    QCOMPARE(m_sourceModel.rowCount(), model->rowCount());

    // change one row to check for inconsistencies

    pending << m_sourceModel.index(0, 0, parent);
    WaitForDataChanged w(pending, &dataChangedSpy);
    m_sourceModel.setData(m_sourceModel.index(0, 0, parent), QColor(Qt::green), Qt::BackgroundRole);
    m_sourceModel.setData(m_sourceModel.index(0, 0, parent), QLatin1String("foo"), Qt::DisplayRole);

    w.wait();

    compareTreeData(&m_sourceModel, model.data());
}

void TestModelView::testDataRemoval()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));
    qputenv("QTRO_NODES_CACHE_SIZE", "1000");
    model->setRootCacheSize(1000);
    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    QVector<InsertedRow> removedRows;
    QSignalSpy rowSpy(model.data(), SIGNAL(rowsRemoved(QModelIndex,int,int)));


    const QModelIndex parent = m_sourceModel.index(10, 0);
    m_sourceModel.removeRows(0, 4, parent);
    removedRows.append(InsertedRow(parent, 0, 3));
    QVERIFY(waitForSignal(&removedRows, &rowSpy));
    rowSpy.clear();
    QCOMPARE(m_sourceModel.rowCount(parent), model->rowCount(model->index(10, 0)));
    m_sourceModel.removeRows(2, 9);
    removedRows.append(InsertedRow(QModelIndex(), 2, 10));
    QVERIFY(waitForSignal(&removedRows, &rowSpy));


    QCOMPARE(m_sourceModel.rowCount(), model->rowCount());

    // change one row to check for inconsistencies

    QVector<QModelIndex> pending;
    QSignalSpy dataChangedSpy(model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    pending << m_sourceModel.index(0, 0, parent);
    WaitForDataChanged w(pending, &dataChangedSpy);
    m_sourceModel.setData(m_sourceModel.index(0, 0, parent), QColor(Qt::green), Qt::BackgroundRole);
    m_sourceModel.setData(m_sourceModel.index(0, 0, parent), QLatin1String("foo"), Qt::DisplayRole);

    w.wait();

    compareTreeData(&m_sourceModel, model.data());
}

void TestModelView::testRoleNames()
{
    QScopedPointer<QAbstractItemModelReplica> repModel( m_client.acquireModel(QStringLiteral("testRoleNames")));
    // Set a bigger cache enough to keep all the data otherwise the last test will fail
    repModel->setRootCacheSize(1500);
    FetchData f(repModel.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    // test custom role names
    QCOMPARE(repModel.data()->roleNames(), m_listModel.roleNames());

    // test data associated with custom roles
    compareData(&m_listModel,repModel.data());
}

void TestModelView::testDataRemovalTree()
{
    m_sourceModel.removeRows(2, 4);
    //Maybe some checks here?
}

void TestModelView::testServerInsertDataTree()
{
    QVector<int> roles = QVector<int>() << Qt::DisplayRole << Qt::BackgroundRole;
    QStandardItemModel testTreeModel;
    m_basicServer.enableRemoting(&testTreeModel, "testTreeModel", roles);

    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("testTreeModel"));

    QTRY_COMPARE(testTreeModel.rowCount(), model->rowCount());

    QVERIFY(testTreeModel.insertRow(0));
    QVERIFY(testTreeModel.insertColumn(0));
    auto root = testTreeModel.index(0, 0);
    QVERIFY(testTreeModel.setData(root, QLatin1String("Root"), Qt::DisplayRole));
    QVERIFY(testTreeModel.setData(root, QColor(Qt::green), Qt::BackgroundRole));
    QVERIFY(testTreeModel.insertRow(0, root));
    QVERIFY(testTreeModel.insertColumn(0, root));
    auto child1 = testTreeModel.index(0, 0, root);
    QVERIFY(testTreeModel.setData(child1, QLatin1String("Child1"), Qt::DisplayRole));
    QVERIFY(testTreeModel.setData(child1, QColor(Qt::red), Qt::BackgroundRole));

    QTRY_COMPARE(testTreeModel.rowCount(), model->rowCount());

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    compareData(&testTreeModel, model.data());
}

void TestModelView::testModelTest()
{
    QScopedPointer<QAbstractItemModelReplica> repModel( m_client.acquireModel(QStringLiteral("test")));
    ModelTest test(repModel.data());

    FetchData f(repModel.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());
    Q_UNUSED(test);
}

void TestModelView::testSortFilterModel()
{
    QScopedPointer<QAbstractItemModelReplica> repModel( m_client.acquireModel(QStringLiteral("test")));

    FetchData f(repModel.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    QSortFilterProxyModel clientSort;
    clientSort.setSourceModel(repModel.data());
    clientSort.setSortRole(Qt::DisplayRole);
    QSortFilterProxyModel sourceSort;
    sourceSort.setSourceModel(&m_sourceModel);
    sourceSort.setSortRole(Qt::DisplayRole);

    compareTreeData(&sourceSort, &clientSort, repModel->availableRoles());
}

void TestModelView::testSetData()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());
    compareTreeData(&m_sourceModel, model.data(), model->availableRoles());

    //fetched and verified initial state, now setData on the client
    QSignalSpy dataChangedSpy(&m_sourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    QSignalSpy dataChangedReplicaSpy(model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    QVector<QModelIndex> pending;
    QVector<QModelIndex> pendingReplica;
    for (int row = 0, numRows = model->rowCount(); row < numRows; ++row) {
        for (int column = 0, numColumns = model->columnCount(); column != numColumns; ++column) {
            const QModelIndex index = model->index(row, column);
            const QString newData = QStringLiteral("This entry was changed with setData");
            QVERIFY(model->setData(index, newData, Qt::DisplayRole));
            pending.append(m_sourceModel.index(row, column));
            pendingReplica.append(model->index(row, column));
        }
    }
    WaitForDataChanged waiter(pending, &dataChangedSpy);
    QVERIFY(waiter.wait());
    WaitForDataChanged waiterReplica(pendingReplica, &dataChangedReplicaSpy);
    QVERIFY(waiterReplica.wait());
    compareData(&m_sourceModel, model.data());
}

void TestModelView::testSetDataTree()
{
    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("test"));

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());
    compareTreeData(&m_sourceModel, model.data(), model->availableRoles());

    //fetched and verified initial state, now setData on the client
    QSignalSpy dataChangedSpy(&m_sourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    QSignalSpy dataChangedReplicaSpy(model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    QVector<QModelIndex> pending;
    QVector<QModelIndex> pendingReplica;

    QVector<QModelIndex> stack;
    stack.push_back(QModelIndex());
    QVector<QModelIndex> sourceStack;
    sourceStack.push_back(QModelIndex());


    const QString newData = QStringLiteral("This entry was changed with setData in a tree %1 %2 %3");
    while (!stack.isEmpty()) {
        const QModelIndex parent = stack.takeLast();
        const QModelIndex parentSource = sourceStack.takeLast();
        for (int row = 0; row < model->rowCount(parent); ++row) {
            for (int column = 0; column < model->columnCount(parent); ++column) {
                const QModelIndex index = model->index(row, column, parent);
                const QModelIndex indexSource = m_sourceModel.index(row, column, parentSource);
                QVERIFY(model->setData(index, newData.arg(parent.isValid()).arg(row).arg(column), Qt::DisplayRole));
                pending.append(indexSource);
                pendingReplica.append(index);
                if (column == 0) {
                    stack.push_back(index);
                    sourceStack.push_back(indexSource);
                }
            }
        }
    }
    WaitForDataChanged waiter(pending, &dataChangedSpy);
    QVERIFY(waiter.wait());
    WaitForDataChanged waiterReplica(pendingReplica, &dataChangedReplicaSpy);
    QVERIFY(waiterReplica.wait());
    compareData(&m_sourceModel, model.data());
}

void TestModelView::testSelectionFromReplica()
{
    QVector<int> roles = QVector<int>() << Qt::DisplayRole << Qt::BackgroundRole;
    QStandardItemModel simpleModel;
    for (int i = 0; i < 4; ++i)
        simpleModel.appendRow(new QStandardItem(QString("item %0").arg(i)));
    QItemSelectionModel selectionModel(&simpleModel);
    m_basicServer.enableRemoting(&simpleModel, "simpleModelFromReplica", roles, &selectionModel);

    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("simpleModelFromReplica"));
    QItemSelectionModel *replicaSelectionModel = model->selectionModel();

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    replicaSelectionModel->setCurrentIndex(model->index(1,0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
    QTRY_COMPARE(selectionModel.currentIndex().row(), 1);
}

void TestModelView::testSelectionFromSource()
{
    QVector<int> roles = QVector<int>() << Qt::DisplayRole << Qt::BackgroundRole;
    QStandardItemModel simpleModel;
    for (int i = 0; i < 4; ++i)
        simpleModel.appendRow(new QStandardItem(QString("item %0").arg(i)));
    QItemSelectionModel selectionModel(&simpleModel);
    m_basicServer.enableRemoting(&simpleModel, "simpleModelFromSource", roles, &selectionModel);

    QScopedPointer<QAbstractItemModelReplica> model(m_client.acquireModel("simpleModelFromSource"));
    QItemSelectionModel *replicaSelectionModel = model->selectionModel();

    FetchData f(model.data());
    f.addAll();
    QVERIFY(f.fetchAndWait());

    selectionModel.setCurrentIndex(simpleModel.index(1,0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
    QTRY_COMPARE(replicaSelectionModel->currentIndex().row(), 1);
}

void TestModelView::cleanup()
{
    // wait for delivery of RemoveObject events to the source
    QTest::qWait(20);
}

QTEST_MAIN(TestModelView)

#include "tst_modelview.moc"
