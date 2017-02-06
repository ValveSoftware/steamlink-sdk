// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "mojo/common/common_custom_types.mojom.h"
#include "mojo/common/test_common_custom_types.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace common {
namespace test {
namespace {

template <typename T>
struct BounceTestTraits {
  static void ExpectEquality(const T& a, const T& b) {
    EXPECT_EQ(a, b);
  }
};

template <>
struct BounceTestTraits<base::DictionaryValue> {
  static void ExpectEquality(const base::DictionaryValue& a,
                             const base::DictionaryValue& b) {
    EXPECT_TRUE(a.Equals(&b));
  }
};

template <>
struct BounceTestTraits<base::ListValue> {
  static void ExpectEquality(const base::ListValue& a,
                             const base::ListValue& b) {
    EXPECT_TRUE(a.Equals(&b));
  }
};

template <typename T>
void DoExpectResponse(T* expected_value,
                      const base::Closure& closure,
                      const T& value) {
  BounceTestTraits<T>::ExpectEquality(*expected_value, value);
  closure.Run();
}

template <typename T>
base::Callback<void(const T&)> ExpectResponse(T* expected_value,
                                              const base::Closure& closure) {
  return base::Bind(&DoExpectResponse<T>, expected_value, closure);
}

class TestFilePathImpl : public TestFilePath {
 public:
  explicit TestFilePathImpl(TestFilePathRequest request)
      : binding_(this, std::move(request)) {}

  // TestFilePath implementation:
  void BounceFilePath(const base::FilePath& in,
                      const BounceFilePathCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<TestFilePath> binding_;
};

class TestTimeImpl : public TestTime {
 public:
  explicit TestTimeImpl(TestTimeRequest request)
      : binding_(this, std::move(request)) {}

  // TestTime implementation:
  void BounceTime(const base::Time& in,
                  const BounceTimeCallback& callback) override {
    callback.Run(in);
  }

  void BounceTimeDelta(const base::TimeDelta& in,
                  const BounceTimeDeltaCallback& callback) override {
    callback.Run(in);
  }

  void BounceTimeTicks(const base::TimeTicks& in,
                  const BounceTimeTicksCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<TestTime> binding_;
};

class TestValueImpl : public TestValue {
 public:
  explicit TestValueImpl(TestValueRequest request)
      : binding_(this, std::move(request)) {}

  // TestValue implementation:
  void BounceDictionaryValue(
      const base::DictionaryValue& in,
      const BounceDictionaryValueCallback& callback) override {
    callback.Run(in);
  }
  void BounceListValue(const base::ListValue& in,
                       const BounceListValueCallback& callback) override {
    callback.Run(in);
  }

 private:
  mojo::Binding<TestValue> binding_;
};

class CommonCustomTypesTest : public testing::Test {
 protected:
  CommonCustomTypesTest() {}
  ~CommonCustomTypesTest() override {}

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(CommonCustomTypesTest);
};

}  // namespace

TEST_F(CommonCustomTypesTest, FilePath) {
  base::RunLoop run_loop;

  TestFilePathPtr ptr;
  TestFilePathImpl impl(GetProxy(&ptr));

  base::FilePath dir(FILE_PATH_LITERAL("hello"));
  base::FilePath file = dir.Append(FILE_PATH_LITERAL("world"));

  ptr->BounceFilePath(file, ExpectResponse(&file, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, Time) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(GetProxy(&ptr));

  base::Time t = base::Time::Now();

  ptr->BounceTime(t, ExpectResponse(&t, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, TimeDelta) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(GetProxy(&ptr));

  base::TimeDelta t = base::TimeDelta::FromDays(123);

  ptr->BounceTimeDelta(t, ExpectResponse(&t, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, TimeTicks) {
  base::RunLoop run_loop;

  TestTimePtr ptr;
  TestTimeImpl impl(GetProxy(&ptr));

  base::TimeTicks t = base::TimeTicks::Now();

  ptr->BounceTimeTicks(t, ExpectResponse(&t, run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(CommonCustomTypesTest, Value) {
  TestValuePtr ptr;
  TestValueImpl impl(GetProxy(&ptr));

  base::DictionaryValue dict;
  dict.SetBoolean("bool", false);
  dict.SetInteger("int", 2);
  dict.SetString("string", "some string");
  dict.SetBoolean("nested.bool", true);
  dict.SetInteger("nested.int", 9);
  dict.Set("some_binary", base::BinaryValue::CreateWithCopiedBuffer("mojo", 4));
  {
    std::unique_ptr<base::ListValue> dict_list(new base::ListValue());
    dict_list->AppendString("string");
    dict_list->AppendBoolean(true);
    dict.Set("list", std::move(dict_list));
  }
  {
    base::RunLoop run_loop;
    ptr->BounceDictionaryValue(
        dict, ExpectResponse(&dict, run_loop.QuitClosure()));
    run_loop.Run();
  }

  base::ListValue list;
  list.AppendString("string");
  list.AppendDouble(42.1);
  list.AppendBoolean(true);
  list.Append(base::BinaryValue::CreateWithCopiedBuffer("mojo", 4));
  {
    std::unique_ptr<base::DictionaryValue> list_dict(
        new base::DictionaryValue());
    list_dict->SetString("string", "str");
    list.Append(std::move(list_dict));
  }
  {
    base::RunLoop run_loop;
    ptr->BounceListValue(list, ExpectResponse(&list, run_loop.QuitClosure()));
    run_loop.Run();
  }
}

}  // namespace test
}  // namespace common
}  // namespace mojo
