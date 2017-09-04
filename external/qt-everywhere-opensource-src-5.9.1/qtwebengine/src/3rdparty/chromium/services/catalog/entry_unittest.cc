// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/entry.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/values.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace catalog {

class EntryTest : public testing::Test {
 public:
  EntryTest() {}
  ~EntryTest() override {}

 protected:
  std::unique_ptr<Entry> ReadEntry(const std::string& manifest,
                                   std::unique_ptr<base::Value>* out_value) {
    std::unique_ptr<base::Value> value = ReadManifest(manifest);
    base::DictionaryValue* dictionary = nullptr;
    CHECK(value->GetAsDictionary(&dictionary));
    if (out_value)
      *out_value = std::move(value);
    return Entry::Deserialize(*dictionary);
  }

  std::unique_ptr<base::Value> ReadManifest(const std::string& manifest) {
    base::FilePath manifest_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &manifest_path);
    manifest_path = manifest_path.AppendASCII(
        "services/catalog/data/" + manifest);

    JSONFileValueDeserializer deserializer(manifest_path);
    int error = 0;
    std::string message;
    // TODO(beng): probably want to do more detailed error checking. This should
    //             be done when figuring out if to unblock connection
    //             completion.
    return deserializer.Deserialize(&error, &message);
  }

 private:
  void SetUp() override {}
  void TearDown() override {}

  DISALLOW_COPY_AND_ASSIGN(EntryTest);
};

TEST_F(EntryTest, Simple) {
  std::unique_ptr<Entry> entry = ReadEntry("simple", nullptr);
  EXPECT_EQ("foo", entry->name());
  EXPECT_EQ("Foo", entry->display_name());
}

TEST_F(EntryTest, Instance) {
  std::unique_ptr<Entry> entry = ReadEntry("instance", nullptr);
  EXPECT_EQ("foo", entry->name());
  EXPECT_EQ("Foo", entry->display_name());
}

TEST_F(EntryTest, ConnectionSpec) {
  std::unique_ptr<Entry> entry = ReadEntry("connection_spec", nullptr);

  EXPECT_EQ("foo", entry->name());
  EXPECT_EQ("Foo", entry->display_name());
  service_manager::InterfaceProviderSpec spec;
  service_manager::CapabilitySet capabilities;
  capabilities.insert("bar:bar");
  spec.requires["bar"] = capabilities;
  service_manager::InterfaceProviderSpecMap specs;
  specs[service_manager::mojom::kServiceManager_ConnectorSpec] = spec;
  EXPECT_EQ(specs, entry->interface_provider_specs());
}

TEST_F(EntryTest, Serialization) {
  std::unique_ptr<base::Value> value;
  std::unique_ptr<Entry> entry = ReadEntry("serialization", &value);

  std::unique_ptr<base::DictionaryValue> serialized(entry->Serialize());

  // We can't just compare values, since during deserialization some of the
  // lists get converted to std::sets, which are sorted, so Value::Equals will
  // fail.
  std::unique_ptr<Entry> reconstituted = Entry::Deserialize(*serialized.get());
  EXPECT_EQ(*entry, *reconstituted);
}

TEST_F(EntryTest, Malformed) {
  std::unique_ptr<base::Value> value = ReadManifest("malformed");
  EXPECT_FALSE(value.get());
}


}  // namespace catalog
