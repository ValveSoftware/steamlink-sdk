// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/pref_hash_store_impl.h"

#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "components/user_prefs/tracked/dictionary_hash_store_contents.h"
#include "components/user_prefs/tracked/hash_store_contents.h"
#include "components/user_prefs/tracked/pref_hash_store_impl.h"
#include "components/user_prefs/tracked/pref_hash_store_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefHashStoreImplTest : public testing::Test {
 protected:
  std::unique_ptr<HashStoreContents> CreateHashStoreContents() {
    return std::unique_ptr<HashStoreContents>(
        new DictionaryHashStoreContents(&pref_store_contents_));
  }

 private:
  base::DictionaryValue pref_store_contents_;
};

TEST_F(PrefHashStoreImplTest, AtomicHashStoreAndCheck) {
  base::StringValue string_1("string1");
  base::StringValue string_2("string2");

  {
    // 32 NULL bytes is the seed that was used to generate the legacy hash.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));

    // Only NULL should be trusted in the absence of a hash.
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", NULL));

    transaction->StoreHash("path1", &string_1);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::CLEARED,
              transaction->CheckValue("path1", NULL));
    transaction->StoreHash("path1", NULL);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", NULL));
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckValue("path1", &string_2));

    base::DictionaryValue dict;
    dict.Set("a", new base::StringValue("foo"));
    dict.Set("d", new base::StringValue("bad"));
    dict.Set("b", new base::StringValue("bar"));
    dict.Set("c", new base::StringValue("baz"));

    transaction->StoreHash("path1", &dict);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &dict));
  }

  ASSERT_FALSE(CreateHashStoreContents()->GetSuperMac().empty());

  {
    // |pref_hash_store2| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store2(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction(CreateHashStoreContents()));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_NULL_VALUE,
              transaction->CheckValue("new_path", NULL));
  }

  // Manually corrupt the super MAC.
  CreateHashStoreContents()->SetSuperMac(std::string(64, 'A'));

  {
    // |pref_hash_store3| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust non-NULL unknown values.
    PrefHashStoreImpl pref_hash_store3(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store3.BeginTransaction(CreateHashStoreContents()));
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_NULL_VALUE,
              transaction->CheckValue("new_path", NULL));
  }
}

TEST_F(PrefHashStoreImplTest, ImportExportOperations) {
  base::StringValue string_1("string1");
  base::StringValue string_2("string2");

  // Initial state: no super MAC.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperMACValid());

    ASSERT_FALSE(transaction->HasHash("path1"));

    // Storing a hash will stamp the super MAC.
    transaction->StoreHash("path1", &string_1);

    ASSERT_TRUE(transaction->HasHash("path1"));
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckValue("path1", &string_2));
  }

  // Make a copy of the stored hash for future use.
  const base::Value* hash = NULL;
  ASSERT_TRUE(CreateHashStoreContents()->GetContents()->Get("path1", &hash));
  std::unique_ptr<base::Value> path_1_string_1_hash_copy(hash->DeepCopy());
  hash = NULL;

  // Verify that the super MAC was stamped.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());
    ASSERT_TRUE(transaction->HasHash("path1"));

    // Clearing the hash should preserve validity.
    transaction->ClearHash("path1");

    // The effects of the clear should be immediately visible.
    ASSERT_FALSE(transaction->HasHash("path1"));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", NULL));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
  }

  // Verify that validity was preserved and that the clear took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());
    ASSERT_FALSE(transaction->HasHash("path1"));
  }

  // Invalidate the super MAC.
  CreateHashStoreContents()->SetSuperMac(std::string());

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperMACValid());
    ASSERT_FALSE(transaction->HasHash("path1"));

    // An import should preserve invalidity.
    transaction->ImportHash("path1", path_1_string_1_hash_copy.get());

    ASSERT_TRUE(transaction->HasHash("path1"));

    // The imported hash should be usable for validating the original value.
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
  }

  // Verify that invalidity was preserved and that the import took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperMACValid());
    ASSERT_TRUE(transaction->HasHash("path1"));
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_1));

    // After clearing the hash, non-null values are UNTRUSTED_UNKNOWN.
    transaction->ClearHash("path1");

    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_NULL_VALUE,
              transaction->CheckValue("path1", NULL));
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
  }

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_FALSE(transaction->IsSuperMACValid());

    // Test StampSuperMac.
    transaction->StampSuperMac();
  }

  // Verify that the store is now valid.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());

    // Store the hash of a different value to test an "over-import".
    transaction->StoreHash("path1", &string_2);
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_2));
  }

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());

    // "Over-import". An import should preserve validity.
    transaction->ImportHash("path1", path_1_string_1_hash_copy.get());
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckValue("path1", &string_2));
  }

  // Verify that validity was preserved and the "over-import" took effect.
  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));
    ASSERT_TRUE(transaction->IsSuperMACValid());
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckValue("path1", &string_2));
  }
}

TEST_F(PrefHashStoreImplTest, SuperMACDisabled) {
  base::StringValue string_1("string1");
  base::StringValue string_2("string2");

  {
    // Pass |use_super_mac| => false.
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", false);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));

    transaction->StoreHash("path1", &string_2);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_2));
  }

  ASSERT_TRUE(CreateHashStoreContents()->GetSuperMac().empty());

  {
    PrefHashStoreImpl pref_hash_store2(std::string(32, 0), "device_id", false);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction(CreateHashStoreContents()));
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
  }
}

TEST_F(PrefHashStoreImplTest, SplitHashStoreAndCheck) {
  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("to be replaced"));
  dict.Set("b", new base::StringValue("same"));
  dict.Set("o", new base::StringValue("old"));

  base::DictionaryValue modified_dict;
  modified_dict.Set("a", new base::StringValue("replaced"));
  modified_dict.Set("b", new base::StringValue("same"));
  modified_dict.Set("c", new base::StringValue("new"));

  base::DictionaryValue empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));

    // No hashes stored yet and hashes dictionary is empty (and thus not
    // trusted).
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    transaction->StoreSplitHash("path1", &dict);

    // Verify match post storage.
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify new path is still unknown.
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path2", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify NULL or empty dicts are declared as having been cleared.
    EXPECT_EQ(PrefHashStoreTransaction::CLEARED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(
        PrefHashStoreTransaction::CLEARED,
        transaction->CheckSplitValue("path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify changes are properly detected.
    EXPECT_EQ(
        PrefHashStoreTransaction::CHANGED,
        transaction->CheckSplitValue("path1", &modified_dict, &invalid_keys));
    std::vector<std::string> expected_invalid_keys1;
    expected_invalid_keys1.push_back("a");
    expected_invalid_keys1.push_back("c");
    expected_invalid_keys1.push_back("o");
    EXPECT_EQ(expected_invalid_keys1, invalid_keys);
    invalid_keys.clear();

    // Verify |dict| still matches post check.
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Store hash for |modified_dict|.
    transaction->StoreSplitHash("path1", &modified_dict);

    // Verify |modified_dict| is now the one that verifies correctly.
    EXPECT_EQ(
        PrefHashStoreTransaction::UNCHANGED,
        transaction->CheckSplitValue("path1", &modified_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify old dict no longer matches.
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    std::vector<std::string> expected_invalid_keys2;
    expected_invalid_keys2.push_back("a");
    expected_invalid_keys2.push_back("o");
    expected_invalid_keys2.push_back("c");
    EXPECT_EQ(expected_invalid_keys2, invalid_keys);
    invalid_keys.clear();
  }

  {
    // |pref_hash_store2| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store2(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction(CreateHashStoreContents()));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  // Manually corrupt the super MAC.
  CreateHashStoreContents()->SetSuperMac(std::string(64, 'A'));

  {
    // |pref_hash_store3| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust unknown values.
    PrefHashStoreImpl pref_hash_store3(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store3.BeginTransaction(CreateHashStoreContents()));
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

TEST_F(PrefHashStoreImplTest, EmptyAndNULLSplitDict) {
  base::DictionaryValue empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));

    // Store hashes for a random dict to be overwritten below.
    base::DictionaryValue initial_dict;
    initial_dict.Set("a", new base::StringValue("foo"));
    transaction->StoreSplitHash("path1", &initial_dict);

    // Verify stored empty dictionary matches NULL and empty dictionary back.
    transaction->StoreSplitHash("path1", &empty_dict);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(
        PrefHashStoreTransaction::UNCHANGED,
        transaction->CheckSplitValue("path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Same when storing NULL directly.
    transaction->StoreSplitHash("path1", NULL);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(
        PrefHashStoreTransaction::UNCHANGED,
        transaction->CheckSplitValue("path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  {
    // |pref_hash_store2| should trust its initial hashes dictionary (and thus
    // trust new unknown values) even though the last action done was to clear
    // the hashes for path1 by setting its value to NULL (this is a regression
    // test ensuring that the internal action of clearing some hashes does
    // update the stored hash of hashes).
    PrefHashStoreImpl pref_hash_store2(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction(CreateHashStoreContents()));

    base::DictionaryValue tested_dict;
    tested_dict.Set("a", new base::StringValue("foo"));
    tested_dict.Set("b", new base::StringValue("bar"));
    EXPECT_EQ(
        PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
        transaction->CheckSplitValue("new_path", &tested_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

// Test that the PrefHashStore returns TRUSTED_UNKNOWN_VALUE when checking for
// a split preference even if there is an existing atomic preference's hash
// stored. There is no point providing a migration path for preferences
// switching strategies after their initial release as split preferences are
// turned into split preferences specifically because the atomic hash isn't
// considered useful.
TEST_F(PrefHashStoreImplTest, TrustedUnknownSplitValueFromExistingAtomic) {
  base::StringValue string("string1");

  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("foo"));
  dict.Set("d", new base::StringValue("bad"));
  dict.Set("b", new base::StringValue("bar"));
  dict.Set("c", new base::StringValue("baz"));

  {
    PrefHashStoreImpl pref_hash_store(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction(CreateHashStoreContents()));

    transaction->StoreHash("path1", &string);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string));
  }

  {
    // Load a new |pref_hash_store2| in which the hashes dictionary is trusted.
    PrefHashStoreImpl pref_hash_store2(std::string(32, 0), "device_id", true);
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction(CreateHashStoreContents()));
    std::vector<std::string> invalid_keys;
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}
