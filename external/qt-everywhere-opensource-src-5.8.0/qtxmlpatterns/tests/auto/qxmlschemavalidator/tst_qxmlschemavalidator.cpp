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
#include <QXmlSchemaValidator>

#include "../qabstracturiresolver/TestURIResolver.h"
#include "../qxmlquery/MessageSilencer.h"

/*!
 \class tst_QXmlSchemaValidatorValidator
 \internal
 \brief Tests class QXmlSchemaValidator.

 This test is not intended for testing the engine, but the functionality specific
 to the QXmlSchemaValidator class.
 */
class tst_QXmlSchemaValidator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultConstructor() const;
    void constructorQXmlNamePool() const;
    void propertyInitialization() const;
    void resetSchemaNamePool() const;

    void loadInstanceUrlSuccess() const;
    void loadInstanceUrlFail() const;
    void loadInstanceDeviceSuccess() const;
    void loadInstanceDeviceFail() const;
    void loadInstanceDataSuccess() const;
    void loadInstanceDataFail() const;

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

static QXmlSchema createValidSchema()
{
    const QByteArray data( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                           "<xsd:schema"
                           "        xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\""
                           "        xmlns=\"http://www.qt-project.org/xmlschematest\""
                           "        targetNamespace=\"http://www.qt-project.org/xmlschematest\""
                           "        version=\"1.0\""
                           "        elementFormDefault=\"qualified\">"
                           "  <xsd:element name=\"myRoot\" type=\"xsd:string\"/>"
                           "</xsd:schema>" );

    const QUrl documentUri("http://www.qt-project.org/xmlschematest");

    QXmlSchema schema;
    schema.load(data, documentUri);

    return schema;
}

void tst_QXmlSchemaValidator::defaultConstructor() const
{
    /* Allocate instance in different orders. */
    {
        QXmlSchema schema;
        QXmlSchemaValidator validator(schema);
    }

    {
        QXmlSchema schema1;
        QXmlSchema schema2;

        QXmlSchemaValidator validator1(schema1);
        QXmlSchemaValidator validator2(schema2);
    }

    {
        QXmlSchema schema;

        QXmlSchemaValidator validator1(schema);
        QXmlSchemaValidator validator2(schema);
    }
}

void tst_QXmlSchemaValidator::propertyInitialization() const
{
    /* Verify that properties set in the schema are used as default values for the validator */
    {
        MessageSilencer handler;
        TestURIResolver resolver;
        QNetworkAccessManager manager;

        QXmlSchema schema;
        schema.setMessageHandler(&handler);
        schema.setUriResolver(&resolver);
        schema.setNetworkAccessManager(&manager);

        QXmlSchemaValidator validator(schema);
        QCOMPARE(validator.messageHandler(), static_cast<QAbstractMessageHandler *>(&handler));
        QCOMPARE(validator.uriResolver(), static_cast<const QAbstractUriResolver *>(&resolver));
        QCOMPARE(validator.networkAccessManager(), &manager);
    }
}

void tst_QXmlSchemaValidator::constructorQXmlNamePool() const
{
    // test that the name pool from the schema is used by
    // the schema validator as well
    QXmlSchema schema;

    QXmlNamePool np = schema.namePool();

    const QXmlName name(np, QLatin1String("localName"),
                            QLatin1String("http://example.com/"),
                            QLatin1String("prefix"));

    QXmlSchemaValidator validator(schema);

    QXmlNamePool np2(validator.namePool());
    QCOMPARE(name.namespaceUri(np2), QString::fromLatin1("http://example.com/"));
    QCOMPARE(name.localName(np2), QString::fromLatin1("localName"));
    QCOMPARE(name.prefix(np2), QString::fromLatin1("prefix"));

    // make sure namePool() is const
    const QXmlSchemaValidator constValidator(schema);
    np = constValidator.namePool();
}

void tst_QXmlSchemaValidator::resetSchemaNamePool() const
{
    QXmlSchema schema1;
    QXmlNamePool np1 = schema1.namePool();

    const QXmlName name1(np1, QLatin1String("localName"),
                              QLatin1String("http://example.com/"),
                              QLatin1String("prefix"));

    QXmlSchemaValidator validator(schema1);

    {
        QXmlNamePool compNamePool(validator.namePool());
        QCOMPARE(name1.namespaceUri(compNamePool), QString::fromLatin1("http://example.com/"));
        QCOMPARE(name1.localName(compNamePool), QString::fromLatin1("localName"));
        QCOMPARE(name1.prefix(compNamePool), QString::fromLatin1("prefix"));
    }

    QXmlSchema schema2;
    QXmlNamePool np2 = schema2.namePool();

    const QXmlName name2(np2, QLatin1String("remoteName"),
                              QLatin1String("http://example.com/"),
                              QLatin1String("suffix"));

    // make sure that after re-setting the schema, the new namepool is used
    validator.setSchema(schema2);

    {
        QXmlNamePool compNamePool(validator.namePool());
        QCOMPARE(name2.namespaceUri(compNamePool), QString::fromLatin1("http://example.com/"));
        QCOMPARE(name2.localName(compNamePool), QString::fromLatin1("remoteName"));
        QCOMPARE(name2.prefix(compNamePool), QString::fromLatin1("suffix"));
    }
}

void tst_QXmlSchemaValidator::loadInstanceUrlSuccess() const
{
/*
    TODO: put valid schema file on given url and enable test
    const QXmlSchema schema(createValidSchema());
    const QUrl url("http://notavailable/");

    QXmlSchemaValidator validator(schema);
    QVERIFY(!validator.validate(url));
*/
}

void tst_QXmlSchemaValidator::loadInstanceUrlFail() const
{
    const QXmlSchema schema(createValidSchema());
    const QUrl url("http://notavailable/");

    QXmlSchemaValidator validator(schema);
    QVERIFY(!validator.validate(url));
}

void tst_QXmlSchemaValidator::loadInstanceDeviceSuccess() const
{
    const QXmlSchema schema(createValidSchema());

    QByteArray data( "<myRoot xmlns=\"http://www.qt-project.org/xmlschematest\">Testme</myRoot>" );
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QXmlSchemaValidator validator(schema);
    QVERIFY(validator.validate(&buffer));
}

void tst_QXmlSchemaValidator::loadInstanceDeviceFail() const
{
    const QXmlSchema schema(createValidSchema());

    QByteArray data( "<myRoot xmlns=\"http://www.qt-project.org/xmlschematest\">Testme</myRoot>" );
    QBuffer buffer(&data);
    // a closed device can not be loaded

    QXmlSchemaValidator validator(schema);
    QVERIFY(!validator.validate(&buffer));
}

void tst_QXmlSchemaValidator::loadInstanceDataSuccess() const
{
    const QXmlSchema schema(createValidSchema());

    const QByteArray data( "<myRoot xmlns=\"http://www.qt-project.org/xmlschematest\">Testme</myRoot>" );

    QXmlSchemaValidator validator(schema);
    QVERIFY(validator.validate(data));
}

void tst_QXmlSchemaValidator::loadInstanceDataFail() const
{
    const QXmlSchema schema(createValidSchema());

    // empty instance can not be loaded
    const QByteArray data;

    QXmlSchemaValidator validator(schema);
    QVERIFY(!validator.validate(data));
}

void tst_QXmlSchemaValidator::networkAccessManagerSignature() const
{
    const QXmlSchema schema;

    /* Const object. */
    const QXmlSchemaValidator validator(schema);

    /* The function should be const. */
    validator.networkAccessManager();
}

void tst_QXmlSchemaValidator::networkAccessManagerDefaultValue() const
{
    /* Test that the default value of network access manager is equal to the one from the schema. */
    {
        const QXmlSchema schema;
        const QXmlSchemaValidator validator(schema);
        QVERIFY(validator.networkAccessManager() == schema.networkAccessManager());
    }

    /* Test that the default value of network access manager is equal to the one from the schema. */
    {
        QXmlSchema schema;

        QNetworkAccessManager manager;
        schema.setNetworkAccessManager(&manager);

        const QXmlSchemaValidator validator(schema);
        QVERIFY(validator.networkAccessManager() == schema.networkAccessManager());
    }
}

void tst_QXmlSchemaValidator::networkAccessManager() const
{
    /* Test that we return the network access manager that was set. */
    {
        QNetworkAccessManager manager;

        const QXmlSchema schema;
        QXmlSchemaValidator validator(schema);

        validator.setNetworkAccessManager(&manager);
        QCOMPARE(validator.networkAccessManager(), &manager);
    }

    /* Test that we return the network access manager that was set, even if the schema changed in between. */
    {
        QNetworkAccessManager manager;

        const QXmlSchema schema;
        QXmlSchemaValidator validator(schema);

        validator.setNetworkAccessManager(&manager);

        const QXmlSchema schema2;
        validator.setSchema(schema2);

        QCOMPARE(validator.networkAccessManager(), &manager);
    }
}

void tst_QXmlSchemaValidator::messageHandlerSignature() const
{
    const QXmlSchema schema;

    /* Const object. */
    const QXmlSchemaValidator validator(schema);

    /* The function should be const. */
    validator.messageHandler();
}

void tst_QXmlSchemaValidator::messageHandlerDefaultValue() const
{
    /* Test that the default value of message handler is equal to the one from the schema. */
    {
        const QXmlSchema schema;
        const QXmlSchemaValidator validator(schema);
        QVERIFY(validator.messageHandler() == schema.messageHandler());
    }

    /* Test that the default value of network access manager is equal to the one from the schema. */
    {
        QXmlSchema schema;

        MessageSilencer handler;
        schema.setMessageHandler(&handler);

        const QXmlSchemaValidator validator(schema);
        QVERIFY(validator.messageHandler() == schema.messageHandler());
    }
}

void tst_QXmlSchemaValidator::messageHandler() const
{
    /* Test that we return the message handler that was set. */
    {
        MessageSilencer handler;

        const QXmlSchema schema;
        QXmlSchemaValidator validator(schema);

        validator.setMessageHandler(&handler);
        QCOMPARE(validator.messageHandler(), static_cast<QAbstractMessageHandler *>(&handler));
    }

    /* Test that we return the message handler that was set, even if the schema changed in between. */
    {
        MessageSilencer handler;

        const QXmlSchema schema;
        QXmlSchemaValidator validator(schema);

        validator.setMessageHandler(&handler);

        const QXmlSchema schema2;
        validator.setSchema(schema2);

        QCOMPARE(validator.messageHandler(), static_cast<QAbstractMessageHandler *>(&handler));
    }
}

void tst_QXmlSchemaValidator::uriResolverSignature() const
{
    const QXmlSchema schema;

    /* Const object. */
    const QXmlSchemaValidator validator(schema);

    /* The function should be const. */
    validator.uriResolver();

    /* Const object. */
    const TestURIResolver resolver;

    /* This should compile */
    QXmlSchema schema2;
    schema2.setUriResolver(&resolver);
}

void tst_QXmlSchemaValidator::uriResolverDefaultValue() const
{
    /* Test that the default value of uri resolver is equal to the one from the schema. */
    {
        const QXmlSchema schema;
        const QXmlSchemaValidator validator(schema);
        QVERIFY(validator.uriResolver() == schema.uriResolver());
    }

    /* Test that the default value of uri resolver is equal to the one from the schema. */
    {
        QXmlSchema schema;

        TestURIResolver resolver;
        schema.setUriResolver(&resolver);

        const QXmlSchemaValidator validator(schema);
        QVERIFY(validator.uriResolver() == schema.uriResolver());
    }
}

void tst_QXmlSchemaValidator::uriResolver() const
{
    /* Test that we return the uri resolver that was set. */
    {
        TestURIResolver resolver;

        const QXmlSchema schema;
        QXmlSchemaValidator validator(schema);

        validator.setUriResolver(&resolver);
        QCOMPARE(validator.uriResolver(), static_cast<const QAbstractUriResolver *>(&resolver));
    }

    /* Test that we return the uri resolver that was set, even if the schema changed in between. */
    {
        TestURIResolver resolver;

        const QXmlSchema schema;
        QXmlSchemaValidator validator(schema);

        validator.setUriResolver(&resolver);

        const QXmlSchema schema2;
        validator.setSchema(schema2);

        QCOMPARE(validator.uriResolver(), static_cast<const QAbstractUriResolver *>(&resolver));
    }
}

QTEST_MAIN(tst_QXmlSchemaValidator)

#include "tst_qxmlschemavalidator.moc"
