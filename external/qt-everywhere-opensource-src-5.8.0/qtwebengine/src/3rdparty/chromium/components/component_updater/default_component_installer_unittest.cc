// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/component_updater/default_component_installer.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/update_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Configurator = update_client::Configurator;
using CrxUpdateItem = update_client::CrxUpdateItem;
using TestConfigurator = update_client::TestConfigurator;
using UpdateClient = update_client::UpdateClient;

using ::testing::_;
using ::testing::Invoke;

namespace component_updater {

namespace {

// This hash corresponds to jebgalgnebhfojomionfpkfelancnnkf.crx.
const uint8_t kSha256Hash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                               0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                               0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                               0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

class MockUpdateClient : public UpdateClient {
 public:
  MockUpdateClient() {}
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD3(Install,
               void(const std::string& id,
                    const CrxDataCallback& crx_data_callback,
                    const CompletionCallback& completion_callback));
  MOCK_METHOD3(Update,
               void(const std::vector<std::string>& ids,
                    const CrxDataCallback& crx_data_callback,
                    const CompletionCallback& completion_callback));
  MOCK_CONST_METHOD2(GetCrxUpdateState,
                     bool(const std::string& id, CrxUpdateItem* update_item));
  MOCK_CONST_METHOD1(IsUpdating, bool(const std::string& id));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD3(SendUninstallPing,
               void(const std::string& id, const Version& version, int reason));

 private:
  ~MockUpdateClient() override {}
};

class FakeInstallerTraits : public ComponentInstallerTraits {
 public:
  ~FakeInstallerTraits() override {}

  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& dir) const override {
    return true;
  }

  bool CanAutoUpdate() const override { return true; }

  bool RequiresNetworkEncryption() const override { return true; }

  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override {
    return true;
  }

  void ComponentReady(
      const base::Version& version,
      const base::FilePath& install_dir,
      std::unique_ptr<base::DictionaryValue> manifest) override {}

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(FILE_PATH_LITERAL("fake"));
  }

  void GetHash(std::vector<uint8_t>* hash) const override { GetPkHash(hash); }

  std::string GetName() const override { return "fake name"; }

  update_client::InstallerAttributes GetInstallerAttributes() const override {
    update_client::InstallerAttributes installer_attributes;
    installer_attributes["ap"] = "fake-ap";
    installer_attributes["is-enterprise"] = "1";
    return installer_attributes;
  }

 private:
  static void GetPkHash(std::vector<uint8_t>* hash) {
    hash->assign(std::begin(kSha256Hash), std::end(kSha256Hash));
  }
};

class DefaultComponentInstallerTest : public testing::Test {
 public:
  DefaultComponentInstallerTest();
  ~DefaultComponentInstallerTest() override;

  MockUpdateClient& update_client() { return *update_client_; }
  ComponentUpdateService* component_updater() {
    return component_updater_.get();
  }
  scoped_refptr<TestConfigurator> configurator() const { return config_; }
  base::Closure quit_closure() const { return quit_closure_; }

 protected:
  void RunThreads();

 private:
  static const int kNumWorkerThreads_ = 1;

  base::MessageLoopForUI message_loop_;
  base::RunLoop runloop_;
  base::Closure quit_closure_;

  std::unique_ptr<base::SequencedWorkerPoolOwner> worker_pool_;

  scoped_refptr<TestConfigurator> config_;
  scoped_refptr<MockUpdateClient> update_client_;
  std::unique_ptr<ComponentUpdateService> component_updater_;
};

DefaultComponentInstallerTest::DefaultComponentInstallerTest()
    : worker_pool_(
          new base::SequencedWorkerPoolOwner(kNumWorkerThreads_, "test")) {
  quit_closure_ = runloop_.QuitClosure();

  auto pool = worker_pool_->pool();
  config_ = new TestConfigurator(
      pool->GetSequencedTaskRunner(pool->GetSequenceToken()),
      message_loop_.task_runner());

  update_client_ = new MockUpdateClient();
  EXPECT_CALL(update_client(), AddObserver(_)).Times(1);
  component_updater_.reset(new CrxUpdateService(config_, update_client_));
}

DefaultComponentInstallerTest::~DefaultComponentInstallerTest() {
  EXPECT_CALL(update_client(), RemoveObserver(_)).Times(1);
  component_updater_.reset();
}

void DefaultComponentInstallerTest::RunThreads() {
  runloop_.Run();
}

}  // namespace

// Tests that the component metadata is propagated from the default
// component installer and its component traits, through the instance of the
// CrxComponent, to the component updater service.
TEST_F(DefaultComponentInstallerTest, RegisterComponent) {
  class LoopHandler {
   public:
    LoopHandler(int max_cnt, const base::Closure& quit_closure)
        : max_cnt_(max_cnt), quit_closure_(quit_closure) {}

    void OnUpdate(const std::vector<std::string>& ids,
                  const UpdateClient::CrxDataCallback& crx_data_callback,
                  const UpdateClient::CompletionCallback& completion_callback) {
      completion_callback.Run(0);
      static int cnt = 0;
      ++cnt;
      if (cnt >= max_cnt_)
        quit_closure_.Run();
    }

   private:
    const int max_cnt_;
    base::Closure quit_closure_;
  };

  const std::string id("jebgalgnebhfojomionfpkfelancnnkf");

  // Quit after one update check has been fired.
  LoopHandler loop_handler(1, quit_closure());
  EXPECT_CALL(update_client(), Update(_, _, _))
      .WillRepeatedly(Invoke(&loop_handler, &LoopHandler::OnUpdate));

  EXPECT_CALL(update_client(), GetCrxUpdateState(id, _)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);

  std::unique_ptr<ComponentInstallerTraits> traits(new FakeInstallerTraits());

  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(component_updater(), base::Closure());

  RunThreads();

  CrxUpdateItem item;
  EXPECT_TRUE(component_updater()->GetComponentDetails(id, &item));
  const CrxComponent& component(item.component);

  update_client::InstallerAttributes expected_attrs;
  expected_attrs["ap"] = "fake-ap";
  expected_attrs["is-enterprise"] = "1";

  EXPECT_EQ(
      std::vector<uint8_t>(std::begin(kSha256Hash), std::end(kSha256Hash)),
      component.pk_hash);
  EXPECT_EQ(base::Version("0.0.0.0"), component.version);
  EXPECT_TRUE(component.fingerprint.empty());
  EXPECT_STREQ("fake name", component.name.c_str());
  EXPECT_EQ(expected_attrs, component.installer_attributes);
  EXPECT_TRUE(component.requires_network_encryption);
}

}  // namespace component_updater
