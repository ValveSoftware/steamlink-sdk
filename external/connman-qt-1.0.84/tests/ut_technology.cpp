#include <QtCore/QPointer>

#include "../libconnman-qt/networktechnology.h"
#include "testbase.h"

namespace Tests {

class UtTechnology : public TestBase
{
    Q_OBJECT

public:
    class ManagerMock;
    class TechnologyMock;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testProperties_data();
    void testProperties();
    void testWriteProperties_data();
    void testWriteProperties();
    void testScan();
    void testSetPath();
    void testPropertiesAfterSetPath_data();
    void testPropertiesAfterSetPath();

private:
    QPointer<NetworkTechnology> m_technology;
    // Second instance used to verify change is really propagated through connman
    QPointer<NetworkTechnology> m_otherTechnology;

};

class UtTechnology::ManagerMock : public MainObjectMock
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Manager")

public:
    ManagerMock();
};

class UtTechnology::TechnologyMock : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Technology")

public:
    TechnologyMock(const QVariantMap &properties, ManagerMock *manager);

public:
    Q_SCRIPTABLE QVariantMap GetProperties() const;
    Q_SCRIPTABLE void SetProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message);
    Q_SCRIPTABLE void Scan();

signals:
    Q_SCRIPTABLE void PropertyChanged(const QString &name, const QDBusVariant &value);

private:
    QVariantMap m_properties;
};

} // namespace Tests

using namespace Tests;

/*
 * \class Tests::UtTechnology
 */

void UtTechnology::initTestCase()
{
    QVERIFY(waitForService("net.connman", "/", "net.connman.Manager"));
    m_technology = new NetworkTechnology("/technology0", QVariantMap(), this);
    m_otherTechnology = new NetworkTechnology("/technology0", QVariantMap(), this);
}

void UtTechnology::cleanupTestCase()
{
    delete m_technology;
    delete m_otherTechnology;
}

void UtTechnology::testProperties_data()
{
    QTest::addColumn<QVariant>("expected");

    createTestPropertyData(defaultTechnologyProperties(), technologyDBusProperty2QtProperty);
    QTest::newRow("path") << QVariant("/technology0");
}

void UtTechnology::testProperties()
{
    QFETCH(QVariant, expected);

    testProperty(*m_technology, QTest::currentDataTag(), expected);
}

void UtTechnology::testWriteProperties_data()
{
    QTest::addColumn<QVariant>("newValue");

    QTest::newRow("powered") << QVariant(false);
    // setting 'path' is different - will not test it here
    QTest::newRow("idleTimeout") << QVariant(24);
    QTest::newRow("tethering") << QVariant(true);
    QTest::newRow("tetheringId") << QVariant("foo-tether-new-identifier");
    QTest::newRow("tetheringPassphrase") << QVariant("foo-tether-new-passwd");
}

void UtTechnology::testWriteProperties()
{
    QFETCH(QVariant, newValue);

    testWriteProperty(m_technology, m_otherTechnology, QTest::currentDataTag(), newValue);
}

void UtTechnology::testScan()
{
    m_technology->scan();
    QVERIFY(waitForSignal(m_technology, SIGNAL(scanFinished())));
}

void UtTechnology::testSetPath()
{
    m_technology->setPath("/technology1");
    QCOMPARE(m_technology->objPath(), QString("/technology1"));
}

void UtTechnology::testPropertiesAfterSetPath_data()
{
    QTest::addColumn<QVariant>("expected");

    createTestPropertyData(alternateDefaultTechnologyProperties(),
            technologyDBusProperty2QtProperty);
    QTest::newRow("path") << QVariant("/technology1");
}

void UtTechnology::testPropertiesAfterSetPath()
{
    QFETCH(QVariant, expected);

    testProperty(*m_technology, QTest::currentDataTag(), expected);
}

/*
 * \class Tests::UtTechnology::ManagerMock
 */

UtTechnology::ManagerMock::ManagerMock()
    : MainObjectMock("net.connman", "/")
{
    TechnologyMock *const technology0 = new TechnologyMock(defaultTechnologyProperties(), this);
    if (!bus().registerObject("/technology0", technology0,
                QDBusConnection::ExportScriptableContents)) {
        qFatal("Failed to register /technology0: %s", qPrintable(bus().lastError().message()));
    }

    TechnologyMock *const technology1 = new TechnologyMock(
            alternateDefaultTechnologyProperties(), this);
    if (!bus().registerObject("/technology1", technology1,
                QDBusConnection::ExportScriptableContents)) {
        qFatal("Failed to register /technology1: %s", qPrintable(bus().lastError().message()));
    }
}

/*
 * \class Tests::UtTechnology::TechnologyMock
 */

UtTechnology::TechnologyMock::TechnologyMock(const QVariantMap &properties, ManagerMock *manager)
    : QObject(manager),
      m_properties(properties)
{
}

QVariantMap UtTechnology::TechnologyMock::GetProperties() const
{
    return m_properties;
}

void UtTechnology::TechnologyMock::SetProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message)
{
    static const QSet<QString> writableProperties = QSet<QString>()
        << "Powered"
        << "Tethering"
        << "TetheringIdentifier"
        << "TetheringPassphrase"
        << "IdleTimeout";

    if (!writableProperties.contains(name)) {
        const QString err = QString("Property not writable '%1'").arg(name);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    m_properties[name] = value.variant();

    Q_EMIT PropertyChanged(name, value);
}

void UtTechnology::TechnologyMock::Scan()
{
    // noop
}

TEST_MAIN_WITH_MOCK(UtTechnology, UtTechnology::ManagerMock)

#include "ut_technology.moc"
