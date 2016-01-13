// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_nfc_adapter_client.h"
#include "chromeos/dbus/fake_nfc_device_client.h"
#include "chromeos/dbus/fake_nfc_record_client.h"
#include "chromeos/dbus/fake_nfc_tag_client.h"
#include "device/nfc/nfc_adapter_chromeos.h"
#include "device/nfc/nfc_ndef_record.h"
#include "device/nfc/nfc_ndef_record_utils_chromeos.h"
#include "device/nfc/nfc_peer.h"
#include "device/nfc/nfc_tag.h"
#include "device/nfc/nfc_tag_technology.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::NfcAdapter;
using device::NfcNdefMessage;
using device::NfcNdefRecord;
using device::NfcNdefTagTechnology;
using device::NfcPeer;
using device::NfcTag;

namespace chromeos {

namespace {

// Callback passed to property structures.
void OnPropertyChangedCallback(const std::string& property_name) {
}

// Callback passed to dbus::PropertyBase::Set.
void OnSet(bool success) {
}

class TestObserver : public NfcAdapter::Observer,
                     public NfcPeer::Observer,
                     public NfcTag::Observer,
                     public NfcNdefTagTechnology::Observer {
 public:
  TestObserver(scoped_refptr<NfcAdapter> adapter)
      : present_changed_count_(0),
        powered_changed_count_(0),
        polling_changed_count_(0),
        peer_records_received_count_(0),
        tag_records_received_count_(0),
        peer_count_(0),
        tag_count_(0),
        adapter_(adapter) {
  }

  virtual ~TestObserver() {}

  // NfcAdapter::Observer override.
  virtual void AdapterPresentChanged(NfcAdapter* adapter,
                                     bool present) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    present_changed_count_++;
  }

  // NfcAdapter::Observer override.
  virtual void AdapterPoweredChanged(NfcAdapter* adapter,
                                     bool powered) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    powered_changed_count_++;
  }

  // NfcAdapter::Observer override.
  virtual void AdapterPollingChanged(NfcAdapter* adapter,
                                     bool powered) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    polling_changed_count_++;
  }

  // NfcAdapter::Observer override.
  virtual void PeerFound(NfcAdapter* adapter, NfcPeer* peer) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    peer_count_++;
    peer_identifier_ = peer->GetIdentifier();
  }

  // NfcAdapter::Observer override.
  virtual void PeerLost(NfcAdapter* adapter, NfcPeer* peer) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    EXPECT_EQ(peer_identifier_, peer->GetIdentifier());
    peer_count_--;
    peer_identifier_.clear();
  }

  // NfcAdapter::Observer override.
  virtual void TagFound(NfcAdapter* adapter, NfcTag* tag) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    tag_count_++;
    tag_identifier_ = tag->GetIdentifier();
  }

  // NfcAdapter::Observer override.
  virtual void TagLost(NfcAdapter* adapter, NfcTag* tag) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);
    EXPECT_EQ(tag_identifier_, tag->GetIdentifier());
    tag_count_--;
    tag_identifier_.clear();
  }

  // NfcPeer::Observer override.
  virtual void RecordReceived(
      NfcPeer* peer, const NfcNdefRecord* record) OVERRIDE {
    EXPECT_EQ(peer, adapter_->GetPeer(peer_identifier_));
    EXPECT_EQ(peer_identifier_, peer->GetIdentifier());
    peer_records_received_count_++;
  }

  // NfcNdefTagTechnology::Observer override.
  virtual void RecordReceived(
        NfcTag* tag, const NfcNdefRecord* record) OVERRIDE {
    EXPECT_EQ(tag, adapter_->GetTag(tag_identifier_));
    EXPECT_EQ(tag_identifier_, tag->GetIdentifier());
    tag_records_received_count_++;
  }

  int present_changed_count_;
  int powered_changed_count_;
  int polling_changed_count_;
  int peer_records_received_count_;
  int tag_records_received_count_;
  int peer_count_;
  int tag_count_;
  std::string peer_identifier_;
  std::string tag_identifier_;
  scoped_refptr<NfcAdapter> adapter_;
};

}  // namespace

class NfcChromeOSTest : public testing::Test {
 public:
  virtual void SetUp() {
    DBusThreadManager::InitializeWithStub();
    fake_nfc_adapter_client_ = static_cast<FakeNfcAdapterClient*>(
        DBusThreadManager::Get()->GetNfcAdapterClient());
    fake_nfc_device_client_ = static_cast<FakeNfcDeviceClient*>(
        DBusThreadManager::Get()->GetNfcDeviceClient());
    fake_nfc_record_client_ = static_cast<FakeNfcRecordClient*>(
        DBusThreadManager::Get()->GetNfcRecordClient());
    fake_nfc_tag_client_ = static_cast<FakeNfcTagClient*>(
        DBusThreadManager::Get()->GetNfcTagClient());

    fake_nfc_adapter_client_->EnablePairingOnPoll(false);
    fake_nfc_device_client_->DisableSimulationTimeout();
    fake_nfc_tag_client_->DisableSimulationTimeout();
    success_callback_count_ = 0;
    error_callback_count_ = 0;
  }

  virtual void TearDown() {
    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

  // Assigns a new instance of NfcAdapterChromeOS to |adapter_|.
  void SetAdapter() {
    adapter_ = new NfcAdapterChromeOS();
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
    base::RunLoop().RunUntilIdle();
  }

  // Generic callbacks for success and error.
  void SuccessCallback() {
    success_callback_count_++;
  }

  void ErrorCallback() {
    error_callback_count_++;
  }

  void ErrorCallbackWithParameters(const std::string& error_name,
                                   const std::string& error_message) {
    LOG(INFO) << "Error callback called: " << error_name << ", "
              << error_message;
    error_callback_count_++;
  }

 protected:
  // MessageLoop instance, used to simulate asynchronous behavior.
  base::MessageLoop message_loop_;

  // Fields for storing the number of times SuccessCallback and ErrorCallback
  // have been called.
  int success_callback_count_;
  int error_callback_count_;

  // The NfcAdapter instance under test.
  scoped_refptr<NfcAdapter> adapter_;

  // The fake D-Bus client instances used for testing.
  FakeNfcAdapterClient* fake_nfc_adapter_client_;
  FakeNfcDeviceClient* fake_nfc_device_client_;
  FakeNfcRecordClient* fake_nfc_record_client_;
  FakeNfcTagClient* fake_nfc_tag_client_;
};

// Tests that the adapter updates correctly to reflect the current "default"
// adapter, when multiple adapters appear and disappear.
TEST_F(NfcChromeOSTest, PresentChanged) {
  SetAdapter();
  EXPECT_TRUE(adapter_->IsPresent());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  // Remove all adapters.
  fake_nfc_adapter_client_->SetAdapterPresent(false);
  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_FALSE(adapter_->IsPresent());

  // Add two adapters.
  fake_nfc_adapter_client_->SetAdapterPresent(true);
  fake_nfc_adapter_client_->SetSecondAdapterPresent(true);
  EXPECT_EQ(2, observer.present_changed_count_);
  EXPECT_TRUE(adapter_->IsPresent());

  // Remove the first adapter. Adapter  should update to the second one.
  fake_nfc_adapter_client_->SetAdapterPresent(false);
  EXPECT_EQ(4, observer.present_changed_count_);
  EXPECT_TRUE(adapter_->IsPresent());

  fake_nfc_adapter_client_->SetSecondAdapterPresent(false);
  EXPECT_EQ(5, observer.present_changed_count_);
  EXPECT_FALSE(adapter_->IsPresent());
}

// Tests that the adapter correctly reflects the power state.
TEST_F(NfcChromeOSTest, SetPowered) {
  SetAdapter();
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  EXPECT_FALSE(adapter_->IsPowered());

  // SetPowered(false), while not powered.
  adapter_->SetPowered(
      false,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  // SetPowered(true).
  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  // SetPowered(true), while powered.
  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);

  // SetPowered(false).
  adapter_->SetPowered(
      false,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(2, observer.powered_changed_count_);
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
}

// Tests that the power state updates correctly when the adapter disappears.
TEST_F(NfcChromeOSTest, PresentChangedWhilePowered) {
  SetAdapter();
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_TRUE(adapter_->IsPresent());

  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(adapter_->IsPowered());

  fake_nfc_adapter_client_->SetAdapterPresent(false);
  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_EQ(2, observer.powered_changed_count_);
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsPresent());
}

// Tests that peer and record objects are created for all peers and records
// that already exist when the adapter is created.
TEST_F(NfcChromeOSTest, PeersInitializedWhenAdapterCreated) {
  // Set up the adapter client.
  NfcAdapterClient::Properties* properties =
      fake_nfc_adapter_client_->GetProperties(
          dbus::ObjectPath(FakeNfcAdapterClient::kAdapterPath0));
  properties->powered.Set(true, base::Bind(&OnSet));

  fake_nfc_adapter_client_->StartPollLoop(
      dbus::ObjectPath(FakeNfcAdapterClient::kAdapterPath0),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallbackWithParameters,
                 base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_TRUE(properties->powered.value());
  EXPECT_TRUE(properties->polling.value());

  // Start pairing simulation, which will add a fake device and fake records.
  fake_nfc_device_client_->BeginPairingSimulation(0, 0);
  base::RunLoop().RunUntilIdle();

  // Create the adapter.
  SetAdapter();
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  // Observer shouldn't have received any calls, as it got created AFTER the
  // notifications were sent.
  EXPECT_EQ(0, observer.present_changed_count_);
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.polling_changed_count_);
  EXPECT_EQ(0, observer.peer_count_);

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsPolling());

  NfcAdapter::PeerList peers;
  adapter_->GetPeers(&peers);
  EXPECT_EQ(static_cast<size_t>(1), peers.size());

  NfcPeer* peer = peers[0];
  const NfcNdefMessage& message = peer->GetNdefMessage();
  EXPECT_EQ(static_cast<size_t>(3), message.records().size());
}

// Tests that tag and record objects are created for all tags and records that
// already exist when the adapter is created.
TEST_F(NfcChromeOSTest, TagsInitializedWhenAdapterCreated) {
  const char kTestURI[] = "fake://path/for/testing";

  // Set up the adapter client.
  NfcAdapterClient::Properties* properties =
      fake_nfc_adapter_client_->GetProperties(
          dbus::ObjectPath(FakeNfcAdapterClient::kAdapterPath0));
  properties->powered.Set(true, base::Bind(&OnSet));

  fake_nfc_adapter_client_->StartPollLoop(
      dbus::ObjectPath(FakeNfcAdapterClient::kAdapterPath0),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallbackWithParameters,
                 base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_TRUE(properties->powered.value());
  EXPECT_TRUE(properties->polling.value());

  // Add the fake tag.
  fake_nfc_tag_client_->BeginPairingSimulation(0);
  base::RunLoop().RunUntilIdle();

  // Create a fake record.
  base::DictionaryValue test_record_data;
  test_record_data.SetString(nfc_record::kTypeProperty, nfc_record::kTypeUri);
  test_record_data.SetString(nfc_record::kUriProperty, kTestURI);
  fake_nfc_tag_client_->Write(
      dbus::ObjectPath(FakeNfcTagClient::kTagPath),
      test_record_data,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallbackWithParameters,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);

  // Create the adapter.
  SetAdapter();
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  // Observer shouldn't have received any calls, as it got created AFTER the
  // notifications were sent.
  EXPECT_EQ(0, observer.present_changed_count_);
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.polling_changed_count_);
  EXPECT_EQ(0, observer.peer_count_);

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsPolling());

  NfcAdapter::TagList tags;
  adapter_->GetTags(&tags);
  EXPECT_EQ(static_cast<size_t>(1), tags.size());

  NfcTag* tag = tags[0];
  const NfcNdefMessage& message = tag->GetNdefTagTechnology()->GetNdefMessage();
  EXPECT_EQ(static_cast<size_t>(1), message.records().size());

  const NfcNdefRecord* record = message.records()[0];
  std::string uri;
  EXPECT_TRUE(record->data().GetString(NfcNdefRecord::kFieldURI, &uri));
  EXPECT_EQ(kTestURI, uri);
}

// Tests that the adapter correctly updates its state when polling is started
// and stopped.
TEST_F(NfcChromeOSTest, StartAndStopPolling) {
  SetAdapter();
  EXPECT_TRUE(adapter_->IsPresent());

  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  // Start polling while not powered. Should fail.
  EXPECT_FALSE(adapter_->IsPowered());
  adapter_->StartPolling(
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_FALSE(adapter_->IsPolling());

  // Start polling while powered. Should succeed.
  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());

  adapter_->StartPolling(
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPolling());

  // Start polling while already polling. Should fail.
  adapter_->StartPolling(
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPolling());

  // Stop polling. Should succeed.
  adapter_->StopPolling(
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_FALSE(adapter_->IsPolling());

  // Stop polling while not polling. Should fail.
  adapter_->StopPolling(
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(3, error_callback_count_);
  EXPECT_FALSE(adapter_->IsPolling());
}

// Tests a simple peer pairing simulation.
TEST_F(NfcChromeOSTest, PeerTest) {
  SetAdapter();
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartPolling(
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);

  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_TRUE(adapter_->IsPolling());
  EXPECT_EQ(0, observer.peer_count_);

  // Add the fake device.
  fake_nfc_device_client_->BeginPairingSimulation(0, -1);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.peer_count_);
  EXPECT_EQ(FakeNfcDeviceClient::kDevicePath, observer.peer_identifier_);

  NfcPeer* peer = adapter_->GetPeer(observer.peer_identifier_);
  CHECK(peer);
  peer->AddObserver(&observer);

  // Peer should have no records on it.
  EXPECT_TRUE(peer->GetNdefMessage().records().empty());
  EXPECT_EQ(0, observer.peer_records_received_count_);

  // Make records visible.
  fake_nfc_record_client_->SetDeviceRecordsVisible(true);
  EXPECT_EQ(3, observer.peer_records_received_count_);
  EXPECT_EQ(static_cast<size_t>(3), peer->GetNdefMessage().records().size());

  // End the simulation. Peer should get removed.
  fake_nfc_device_client_->EndPairingSimulation();
  EXPECT_EQ(0, observer.peer_count_);
  EXPECT_TRUE(observer.peer_identifier_.empty());

  peer = adapter_->GetPeer(observer.peer_identifier_);
  EXPECT_FALSE(peer);

  // No record related notifications will be sent when a peer gets removed.
  EXPECT_EQ(3, observer.peer_records_received_count_);
}

// Tests a simple tag pairing simulation.
TEST_F(NfcChromeOSTest, TagTest) {
  const char kTestURI[] = "fake://path/for/testing";

  SetAdapter();
  TestObserver observer(adapter_);
  adapter_->AddObserver(&observer);

  adapter_->SetPowered(
      true,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartPolling(
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);

  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_TRUE(adapter_->IsPolling());
  EXPECT_EQ(0, observer.tag_count_);

  // Add the fake tag.
  fake_nfc_tag_client_->BeginPairingSimulation(0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.tag_count_);
  EXPECT_EQ(FakeNfcTagClient::kTagPath, observer.tag_identifier_);

  NfcTag* tag = adapter_->GetTag(observer.tag_identifier_);
  CHECK(tag);
  tag->AddObserver(&observer);
  EXPECT_TRUE(tag->IsReady());
  CHECK(tag->GetNdefTagTechnology());
  tag->GetNdefTagTechnology()->AddObserver(&observer);

  NfcNdefTagTechnology* tag_technology = tag->GetNdefTagTechnology();
  EXPECT_TRUE(tag_technology->IsSupportedByTag());

  // Tag should have no records on it.
  EXPECT_TRUE(tag_technology->GetNdefMessage().records().empty());
  EXPECT_EQ(0, observer.tag_records_received_count_);

  // Set the tag record visible. By default the record has no content, so no
  // NfcNdefMessage should be received.
  fake_nfc_record_client_->SetTagRecordsVisible(true);
  EXPECT_TRUE(tag_technology->GetNdefMessage().records().empty());
  EXPECT_EQ(0, observer.tag_records_received_count_);
  fake_nfc_record_client_->SetTagRecordsVisible(false);

  // Write an NDEF record to the tag.
  EXPECT_EQ(2, success_callback_count_);  // 2 for SetPowered and StartPolling.
  EXPECT_EQ(0, error_callback_count_);

  base::DictionaryValue record_data;
  record_data.SetString(NfcNdefRecord::kFieldURI, kTestURI);
  NfcNdefRecord written_record;
  written_record.Populate(NfcNdefRecord::kTypeURI, &record_data);
  NfcNdefMessage written_message;
  written_message.AddRecord(&written_record);

  tag_technology->WriteNdef(
      written_message,
      base::Bind(&NfcChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&NfcChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(static_cast<size_t>(1),
            tag_technology->GetNdefMessage().records().size());
  EXPECT_EQ(1, observer.tag_records_received_count_);

  NfcNdefRecord* received_record =
      tag_technology->GetNdefMessage().records()[0];
  EXPECT_EQ(NfcNdefRecord::kTypeURI, received_record->type());
  std::string uri;
  EXPECT_TRUE(received_record->data().GetString(
      NfcNdefRecord::kFieldURI, &uri));
  EXPECT_EQ(kTestURI, uri);

  // End the simulation. Tag should get removed.
  fake_nfc_tag_client_->EndPairingSimulation();
  EXPECT_EQ(0, observer.tag_count_);
  EXPECT_TRUE(observer.tag_identifier_.empty());

  tag = adapter_->GetTag(observer.tag_identifier_);
  EXPECT_FALSE(tag);

  // No record related notifications will be sent when a tag gets removed.
  EXPECT_EQ(1, observer.tag_records_received_count_);
}

// Unit tests for nfc_ndef_record_utils methods.
TEST_F(NfcChromeOSTest, NfcNdefRecordToDBusAttributes) {
  const char kText[] = "text";
  const char kURI[] = "test://uri";
  const char kEncoding[] = "encoding";
  const char kLanguageCode[] = "en";
  const char kMimeType[] = "mime-type";
  const double kSize = 5;

  // Text record.
  base::DictionaryValue data;
  data.SetString(NfcNdefRecord::kFieldText, kText);
  data.SetString(NfcNdefRecord::kFieldLanguageCode, kLanguageCode);
  data.SetString(NfcNdefRecord::kFieldEncoding, kEncoding);

  scoped_ptr<NfcNdefRecord> record(new NfcNdefRecord());
  ASSERT_TRUE(record->Populate(NfcNdefRecord::kTypeText, &data));

  base::DictionaryValue result;
  EXPECT_TRUE(nfc_ndef_record_utils::NfcNdefRecordToDBusAttributes(
      record.get(), &result));

  std::string string_value;
  EXPECT_TRUE(result.GetString(
      nfc_record::kTypeProperty, &string_value));
  EXPECT_EQ(nfc_record::kTypeText, string_value);
  EXPECT_TRUE(result.GetString(
      nfc_record::kRepresentationProperty, &string_value));
  EXPECT_EQ(kText, string_value);
  EXPECT_TRUE(result.GetString(
      nfc_record::kLanguageProperty, &string_value));
  EXPECT_EQ(kLanguageCode, string_value);
  EXPECT_TRUE(result.GetString(
      nfc_record::kEncodingProperty, &string_value));
  EXPECT_EQ(kEncoding, string_value);

  // URI record.
  data.Clear();
  data.SetString(NfcNdefRecord::kFieldURI, kURI);
  data.SetString(NfcNdefRecord::kFieldMimeType, kMimeType);
  data.SetDouble(NfcNdefRecord::kFieldTargetSize, kSize);

  record.reset(new NfcNdefRecord());
  ASSERT_TRUE(record->Populate(NfcNdefRecord::kTypeURI, &data));

  result.Clear();
  EXPECT_TRUE(nfc_ndef_record_utils::NfcNdefRecordToDBusAttributes(
      record.get(), &result));

  EXPECT_TRUE(result.GetString(nfc_record::kTypeProperty, &string_value));
  EXPECT_EQ(nfc_record::kTypeUri, string_value);
  EXPECT_TRUE(result.GetString(nfc_record::kUriProperty, &string_value));
  EXPECT_EQ(kURI, string_value);
  EXPECT_TRUE(result.GetString(nfc_record::kMimeTypeProperty, &string_value));
  EXPECT_EQ(kMimeType, string_value);
  double double_value;
  EXPECT_TRUE(result.GetDouble(nfc_record::kSizeProperty, &double_value));
  EXPECT_EQ(kSize, double_value);

  // SmartPoster record.
  base::DictionaryValue* title = new base::DictionaryValue();
  title->SetString(NfcNdefRecord::kFieldText, kText);
  title->SetString(NfcNdefRecord::kFieldLanguageCode, kLanguageCode);
  title->SetString(NfcNdefRecord::kFieldEncoding, kEncoding);

  base::ListValue* titles = new base::ListValue();
  titles->Append(title);
  data.Set(NfcNdefRecord::kFieldTitles, titles);

  record.reset(new NfcNdefRecord());
  ASSERT_TRUE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));

  result.Clear();
  EXPECT_TRUE(nfc_ndef_record_utils::NfcNdefRecordToDBusAttributes(
      record.get(), &result));

  EXPECT_TRUE(result.GetString(
      nfc_record::kTypeProperty, &string_value));
  EXPECT_EQ(nfc_record::kTypeSmartPoster, string_value);
  EXPECT_TRUE(result.GetString(
      nfc_record::kRepresentationProperty, &string_value));
  EXPECT_EQ(kText, string_value);
  EXPECT_TRUE(result.GetString(
      nfc_record::kLanguageProperty, &string_value));
  EXPECT_EQ(kLanguageCode, string_value);
  EXPECT_TRUE(result.GetString(
      nfc_record::kEncodingProperty, &string_value));
  EXPECT_EQ(kEncoding, string_value);
  EXPECT_TRUE(result.GetString(nfc_record::kUriProperty, &string_value));
  EXPECT_EQ(kURI, string_value);
  EXPECT_TRUE(result.GetString(nfc_record::kMimeTypeProperty, &string_value));
  EXPECT_EQ(kMimeType, string_value);
  EXPECT_TRUE(result.GetDouble(nfc_record::kSizeProperty, &double_value));
  EXPECT_EQ(kSize, double_value);
}

TEST_F(NfcChromeOSTest, RecordPropertiesToNfcNdefRecord) {
  const char kText[] = "text";
  const char kURI[] = "test://uri";
  const char kEncoding[] = "encoding";
  const char kLanguageCode[] = "en";
  const char kMimeType[] = "mime-type";
  const uint32 kSize = 5;

  FakeNfcRecordClient::Properties record_properties(
      base::Bind(&OnPropertyChangedCallback));

  // Text record.
  record_properties.type.ReplaceValue(nfc_record::kTypeText);
  record_properties.representation.ReplaceValue(kText);
  record_properties.language.ReplaceValue(kLanguageCode);
  record_properties.encoding.ReplaceValue(kEncoding);

  scoped_ptr<NfcNdefRecord> record(new NfcNdefRecord());
  EXPECT_TRUE(nfc_ndef_record_utils::RecordPropertiesToNfcNdefRecord(
      &record_properties, record.get()));
  EXPECT_TRUE(record->IsPopulated());

  std::string string_value;
  EXPECT_EQ(NfcNdefRecord::kTypeText, record->type());
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldText, &string_value));
  EXPECT_EQ(kText, string_value);
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldLanguageCode, &string_value));
  EXPECT_EQ(kLanguageCode, string_value);
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldEncoding, &string_value));
  EXPECT_EQ(kEncoding, string_value);

  // URI record.
  record_properties.representation.ReplaceValue("");
  record_properties.language.ReplaceValue("");
  record_properties.encoding.ReplaceValue("");

  record_properties.type.ReplaceValue(nfc_record::kTypeUri);
  record_properties.uri.ReplaceValue(kURI);
  record_properties.mime_type.ReplaceValue(kMimeType);
  record_properties.size.ReplaceValue(kSize);

  record.reset(new NfcNdefRecord());
  EXPECT_TRUE(nfc_ndef_record_utils::RecordPropertiesToNfcNdefRecord(
      &record_properties, record.get()));
  EXPECT_TRUE(record->IsPopulated());

  EXPECT_EQ(NfcNdefRecord::kTypeURI, record->type());
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldURI, &string_value));
  EXPECT_EQ(kURI, string_value);
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldMimeType, &string_value));
  EXPECT_EQ(kMimeType, string_value);
  double double_value;
  EXPECT_TRUE(record->data().GetDouble(
      NfcNdefRecord::kFieldTargetSize, &double_value));
  EXPECT_EQ(kSize, double_value);

  // Contents not matching type.
  record_properties.representation.ReplaceValue(kText);
  record_properties.language.ReplaceValue(kLanguageCode);
  record_properties.encoding.ReplaceValue(kEncoding);

  record.reset(new NfcNdefRecord());
  EXPECT_FALSE(nfc_ndef_record_utils::RecordPropertiesToNfcNdefRecord(
      &record_properties, record.get()));
  EXPECT_FALSE(record->IsPopulated());

  // SmartPoster record.
  record_properties.type.ReplaceValue(nfc_record::kTypeSmartPoster);
  EXPECT_TRUE(nfc_ndef_record_utils::RecordPropertiesToNfcNdefRecord(
      &record_properties, record.get()));
  EXPECT_TRUE(record->IsPopulated());

  EXPECT_EQ(NfcNdefRecord::kTypeSmartPoster, record->type());
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldURI, &string_value));
  EXPECT_EQ(kURI, string_value);
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldMimeType, &string_value));
  EXPECT_EQ(kMimeType, string_value);
  EXPECT_TRUE(record->data().GetDouble(
      NfcNdefRecord::kFieldTargetSize, &double_value));
  EXPECT_EQ(kSize, double_value);

  const base::ListValue* titles = NULL;
  EXPECT_TRUE(record->data().GetList(NfcNdefRecord::kFieldTitles, &titles));
  EXPECT_EQ(static_cast<size_t>(1), titles->GetSize());
  ASSERT_TRUE(titles);
  const base::DictionaryValue* title = NULL;
  EXPECT_TRUE(titles->GetDictionary(0, &title));
  CHECK(title);

  EXPECT_TRUE(title->GetString(NfcNdefRecord::kFieldText, &string_value));
  EXPECT_EQ(kText, string_value);
  EXPECT_TRUE(title->GetString(
      NfcNdefRecord::kFieldLanguageCode, &string_value));
  EXPECT_EQ(kLanguageCode, string_value);
  EXPECT_TRUE(title->GetString(NfcNdefRecord::kFieldEncoding, &string_value));
  EXPECT_EQ(kEncoding, string_value);
}

}  // namespace chromeos
