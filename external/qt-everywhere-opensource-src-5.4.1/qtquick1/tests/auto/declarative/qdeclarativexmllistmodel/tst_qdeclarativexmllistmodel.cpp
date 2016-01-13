/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <private/qdeclarativeengine_p.h>

#include <QtTest/QtTest>
#include <QtGlobal>
#include <math.h>

#include <qtest.h>
#include <QtTest/qsignalspy.h>
#include <QtDeclarative/qdeclarativenetworkaccessmanagerfactory.h>
#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtNetwork/qnetworkrequest.h>
#include <QtCore/qtimer.h>
#include <QtCore/qfile.h>
#include <QtCore/qtemporaryfile.h>

#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <private/qdeclarativexmllistmodel_p.h>

typedef QPair<int, int> QDeclarativeXmlListRange;
typedef QList<QVariantList> QDeclarativeXmlModelData;

Q_DECLARE_METATYPE(QList<QDeclarativeXmlListRange>)
Q_DECLARE_METATYPE(QDeclarativeXmlModelData)
Q_DECLARE_METATYPE(QDeclarativeXmlListModel::Status)

class tst_qdeclarativexmllistmodel : public QObject

{
    Q_OBJECT
public:
    tst_qdeclarativexmllistmodel() {}

private slots:
    void initTestCase() {
        qRegisterMetaType<QDeclarativeXmlListModel::Status>("QDeclarativeXmlListModel::Status");
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

    void roleCrash();

private:
    QString makeItemXmlAndData(const QString &data, QDeclarativeXmlModelData *modelData = 0) const
    {
        if (modelData)
            modelData->clear();
        QString xml;

        if (!data.isEmpty()) {
            QStringList items = data.split(";");
            foreach(const QString &item, items) {
                if (item.isEmpty())
                    continue;
                QVariantList variants;
                xml += QLatin1String("<item>");
                QStringList fields = item.split(",");
                foreach(const QString &field, fields) {
                    QStringList values = field.split("=");
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

    QDeclarativeEngine engine;
};

class CustomNetworkAccessManagerFactory : public QObject, public QDeclarativeNetworkAccessManagerFactory
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


void tst_qdeclarativexmllistmodel::buildModel()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->count(), 9);

    QList<int> roles;
    roles << Qt::UserRole << Qt::UserRole + 1 << Qt::UserRole + 2 << Qt::UserRole + 3;
    QHash<int, QVariant> data = model->data(3, roles);
    QVERIFY(data.count() == 4);
    QCOMPARE(data.value(Qt::UserRole).toString(), QLatin1String("Spot"));
    QCOMPARE(data.value(Qt::UserRole+1).toString(), QLatin1String("Dog"));
    QCOMPARE(data.value(Qt::UserRole+2).toInt(), 9);
    QCOMPARE(data.value(Qt::UserRole+3).toString(), QLatin1String("Medium"));

    delete model;
}

void tst_qdeclarativexmllistmodel::testTypes()
{
    QFETCH(QString, xml);
    QFETCH(QString, roleName);
    QFETCH(QVariant, expectedValue);

    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/testtypes.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    model->setXml(xml.toUtf8());
    model->reload();
    QTRY_COMPARE(model->count(), 1);

    int role = -1;
    foreach (int i, model->roles()) {
        if (model->toString(i) == roleName) {
            role = i;
            break;
        }
    }
    QVERIFY(role >= 0);

    if (expectedValue.toString() == "nan")
        QVERIFY(qIsNaN(model->data(0, role).toDouble()));
    else
        QCOMPARE(model->data(0, role), expectedValue);

    delete model;
}

void tst_qdeclarativexmllistmodel::testTypes_data()
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

void tst_qdeclarativexmllistmodel::cdata()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/recipes.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->count(), 5);

    QList<int> roles;
    roles << Qt::UserRole + 2;
    QHash<int, QVariant> data = model->data(2, roles);
    QVERIFY(data.count() == 1);
    QVERIFY(data.value(Qt::UserRole+2).toString().startsWith(QLatin1String("<html>")));

    delete model;
}

void tst_qdeclarativexmllistmodel::attributes()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/recipes.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->count(), 5);
    QList<int> roles;
    roles << Qt::UserRole;
    QHash<int, QVariant> data = model->data(2, roles);
    QVERIFY(data.count() == 1);
    QCOMPARE(data.value(Qt::UserRole).toString(), QLatin1String("Vegetable Soup"));

    delete model;
}

void tst_qdeclarativexmllistmodel::roles()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->count(), 9);

    QList<int> roles = model->roles();
    QCOMPARE(roles.count(), 4);
    QCOMPARE(model->toString(roles.at(0)), QLatin1String("name"));
    QCOMPARE(model->toString(roles.at(1)), QLatin1String("type"));
    QCOMPARE(model->toString(roles.at(2)), QLatin1String("age"));
    QCOMPARE(model->toString(roles.at(3)), QLatin1String("size"));

    delete model;
}

void tst_qdeclarativexmllistmodel::roleErrors()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/roleErrors.qml"));
    QTest::ignoreMessage(QtWarningMsg, (QUrl::fromLocalFile(SRCDIR "/data/roleErrors.qml").toString() + ":6:5: QML XmlRole: An XmlRole query must not start with '/'").toUtf8().constData());
    QTest::ignoreMessage(QtWarningMsg, (QUrl::fromLocalFile(SRCDIR "/data/roleErrors.qml").toString() + ":9:5: QML XmlRole: invalid query: \"age/\"").toUtf8().constData());

    //### make sure we receive all expected warning messages.
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->count(), 9);

    QList<int> roles;
    roles << Qt::UserRole << Qt::UserRole + 1 << Qt::UserRole + 2 << Qt::UserRole + 3;
    QHash<int, QVariant> data = model->data(3, roles);
    QVERIFY(data.count() == 4);

    //### should any of these return valid values?
    QCOMPARE(data.value(Qt::UserRole), QVariant());
    QCOMPARE(data.value(Qt::UserRole+1), QVariant());
    QCOMPARE(data.value(Qt::UserRole+2), QVariant());

    QEXPECT_FAIL("", "QTBUG-10797", Continue);
    QCOMPARE(data.value(Qt::UserRole+3), QVariant());

    delete model;
}

void tst_qdeclarativexmllistmodel::uniqueRoleNames()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/unique.qml"));
    QTest::ignoreMessage(QtWarningMsg, (QUrl::fromLocalFile(SRCDIR "/data/unique.qml").toString() + ":7:5: QML XmlRole: \"name\" duplicates a previous role name and will be disabled.").toUtf8().constData());
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->count(), 9);

    QList<int> roles = model->roles();
    QCOMPARE(roles.count(), 1);

    delete model;
}


void tst_qdeclarativexmllistmodel::xml()
{
    QFETCH(QString, xml);
    QFETCH(int, count);

    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QSignalSpy spy(model, SIGNAL(statusChanged(QDeclarativeXmlListModel::Status)));

    QVERIFY(model->errorString().isEmpty());
    QCOMPARE(model->progress(), qreal(0.0));
    QCOMPARE(model->status(), QDeclarativeXmlListModel::Loading);
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QCOMPARE(model->status(), QDeclarativeXmlListModel::Ready);
    QVERIFY(model->errorString().isEmpty());
    QCOMPARE(model->progress(), qreal(1.0));
    QCOMPARE(model->count(), 9);

    // if xml is empty (i.e. clearing) it won't have any effect if a source is set
    if (xml.isEmpty())
        model->setSource(QUrl());
    model->setXml(xml);
    QCOMPARE(model->progress(), qreal(1.0));   // immediately goes to 1.0 if using setXml()
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QCOMPARE(model->status(), QDeclarativeXmlListModel::Loading);
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QCOMPARE(model->status(), QDeclarativeXmlListModel::Ready);
    QVERIFY(model->errorString().isEmpty());
    QCOMPARE(model->count(), count);

    delete model;
}

void tst_qdeclarativexmllistmodel::xml_data()
{
    QTest::addColumn<QString>("xml");
    QTest::addColumn<int>("count");

    QTest::newRow("xml with no items") << "<Pets></Pets>" << 0;
    QTest::newRow("empty xml") << "" << 0;
    QTest::newRow("one item") << "<Pets><Pet><name>Hobbes</name><type>Tiger</type><age>7</age><size>Large</size></Pet></Pets>" << 1;
}

void tst_qdeclarativexmllistmodel::headers()
{
    // ensure the QNetworkAccessManagers created for this test are immediately deleted
    QDeclarativeEngine qmlEng;

    CustomNetworkAccessManagerFactory factory;
    qmlEng.setNetworkAccessManagerFactory(&factory);

    QDeclarativeComponent component(&qmlEng, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->status(), QDeclarativeXmlListModel::Ready);

    QVariantMap expectedHeaders;
    expectedHeaders["Accept"] = "application/xml,*/*";

    QCOMPARE(factory.lastSentHeaders.count(), expectedHeaders.count());
    foreach (const QString &header, expectedHeaders.keys()) {
        QVERIFY(factory.lastSentHeaders.contains(header));
        QCOMPARE(factory.lastSentHeaders[header].toString(), expectedHeaders[header].toString());
    }

    delete model;
}

void tst_qdeclarativexmllistmodel::source()
{
    QFETCH(QUrl, source);
    QFETCH(int, count);
    QFETCH(QDeclarativeXmlListModel::Status, status);

    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QSignalSpy spy(model, SIGNAL(statusChanged(QDeclarativeXmlListModel::Status)));

    QVERIFY(model->errorString().isEmpty());
    QCOMPARE(model->progress(), qreal(0.0));
    QCOMPARE(model->status(), QDeclarativeXmlListModel::Loading);
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QCOMPARE(model->status(), QDeclarativeXmlListModel::Ready);
    QVERIFY(model->errorString().isEmpty());
    QCOMPARE(model->progress(), qreal(1.0));
    QCOMPARE(model->count(), 9);

    model->setSource(source);
    QCOMPARE(model->progress(), qreal(0.0));
    QTRY_COMPARE(spy.count(), 1); spy.clear();
    QCOMPARE(model->status(), QDeclarativeXmlListModel::Loading);
    QVERIFY(model->errorString().isEmpty());

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(model, SIGNAL(statusChanged(QDeclarativeXmlListModel::Status)), &loop, SLOT(quit()));
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(20000);
    loop.exec();

    if (spy.count() == 0 && status != QDeclarativeXmlListModel::Ready) {
        qWarning("QDeclarativeXmlListModel invalid source test timed out");
    } else {
        QCOMPARE(spy.count(), 1); spy.clear();
    }

    QCOMPARE(model->status(), status);
    QCOMPARE(model->count(), count);

    if (status == QDeclarativeXmlListModel::Ready)
        QCOMPARE(model->progress(), qreal(1.0));

    QCOMPARE(model->errorString().isEmpty(), status == QDeclarativeXmlListModel::Ready);

    delete model;
}

void tst_qdeclarativexmllistmodel::source_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<int>("count");
    QTest::addColumn<QDeclarativeXmlListModel::Status>("status");

    QTest::newRow("valid") << QUrl::fromLocalFile(SRCDIR "/data/model2.xml") << 2 << QDeclarativeXmlListModel::Ready;
    QTest::newRow("invalid") << QUrl("http://blah.blah/blah.xml") << 0 << QDeclarativeXmlListModel::Error;

    // empty file
    QTemporaryFile *temp = new QTemporaryFile(this);
    if (temp->open())
        QTest::newRow("empty file") << QUrl::fromLocalFile(temp->fileName()) << 0 << QDeclarativeXmlListModel::Ready;
    temp->close();
}

void tst_qdeclarativexmllistmodel::data()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);

    QHash<int,QVariant> blank;
    for (int i=0; i<model->roles().count(); i++)
        blank.insert(model->roles()[i], QVariant());
    for (int i=0; i<9; i++)  {
        QCOMPARE(model->data(i, model->roles()), blank);
        for (int j=0; j<model->roles().count(); j++) {
            QCOMPARE(model->data(i, j), QVariant());
        }
    }
    QTRY_COMPARE(model->count(), 9);

    delete model;
}

void tst_qdeclarativexmllistmodel::get()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QVERIFY(model->get(0).isUndefined());

    QTRY_COMPARE(model->count(), 9);
    QVERIFY(model->get(-1).isUndefined());

    QScriptValue sv = model->get(0);
    QCOMPARE(sv.property("name").toString(), QLatin1String("Polly"));
    QCOMPARE(sv.property("type").toString(), QLatin1String("Parrot"));
    QCOMPARE(sv.property("age").toNumber(), qsreal(12));
    QCOMPARE(sv.property("size").toString(), QLatin1String("Small"));

    sv = model->get(1);
    QCOMPARE(sv.property("name").toString(), QLatin1String("Penny"));
    QCOMPARE(sv.property("type").toString(), QLatin1String("Turtle"));
    QCOMPARE(sv.property("age").toNumber(), qsreal(4));
    QCOMPARE(sv.property("size").toString(), QLatin1String("Small"));

    sv = model->get(7);
    QCOMPARE(sv.property("name").toString(), QLatin1String("Rover"));
    QCOMPARE(sv.property("type").toString(), QLatin1String("Dog"));
    QCOMPARE(sv.property("age").toNumber(), qsreal(0));
    QCOMPARE(sv.property("size").toString(), QLatin1String("Large"));

    sv = model->get(8);
    QCOMPARE(sv.property("name").toString(), QLatin1String("Tiny"));
    QCOMPARE(sv.property("type").toString(), QLatin1String("Elephant"));
    QCOMPARE(sv.property("age").toNumber(), qsreal(15));
    QCOMPARE(sv.property("size").toString(), QLatin1String("Large"));

    sv = model->get(9);
    QVERIFY(sv.isUndefined());

    delete model;
}

void tst_qdeclarativexmllistmodel::reload()
{
    // If no keys are used, the model should be rebuilt from scratch when
    // reload() is called.

    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/model.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->count(), 9);

    QSignalSpy spyInsert(model, SIGNAL(itemsInserted(int,int)));
    QSignalSpy spyRemove(model, SIGNAL(itemsRemoved(int,int)));
    QSignalSpy spyCount(model, SIGNAL(countChanged()));

    //reload multiple times to test the xml query aborting
    model->reload();
    model->reload();
    QCoreApplication::processEvents();
    model->reload();
    model->reload();
    QTRY_COMPARE(spyCount.count(), 1);
    QTRY_COMPARE(spyInsert.count(), 1);
    QTRY_COMPARE(spyRemove.count(), 1);

    QCOMPARE(spyInsert[0][0].toInt(), 0);
    QCOMPARE(spyInsert[0][1].toInt(), 9);

    QCOMPARE(spyRemove[0][0].toInt(), 0);
    QCOMPARE(spyRemove[0][1].toInt(), 9);

    delete model;
}

void tst_qdeclarativexmllistmodel::useKeys()
{
    // If using incremental updates through keys, the model should only
    // insert & remove some of the items, instead of throwing everything
    // away and causing the view to repaint the whole view.

    QFETCH(QString, oldXml);
    QFETCH(int, oldCount);
    QFETCH(QString, newXml);
    QFETCH(QDeclarativeXmlModelData, newData);
    QFETCH(QList<QDeclarativeXmlListRange>, insertRanges);
    QFETCH(QList<QDeclarativeXmlListRange>, removeRanges);

    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/roleKeys.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);

    model->setXml(oldXml);
    QTRY_COMPARE(model->count(), oldCount);

    QSignalSpy spyInsert(model, SIGNAL(itemsInserted(int,int)));
    QSignalSpy spyRemove(model, SIGNAL(itemsRemoved(int,int)));
    QSignalSpy spyCount(model, SIGNAL(countChanged()));

    model->setXml(newXml);

    if (oldCount != newData.count()) {
        QTRY_COMPARE(model->count(), newData.count());
        QCOMPARE(spyCount.count(), 1);
    } else {
        QTRY_VERIFY(spyInsert.count() > 0 || spyRemove.count() > 0);
        QCOMPARE(spyCount.count(), 0);
    }

    QList<int> roles = model->roles();
    for (int i=0; i<model->count(); i++) {
        for (int j=0; j<roles.count(); j++)
            QCOMPARE(model->data(i, roles[j]), newData[i][j]);
    }

    QCOMPARE(spyInsert.count(), insertRanges.count());
    for (int i=0; i<spyInsert.count(); i++) {
        QCOMPARE(spyInsert[i][0].toInt(), insertRanges[i].first);
        QCOMPARE(spyInsert[i][1].toInt(), insertRanges[i].second);
    }

    QCOMPARE(spyRemove.count(), removeRanges.count());
    for (int i=0; i<spyRemove.count(); i++) {
        QCOMPARE(spyRemove[i][0].toInt(), removeRanges[i].first);
        QCOMPARE(spyRemove[i][1].toInt(), removeRanges[i].second);
    }

    delete model;
}

void tst_qdeclarativexmllistmodel::useKeys_data()
{
    QTest::addColumn<QString>("oldXml");
    QTest::addColumn<int>("oldCount");
    QTest::addColumn<QString>("newXml");
    QTest::addColumn<QDeclarativeXmlModelData>("newData");
    QTest::addColumn<QList<QDeclarativeXmlListRange> >("insertRanges");
    QTest::addColumn<QList<QDeclarativeXmlListRange> >("removeRanges");

    QDeclarativeXmlModelData modelData;

    QTest::newRow("append 1")
        << makeItemXmlAndData("name=A,age=25,sport=Football") << 1
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(1, 1))
        << QList<QDeclarativeXmlListRange>();

    QTest::newRow("append multiple")
        << makeItemXmlAndData("name=A,age=25,sport=Football") << 1
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(1, 2))
        << QList<QDeclarativeXmlListRange>();

    QTest::newRow("insert in different spots")
        << makeItemXmlAndData("name=B,age=35,sport=Athletics") << 1
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling;name=D,age=55,sport=Golf", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 1) << qMakePair(2,2))
        << QList<QDeclarativeXmlListRange>();

    QTest::newRow("insert in middle")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=D,age=55,sport=Golf") << 2
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling;name=D,age=55,sport=Golf", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(1, 2))
        << QList<QDeclarativeXmlListRange>();

    QTest::newRow("remove first")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics") << 2
        << makeItemXmlAndData("name=B,age=35,sport=Athletics", &modelData)
        << modelData
        << QList<QDeclarativeXmlListRange>()
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 1));

    QTest::newRow("remove last")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics") << 2
        << makeItemXmlAndData("name=A,age=25,sport=Football", &modelData)
        << modelData
        << QList<QDeclarativeXmlListRange>()
        << (QList<QDeclarativeXmlListRange>() << qMakePair(1, 1));

    QTest::newRow("remove from multiple spots")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling;name=D,age=55,sport=Golf;name=E,age=65,sport=Fencing") << 5
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=C,age=45,sport=Curling", &modelData)
        << modelData
        << QList<QDeclarativeXmlListRange>()
        << (QList<QDeclarativeXmlListRange>() << qMakePair(1, 1) << qMakePair(3,2));

    QTest::newRow("remove all")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling") << 3
        << makeItemXmlAndData("", &modelData)
        << modelData
        << QList<QDeclarativeXmlListRange>()
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 3));

    QTest::newRow("replace item")
        << makeItemXmlAndData("name=A,age=25,sport=Football") << 1
        << makeItemXmlAndData("name=ZZZ,age=25,sport=Football", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 1))
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 1));

    QTest::newRow("add and remove simultaneously, in different spots")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling;name=D,age=55,sport=Golf") << 4
        << makeItemXmlAndData("name=B,age=35,sport=Athletics;name=E,age=65,sport=Fencing", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(1, 1))
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 1) << qMakePair(2,2));

    QTest::newRow("insert at start, remove at end i.e. rss feed")
        << makeItemXmlAndData("name=C,age=45,sport=Curling;name=D,age=55,sport=Golf;name=E,age=65,sport=Fencing") << 3
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 2))
        << (QList<QDeclarativeXmlListRange>() << qMakePair(1, 2));

    QTest::newRow("remove at start, insert at end")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics;name=C,age=45,sport=Curling") << 3
        << makeItemXmlAndData("name=C,age=45,sport=Curling;name=D,age=55,sport=Golf;name=E,age=65,sport=Fencing", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(1, 2))
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 2));

    QTest::newRow("all data has changed")
        << makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35") << 2
        << makeItemXmlAndData("name=C,age=45,sport=Curling;name=D,age=55,sport=Golf", &modelData)
        << modelData
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 2))
        << (QList<QDeclarativeXmlListRange>() << qMakePair(0, 2));
}

void tst_qdeclarativexmllistmodel::noKeysValueChanges()
{
    // The 'key' roles are 'name' and 'age', as defined in roleKeys.qml.
    // If a 'sport' value is changed, the model should not be reloaded,
    // since 'sport' is not marked as a key.

    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/roleKeys.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);

    QString xml;

    xml = makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics");
    model->setXml(xml);
    QTRY_COMPARE(model->count(), 2);

    model->setXml("");

    QSignalSpy spyInsert(model, SIGNAL(itemsInserted(int,int)));
    QSignalSpy spyRemove(model, SIGNAL(itemsRemoved(int,int)));
    QSignalSpy spyCount(model, SIGNAL(countChanged()));

    xml = makeItemXmlAndData("name=A,age=25,sport=AussieRules;name=B,age=35,sport=Athletics");
    model->setXml(xml);

    // wait for the new xml data to be set, and verify no signals were emitted
    QTRY_VERIFY(model->data(0, model->roles()[2]).toString() != QLatin1String("Football"));
    QCOMPARE(model->data(0, model->roles()[2]).toString(), QLatin1String("AussieRules"));

    QVERIFY(spyInsert.count() == 0);
    QVERIFY(spyRemove.count() == 0);
    QVERIFY(spyCount.count() == 0);

    QCOMPARE(model->count(), 2);

    delete model;
}

void tst_qdeclarativexmllistmodel::keysChanged()
{
    // If the key roles change, the next time the data is reloaded, it should
    // delete all its data and build a clean model (i.e. same behaviour as
    // if no keys are set).

    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/roleKeys.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);

    QString xml = makeItemXmlAndData("name=A,age=25,sport=Football;name=B,age=35,sport=Athletics");
    model->setXml(xml);
    QTRY_COMPARE(model->count(), 2);

    model->setXml("");

    QSignalSpy spyInsert(model, SIGNAL(itemsInserted(int,int)));
    QSignalSpy spyRemove(model, SIGNAL(itemsRemoved(int,int)));
    QSignalSpy spyCount(model, SIGNAL(countChanged()));

    QVERIFY(QMetaObject::invokeMethod(model, "disableNameKey"));
    model->setXml(xml);

    QTRY_VERIFY(spyInsert.count() > 0 && spyRemove.count() > 0);

    QCOMPARE(spyInsert.count(), 1);
    QCOMPARE(spyInsert[0][0].toInt(), 0);
    QCOMPARE(spyInsert[0][1].toInt(), 2);

    QCOMPARE(spyRemove.count(), 1);
    QCOMPARE(spyRemove[0][0].toInt(), 0);
    QCOMPARE(spyRemove[0][1].toInt(), 2);

    QCOMPARE(spyCount.count(), 0);

    delete model;
}

void tst_qdeclarativexmllistmodel::threading()
{
    QFETCH(int, xmlDataCount);

    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/roleKeys.qml"));

    QDeclarativeXmlListModel *m1 = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(m1 != 0);
    QDeclarativeXmlListModel *m2 = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(m2 != 0);
    QDeclarativeXmlListModel *m3 = qobject_cast<QDeclarativeXmlListModel*>(component.create());
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
        m1->setXml(makeItemXmlAndData(data1));
        m2->setXml(makeItemXmlAndData(data2));
        m3->setXml(makeItemXmlAndData(data3));
        QCoreApplication::processEvents();
        m2->setXml(makeItemXmlAndData(data2));
        m1->setXml(makeItemXmlAndData(data1));
        m2->setXml(makeItemXmlAndData(data2));
        QCoreApplication::processEvents();
        m3->setXml(makeItemXmlAndData(data3));
        QCoreApplication::processEvents();
        m2->setXml(makeItemXmlAndData(data2));
        m1->setXml(makeItemXmlAndData(data1));
        m2->setXml(makeItemXmlAndData(data2));
        m3->setXml(makeItemXmlAndData(data3));
        QCoreApplication::processEvents();
        m2->setXml(makeItemXmlAndData(data2));
        m3->setXml(makeItemXmlAndData(data3));
        m3->setXml(makeItemXmlAndData(data3));
        QCoreApplication::processEvents();

        QTRY_VERIFY(m1->count() == dataCount && m2->count() == dataCount && m3->count() == dataCount);

        for (int i=0; i<dataCount; i++) {
            QCOMPARE(m1->data(i, m1->roles()[0]).toString(), QString("A" + QString::number(i)));
            QCOMPARE(m1->data(i, m1->roles()[1]).toString(), QString("1" + QString::number(i)));
            QCOMPARE(m1->data(i, m1->roles()[2]).toString(), QString("Football"));

            QCOMPARE(m2->data(i, m2->roles()[0]).toString(), QString("B" + QString::number(i)));
            QCOMPARE(m2->data(i, m2->roles()[1]).toString(), QString("2" + QString::number(i)));
            QCOMPARE(m2->data(i, m2->roles()[2]).toString(), QString("Athletics"));

            QCOMPARE(m3->data(i, m3->roles()[0]).toString(), QString("C" + QString::number(i)));
            QCOMPARE(m3->data(i, m3->roles()[1]).toString(), QString("3" + QString::number(i)));
            QCOMPARE(m3->data(i, m3->roles()[2]).toString(), QString("Curling"));
        }
    }

    delete m1;
    delete m2;
    delete m3;
}

void tst_qdeclarativexmllistmodel::threading_data()
{
    QTest::addColumn<int>("xmlDataCount");

    QTest::newRow("1") << 1;
    QTest::newRow("2") << 2;
    QTest::newRow("10") << 10;
}

void tst_qdeclarativexmllistmodel::propertyChanges()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/propertychanges.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    QTRY_COMPARE(model->count(), 9);

    QDeclarativeXmlListModelRole *role = model->findChild<QDeclarativeXmlListModelRole*>("role");
    QVERIFY(role);

    QSignalSpy nameSpy(role, SIGNAL(nameChanged()));
    QSignalSpy querySpy(role, SIGNAL(queryChanged()));
    QSignalSpy isKeySpy(role, SIGNAL(isKeyChanged()));

    role->setName("size");
    role->setQuery("size/string()");
    role->setIsKey(true);

    QCOMPARE(role->name(), QString("size"));
    QCOMPARE(role->query(), QString("size/string()"));
    QVERIFY(role->isKey());

    QCOMPARE(nameSpy.count(),1);
    QCOMPARE(querySpy.count(),1);
    QCOMPARE(isKeySpy.count(),1);

    role->setName("size");
    role->setQuery("size/string()");
    role->setIsKey(true);

    QCOMPARE(nameSpy.count(),1);
    QCOMPARE(querySpy.count(),1);
    QCOMPARE(isKeySpy.count(),1);

    QSignalSpy sourceSpy(model, SIGNAL(sourceChanged()));
    QSignalSpy xmlSpy(model, SIGNAL(xmlChanged()));
    QSignalSpy modelQuerySpy(model, SIGNAL(queryChanged()));
    QSignalSpy namespaceDeclarationsSpy(model, SIGNAL(namespaceDeclarationsChanged()));

    model->setSource(QUrl(""));
    model->setXml("<Pets><Pet><name>Polly</name><type>Parrot</type><age>12</age><size>Small</size></Pet></Pets>");
    model->setQuery("/Pets");
    model->setNamespaceDeclarations("declare namespace media=\"http://search.yahoo.com/mrss/\";");

    QCOMPARE(model->source(), QUrl(""));
    QCOMPARE(model->xml(), QString("<Pets><Pet><name>Polly</name><type>Parrot</type><age>12</age><size>Small</size></Pet></Pets>"));
    QCOMPARE(model->query(), QString("/Pets"));
    QCOMPARE(model->namespaceDeclarations(), QString("declare namespace media=\"http://search.yahoo.com/mrss/\";"));

    QTRY_VERIFY(model->count() == 1);

    QCOMPARE(sourceSpy.count(),1);
    QCOMPARE(xmlSpy.count(),1);
    QCOMPARE(modelQuerySpy.count(),1);
    QCOMPARE(namespaceDeclarationsSpy.count(),1);

    model->setSource(QUrl(""));
    model->setXml("<Pets><Pet><name>Polly</name><type>Parrot</type><age>12</age><size>Small</size></Pet></Pets>");
    model->setQuery("/Pets");
    model->setNamespaceDeclarations("declare namespace media=\"http://search.yahoo.com/mrss/\";");

    QCOMPARE(sourceSpy.count(),1);
    QCOMPARE(xmlSpy.count(),1);
    QCOMPARE(modelQuerySpy.count(),1);
    QCOMPARE(namespaceDeclarationsSpy.count(),1);

    QTRY_VERIFY(model->count() == 1);
    delete model;
}

void tst_qdeclarativexmllistmodel::roleCrash()
{
    // don't crash
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/roleCrash.qml"));
    QDeclarativeXmlListModel *model = qobject_cast<QDeclarativeXmlListModel*>(component.create());
    QVERIFY(model != 0);
    delete model;
}

QTEST_MAIN(tst_qdeclarativexmllistmodel)

#include "tst_qdeclarativexmllistmodel.moc"

