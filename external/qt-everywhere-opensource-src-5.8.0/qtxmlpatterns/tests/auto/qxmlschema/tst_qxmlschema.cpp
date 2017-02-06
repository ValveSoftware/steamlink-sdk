/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite module of the Qt Toolkit.
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

#include <QAbstractMessageHandler>
#include <QAbstractUriResolver>
#include <QtNetwork/QNetworkAccessManager>
#include <QXmlName>
#include <QXmlSchema>

#include "../qabstracturiresolver/TestURIResolver.h"
#include "../qxmlquery/MessageSilencer.h"

/*!
 \class tst_QXmlSchema
 \internal
 \brief Tests class QXmlSchema.

 This test is not intended for testing the engine, but the functionality specific
 to the QXmlSchema class.
 */
class tst_QXmlSchema : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultConstructor() const;
    void copyConstructor() const;
    void constructorQXmlNamePool() const;
    void copyMutationTest() const;

    void isValid() const;
    void documentUri() const;

    void loadSchemaUrlSuccess() const;
    void loadSchemaUrlFail() const;
    void loadSchemaDeviceSuccess() const;
    void loadSchemaDeviceFail() const;
    void loadSchemaDataSuccess() const;
    void loadSchemaDataFail() const;

    void networkAccessManagerSignature() const;
    void networkAccessManagerDefaultValue() const;
    void networkAccessManager() const;

    void messageHandlerSignature() const;
    void messageHandlerDefaultValue() const;
    void messageHandler() const;

    void uriResolverSignature() const;
    void uriResolverDefaultValue() const;
    void uriResolver() const;
};

void tst_QXmlSchema::defaultConstructor() const
{
    /* Allocate instance in different orders. */
    {
        QXmlSchema schema;
    }

    {
        QXmlSchema schema1;
        QXmlSchema schema2;
    }

    {
        QXmlSchema schema1;
        QXmlSchema schema2;
        QXmlSchema schema3;
    }
}

void tst_QXmlSchema::copyConstructor() const
{
    /* Verify that we can take a const reference, and simply do a copy of a default constructed object. */
    {
        const QXmlSchema schema1;
        QXmlSchema schema2(schema1);
    }

    /* Copy twice. */
    {
        const QXmlSchema schema1;
        QXmlSchema schema2(schema1);
        QXmlSchema schema3(schema2);
    }

    /* Verify that copying default values works. */
    {
        const QXmlSchema schema1;
        const QXmlSchema schema2(schema1);
        QCOMPARE(schema2.messageHandler(), schema1.messageHandler());
        QCOMPARE(schema2.uriResolver(), schema1.uriResolver());
        QCOMPARE(schema2.networkAccessManager(), schema1.networkAccessManager());
        QCOMPARE(schema2.isValid(), schema1.isValid());
    }
}

void tst_QXmlSchema::constructorQXmlNamePool() const
{
    QXmlSchema schema;

    QXmlNamePool np = schema.namePool();

    const QXmlName name(np, QLatin1String("localName"),
                            QLatin1String("http://example.com/"),
                            QLatin1String("prefix"));

    QXmlNamePool np2(schema.namePool());
    QCOMPARE(name.namespaceUri(np2), QString::fromLatin1("http://example.com/"));
    QCOMPARE(name.localName(np2), QString::fromLatin1("localName"));
    QCOMPARE(name.prefix(np2), QString::fromLatin1("prefix"));

    // make sure namePool() is const
    const QXmlSchema constSchema;
    np = constSchema.namePool();
}

void tst_QXmlSchema::copyMutationTest() const
{
    QXmlSchema schema1;
    QXmlSchema schema2(schema1);

    // check that everything is equal
    QVERIFY(schema2.messageHandler() == schema1.messageHandler());
    QVERIFY(schema2.uriResolver() == schema1.uriResolver());
    QVERIFY(schema2.networkAccessManager() == schema1.networkAccessManager());

    MessageSilencer handler;
    const TestURIResolver resolver;
    QNetworkAccessManager manager;

    // modify schema1
    schema1.setMessageHandler(&handler);
    schema1.setUriResolver(&resolver);
    schema1.setNetworkAccessManager(&manager);

    // check that schema2 is not effected by the modifications of schema1
    QVERIFY(schema2.messageHandler() != schema1.messageHandler());
    QVERIFY(schema2.uriResolver() != schema1.uriResolver());
    QVERIFY(schema2.networkAccessManager() != schema1.networkAccessManager());

    // modify schema1 further
    const QByteArray data( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                           "<xsd:schema"
                           "        xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\""
                           "        xmlns=\"http://www.qt-project.org/xmlschematest\""
                           "        targetNamespace=\"http://www.qt-project.org/xmlschematest\""
                           "        version=\"1.0\""
                           "        elementFormDefault=\"qualified\">"
                           "</xsd:schema>" );

    const QUrl documentUri("http://www.qt-project.org/xmlschematest");
    schema1.load(data, documentUri);

    QVERIFY(schema2.isValid() != schema1.isValid());
}

void tst_QXmlSchema::isValid() const
{
    /* Check default value. */
    QXmlSchema schema;
    QVERIFY(!schema.isValid());
}

void tst_QXmlSchema::documentUri() const
{
    const QByteArray data( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                           "<xsd:schema"
                           "        xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\""
                           "        xmlns=\"http://www.qt-project.org/xmlschematest\""
                           "        targetNamespace=\"http://www.qt-project.org/xmlschematest\""
                           "        version=\"1.0\""
                           "        elementFormDefault=\"qualified\">"
                           "</xsd:schema>" );

    const QUrl documentUri("http://www.qt-project.org/xmlschematest");
    QXmlSchema schema;
    schema.load(data, documentUri);

    QCOMPARE(documentUri, schema.documentUri());
}

void tst_QXmlSchema::loadSchemaUrlSuccess() const
{
/**
    TODO: put valid schema file on given url and enable test
    const QUrl url("http://notavailable/");

    QXmlSchema schema;
    QVERIFY(!schema.load(url));
*/
}

void tst_QXmlSchema::loadSchemaUrlFail() const
{
    const QUrl url("http://notavailable/");

    QXmlSchema schema;
    QVERIFY(!schema.load(url));
}

void tst_QXmlSchema::loadSchemaDeviceSuccess() const
{
    QByteArray data( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                     "<xsd:schema"
                     "        xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\""
                     "        xmlns=\"http://www.qt-project.org/xmlschematest\""
                     "        targetNamespace=\"http://www.qt-project.org/xmlschematest\""
                     "        version=\"1.0\""
                     "        elementFormDefault=\"qualified\">"
                     "</xsd:schema>" );

    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QXmlSchema schema;
    QVERIFY(schema.load(&buffer));
}

void tst_QXmlSchema::loadSchemaDeviceFail() const
{
    QByteArray data( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                     "<xsd:schema"
                     "        xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\""
                     "        xmlns=\"http://www.qt-project.org/xmlschematest\""
                     "        targetNamespace=\"http://www.qt-project.org/xmlschematest\""
                     "        version=\"1.0\""
                     "        elementFormDefault=\"qualified\">"
                     "</xsd:schema>" );

    QBuffer buffer(&data);
    // a closed device can not be loaded

    QXmlSchema schema;
    QVERIFY(!schema.load(&buffer));
}

void tst_QXmlSchema::loadSchemaDataSuccess() const
{
    const QByteArray data( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                           "<xsd:schema"
                           "        xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\""
                           "        xmlns=\"http://www.qt-project.org/xmlschematest\""
                           "        targetNamespace=\"http://www.qt-project.org/xmlschematest\""
                           "        version=\"1.0\""
                           "        elementFormDefault=\"qualified\">"
                           "</xsd:schema>" );
    QXmlSchema schema;
    QVERIFY(schema.load(data));
}

void tst_QXmlSchema::loadSchemaDataFail() const
{
    // empty schema can not be loaded
    const QByteArray data;

    QXmlSchema schema;
    QVERIFY(!schema.load(data));
}


void tst_QXmlSchema::networkAccessManagerSignature() const
{
    /* Const object. */
    const QXmlSchema schema;

    /* The function should be const. */
    schema.networkAccessManager();
}

void tst_QXmlSchema::networkAccessManagerDefaultValue() const
{
    /* Test that the default value of network access manager is not empty. */
    {
        QXmlSchema schema;
        QVERIFY(schema.networkAccessManager() != static_cast<QNetworkAccessManager*>(0));
    }
}

void tst_QXmlSchema::networkAccessManager() const
{
    /* Test that we return the network manager that was set. */
    {
        QNetworkAccessManager manager;
        QXmlSchema schema;
        schema.setNetworkAccessManager(&manager);
        QCOMPARE(schema.networkAccessManager(), &manager);
    }
}

void tst_QXmlSchema::messageHandlerSignature() const
{
    /* Const object. */
    const QXmlSchema schema;

    /* The function should be const. */
    schema.messageHandler();
}

void tst_QXmlSchema::messageHandlerDefaultValue() const
{
    /* Test that the default value of message handler is not empty. */
    {
        QXmlSchema schema;
        QVERIFY(schema.messageHandler() != static_cast<QAbstractMessageHandler*>(0));
    }
}

void tst_QXmlSchema::messageHandler() const
{
    /* Test that we return the message handler that was set. */
    {
        MessageSilencer handler;

        QXmlSchema schema;
        schema.setMessageHandler(&handler);
        QCOMPARE(schema.messageHandler(), static_cast<QAbstractMessageHandler *>(&handler));
    }
}

void tst_QXmlSchema::uriResolverSignature() const
{
    /* Const object. */
    const QXmlSchema schema;

    /* The function should be const. */
    schema.uriResolver();

    /* Const object. */
    const TestURIResolver resolver;

    /* This should compile */
    QXmlSchema schema2;
    schema2.setUriResolver(&resolver);
}

void tst_QXmlSchema::uriResolverDefaultValue() const
{
    /* Test that the default value of uri resolver is empty. */
    {
        QXmlSchema schema;
        QVERIFY(schema.uriResolver() == static_cast<QAbstractUriResolver*>(0));
    }
}

void tst_QXmlSchema::uriResolver() const
{
    /* Test that we return the uri resolver that was set. */
    {
        TestURIResolver resolver;

        QXmlSchema schema;
        schema.setUriResolver(&resolver);
        QCOMPARE(schema.uriResolver(), static_cast<const QAbstractUriResolver *>(&resolver));
    }
}

QTEST_MAIN(tst_QXmlSchema)

#include "tst_qxmlschema.moc"
