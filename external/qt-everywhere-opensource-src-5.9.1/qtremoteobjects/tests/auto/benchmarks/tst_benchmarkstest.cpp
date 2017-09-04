/****************************************************************************
**
** Copyright (C) 2017-2015 Ford Motor Company
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

#include <QString>
#include <QDataStream>
#include <QLocalSocket>
#include <QLocalServer>
#include <QtTest>
#include <QtRemoteObjects/QAbstractItemModelReplica>
#include <QtRemoteObjects/QRemoteObjectNode>
#include "rep_localdatacenter_replica.h"
#include "rep_localdatacenter_source.h"

class BenchmarksModel : public QAbstractListModel
{
    // QAbstractItemModel interface
public:
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
};

int BenchmarksModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 100000;
}

QVariant BenchmarksModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        return QStringLiteral("Benchmark data %1").arg(index.row());
    case Qt::BackgroundRole:
        return index.row() % 2 ? QStringLiteral("red") : QStringLiteral("green");
    }
    return QVariant();
}

QHash<int, QByteArray> BenchmarksModel::roleNames() const
{
    static QHash<int,QByteArray> roleNames = {
        {Qt::DisplayRole, "_text"},
        {Qt::BackgroundRole, "_color"}
    };
    return roleNames;
}

class BenchmarksTest : public QObject
{
    Q_OBJECT

public:
    BenchmarksTest();
private:
    QRemoteObjectHost m_basicServer;
    QRemoteObjectNode m_basicClient;
    QScopedPointer<LocalDataCenterSimpleSource> dataCenterLocal;
    BenchmarksModel m_sourceModel;

private Q_SLOTS:
    void initTestCase();
    void benchPropertyChangesInt();
    void benchQDataStreamInt();
    void benchQLocalSocketInt();
    void benchQLocalSocketQDataStreamInt();
    void benchModelLinearAccess();
    void benchModelRandomAccess();
};

BenchmarksTest::BenchmarksTest()
{
}

void BenchmarksTest::initTestCase() {
    m_basicServer.setHostUrl(QUrl(QStringLiteral("local:benchmark_replica")));
    dataCenterLocal.reset(new LocalDataCenterSimpleSource);
    dataCenterLocal->setData1(5);
    const bool remoted = m_basicServer.enableRemoting(dataCenterLocal.data());
    Q_ASSERT(remoted);
    Q_UNUSED(remoted);

    m_basicClient.connectToNode(QUrl(QStringLiteral("local:benchmark_replica")));
    Q_ASSERT(m_basicClient.lastError() == QRemoteObjectNode::NoError);

    m_basicServer.enableRemoting(&m_sourceModel, QStringLiteral("BenchmarkRemoteModel"),
                                 m_sourceModel.roleNames().keys().toVector());
}

void BenchmarksTest::benchPropertyChangesInt()
{
    QScopedPointer<LocalDataCenterReplica> center;
    center.reset(m_basicClient.acquire<LocalDataCenterReplica>());
    if (!center->isInitialized()) {
        QEventLoop loop;
        connect(center.data(), &LocalDataCenterReplica::initialized, &loop, &QEventLoop::quit);
        loop.exec();
    }
    QEventLoop loop;
    int lastValue = 0;
    connect(center.data(), &LocalDataCenterReplica::data1Changed, [&lastValue, &center, &loop]() {
        const bool res = (lastValue++ == center->data1());
        Q_ASSERT(res);
        Q_UNUSED(res);
        if (lastValue == 50000)
            loop.quit();
    });
    QBENCHMARK {
        for (int i = 0; i < 50000; ++i) {
            dataCenterLocal->setData1(i);
        }
        loop.exec();
    }
}
// This ONLY tests the optimal case of a non resizing QByteArray
void BenchmarksTest::benchQDataStreamInt()
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    QDataStream rStream(&buffer, QIODevice::ReadOnly);
    int readout = 0;
    QBENCHMARK {
        for (int i = 0; i < 50000; ++i) {
            stream << i;
        }
        for (int i = 0; i < 50000; ++i) {
            rStream >> readout;
            Q_ASSERT(i == readout);
        }
    }
}

void BenchmarksTest::benchQLocalSocketInt()
{
    const QString socketName = QStringLiteral("benchLocalSocket");
    QLocalServer server;
    QLocalServer::removeServer(socketName);
    server.listen(socketName);
    QLocalSocket client;
    client.connectToServer(socketName);
    QEventLoop loop;
    QScopedPointer<QLocalSocket> serverSock;
    if (!server.hasPendingConnections()) {
        connect(&server, &QLocalServer::newConnection, &loop, &QEventLoop::quit);
        loop.exec();
    }
    Q_ASSERT(server.hasPendingConnections());
    serverSock.reset(server.nextPendingConnection());
    int lastValue = 0;
    connect(&client, &QLocalSocket::readyRead, [&loop, &lastValue, &client]() {
        int readout = 0;
        while (client.bytesAvailable() && lastValue < 50000) {
            client.read(reinterpret_cast<char*>(&readout), sizeof(int));
            const bool res = (lastValue++ == readout);
            Q_ASSERT(res);
            Q_UNUSED(res);
        }
        if (lastValue >= 50000)
            loop.quit();
    });
    QBENCHMARK {
        for (int i = 0; i < 50000; ++i) {
            const int res = serverSock->write(reinterpret_cast<char*>(&i), sizeof(int));
            Q_ASSERT(res == sizeof(int));
            Q_UNUSED(res);
        }
        loop.exec();
    }

#ifdef Q_OS_WIN
    // Work-around QTBUG-38185: immediately close socket
    client.abort();
#endif
}

void BenchmarksTest::benchQLocalSocketQDataStreamInt()
{
    const QString socketName = QStringLiteral("benchLocalSocket");
    QLocalServer server;
    QLocalServer::removeServer(socketName);
    server.listen(socketName);
    QLocalSocket client;
    client.connectToServer(socketName);
    QDataStream readStream(&client);
    QEventLoop loop;
    QScopedPointer<QLocalSocket> serverSock;
    if (!server.hasPendingConnections()) {
        connect(&server, &QLocalServer::newConnection, &loop, &QEventLoop::quit);
        loop.exec();
    }
    Q_ASSERT(server.hasPendingConnections());
    serverSock.reset(server.nextPendingConnection());
    QDataStream writeStream(serverSock.data());
    int lastValue = 0;
    connect(&client, &QIODevice::readyRead, [&loop, &lastValue, &readStream]() {
        int readout = 0;
        while (readStream.device()->bytesAvailable() && lastValue < 50000) {
            readStream >> readout;
            const bool res = (lastValue++ == readout);
            Q_ASSERT(res);
            Q_UNUSED(res);
        }
        if (lastValue >= 50000)
            loop.quit();
    });
    QBENCHMARK {
        for (int i = 0; i < 50000; ++i) {
            writeStream << i;
        }
        loop.exec();
    }

#ifdef Q_OS_WIN
    // Work-around QTBUG-38185: immediately close socket
    client.abort();
#endif
}

void BenchmarksTest::benchModelLinearAccess()
{
    // Simulate an user browse through item them stops.
    // We're measuring the time needed needed to deliver the visible chunk of data
    // which are the last 50 items
    QBENCHMARK {
        QRemoteObjectNode localClient;
        localClient.connectToNode(QUrl(QStringLiteral("local:benchmark_replica")));
        QScopedPointer<QAbstractItemModelReplica> model(localClient.acquireModel(QStringLiteral("BenchmarkRemoteModel")));
        QEventLoop loop;
        QHash<int, QPair<QString, QString>> dataToWait;
        connect(model.data(), &QAbstractItemModelReplica::dataChanged, [&model, &loop, &dataToWait](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                // we're assuming that the view will try use the sent data,
                // therefore we're not optimizing the code
                auto it = dataToWait.find(row);
                if (it == dataToWait.end()) {
                    // simulate some work with the received data
                    QThread::usleep(10);
                    continue;
                }
                foreach (int role, roles) {
                    QVariant data = model->data(model->index(row, 0), role);
                    switch (role) {
                    case Qt::DisplayRole:
                        it->first = data.toString();
                        break;
                    case Qt::BackgroundRole:
                        it->second = data.toString();
                        break;
                    }
                }

                if (it->first == QStringLiteral("Benchmark data %1").arg(row) &&
                        it->second == (row % 2 ? QStringLiteral("red") : QStringLiteral("green"))) {
                    dataToWait.erase(it);
                    if (dataToWait.isEmpty())
                        break;
                }
            }
            if (dataToWait.isEmpty())
                loop.quit();
        });

        auto beginBenchmark = [&model, &loop, &dataToWait] {
            for (int row = 0; row < 1000; ++row) {
                if (row >= 950)
                    dataToWait.insert(row, QPair<QString, QString>());
                model->data(model->index(row, 0), Qt::DisplayRole);
                model->data(model->index(row, 0), Qt::BackgroundRole);

                // Views (e.g. QTreeView) are accessing other roles
                model->data(model->index(row, 0), Qt::FontRole);
                model->data(model->index(row, 0), Qt::DecorationRole);
                model->data(model->index(row, 0), Qt::SizeHintRole);
            }

        };
        connect(model.data(), &QAbstractItemModelReplica::initialized, [&model, &loop, &beginBenchmark] {
            if (model->isInitialized()) {
                beginBenchmark();
            } else {
                Q_ASSERT(false);
                loop.quit();
            }
        });
        if (model->isInitialized())
            beginBenchmark();

        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        QVERIFY(dataToWait.isEmpty());
    }
}

void BenchmarksTest::benchModelRandomAccess()
{
    QBENCHMARK {
        QRemoteObjectNode localClient;
        localClient.connectToNode(QUrl(QStringLiteral("local:benchmark_replica")));
        QScopedPointer<QAbstractItemModelReplica> model(localClient.acquireModel(QStringLiteral("BenchmarkRemoteModel")));
        model->setRootCacheSize(5000); // we need to make room for all 5000 rows that we'll use
        QEventLoop loop;
        QHash<int, QPair<QString, QString>> dataToWait;
        connect(model.data(), &QAbstractItemModelReplica::dataChanged, [&model, &loop, &dataToWait](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                // we're assuming that the view will try use the sent data,
                // therefore we're not optimizing the code
                auto it = dataToWait.find(row);
                if (it == dataToWait.end()) {
                    QThread::yieldCurrentThread(); // instead to wait
                    continue;
                }
                foreach (int role, roles) {
                    QVariant data = model->data(model->index(row, 0), role);
                    switch (role) {
                    case Qt::DisplayRole:
                        it->first = data.toString();
                        break;
                    case Qt::BackgroundRole:
                        it->second = data.toString();
                        break;
                    }
                }

                if (it->first == QStringLiteral("Benchmark data %1").arg(row) &&
                        it->second == (row % 2 ? QStringLiteral("red") : QStringLiteral("green"))) {
                    dataToWait.erase(it);
                    if (dataToWait.isEmpty())
                        break;
                }
            }
            if (dataToWait.isEmpty())
                loop.quit();
        });

        auto beginBenchmark = [&model, &loop, &dataToWait] {
            for (int chunck = 0; chunck < 100; ++chunck) {
                int row = chunck * 950;
                for (int r = 0; r < 50; ++r) {
                    dataToWait.insert(r + row, QPair<QString, QString>());
                    model->data(model->index(r + row, 0), Qt::DisplayRole);
                    model->data(model->index(r + row, 0), Qt::BackgroundRole);

                    // Views (e.g. QTreeView) are accessing other roles
                    model->data(model->index(r + row, 0), Qt::FontRole);
                    model->data(model->index(r + row, 0), Qt::DecorationRole);
                    model->data(model->index(r + row, 0), Qt::SizeHintRole);
                }
            }

        };
        connect(model.data(), &QAbstractItemModelReplica::initialized, [&model, &loop, &beginBenchmark] {
            if (model->isInitialized()) {
                beginBenchmark();
            } else {
                Q_ASSERT(false);
                loop.quit();
            }
        });
        if (model->isInitialized())
            beginBenchmark();

        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        QVERIFY(dataToWait.isEmpty());
    }
}

QTEST_MAIN(BenchmarksTest)

#include "tst_benchmarkstest.moc"
