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

#include <QtTest/QtTest>
#include <QtGlobal>
#include <math.h>
#include <QMetaObject>
#include <qtest.h>
#include <QtTest/qsignalspy.h>
#include <QtQml/qqmlnetworkaccessmanagerfactory.h>
#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtNetwork/qnetworkrequest.h>
#include <QtCore/qtimer.h>
#include <QtCore/qfile.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qsortfilterproxymodel.h>
#include "../../shared/util.h"
#include <private/qqmlengine_p.h>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include "../../../../src/imports/xmllistmodel/qqmlxmllistmodel_p.h"

#include <algorithm>

typedef QPair<int, int> QQuickXmlListRange;
typedef QList<QVariantList> QQmlXmlModelData;

Q_DECLARE_METATYPE(QList<QQuickXmlListRange>)
Q_DECLARE_METATYPE(QQmlXmlModelData)
Q_DECLARE_METATYPE(QQuickXmlListModel::Status)

class tst_qquickxmllistmodel : public QQmlDataTest

{
    Q_OBJECT
public:
    tst_qquickxmllistmodel() {}

private slots:
    void initTestCase() {
        QQmlDataTest::initTestCase();
        qRegisterMetaType<QQuickXmlListModel::Status>();
    }

    void buildModel();
    void testTypes();
    void testTypes_data();
    void cdata();
    void attributes();
    void roles();
    void roleErrors();
    void uniqueRoleNames();
    void headers();
    void xml();
    void xml_data();
    void source();
    void source_data();
    void data();
    void get();
    void reload();
    void useKeys();
    void useKeys_data();
    void noKeysValueChanges();
    void keysChanged();
    void threading();
    void threading_data();
    void propertyChanges();
    void selectAncestor();

    void roleCrash();
    void proxyCrash();

private:
    QString errorString(QAbstractItemModel *model) {
        QString ret;
        QMetaObject::invokeMethod(model, "errorString", Q_RETURN_ARG(QString, ret));
        return ret;
    }

    QString makeItemXmlAndData(const QString &data, QQmlXmlModelData *modelData = 0) const
    {
        if (modelData)
            modelData->clear();
        QString xml;

        if (!data.isEmpty()) {
            QStringList items = data.split(QLatin1Char(';'));
            foreach(const QString &item, items) {
                if (item.isEmpty())
                    continue;
                QVariantList variants;
                xml += QLatin1String("<item>");
                QStringList fields = item.split(QLatin1Char(','));
                foreach(const QString &field, fields) {
                    QStringList values = field.split(QLatin1Char('='));
                    if (values.count() != 2) {
                        qWarning() << "makeItemXmlAndData: invalid field:" << field;
                        continue;
                    }
                    xml += QString("<%1>%2</%1>").arg(values[0], values[1]);
                    if (!modelData)
                        continue;
                    bool isNum = false;
                    int number = values[1].toInt(&isNum);
                    if (isNum)
                        variants << number;
                    else
                        variants << values[1];
                }
                xml += QLatin1String("</item>");
                if (modelData)
                    modelData->append(variants);
            }
        }

        QString decl = "<?xml version=\"1.0\" encoding=\"iso-8859-1\" ?>";
        return decl + QLatin1String("<data>") + xml + QLatin1String("</data>");
    }

    QQmlEngine engine;
};

class CustomNetworkAccessManagerFactory : public QObject, public QQmlNetworkAccessManagerFactory
{
    Q_OBJECT
public:
    QVariantMap lastSentHeaders;

protected:
    QNetworkAccessManager *create(QObject *parent);
};

class CustomNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    CustomNetworkAccessManager(CustomNetworkAccessManagerFactory *factory, QObject *parent)
        : QNetworkAccessManager(parent), m_factory(factory) {}

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &req, QIODevice * outgoingData = 0)
    {
        if (m_factory) {
            QVariantMap map;
            foreach (const QString &header, req.rawHeaderList())
                map[header] = req.rawHeader(header.toUtf8());
            m_factory->lastSentHeaders = map;
        }
        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }

    QPointer<CustomNetworkAccessManagerFactory> m_factory;
};

QNetworkAccessManager *CustomNetworkAccessManagerFactory::create(QObject *parent)
{
    return new CustomNetworkAccessManager(this, parent);
}


void tst_qquickxmllistmodel::buildModel()
{
    QQmlComponent component(&engine, testFileUrl("model.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 9);

    QModelIndex index = model->index(3, 0);
    QCOMPARE(model->data(index, Qt::UserRole).toString(), QLatin1String("Spot"));
    QCOMPARE(model->data(index, Qt::UserRole+1).toString(), QLatin1String("Dog"));
    QCOMPARE(model->data(index, Qt::UserRole+2).toInt(), 9);
    QCOMPARE(model->data(index, Qt::UserRole+3).toString(), QLatin1String("Medium"));

    delete model;
}

void tst_qquickxmllistmodel::testTypes()
{
    QFETCH(QString, xml);
    QFETCH(QString, roleName);
    QFETCH(QVariant, expectedValue);

    QQmlComponent component(&engine, testFileUrl("testtypes.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    model->setProperty("xml",xml.toUtf8());
    QMetaObject::invokeMethod(model, "reload");
    QTRY_COMPARE(model->rowCount(), 1);

    int role = model->roleNames().key(roleName.toUtf8(), -1);
    QVERIFY(role >= 0);

    QModelIndex index = model->index(0, 0);
    if (expectedValue.toString() == "nan")
        QVERIFY(qIsNaN(model->data(index, role).toDouble()));
    else
        QCOMPARE(model->data(index, role), expectedValue);

    delete model;
}

void tst_qquickxmllistmodel::testTypes_data()
{
    QTest::addColumn<QString>("xml");
    QTest::addColumn<QString>("roleName");
    QTest::addColumn<QVariant>("expectedValue");

    QTest::newRow("missing string field") << "<data></data>"
            << "stringValue" << QVariant("");
    QTest::newRow("empty string") << "<data><a-string></a-string></data>"
            << "stringValue" << QVariant("");
    QTest::newRow("1-char string") << "<data><a-string>5</a-string></data>"
            << "stringValue" << QVariant("5");
    QTest::newRow("string ok") << "<data><a-string>abc def g</a-string></data>"
            << "stringValue" << QVariant("abc def g");

    QTest::newRow("missing number field") << "<data></data>"
            << "numberValue" << QVariant("");
    double nan = qQNaN();
    QTest::newRow("empty number field") << "<data><a-number></a-number></data>"
            << "numberValue" << QVariant(nan);
    QTest::newRow("number field with string") << "<data><a-number>a string</a-number></data>"
            << "numberValue" << QVariant(nan);
    QTest::newRow("-1") << "<data><a-number>-1</a-number></data>"
            << "numberValue" << QVariant("-1");
    QTest::newRow("-1.5") << "<data><a-number>-1.5</a-number></data>"
            << "numberValue" << QVariant("-1.5");
    QTest::newRow("0") << "<data><a-number>0</a-number></data>"
            << "numberValue" << QVariant("0");
    QTest::newRow("+1") << "<data><a-number>1</a-number></data>"
            << "numberValue" << QVariant("1");
    QTest::newRow("+1.5") << "<data><a-number>1.5</a-number></data>"
            << "numberValue" << QVariant("1.5");
}

void tst_qquickxmllistmodel::cdata()
{
    QQmlComponent component(&engine, testFileUrl("recipes.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 5);

    QVERIFY(model->data(model->index(2, 0), Qt::UserRole+2).toString().startsWith(QLatin1String("<html>")));

    delete model;
}

void tst_qquickxmllistmodel::attributes()
{
    QQmlComponent component(&engine, testFileUrl("recipes.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 5);
    QCOMPARE(model->data(model->index(2, 0), Qt::UserRole).toString(), QLatin1String("Vegetable Soup"));

    delete model;
}

void tst_qquickxmllistmodel::roles()
{
    QQmlComponent component(&engine, testFileUrl("model.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 9);

    QHash<int, QByteArray> roleNames = model->roleNames();
    QCOMPARE(roleNames.count(), 4);
    QVERIFY(roleNames.key("name", -1) >= 0);
    QVERIFY(roleNames.key("type", -1) >= 0);
    QVERIFY(roleNames.key("age", -1) >= 0);
    QVERIFY(roleNames.key("size", -1) >= 0);

    QSet<int> roles;
    roles.insert(roleNames.key("name"));
    roles.insert(roleNames.key("type"));
    roles.insert(roleNames.key("age"));
    roles.insert(roleNames.key("size"));
    QCOMPARE(roles.count(), 4);

    delete model;
}

void tst_qquickxmllistmodel::roleErrors()
{
    QQmlComponent component(&engine, testFileUrl("roleErrors.qml"));
    QTest::ignoreMessage(QtWarningMsg, (testFileUrl("roleErrors.qml").toString() + ":7:5: QML XmlRole: An XmlRole query must not start with '/'").toUtf8().constData());
    QTest::ignoreMessage(QtWarningMsg, (testFileUrl("roleErrors.qml").toString() + ":10:5: QML XmlRole: invalid query: \"age/\"").toUtf8().constData());

    //### make sure we receive all expected warning messages.
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 9);

    QModelIndex index = model->index(3, 0);
    //### should any of these return valid values?
    QCOMPARE(model->data(index, Qt::UserRole), QVariant());
    QCOMPARE(model->data(index, Qt::UserRole+1), QVariant());
    QCOMPARE(model->data(index, Qt::UserRole+2), QVariant());

    QEXPECT_FAIL("", "QTBUG-10797", Continue);
    QCOMPARE(model->data(index, Qt::UserRole+3), QVariant());

    delete model;
}

void tst_qquickxmllistmodel::uniqueRoleNames()
{
    QQmlComponent component(&engine, testFileUrl("unique.qml"));
    QTest::ignoreMessage(QtWarningMsg, (testFileUrl("unique.qml").toString() + ":8:5: QML XmlRole: \"name\" duplicates a previous role name and will be disabled.").toUtf8().constData());
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 9);

    QHash<int, QByteArray> roleNames = model->roleNames();
    QCOMPARE(roleNames.count(), 1);

    delete model;
}


void tst_qquickxmllistmodel::xml()
{
    QFETCH(QString, xml);
    QFETCH(int, count);

    QQmlComponent component(&engine, testFileUrl("model.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());

    QSignalSpy spy(model, SIGNAL(statusChanged(QQuickXmlListModel::Status)));
    QVERIFY(errorString(model).isEmpty());
    QCOMPARE(model->property("progress").toDouble(), qreal(0.0));
    QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
             QQuickXmlListModel::Loading);
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QTest::qWait(50);
    QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
             QQuickXmlListModel::Ready);
    QVERIFY(errorString(model).isEmpty());
    QCOMPARE(model->property("progress").toDouble(), qreal(1.0));
    QCOMPARE(model->rowCount(), 9);

    // if xml is empty (i.e. clearing) it won't have any effect if a source is set
    if (xml.isEmpty())
        model->setProperty("source",QUrl());
    model->setProperty("xml",xml);
    QCOMPARE(model->property("progress").toDouble(), qreal(1.0));   // immediately goes to 1.0 if using setXml()
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
             QQuickXmlListModel::Loading);
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    if (xml.isEmpty())
        QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
                 QQuickXmlListModel::Null);
    else
        QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
                 QQuickXmlListModel::Ready);
    QVERIFY(errorString(model).isEmpty());
    QCOMPARE(model->rowCount(), count);

    delete model;
}

void tst_qquickxmllistmodel::xml_data()
{
    QTest::addColumn<QString>("xml");
    QTest::addColumn<int>("count");

    QTest::newRow("xml with no items") << "<Pets></Pets>" << 0;
    QTest::newRow("empty xml") << "" << 0;
    QTest::newRow("one item") << "<Pets><Pet><name>Hobbes</name><type>Tiger</type><age>7</age><size>Large</size></Pet></Pets>" << 1;
}

void tst_qquickxmllistmodel::headers()
{
    // ensure the QNetworkAccessManagers created for this test are immediately deleted
    QQmlEngine qmlEng;

    CustomNetworkAccessManagerFactory factory;
    qmlEng.setNetworkAccessManagerFactory(&factory);

    QQmlComponent component(&qmlEng, testFileUrl("model.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
                 QQuickXmlListModel::Ready);

    QVariantMap expectedHeaders;
    expectedHeaders["Accept"] = "application/xml,*/*";

    QCOMPARE(factory.lastSentHeaders.count(), expectedHeaders.count());
    foreach (const QString &header, expectedHeaders.keys()) {
        QVERIFY(factory.lastSentHeaders.contains(header));
        QCOMPARE(factory.lastSentHeaders[header].toString(), expectedHeaders[header].toString());
    }

    delete model;
}

void tst_qquickxmllistmodel::source()
{
    QFETCH(QUrl, source);
    QFETCH(int, count);
    QFETCH(QQuickXmlListModel::Status, status);

    QQmlComponent component(&engine, testFileUrl("model.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QSignalSpy spy(model, SIGNAL(statusChanged(QQuickXmlListModel::Status)));

    QVERIFY(errorString(model).isEmpty());
    QCOMPARE(model->property("progress").toDouble(), qreal(0.0));
    QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
             QQuickXmlListModel::Loading);
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
             QQuickXmlListModel::Ready);
    QVERIFY(errorString(model).isEmpty());
    QCOMPARE(model->property("progress").toDouble(), qreal(1.0));
    QCOMPARE(model->rowCount(), 9);

    model->setProperty("source",source);
    if (model->property("source").toString().isEmpty())
        QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
                 QQuickXmlListModel::Null);
    QCOMPARE(model->property("progress").toDouble(), qreal(0.0));
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")),
             QQuickXmlListModel::Loading);
    QVERIFY(errorString(model).isEmpty());

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(model, SIGNAL(statusChanged(QQuickXmlListModel::Status)), &loop, SLOT(quit()));
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(20000);
    loop.exec();

    if (spy.count() == 0 && status != QQuickXmlListModel::Ready) {
        qWarning("QQuickXmlListModel invalid source test timed out");
    } else {
        QCOMPARE(spy.count(), 1); spy.clear();
    }

    QCOMPARE(qvariant_cast<QQuickXmlListModel::Status>(model->property("status")), status);
    QCOMPARE(model->rowCount(), count);

    if (status == QQuickXmlListModel::Ready)
        QCOMPARE(model->property("progress").toDouble(), qreal(1.0));

    QCOMPARE(errorString(model).isEmpty(), status == QQuickXmlListModel::Ready);

    delete model;
}

void tst_qquickxmllistmodel::source_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<int>("count");
    QTest::addColumn<QQuickXmlListModel::Status>("status");

    QTest::newRow("valid") << testFileUrl("model2.xml") << 2
                           << QQuickXmlListModel::Ready;
    QTest::newRow("invalid") << QUrl("http://blah.blah/blah.xml") << 0
                             << QQuickXmlListModel::Error;

    // empty file
    QTemporaryFile *temp = new QTemporaryFile(this);
    if (temp->open())
        QTest::newRow("empty file") << QUrl::fromLocalFile(temp->fileName()) << 0
                                    << QQuickXmlListModel::Ready;
    temp->close();
}

void tst_qquickxmllistmodel::data()
{
    QQmlComponent component(&engine, testFileUrl("model.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);

    for (int i=0; i<9; i++)  {
        QModelIndex index = model->index(i, 0);
        for (int j=0; j<model->roleNames().count(); j++) {
            QCOMPARE(model->data(index, j), QVariant());
        }
    }
    QTRY_COMPARE(model->rowCount(), 9);

    delete model;
}

void tst_qquickxmllistmodel::get()
{
    QQmlComponent component(&engine, testFileUrl("get.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());

    QVERIFY(model != 0);

    QVERIFY(QMetaObject::invokeMethod(model, "runPreTest"));
    QCOMPARE(model->property("preTest").toBool(), true);

    QTRY_COMPARE(model->rowCount(), 9);

    QVERIFY(QMetaObject::invokeMethod(model, "runPostTest"));
    QCOMPARE(model->property("postTest").toBool(), true);

    delete model;
}

void tst_qquickxmllistmodel::reload()
{
    // If no keys are used, the model should be rebuilt from scratch when
    // reload() is called.

    QQmlComponent component(&engine, testFileUrl("model.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 9);

    QSignalSpy spyInsert(model, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spyRemove(model, SIGNAL(rowsRemoved(QModelIndex,int,int)));
    QSignalSpy spyCount(model, SIGNAL(countChanged()));
    //reload multiple times to test the xml query aborting
    QMetaObject::invokeMethod(model, "reload");
    QMetaObject::invokeMethod(model, "reload");
    QCoreApplication::processEvents();
    QMetaObject::invokeMethod(model, "reload");
    QMetaObject::invokeMethod(model, "reload");
    QTRY_COMPARE(spyCount.count(), 0);
    QTRY_COMPARE(spyInsert.count(), 1);
    QTRY_COMPARE(spyRemove.count(), 1);

    QCOMPARE(spyInsert[0][1].toInt(), 0);
    QCOMPARE(spyInsert[0][2].toInt(), 8);

    QCOMPARE(spyRemove[0][1].toInt(), 0);
    QCOMPARE(spyRemove[0][2].toInt(), 8);

    delete model;
}

void tst_qquickxmllistmodel::useKeys()
{
    // If using incremental updates through keys, the model should only
    // insert & remove some of the items, instead of throwing everything
    // away and causing the view to repaint the whole view.

    QFETCH(QString, oldXml);
    QFETCH(int, oldCount);
    QFETCH(QString, newXml);
    QFETCH(QQmlXmlModelData, newData);
    QFETCH(QList<QQuickXmlListRange>, insertRanges);
    QFETCH(QList<QQuickXmlListRange>, removeRanges);

    QQmlComponent component(&engine, testFileUrl("roleKeys.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);

    model->setProperty("xml",oldXml);
    QTRY_COMPARE(model->rowCount(), oldCount);

    QSignalSpy spyInsert(model, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spyRemove(model, SIGNAL(rowsRemoved(QModelIndex,int,int)));
    QSignalSpy spyCount(model, SIGNAL(countChanged()));

    model->setProperty("xml",newXml);

    if (oldCount != newData.count()) {
        QTRY_COMPARE(model->rowCount(), newData.count());
        QCOMPARE(spyCount.count(), 1);
    } else {
        QTRY_VERIFY(spyInsert.count() > 0 || spyRemove.count() > 0);
        QCOMPARE(spyCount.count(), 0);
    }

    QList<int> roles = model->roleNames().keys();
    std::sort(roles.begin(), roles.end());
    for (int i=0; i<model->rowCount(); i++) {
        QModelIndex index = model->index(i, 0);
        for (int j=0; j<roles.count(); j++)
            QCOMPARE(model->data(index, roles.at(j)), newData[i][j]);
    }

    QCOMPARE(spyInsert.count(), insertRanges.count());
    for (int i=0; i<spyInsert.count(); i++) {
        QCOMPARE(spyInsert[i][1].toInt(), insertRanges[i].first);
        QCOMPARE(spyInsert[i][2].toInt(), insertRanges[i].first + insertRanges[i].second - 1);
    }

    QCOMPARE(spyRemove.count(), removeRanges.count());
    for (int i=0; i<spyRemove.count(); i++) {
        QCOMPARE(spyRemove[i][1].toInt(), removeRanges[i].first);
        QCOMPARE(spyRemove[i][2].toInt(), removeRanges[i].first + removeRanges[i].second - 1);
    }

    delete model;
}

void tst_qquickxmllistmodel::useKeys_data()
{
    QTest::addColumn<QString>("oldXml");
    QTest::addColumn<int>("oldCount");
    QTest::addColumn<QString>("newXml");
    QTest::addColumn<QQmlXmlModelData>("newData");
    QTest::addColumn<QList<QQuickXmlListRange> >("insertRanges");
    QTest::addColumn<QList<QQuickXmlListRange> >("removeRanges");

    QQmlXmlModelData modelData;

    QTest::newRow("append 1")
        << makeItemXmlAndData("name=A,age=25,sport=Football") << 1
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(1, 1))
        << QList<QQuickXmlListRange>();

    QTest::newRow("append multiple")
        << makeItemXmlAndData("name=A,age=25,sport=Football") << 1
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(1, 2))
        << QList<QQuickXmlListRange>();

    QTest::newRow("insert in different spots")
        << makeItemXmlAndData("name=B,age=35,sport=Athletics") << 1
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling;name=D,age=55,sport=Golf", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(0, 1) << qMakePair(2,2))
        << QList<QQuickXmlListRange>();

    QTest::newRow("insert in middle")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=D,age=55,sport=Golf") << 2
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling;name=D,age=55,sport=Golf", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(1, 2))
        << QList<QQuickXmlListRange>();

    QTest::newRow("remove first")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics") << 2
        << makeItemXmlAndData("name=B,age=35,sport=Athletics", &modelData)
        << modelData
        << QList<QQuickXmlListRange>()
        << (QList<QQuickXmlListRange>() << qMakePair(0, 1));

    QTest::newRow("remove last")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics") << 2
        << makeItemXmlAndData("name=A,age=25,sport=Football", &modelData)
        << modelData
        << QList<QQuickXmlListRange>()
        << (QList<QQuickXmlListRange>() << qMakePair(1, 1));

    QTest::newRow("remove from multiple spots")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling;name=D,age=55,sport=Golf;name=E,age=65,sport=Fencing") << 5
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=C,age=45,sport=Curling", &modelData)
        << modelData
        << QList<QQuickXmlListRange>()
        << (QList<QQuickXmlListRange>() << qMakePair(1, 1) << qMakePair(3,2));

    QTest::newRow("remove all")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling") << 3
        << makeItemXmlAndData("", &modelData)
        << modelData
        << QList<QQuickXmlListRange>()
        << (QList<QQuickXmlListRange>() << qMakePair(0, 3));

    QTest::newRow("replace item")
        << makeItemXmlAndData("name=A,age=25,sport=Football") << 1
        << makeItemXmlAndData("name=ZZZ,age=25,sport=Football", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(0, 1))
        << (QList<QQuickXmlListRange>() << qMakePair(0, 1));

    QTest::newRow("add and remove simultaneously, in different spots")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling;name=D,age=55,sport=Golf") << 4
        << makeItemXmlAndData("name=B,age=35,sport=Athletics;name=E,age=65,sport=Fencing", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(1, 1))
        << (QList<QQuickXmlListRange>() << qMakePair(0, 1) << qMakePair(2,2));

    QTest::newRow("insert at start, remove at end i.e. rss feed")
        << makeItemXmlAndData("name=C,age=45,sport=Curling;name=D,age=55,sport=Golf;name=E,age=65,sport=Fencing") << 3
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(0, 2))
        << (QList<QQuickXmlListRange>() << qMakePair(1, 2));

    QTest::newRow("remove at start, insert at end")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling") << 3
        << makeItemXmlAndData("name=C,age=45,sport=Curling;name=D,age=55,sport=Golf;name=E,age=65,sport=Fencing", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(1, 2))
        << (QList<QQuickXmlListRange>() << qMakePair(0, 2));

    QTest::newRow("all data has changed")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35") << 2
        << makeItemXmlAndData("name=C,age=45,sport=Curling;name=D,age=55,sport=Golf", &modelData)
        << modelData
        << (QList<QQuickXmlListRange>() << qMakePair(0, 2))
        << (QList<QQuickXmlListRange>() << qMakePair(0, 2));
}

void tst_qquickxmllistmodel::noKeysValueChanges()
{
    // The 'key' roles are 'name' and 'age', as defined in roleKeys.qml.
    // If a 'sport' value is changed, the model should not be reloaded,
    // since 'sport' is not marked as a key.

    QQmlComponent component(&engine, testFileUrl("roleKeys.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);

    QString xml;

    xml = makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics");
    model->setProperty("xml",xml);
    QTRY_COMPARE(model->rowCount(), 2);

    model->setProperty("xml","");

    QSignalSpy spyInsert(model, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spyRemove(model, SIGNAL(rowsRemoved(QModelIndex,int,int)));
    QSignalSpy spyCount(model, SIGNAL(countChanged()));

    xml = makeItemXmlAndData("name=A,age=25,sport=AussieRules;name=B,age=35,sport=Athletics");
    model->setProperty("xml",xml);

    QList<int> roles = model->roleNames().keys();
    std::sort(roles.begin(), roles.end());
    // wait for the new xml data to be set, and verify no signals were emitted
    QTRY_VERIFY(model->data(model->index(0, 0), roles.at(2)).toString() != QLatin1String("Football"));
    QCOMPARE(model->data(model->index(0, 0), roles.at(2)).toString(), QLatin1String("AussieRules"));

    QCOMPARE(spyInsert.count(), 0);
    QCOMPARE(spyRemove.count(), 0);
    QCOMPARE(spyCount.count(), 0);

    QCOMPARE(model->rowCount(), 2);

    delete model;
}

void tst_qquickxmllistmodel::keysChanged()
{
    // If the key roles change, the next time the data is reloaded, it should
    // delete all its data and build a clean model (i.e. same behaviour as
    // if no keys are set).

    QQmlComponent component(&engine, testFileUrl("roleKeys.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);

    QString xml = makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics");
    model->setProperty("xml",xml);
    QTRY_COMPARE(model->rowCount(), 2);

    model->setProperty("xml","");

    QSignalSpy spyInsert(model, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spyRemove(model, SIGNAL(rowsRemoved(QModelIndex,int,int)));
    QSignalSpy spyCount(model, SIGNAL(countChanged()));

    QVERIFY(QMetaObject::invokeMethod(model, "disableNameKey"));
    model->setProperty("xml",xml);

    QTRY_VERIFY(spyInsert.count() > 0 && spyRemove.count() > 0);

    QCOMPARE(spyInsert.count(), 1);
    QCOMPARE(spyInsert[0][1].toInt(), 0);
    QCOMPARE(spyInsert[0][2].toInt(), 1);

    QCOMPARE(spyRemove.count(), 1);
    QCOMPARE(spyRemove[0][1].toInt(), 0);
    QCOMPARE(spyRemove[0][2].toInt(), 1);

    QCOMPARE(spyCount.count(), 0);

    delete model;
}

void tst_qquickxmllistmodel::threading()
{
    QFETCH(int, xmlDataCount);

    QQmlComponent component(&engine, testFileUrl("roleKeys.qml"));

    QAbstractItemModel *m1 = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(m1 != 0);
    QAbstractItemModel *m2 = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(m2 != 0);
    QAbstractItemModel *m3 = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(m3 != 0);

    for (int dataCount=0; dataCount<xmlDataCount; dataCount++) {

        QString data1, data2, data3;
        for (int i=0; i<dataCount; i++) {
            data1 += "name=A" + QString::number(i) + ",age=1" + QString::number(i) + ",sport=Football;";
            data2 += "name=B" + QString::number(i) + ",age=2" + QString::number(i) + ",sport=Athletics;";
            data3 += "name=C" + QString::number(i) + ",age=3" + QString::number(i) + ",sport=Curling;";
        }

        //Set the xml data multiple times with randomized order and mixed with multiple event loops
        //to test the xml query reloading/aborting, the result should be stable.
        m1->setProperty("xml",makeItemXmlAndData(data1));
        m2->setProperty("xml",makeItemXmlAndData(data2));
        m3->setProperty("xml",makeItemXmlAndData(data3));
        QCoreApplication::processEvents();
        m2->setProperty("xml",makeItemXmlAndData(data2));
        m1->setProperty("xml",makeItemXmlAndData(data1));
        m2->setProperty("xml",makeItemXmlAndData(data2));
        QCoreApplication::processEvents();
        m3->setProperty("xml",makeItemXmlAndData(data3));
        QCoreApplication::processEvents();
        m2->setProperty("xml",makeItemXmlAndData(data2));
        m1->setProperty("xml",makeItemXmlAndData(data1));
        m2->setProperty("xml",makeItemXmlAndData(data2));
        m3->setProperty("xml",makeItemXmlAndData(data3));
        QCoreApplication::processEvents();
        m2->setProperty("xml",makeItemXmlAndData(data2));
        m3->setProperty("xml",makeItemXmlAndData(data3));
        m3->setProperty("xml",makeItemXmlAndData(data3));
        QCoreApplication::processEvents();

        QTRY_VERIFY(m1->rowCount() == dataCount && m2->rowCount() == dataCount && m3->rowCount() == dataCount);

        for (int i=0; i<dataCount; i++) {
            QModelIndex index = m1->index(i, 0);
            QList<int> roles = m1->roleNames().keys();
            std::sort(roles.begin(), roles.end());
            QCOMPARE(m1->data(index, roles.at(0)).toString(), QLatin1Char('A') + QString::number(i));
            QCOMPARE(m1->data(index, roles.at(1)).toString(), QLatin1Char('1') + QString::number(i));
            QCOMPARE(m1->data(index, roles.at(2)).toString(), QString("Football"));

            index = m2->index(i, 0);
            roles = m2->roleNames().keys();
            std::sort(roles.begin(), roles.end());
            QCOMPARE(m2->data(index, roles.at(0)).toString(), QLatin1Char('B') + QString::number(i));
            QCOMPARE(m2->data(index, roles.at(1)).toString(), QLatin1Char('2') + QString::number(i));
            QCOMPARE(m2->data(index, roles.at(2)).toString(), QString("Athletics"));

            index = m3->index(i, 0);
            roles = m3->roleNames().keys();
            std::sort(roles.begin(), roles.end());
            QCOMPARE(m3->data(index, roles.at(0)).toString(), QLatin1Char('C') + QString::number(i));
            QCOMPARE(m3->data(index, roles.at(1)).toString(), QLatin1Char('3') + QString::number(i));
            QCOMPARE(m3->data(index, roles.at(2)).toString(), QString("Curling"));
        }
    }

    delete m1;
    delete m2;
    delete m3;
}

void tst_qquickxmllistmodel::threading_data()
{
    QTest::addColumn<int>("xmlDataCount");

    QTest::newRow("1") << 1;
    QTest::newRow("2") << 2;
    QTest::newRow("10") << 10;
}

void tst_qquickxmllistmodel::propertyChanges()
{
    QQmlComponent component(&engine, testFileUrl("propertychanges.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 9);

    QObject *role = model->findChild<QObject*>("role");
    QVERIFY(role);

    QSignalSpy nameSpy(role, SIGNAL(nameChanged()));
    QSignalSpy querySpy(role, SIGNAL(queryChanged()));
    QSignalSpy isKeySpy(role, SIGNAL(isKeyChanged()));

    role->setProperty("name","size");
    role->setProperty("query","size/string()");
    role->setProperty("isKey",true);

    QCOMPARE(role->property("name").toString(), QString("size"));
    QCOMPARE(role->property("query").toString(), QString("size/string()"));
    QVERIFY(role->property("isKey").toBool());

    QCOMPARE(nameSpy.count(),1);
    QCOMPARE(querySpy.count(),1);
    QCOMPARE(isKeySpy.count(),1);

    role->setProperty("name","size");
    role->setProperty("query","size/string()");
    role->setProperty("isKey",true);

    QCOMPARE(nameSpy.count(),1);
    QCOMPARE(querySpy.count(),1);
    QCOMPARE(isKeySpy.count(),1);

    QSignalSpy sourceSpy(model, SIGNAL(sourceChanged()));
    QSignalSpy xmlSpy(model, SIGNAL(xmlChanged()));
    QSignalSpy modelQuerySpy(model, SIGNAL(queryChanged()));
    QSignalSpy namespaceDeclarationsSpy(model, SIGNAL(namespaceDeclarationsChanged()));

    model->setProperty("source",QUrl(""));
    model->setProperty("xml","<Pets><Pet><name>Polly</name><type>Parrot</type><age>12</age><size>Small</size></Pet></Pets>");
    model->setProperty("query","/Pets");
    model->setProperty("namespaceDeclarations","declare namespace media=\"http://search.yahoo.com/mrss/\";");

    QCOMPARE(model->property("source").toUrl(), QUrl(""));
    QCOMPARE(model->property("xml").toString(), QString("<Pets><Pet><name>Polly</name><type>Parrot</type><age>12</age><size>Small</size></Pet></Pets>"));
    QCOMPARE(model->property("query").toString(), QString("/Pets"));
    QCOMPARE(model->property("namespaceDeclarations").toString(), QString("declare namespace media=\"http://search.yahoo.com/mrss/\";"));

    QTRY_COMPARE(model->rowCount(), 1);

    QCOMPARE(sourceSpy.count(),1);
    QCOMPARE(xmlSpy.count(),1);
    QCOMPARE(modelQuerySpy.count(),1);
    QCOMPARE(namespaceDeclarationsSpy.count(),1);

    model->setProperty("source",QUrl(""));
    model->setProperty("xml","<Pets><Pet><name>Polly</name><type>Parrot</type><age>12</age><size>Small</size></Pet></Pets>");
    model->setProperty("query","/Pets");
    model->setProperty("namespaceDeclarations","declare namespace media=\"http://search.yahoo.com/mrss/\";");

    QCOMPARE(sourceSpy.count(),1);
    QCOMPARE(xmlSpy.count(),1);
    QCOMPARE(modelQuerySpy.count(),1);
    QCOMPARE(namespaceDeclarationsSpy.count(),1);

    QTRY_COMPARE(model->rowCount(), 1);
    delete model;
}

void tst_qquickxmllistmodel::selectAncestor()
{
    QQmlComponent component(&engine, testFileUrl("groups.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->rowCount(), 1);

    QModelIndex index = model->index(0, 0);
    QCOMPARE(model->data(index, Qt::UserRole).toInt(), 12);
    QCOMPARE(model->data(index, Qt::UserRole+1).toString(), QLatin1String("cats"));
}

void tst_qquickxmllistmodel::roleCrash()
{
    // don't crash
    QQmlComponent component(&engine, testFileUrl("roleCrash.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    delete model;
}

class SortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QObject *source READ source WRITE setSource)

public:
    SortFilterProxyModel(QObject *parent = 0) : QSortFilterProxyModel(parent) { sort(0); }
    QObject *source() const { return sourceModel(); }
    void setSource(QObject *source) { setSourceModel(qobject_cast<QAbstractItemModel *>(source)); }
};

void tst_qquickxmllistmodel::proxyCrash()
{
    qmlRegisterType<SortFilterProxyModel>("SortFilterProxyModel", 1, 0, "SortFilterProxyModel");

    // don't crash
    QQmlComponent component(&engine, testFileUrl("proxyCrash.qml"));
    QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(component.create());
    QVERIFY(model != 0);
    delete model;
}

QTEST_MAIN(tst_qquickxmllistmodel)

#include "tst_qquickxmllistmodel.moc"
