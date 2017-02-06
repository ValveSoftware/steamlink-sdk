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
#include <QtCore/QCoreApplication>
#include <QtCore/QScopedPointer>
#include <QtCore/QSettings>
#include <QtCore/QVariant>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>
#include "../../shared/util.h"

class tst_QQmlSettings : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void initTestCase();

    void init();
    void cleanup();

    void basic();
    void types();
    void aliases_data();
    void aliases();
    void categories();
    void siblings();
    void initial();
};

class CppObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int intProperty READ intProperty WRITE setIntProperty NOTIFY intPropertyChanged)
    Q_PROPERTY(bool boolProperty READ boolProperty WRITE setBoolProperty NOTIFY boolPropertyChanged)
    Q_PROPERTY(qreal realProperty READ realProperty WRITE setRealProperty NOTIFY realPropertyChanged)
    Q_PROPERTY(double doubleProperty READ doubleProperty WRITE setDoubleProperty NOTIFY doublePropertyChanged)
    Q_PROPERTY(QString stringProperty READ stringProperty WRITE setStringProperty NOTIFY stringPropertyChanged)
    Q_PROPERTY(QUrl urlProperty READ urlProperty WRITE setUrlProperty NOTIFY urlPropertyChanged)
    Q_PROPERTY(QVariant varProperty READ varProperty WRITE setVarProperty NOTIFY varPropertyChanged)
    Q_PROPERTY(QVariantList intListProperty READ intListProperty WRITE setIntListProperty NOTIFY intListPropertyChanged)
    Q_PROPERTY(QVariantList stringListProperty READ stringListProperty WRITE setStringListProperty NOTIFY stringListPropertyChanged)
    Q_PROPERTY(QDate dateProperty READ dateProperty WRITE setDateProperty NOTIFY datePropertyChanged)
    // QTBUG-32295: Q_PROPERTY(QTime timeProperty READ timeProperty WRITE setTimeProperty NOTIFY timePropertyChanged)
    Q_PROPERTY(QSizeF sizeProperty READ sizeProperty WRITE setSizeProperty NOTIFY sizePropertyChanged)
    Q_PROPERTY(QPointF pointProperty READ pointProperty WRITE setPointProperty NOTIFY pointPropertyChanged)
    Q_PROPERTY(QRectF rectProperty READ rectProperty WRITE setRectProperty NOTIFY rectPropertyChanged)
    Q_PROPERTY(QColor colorProperty READ colorProperty WRITE setColorProperty NOTIFY colorPropertyChanged)
    Q_PROPERTY(QFont fontProperty READ fontProperty WRITE setFontProperty NOTIFY fontPropertyChanged)

public:
    CppObject(QObject *parent = 0) : QObject(parent),
        m_intProperty(123),
        m_boolProperty(true),
        m_realProperty(1.23),
        m_doubleProperty(3.45),
        m_stringProperty("foo"),
        m_urlProperty("http://www.qt-project.org"),
        m_intListProperty(QVariantList() << 1 << 2 << 3),
        m_stringListProperty(QVariantList() << "a" << "b" << "c"),
        m_dateProperty(2000, 1, 2),
        // QTBUG-32295: m_timeProperty(12, 34, 56),
        m_sizeProperty(12, 34),
        m_pointProperty(12, 34),
        m_rectProperty(1, 2, 3, 4),
        m_colorProperty("red")
    {
    }

    int intProperty() const { return m_intProperty; }
    bool boolProperty() const { return m_boolProperty; }
    qreal realProperty() const { return m_realProperty; }
    double doubleProperty() const { return m_doubleProperty; }
    QString stringProperty() const { return m_stringProperty; }
    QUrl urlProperty() const { return m_urlProperty; }
    QVariant varProperty() const { return m_varProperty; }
    QVariantList intListProperty() const { return m_intListProperty; }
    QVariantList stringListProperty() const { return m_stringListProperty; }
    QDate dateProperty() const { return m_dateProperty; }
    QSizeF sizeProperty() const { return m_sizeProperty; }
    QPointF pointProperty() const { return m_pointProperty; }
    QRectF rectProperty() const { return m_rectProperty; }
    QColor colorProperty() const { return m_colorProperty; }
    QFont fontProperty() const { return m_fontProperty; }

public slots:
    void setIntProperty(int arg)
    {
        if (m_intProperty != arg) {
            m_intProperty = arg;
            emit intPropertyChanged(arg);
        }
    }

    void setBoolProperty(bool arg)
    {
        if (m_boolProperty != arg) {
            m_boolProperty = arg;
            emit boolPropertyChanged(arg);
        }
    }

    void setRealProperty(qreal arg)
    {
        if (m_realProperty != arg) {
            m_realProperty = arg;
            emit realPropertyChanged(arg);
        }
    }

    void setDoubleProperty(double arg)
    {
        if (m_doubleProperty != arg) {
            m_doubleProperty = arg;
            emit doublePropertyChanged(arg);
        }
    }

    void setStringProperty(const QString &arg)
    {
        if (m_stringProperty != arg) {
            m_stringProperty = arg;
            emit stringPropertyChanged(arg);
        }
    }

    void setUrlProperty(const QUrl &arg)
    {
        if (m_urlProperty != arg) {
            m_urlProperty = arg;
            emit urlPropertyChanged(arg);
        }
    }

    void setVarProperty(const QVariant &arg)
    {
        if (m_varProperty != arg) {
            m_varProperty = arg;
            emit varPropertyChanged(arg);
        }
    }

    void setIntListProperty(const QVariantList &arg)
    {
        if (m_intListProperty != arg) {
            m_intListProperty = arg;
            emit intListPropertyChanged(arg);
        }
    }

    void setStringListProperty(const QVariantList &arg)
    {
        if (m_stringListProperty != arg) {
            m_stringListProperty = arg;
            emit stringListPropertyChanged(arg);
        }
    }

    void setDateProperty(const QDate &arg)
    {
        if (m_dateProperty != arg) {
            m_dateProperty = arg;
            emit datePropertyChanged(arg);
        }
    }

    void setSizeProperty(const QSizeF &arg)
    {
        if (m_sizeProperty != arg) {
            m_sizeProperty = arg;
            emit sizePropertyChanged(arg);
        }
    }

    void setPointProperty(const QPointF &arg)
    {
        if (m_pointProperty != arg) {
            m_pointProperty = arg;
            emit pointPropertyChanged(arg);
        }
    }

    void setRectProperty(const QRectF &arg)
    {
        if (m_rectProperty != arg) {
            m_rectProperty = arg;
            emit rectPropertyChanged(arg);
        }
    }

    void setColorProperty(const QColor &arg)
    {
        if (m_colorProperty != arg) {
            m_colorProperty = arg;
            emit colorPropertyChanged(arg);
        }
    }

    void setFontProperty(const QFont &arg)
    {
        if (m_fontProperty != arg) {
            m_fontProperty = arg;
            emit fontPropertyChanged(arg);
        }
    }

signals:
    void intPropertyChanged(int arg);
    void boolPropertyChanged(bool arg);
    void realPropertyChanged(qreal arg);
    void doublePropertyChanged(double arg);
    void stringPropertyChanged(const QString &arg);
    void urlPropertyChanged(const QUrl &arg);
    void varPropertyChanged(const QVariant &arg);
    void intListPropertyChanged(const QVariantList &arg);
    void stringListPropertyChanged(const QVariantList &arg);
    void datePropertyChanged(const QDate &arg);
    void sizePropertyChanged(const QSizeF &arg);
    void pointPropertyChanged(const QPointF &arg);
    void rectPropertyChanged(const QRectF &arg);
    void colorPropertyChanged(const QColor &arg);
    void fontPropertyChanged(const QFont &arg);

private:
    int m_intProperty;
    bool m_boolProperty;
    qreal m_realProperty;
    double m_doubleProperty;
    QString m_stringProperty;
    QUrl m_urlProperty;
    QVariant m_varProperty;
    QVariantList m_intListProperty;
    QVariantList m_stringListProperty;
    QDate m_dateProperty;
    QSizeF m_sizeProperty;
    QPointF m_pointProperty;
    QRectF m_rectProperty;
    QColor m_colorProperty;
    QFont m_fontProperty;
};

void tst_QQmlSettings::initTestCase()
{
    QQmlDataTest::initTestCase();

    QCoreApplication::setApplicationName("tst_QQmlSettings");
    QCoreApplication::setOrganizationName("QtProject");
    QCoreApplication::setOrganizationDomain("qt-project.org");

    qmlRegisterType<CppObject>("Qt.test", 1, 0, "CppObject");
}

void tst_QQmlSettings::init()
{
    QSettings settings;
    settings.clear();
}

void tst_QQmlSettings::cleanup()
{
    QSettings settings;
    settings.clear();
}

void tst_QQmlSettings::basic()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("basic.qml"));
    QScopedPointer<QObject> root(component.create());
    QVERIFY(root.data());
    QVERIFY(root->property("success").toBool());
    QSettings settings;
    QTRY_VERIFY(settings.value("success").toBool());
}

void tst_QQmlSettings::types()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("types.qml"));
    QScopedPointer<QObject> root(component.create());
    QVERIFY(root.data());
    QObject *settings = root->property("settings").value<QObject *>();
    QVERIFY(settings);

    // default property values
    QCOMPARE(root->property("intProperty").toInt(), 0);
    QCOMPARE(root->property("boolProperty").toBool(), false);
    QCOMPARE(root->property("realProperty").toReal(), static_cast<qreal>(0.0));
    QCOMPARE(root->property("doubleProperty").toDouble(), static_cast<double>(0.0));
    QCOMPARE(root->property("stringProperty").toString(), QString());
    QCOMPARE(root->property("urlProperty").toUrl(), QUrl());
    QCOMPARE(root->property("intListProperty").toList(), QVariantList());
    QCOMPARE(root->property("stringListProperty").toList(), QVariantList());
    QCOMPARE(root->property("dateProperty").toDate(), QDate());
    // QTBUG-32295: QCOMPARE(root->property("timeProperty").toDate(), QTime());
    QCOMPARE(root->property("sizeProperty").toSizeF(), QSizeF());
    QCOMPARE(root->property("pointProperty").toPointF(), QPointF());
    QCOMPARE(root->property("rectProperty").toRectF(), QRectF());
    QCOMPARE(root->property("colorProperty").value<QColor>(), QColor());
    QCOMPARE(root->property("fontProperty").value<QFont>(), QFont());

    // default settings values
    QCOMPARE(settings->property("intProperty").toInt(), 123);
    QCOMPARE(settings->property("boolProperty").toBool(), true);
    QCOMPARE(settings->property("realProperty").toReal(), static_cast<qreal>(1.23));
    QCOMPARE(settings->property("doubleProperty").toDouble(), static_cast<double>(3.45));
    QCOMPARE(settings->property("stringProperty").toString(), QStringLiteral("foo"));
    QCOMPARE(settings->property("urlProperty").toUrl(), QUrl("http://www.qt-project.org"));
    QCOMPARE(settings->property("intListProperty").toList(), QVariantList() << 1 << 2 << 3);
    QCOMPARE(settings->property("stringListProperty").toList(), QVariantList() << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("c"));
    QCOMPARE(settings->property("dateProperty").toDate(), QDate(2000, 01, 02));
    // QTBUG-32295: QCOMPARE(settings->property("timeProperty").toDate(), QTime(12, 34, 56));
    QCOMPARE(settings->property("sizeProperty").toSizeF(), QSizeF(12, 34));
    QCOMPARE(settings->property("pointProperty").toPointF(), QPointF(12, 34));
    QCOMPARE(settings->property("rectProperty").toRectF(), QRectF(1, 2, 3, 4));
    QCOMPARE(settings->property("colorProperty").value<QColor>(), QColor(Qt::red));
    QCOMPARE(settings->property("fontProperty").value<QFont>(), QFont());

    // read settings
    QVERIFY(QMetaObject::invokeMethod(root.data(), "readSettings"));
    QCOMPARE(root->property("intProperty").toInt(), 123);
    QCOMPARE(root->property("boolProperty").toBool(), true);
    QCOMPARE(root->property("realProperty").toReal(), static_cast<qreal>(1.23));
    QCOMPARE(root->property("doubleProperty").toDouble(), static_cast<double>(3.45));
    QCOMPARE(root->property("stringProperty").toString(), QStringLiteral("foo"));
    QCOMPARE(root->property("urlProperty").toUrl(), QUrl("http://www.qt-project.org"));
    QCOMPARE(root->property("intListProperty").toList(), QVariantList() << 1 << 2 << 3);
    QCOMPARE(root->property("stringListProperty").toList(), QVariantList() << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("c"));
    QCOMPARE(root->property("dateProperty").toDate(), QDate(2000, 01, 02));
    // QTBUG-32295: QCOMPARE(root->property("timeProperty").toDate(), QTime(12, 34, 56));
    QCOMPARE(root->property("sizeProperty").toSizeF(), QSizeF(12, 34));
    QCOMPARE(root->property("pointProperty").toPointF(), QPointF(12, 34));
    QCOMPARE(root->property("rectProperty").toRectF(), QRectF(1, 2, 3, 4));
    QCOMPARE(root->property("colorProperty").value<QColor>(), QColor(Qt::red));
    QCOMPARE(root->property("fontProperty").value<QFont>(), QFont());

    // change properties
    QVERIFY(root->setProperty("intProperty", 456));
    QVERIFY(root->setProperty("boolProperty", false));
    QVERIFY(root->setProperty("realProperty", static_cast<qreal>(4.56)));
    QVERIFY(root->setProperty("doubleProperty", static_cast<double>(6.78)));
    QVERIFY(root->setProperty("stringProperty", QStringLiteral("bar")));
    QVERIFY(root->setProperty("urlProperty", QUrl("https://codereview.qt-project.org")));
    QVERIFY(root->setProperty("intListProperty", QVariantList() << 4 << 5 << 6));
    QVERIFY(root->setProperty("stringListProperty", QVariantList() << QStringLiteral("d") << QStringLiteral("e") << QStringLiteral("f")));
    QVERIFY(root->setProperty("dateProperty", QDate(2010, 02, 01)));
    // QTBUG-32295: QVERIFY(root->setProperty("timeProperty", QTime(6, 56, 34)));
    QVERIFY(root->setProperty("sizeProperty", QSizeF(56, 78)));
    QVERIFY(root->setProperty("pointProperty", QPointF(56, 78)));
    QVERIFY(root->setProperty("rectProperty", QRectF(5, 6, 7, 8)));
    QVERIFY(root->setProperty("colorProperty", QColor(Qt::blue)));
    QFont boldFont; boldFont.setBold(true);
    QVERIFY(root->setProperty("fontProperty", boldFont));

    // write settings
    QVERIFY(QMetaObject::invokeMethod(root.data(), "writeSettings"));
    QTRY_COMPARE(settings->property("intProperty").toInt(), 456);
    QTRY_COMPARE(settings->property("boolProperty").toBool(), false);
    QTRY_COMPARE(settings->property("realProperty").toReal(), static_cast<qreal>(4.56));
    QTRY_COMPARE(settings->property("doubleProperty").toDouble(), static_cast<double>(6.78));
    QTRY_COMPARE(settings->property("stringProperty").toString(), QStringLiteral("bar"));
    QTRY_COMPARE(settings->property("urlProperty").toUrl(), QUrl("https://codereview.qt-project.org"));
    QTRY_COMPARE(settings->property("intListProperty").toList(), QVariantList() << 4 << 5 << 6);
    QTRY_COMPARE(settings->property("stringListProperty").toList(), QVariantList() << QStringLiteral("d") << QStringLiteral("e") << QStringLiteral("f"));
    QTRY_COMPARE(settings->property("dateProperty").toDate(), QDate(2010, 02, 01));
    // QTBUG-32295: QTRY_COMPARE(settings->property("timeProperty").toDate(), QTime(6, 56, 34));
    QTRY_COMPARE(settings->property("sizeProperty").toSizeF(), QSizeF(56, 78));
    QTRY_COMPARE(settings->property("pointProperty").toPointF(), QPointF(56, 78));
    QTRY_COMPARE(settings->property("rectProperty").toRectF(), QRectF(5, 6, 7, 8));
    QTRY_COMPARE(settings->property("colorProperty").value<QColor>(), QColor(Qt::blue));
    QTRY_COMPARE(settings->property("fontProperty").value<QFont>(), boldFont);

    QSettings qs;
    QTRY_COMPARE(qs.value("intProperty").toInt(), 456);
    QTRY_COMPARE(qs.value("boolProperty").toBool(), false);
    QTRY_COMPARE(qs.value("realProperty").toReal(), static_cast<qreal>(4.56));
    QTRY_COMPARE(qs.value("doubleProperty").toDouble(), static_cast<double>(6.78));
    QTRY_COMPARE(qs.value("stringProperty").toString(), QStringLiteral("bar"));
    QTRY_COMPARE(qs.value("urlProperty").toUrl(), QUrl("https://codereview.qt-project.org"));
    QTRY_COMPARE(qs.value("intListProperty").toList(), QVariantList() << 4 << 5 << 6);
    QTRY_COMPARE(qs.value("stringListProperty").toList(), QVariantList() << QStringLiteral("d") << QStringLiteral("e") << QStringLiteral("f"));
    QTRY_COMPARE(qs.value("dateProperty").toDate(), QDate(2010, 02, 01));
    // QTBUG-32295: QTRY_COMPARE(qs.value("timeProperty").toDate(), QTime(6, 56, 34));
    QTRY_COMPARE(qs.value("sizeProperty").toSizeF(), QSizeF(56, 78));
    QTRY_COMPARE(qs.value("pointProperty").toPointF(), QPointF(56, 78));
    QTRY_COMPARE(qs.value("rectProperty").toRectF(), QRectF(5, 6, 7, 8));
    QTRY_COMPARE(qs.value("colorProperty").value<QColor>(), QColor(Qt::blue));
    QTRY_COMPARE(qs.value("fontProperty").value<QFont>(), boldFont);
}

void tst_QQmlSettings::aliases_data()
{
    QTest::addColumn<QString>("testFile");
    QTest::newRow("qml") << "aliases.qml";
    QTest::newRow("cpp") << "cpp-aliases.qml";
}

void tst_QQmlSettings::aliases()
{
    QFETCH(QString, testFile);

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl(testFile));
    QScopedPointer<QObject> root(component.create());
    QVERIFY(root.data());
    QObject *settings = root->property("settings").value<QObject *>();
    QVERIFY(settings);

    // default property values
    QCOMPARE(root->property("intProperty").toInt(), 123);
    QCOMPARE(root->property("boolProperty").toBool(), true);
    QCOMPARE(root->property("realProperty").toReal(), static_cast<qreal>(1.23));
    QCOMPARE(root->property("doubleProperty").toDouble(), static_cast<double>(3.45));
    QCOMPARE(root->property("stringProperty").toString(), QStringLiteral("foo"));
    QCOMPARE(root->property("urlProperty").toUrl(), QUrl("http://www.qt-project.org"));
    QCOMPARE(root->property("intListProperty").toList(), QVariantList() << 1 << 2 << 3);
    QCOMPARE(root->property("stringListProperty").toList(), QVariantList() << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("c"));
    QCOMPARE(root->property("dateProperty").toDate(), QDate(2000, 01, 02));
    // QTBUG-32295: QCOMPARE(root->property("timeProperty").toDate(), QTime(12, 34, 56));
    QCOMPARE(root->property("sizeProperty").toSizeF(), QSizeF(12, 34));
    QCOMPARE(root->property("pointProperty").toPointF(), QPointF(12, 34));
    QCOMPARE(root->property("rectProperty").toRectF(), QRectF(1, 2, 3, 4));
    QCOMPARE(root->property("colorProperty").value<QColor>(), QColor(Qt::red));
    QCOMPARE(root->property("fontProperty").value<QFont>(), QFont());

    // default settings values
    QCOMPARE(settings->property("intProperty").toInt(), 123);
    QCOMPARE(settings->property("boolProperty").toBool(), true);
    QCOMPARE(settings->property("realProperty").toReal(), static_cast<qreal>(1.23));
    QCOMPARE(settings->property("doubleProperty").toDouble(), static_cast<double>(3.45));
    QCOMPARE(settings->property("stringProperty").toString(), QStringLiteral("foo"));
    QCOMPARE(settings->property("urlProperty").toUrl(), QUrl("http://www.qt-project.org"));
    QCOMPARE(settings->property("intListProperty").toList(), QVariantList() << 1 << 2 << 3);
    QCOMPARE(settings->property("stringListProperty").toList(), QVariantList() << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("c"));
    QCOMPARE(settings->property("dateProperty").toDate(), QDate(2000, 01, 02));
    // QTBUG-32295: QCOMPARE(settings->property("timeProperty").toDate(), QTime(12, 34, 56));
    QCOMPARE(settings->property("sizeProperty").toSizeF(), QSizeF(12, 34));
    QCOMPARE(settings->property("pointProperty").toPointF(), QPointF(12, 34));
    QCOMPARE(settings->property("rectProperty").toRectF(), QRectF(1, 2, 3, 4));
    QCOMPARE(settings->property("colorProperty").value<QColor>(), QColor(Qt::red));
    QCOMPARE(settings->property("fontProperty").value<QFont>(), QFont());

    // change settings
    QVERIFY(settings->setProperty("intProperty", 456));
    QVERIFY(settings->setProperty("boolProperty", false));
    QVERIFY(settings->setProperty("realProperty", static_cast<qreal>(4.56)));
    QVERIFY(settings->setProperty("doubleProperty", static_cast<double>(6.78)));
    QVERIFY(settings->setProperty("stringProperty", QStringLiteral("bar")));
    QVERIFY(settings->setProperty("urlProperty", QUrl("https://codereview.qt-project.org")));
    QVERIFY(settings->setProperty("intListProperty", QVariantList() << 4 << 5 << 6));
    QVERIFY(settings->setProperty("stringListProperty", QVariantList() << QStringLiteral("d") << QStringLiteral("e") << QStringLiteral("f")));
    QVERIFY(settings->setProperty("dateProperty", QDate(2010, 02, 01)));
    // QTBUG-32295: QVERIFY(settings->setProperty("timeProperty", QTime(6, 56, 34)));
    QVERIFY(settings->setProperty("sizeProperty", QSizeF(56, 78)));
    QVERIFY(settings->setProperty("pointProperty", QPointF(56, 78)));
    QVERIFY(settings->setProperty("rectProperty", QRectF(5, 6, 7, 8)));
    QVERIFY(settings->setProperty("colorProperty", QColor(Qt::blue)));
    QFont boldFont; boldFont.setBold(true);
    QVERIFY(settings->setProperty("fontProperty", boldFont));

    QSettings qs;
    QTRY_COMPARE(qs.value("intProperty").toInt(), 456);
    QTRY_COMPARE(qs.value("boolProperty").toBool(), false);
    QTRY_COMPARE(qs.value("realProperty").toReal(), static_cast<qreal>(4.56));
    QTRY_COMPARE(qs.value("doubleProperty").toDouble(), static_cast<double>(6.78));
    QTRY_COMPARE(qs.value("stringProperty").toString(), QStringLiteral("bar"));
    QTRY_COMPARE(qs.value("urlProperty").toUrl(), QUrl("https://codereview.qt-project.org"));
    QTRY_COMPARE(qs.value("intListProperty").toList(), QVariantList() << 4 << 5 << 6);
    QTRY_COMPARE(qs.value("stringListProperty").toList(), QVariantList() << QStringLiteral("d") << QStringLiteral("e") << QStringLiteral("f"));
    QTRY_COMPARE(qs.value("dateProperty").toDate(), QDate(2010, 02, 01));
    // QTBUG-32295: QTRY_COMPARE(qs.value("timeProperty").toDate(), QTime(6, 56, 34));
    QTRY_COMPARE(qs.value("sizeProperty").toSizeF(), QSizeF(56, 78));
    QTRY_COMPARE(qs.value("pointProperty").toPointF(), QPointF(56, 78));
    QTRY_COMPARE(qs.value("rectProperty").toRectF(), QRectF(5, 6, 7, 8));
    QTRY_COMPARE(qs.value("colorProperty").value<QColor>(), QColor(Qt::blue));
    QTRY_COMPARE(qs.value("fontProperty").value<QFont>(), boldFont);
}

void tst_QQmlSettings::categories()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("categories.qml"));

    {
        // initial state
        QSettings qs;
        QVERIFY(!qs.childGroups().contains("initialCategory"));
        QVERIFY(!qs.childGroups().contains("changedCategory"));
        QVERIFY(!qs.childKeys().contains("value"));
    }

    {
        // load & write default values
        QScopedPointer<QObject> settings(component.create());
        QVERIFY(settings.data());
        QCOMPARE(settings->property("category").toString(), QStringLiteral("initialCategory"));
        QCOMPARE(settings->property("value").toString(), QStringLiteral("initialValue"));
    }

    {
        // verify written settings & change the value
        QSettings qs;
        QVERIFY(qs.childGroups().contains("initialCategory"));
        qs.beginGroup("initialCategory");
        QVERIFY(qs.childKeys().contains("value"));
        QCOMPARE(qs.value("value").toString(), QStringLiteral("initialValue"));
        qs.setValue("value", QStringLiteral("changedValue"));
    }

    {
        // load changed value & change the category
        QScopedPointer<QObject> settings(component.create());
        QVERIFY(settings.data());
        QCOMPARE(settings->property("category").toString(), QStringLiteral("initialCategory"));
        QCOMPARE(settings->property("value").toString(), QStringLiteral("changedValue"));
        QVERIFY(settings->setProperty("category", QStringLiteral("changedCategory")));
    }

    {
        // verify written settings
        QSettings qs;
        QVERIFY(qs.childGroups().contains("changedCategory"));
        qs.beginGroup("changedCategory");
        QVERIFY(qs.childKeys().contains("value"));
        QCOMPARE(qs.value("value").toString(), QStringLiteral("changedValue"));
        qs.setValue("value", QStringLiteral("changedValue"));
        qs.endGroup();
    }
}

void tst_QQmlSettings::siblings()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("siblings.qml"));
    delete component.create();

    // verify setting aliases to destructed siblings
    QSettings settings;
    QCOMPARE(settings.value("alias1").toString(), QStringLiteral("value1"));
    QCOMPARE(settings.value("alias2").toString(), QStringLiteral("value2"));
}

void tst_QQmlSettings::initial()
{
    QSettings qs;
    qs.setValue("value", QStringLiteral("initial"));
    qs.sync();

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import Qt.labs.settings 1.0; Settings { property var value }", QUrl());
    QScopedPointer<QObject> settings(component.create());
    QVERIFY(settings.data());

    // verify that the initial value from QSettings gets properly loaded
    // even if no initial value is set in QML
    QCOMPARE(settings->property("value").toString(), QStringLiteral("initial"));
}

QTEST_MAIN(tst_QQmlSettings)

#include "tst_qqmlsettings.moc"
