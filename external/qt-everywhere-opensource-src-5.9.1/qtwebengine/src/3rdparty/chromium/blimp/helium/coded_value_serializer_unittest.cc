// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/coded_value_serializer.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "blimp/helium/version_vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

class CodedValueSerializerTest : public testing::Test {
 public:
  CodedValueSerializerTest() {}
  ~CodedValueSerializerTest() override = default;

 protected:
  template <class T>
  void SerializeAndDeserialize(const std::vector<T>& input) {
    std::string changeset;
    google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
    google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);

    for (size_t i = 0; i < input.size(); ++i) {
      CodedValueSerializer::Serialize(input[i], &output_stream);
    }

    google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                            changeset.size());
    google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);

    for (size_t i = 0; i < input.size(); ++i) {
      CheckCorrectDeserialization<T>(&input_stream, input[i]);
    }
  }

  template <class T>
  void CheckCorrectDeserialization(
      google::protobuf::io::CodedInputStream* input_stream,
      T expected) {
    T output;
    EXPECT_TRUE(CodedValueSerializer::Deserialize(input_stream, &output));
    EXPECT_EQ(expected, output);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CodedValueSerializerTest);
};

template <>
void CodedValueSerializerTest::CheckCorrectDeserialization<VersionVector>(
    google::protobuf::io::CodedInputStream* input_stream,
    VersionVector expected) {
  VersionVector output;
  EXPECT_TRUE(CodedValueSerializer::Deserialize(input_stream, &output));
  EXPECT_EQ(VersionVector::Comparison::EqualTo, expected.CompareTo(output));
}

TEST_F(CodedValueSerializerTest, SerializesMixedTypes) {
  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);

  CodedValueSerializer::Serialize(345, &output_stream);
  CodedValueSerializer::Serialize("some string", &output_stream);
  CodedValueSerializer::Serialize(VersionVector(12, 13), &output_stream);

  google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                          changeset.size());
  google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);

  CheckCorrectDeserialization<int32_t>(&input_stream, 345);
  CheckCorrectDeserialization<std::string>(&input_stream, "some string");
  CheckCorrectDeserialization<VersionVector>(&input_stream,
                                             VersionVector(12, 13));
}

TEST_F(CodedValueSerializerTest, SerializesInt32) {
  SerializeAndDeserialize<int32_t>({123, 0, -456, 987});
}

TEST_F(CodedValueSerializerTest, SerializesString) {
  SerializeAndDeserialize<std::string>({"Hello, World", "", "Goodbye, World!"});
}

TEST_F(CodedValueSerializerTest,
       FailsToDeserializeStringGivenOutrageousLength) {
  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  output_stream.WriteVarint32(20 << 20);  // 20MB, greater than kMaxStringLength
  output_stream.WriteString("This string is 20MB, I swear!");

  google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                          changeset.size());
  google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);

  std::string output;
  EXPECT_FALSE(CodedValueSerializer::Deserialize(&input_stream, &output));
}

TEST_F(CodedValueSerializerTest, SerializesVersionVector) {
  SerializeAndDeserialize<VersionVector>(
      {VersionVector(0, 0), VersionVector(3, 1), VersionVector(0, 4),
       VersionVector(3, 0)});
}

}  // namespace
}  // namespace helium
}  // namespace blimp
