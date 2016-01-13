#include <QtCore/QPointer>

#include "../libconnman-qt/clockmodel.h"
#include "../libconnman-qt/connman_clock_interface.h"
#include "testbase.h"

namespace Tests {

class UtClock : public TestBase
{
    Q_OBJECT

    enum {
        SHORT_WHILE_TO_PROCESS_TIME_CHANGE = 5, // [s]
    };

public:
    class ClockMock;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testProperties_data();
    void testProperties();
    void testWriteProperties_data();
    void testWriteProperties();
    void testSetDate();
    void testSetTime();
    void testTime();

private:
    void testSetDateTime(bool setDate);
    static quint64 diff(quint64 a1, quint64 a2) { return qMax(a1, a2) - qMin(a1, a2); }

private:
    QPointer<ClockModel> m_clock;
    // Second instance used to verify change is really propagated through connman
    QPointer<ClockModel> m_otherClock;
};

class UtClock::ClockMock : public MainObjectMock
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Clock")

public:
    ClockMock();

public:
    Q_SCRIPTABLE QVariantMap GetProperties() const;
    Q_SCRIPTABLE void SetProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message);

signals:
    Q_SCRIPTABLE void PropertyChanged(const QString &name, const QDBusVariant &value);

private:
    QVariantMap m_properties;
};

} // namespace Tests

using namespace Tests;

/*
 * \class Tests::UtClock
 */

void UtClock::initTestCase()
{
    QVERIFY(waitForService("net.connman", "/", "net.connman.Clock"));

    m_clock = new ClockModel();
    QVERIFY2(waitForSignal(m_clock, SIGNAL(timezoneChanged())),
            "ClockModel m_clock initialization failed");

    m_otherClock = new ClockModel();
    QVERIFY2(waitForSignal(m_otherClock, SIGNAL(timezoneChanged())),
            "ClockModel m_otherClock initialization failed");
}

void UtClock::cleanupTestCase()
{
    delete m_clock;
    delete m_otherClock;
}

void UtClock::testProperties_data()
{
    QTest::addColumn<QVariant>("expected");

    createTestPropertyData(defaultClockProperties(), clockDBusProperty2QtProperty);
}

void UtClock::testProperties()
{
    QFETCH(QVariant, expected);

    testProperty(*m_clock, QTest::currentDataTag(), expected);
}

void UtClock::testWriteProperties_data()
{
    QTest::addColumn<QVariant>("newValue");

    QTest::newRow("timezone") << QVariant("Europe/Ireland");
    QTest::newRow("timezoneUpdates") << QVariant("manual");
    QTest::newRow("timeUpdates") << QVariant("manual");
    QTest::newRow("timeservers") << QVariant(QStringList() << "time1.bar.org" << "time2.bar.org");
}

void UtClock::testWriteProperties()
{
    QFETCH(QVariant, newValue);

    testWriteProperty(m_clock, m_otherClock, QTest::currentDataTag(), newValue);
}

void UtClock::testSetDate()
{
    testSetDateTime(true);
}

void UtClock::testSetTime()
{
    testSetDateTime(false);
}

void UtClock::testTime()
{
    QCOMPARE(m_clock->time("1", "2"), QTime(1, 2));
}

void UtClock::testSetDateTime(bool setDate)
{
    SignalSpy spy(m_clock->mClockProxy, SIGNAL(PropertyChanged(QString,QDBusVariant)));
    SignalSpy otherSpy(m_otherClock->mClockProxy, SIGNAL(PropertyChanged(QString,QDBusVariant)));

    quint64 newSecsSinceEpoch = 0;

    if (setDate) {
        const QDate newDate = QDate::currentDate().addDays(-1234);
        newSecsSinceEpoch = QDateTime(newDate, QTime::currentTime()).toTime_t();
        m_clock->setDate(newDate);
    } else {
        const QTime newTime = QTime::currentTime().addSecs(-1234);
        newSecsSinceEpoch = QDateTime(QDate::currentDate(), newTime).toTime_t();
        m_clock->setTime(newTime);
    }

    QVERIFY(waitForSignals(SignalSpyList() << &spy << &otherSpy));

    QCOMPARE(spy.at(0).at(0).toString(), QString("Time"));
    QCOMPARE(otherSpy.at(0).at(0).toString(), QString("Time"));
    QCOMPARE(spy.at(0).at(1).value<QDBusVariant>().variant(),
            otherSpy.at(0).at(1).value<QDBusVariant>().variant());

    const quint64 Q_EMITtedSecsSinceEpoch =
        spy.at(0).at(1).value<QDBusVariant>().variant().toULongLong();

    QDBusPendingReply<QVariantMap> reply = m_clock->mClockProxy->GetProperties();
    reply.waitForFinished();
    QVERIFY(!reply.isError());
    QVariantMap properties = reply.value();

    QVERIFY(properties.contains("Time"));
    quint64 secsSinceEpoch;
    bool ok;
    QVERIFY((secsSinceEpoch = properties.value("Time").toULongLong(&ok), ok));

    QCOMPARE(Q_EMITtedSecsSinceEpoch, secsSinceEpoch);
    QVERIFY(diff(secsSinceEpoch, newSecsSinceEpoch) < SHORT_WHILE_TO_PROCESS_TIME_CHANGE);
}

/*
 * \class Tests::UtClock::ClockMock
 */

UtClock::ClockMock::ClockMock()
    : MainObjectMock("net.connman", "/"),
      m_properties(defaultClockProperties())
{
}

QVariantMap UtClock::ClockMock::GetProperties() const
{
    return m_properties;
}

void UtClock::ClockMock::SetProperty(const QString &name, const QDBusVariant &value,
        const QDBusMessage &message)
{
    static const QSet<QString> writableProperties = QSet<QString>()
        << "Time"
        << "TimeUpdates"
        << "Timezone"
        << "TimezoneUpdates"
        << "Timeservers";

    if (!writableProperties.contains(name)) {
        const QString err = QString("Property not writable '%1'").arg(name);
        qWarning("%s: %s", Q_FUNC_INFO, qPrintable(err));
        bus().send(message.createErrorReply(QDBusError::Failed, err));
        return;
    }

    m_properties[name] = value.variant();

    Q_EMIT PropertyChanged(name, value);
}

TEST_MAIN_WITH_MOCK(UtClock, UtClock::ClockMock)

#include "ut_clock.moc"
