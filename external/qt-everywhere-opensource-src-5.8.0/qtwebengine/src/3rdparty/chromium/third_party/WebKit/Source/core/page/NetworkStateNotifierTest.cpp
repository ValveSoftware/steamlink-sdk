/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/NetworkStateNotifier.h"

#include "core/dom/Document.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebConnectionType.h"
#include "public/platform/WebThread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Functional.h"

namespace blink {

namespace {
    const double kNoneMaxBandwidthMbps = 0.0;
    const double kBluetoothMaxBandwidthMbps = 1.0;
    const double kEthernetMaxBandwidthMbps = 2.0;
}

class StateObserver : public NetworkStateNotifier::NetworkStateObserver {
public:
    StateObserver()
        : m_observedType(WebConnectionTypeNone)
        , m_observedMaxBandwidthMbps(0.0)
        , m_callbackCount(0)
    {
    }

    virtual void connectionChange(WebConnectionType type, double maxBandwidthMbps)
    {
        m_observedType = type;
        m_observedMaxBandwidthMbps = maxBandwidthMbps;
        m_callbackCount += 1;

        if (m_closure)
            (*m_closure)();
    }

    WebConnectionType observedType() const
    {
        return m_observedType;
    }

    double observedMaxBandwidth() const
    {
        return m_observedMaxBandwidthMbps;
    }

    int callbackCount() const
    {
        return m_callbackCount;
    }

    void setNotificationCallback(std::unique_ptr<WTF::Closure> closure)
    {
        m_closure = std::move(closure);
    }

private:
    std::unique_ptr<WTF::Closure> m_closure;
    WebConnectionType m_observedType;
    double m_observedMaxBandwidthMbps;
    int m_callbackCount;
};

class NetworkStateNotifierTest : public ::testing::Test {
public:
    NetworkStateNotifierTest()
        : m_document(Document::create())
        , m_document2(Document::create())
    {
        // Initialize connection, so that future calls to setWebConnection issue notifications.
        m_notifier.setWebConnection(WebConnectionTypeUnknown, 0.0);
    }

    ExecutionContext* getExecutionContext()
    {
        return m_document.get();
    }

    ExecutionContext* executionContext2()
    {
        return m_document2.get();
    }

protected:
    void setConnection(WebConnectionType type, double maxBandwidthMbps)
    {
        m_notifier.setWebConnection(type, maxBandwidthMbps);
        testing::runPendingTasks();
    }

    void addObserverOnNotification(StateObserver* observer, StateObserver* observerToAdd)
    {
        observer->setNotificationCallback(bind(&NetworkStateNotifier::addObserver, WTF::unretained(&m_notifier), WTF::unretained(observerToAdd), wrapPersistent(getExecutionContext())));
    }

    void removeObserverOnNotification(StateObserver* observer, StateObserver* observerToRemove)
    {
        observer->setNotificationCallback(bind(&NetworkStateNotifier::removeObserver, WTF::unretained(&m_notifier), WTF::unretained(observerToRemove), wrapPersistent(getExecutionContext())));
    }

    bool verifyObservations(const StateObserver& observer, WebConnectionType type, double maxBandwidthMbps)
    {
        EXPECT_EQ(observer.observedType(), type);
        EXPECT_EQ(observer.observedMaxBandwidth(), maxBandwidthMbps);
        return observer.observedType() == type && observer.observedMaxBandwidth() == maxBandwidthMbps;
    }

    Persistent<Document> m_document;
    Persistent<Document> m_document2;
    NetworkStateNotifier m_notifier;
};

TEST_F(NetworkStateNotifierTest, AddObserver)
{
    StateObserver observer;
    m_notifier.addObserver(&observer, getExecutionContext());
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeNone, kNoneMaxBandwidthMbps));

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_EQ(observer.callbackCount(), 1);
}

TEST_F(NetworkStateNotifierTest, RemoveObserver)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, getExecutionContext());
    m_notifier.removeObserver(&observer1, getExecutionContext());
    m_notifier.addObserver(&observer2, getExecutionContext());

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeNone, kNoneMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveSoleObserver)
{
    StateObserver observer1;
    m_notifier.addObserver(&observer1, getExecutionContext());
    m_notifier.removeObserver(&observer1, getExecutionContext());

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeNone, kNoneMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, AddObserverWhileNotifying)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, getExecutionContext());
    addObserverOnNotification(&observer1, &observer2);

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveSoleObserverWhileNotifying)
{
    StateObserver observer1;
    m_notifier.addObserver(&observer1, getExecutionContext());
    removeObserverOnNotification(&observer1, &observer1);

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));

    setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveCurrentObserverWhileNotifying)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, getExecutionContext());
    m_notifier.addObserver(&observer2, getExecutionContext());
    removeObserverOnNotification(&observer1, &observer1);

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));

    setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemovePastObserverWhileNotifying)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, getExecutionContext());
    m_notifier.addObserver(&observer2, getExecutionContext());
    removeObserverOnNotification(&observer2, &observer1);

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_EQ(observer1.observedType(), WebConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), WebConnectionTypeBluetooth);

    setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveFutureObserverWhileNotifying)
{
    StateObserver observer1, observer2, observer3;
    m_notifier.addObserver(&observer1, getExecutionContext());
    m_notifier.addObserver(&observer2, getExecutionContext());
    m_notifier.addObserver(&observer3, getExecutionContext());
    removeObserverOnNotification(&observer1, &observer2);

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeNone, kNoneMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer3, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, MultipleContextsAddObserver)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, getExecutionContext());
    m_notifier.addObserver(&observer2, executionContext2());

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveContext)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, getExecutionContext());
    m_notifier.addObserver(&observer2, executionContext2());
    m_notifier.removeObserver(&observer2, executionContext2());

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeNone, kNoneMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, RemoveAllContexts)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, getExecutionContext());
    m_notifier.addObserver(&observer2, executionContext2());
    m_notifier.removeObserver(&observer1, getExecutionContext());
    m_notifier.removeObserver(&observer2, executionContext2());

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer1, WebConnectionTypeNone, kNoneMaxBandwidthMbps));
    EXPECT_TRUE(verifyObservations(observer2, WebConnectionTypeNone, kNoneMaxBandwidthMbps));
}

TEST_F(NetworkStateNotifierTest, SetOverride)
{
    StateObserver observer;
    m_notifier.addObserver(&observer, getExecutionContext());

    m_notifier.setOnLine(true);
    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_TRUE(m_notifier.onLine());
    EXPECT_EQ(WebConnectionTypeBluetooth, m_notifier.connectionType());
    EXPECT_EQ(kBluetoothMaxBandwidthMbps, m_notifier.maxBandwidth());

    m_notifier.setOverride(true, WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
    testing::runPendingTasks();
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps));
    EXPECT_TRUE(m_notifier.onLine());
    EXPECT_EQ(WebConnectionTypeEthernet, m_notifier.connectionType());
    EXPECT_EQ(kEthernetMaxBandwidthMbps, m_notifier.maxBandwidth());

    // When override is active, calls to setOnLine and setConnection are temporary ignored.
    m_notifier.setOnLine(false);
    setConnection(WebConnectionTypeNone, kNoneMaxBandwidthMbps);
    testing::runPendingTasks();
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps));
    EXPECT_TRUE(m_notifier.onLine());
    EXPECT_EQ(WebConnectionTypeEthernet, m_notifier.connectionType());
    EXPECT_EQ(kEthernetMaxBandwidthMbps, m_notifier.maxBandwidth());

    m_notifier.clearOverride();
    testing::runPendingTasks();
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeNone, kNoneMaxBandwidthMbps));
    EXPECT_FALSE(m_notifier.onLine());
    EXPECT_EQ(WebConnectionTypeNone, m_notifier.connectionType());
    EXPECT_EQ(kNoneMaxBandwidthMbps, m_notifier.maxBandwidth());

    m_notifier.removeObserver(&observer, getExecutionContext());
}

TEST_F(NetworkStateNotifierTest, NoExtraNotifications)
{
    StateObserver observer;
    m_notifier.addObserver(&observer, getExecutionContext());

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_EQ(observer.callbackCount(), 1);

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_EQ(observer.callbackCount(), 1);

    setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps));
    EXPECT_EQ(observer.callbackCount(), 2);

    setConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
    EXPECT_EQ(observer.callbackCount(), 2);

    setConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    EXPECT_TRUE(verifyObservations(observer, WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps));
    EXPECT_EQ(observer.callbackCount(), 3);

    m_notifier.removeObserver(&observer, getExecutionContext());
}

TEST_F(NetworkStateNotifierTest, NoNotificationOnInitialization)
{
    NetworkStateNotifier notifier;
    Persistent<Document> document(Document::create());
    StateObserver observer;

    notifier.addObserver(&observer, document.get());
    testing::runPendingTasks();
    EXPECT_EQ(observer.callbackCount(), 0);

    notifier.setWebConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    testing::runPendingTasks();
    EXPECT_EQ(observer.callbackCount(), 0);

    notifier.setWebConnection(WebConnectionTypeBluetooth, kBluetoothMaxBandwidthMbps);
    testing::runPendingTasks();
    EXPECT_EQ(observer.callbackCount(), 0);

    notifier.setWebConnection(WebConnectionTypeEthernet, kEthernetMaxBandwidthMbps);
    testing::runPendingTasks();
    EXPECT_EQ(observer.callbackCount(), 1);
    EXPECT_EQ(observer.observedType(), WebConnectionTypeEthernet);
    EXPECT_EQ(observer.observedMaxBandwidth(), kEthernetMaxBandwidthMbps);
}

} // namespace blink
