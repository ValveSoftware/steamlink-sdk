// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "device/generic_sensor/linux/platform_sensor_utils_linux.h"
#include "device/generic_sensor/linux/sensor_data_linux.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

const base::FilePath::CharType* kDevice0Dir = FILE_PATH_LITERAL("device0");

const char kSensorFileNameTest1[] = "sensor_data1";
const char kSensorFileNameTest2[] = "sensor_data2";
const char kSensorFileNameTest3[] = "sensor_data3";

const char* kTestSensorFileNamesTest[3][5] = {
    {
        kSensorFileNameTest1, "sensor1_input", "sensor1_raw_input", "sensor1",
        "sensor1_data_raw",
    },
    {
        "sensor2", kSensorFileNameTest2, "sensor2_raw_input", "sensor2_input",
        "sensor2_data_raw",
    },
    {
        "sensor3", "sensor3_input", "sensor3_raw_input", "sensor3_data_raw",
        kSensorFileNameTest3,
    },
};

const char kTestSensorFileNameScaling[] = "test_scaling";

void CreateFile(const base::FilePath& file) {
  EXPECT_EQ(base::WriteFile(file, nullptr, 0), 0);
}

void DeleteFile(const base::FilePath& file) {
  EXPECT_TRUE(base::DeleteFile(file, false));
}

void WriteValueToFile(const base::FilePath& path, double value) {
  const std::string str = base::DoubleToString(value);
  int bytes_written = base::WriteFile(path, str.data(), str.size());
  EXPECT_EQ(static_cast<size_t>(bytes_written), str.size());
}

}  // namespace

class SensorReaderTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(base_temp_dir_.CreateUniqueTempDir());
    base_dir_ = base_temp_dir_.GetPath();
    sensors_dir_ = base_dir_.Append(kDevice0Dir);
    ASSERT_TRUE(base::CreateDirectory(sensors_dir_));
  }

  // Deletes base dir recursively.
  void TearDown() override { ASSERT_TRUE(base_temp_dir_.Delete()); }

  // Initialize SensorDataLinux with values for a sensor reader.
  void InitSensorDataForTest(
      size_t rows,
      const SensorDataLinux::ReaderFunctor& apply_scaling_func,
      SensorDataLinux* data) {
    // Corresponds to maximum values in SensorReading.
    // We must read only from up to three files. Thus - 3 sets of files
    // should be fill in here.
    const size_t max_rows = 3;
    if (rows > 3)
      rows = max_rows;

    data->apply_scaling_func = apply_scaling_func;
    data->sensor_scale_name = kTestSensorFileNameScaling;
    data->base_path_sensor_linux = base_dir_.value().c_str();
    for (size_t i = 0; i < rows; ++i) {
      std::vector<std::string> file_names(
          kTestSensorFileNamesTest[i],
          kTestSensorFileNamesTest[i] + arraysize(kTestSensorFileNamesTest[i]));
      data->sensor_file_names.push_back(std::move(file_names));
    }
  }

  // Check SensorReading values are properly read.
  void CheckSensorDataFields(const SensorReading& data,
                             double value1,
                             double value2,
                             double value3) {
    EXPECT_EQ(value1, data.values[0]);
    EXPECT_EQ(value2, data.values[1]);
    EXPECT_EQ(value3, data.values[2]);
  }

 protected:
  // Holds a path to a sensor dir that is located in |base_dir_|
  base::FilePath sensors_dir_;
  // Holds a path to a base dir.
  base::FilePath base_dir_;
  // Holds base dir where a sensor dir is located.
  base::ScopedTempDir base_temp_dir_;
};

// Test a reader is not created if sensor read files
// do not exist.
TEST_F(SensorReaderTest, FileDoesNotExist) {
  const char* kGiberishFiles[] = {"temp1", "temp2", "temp3", "temp4"};
  const size_t rows = 3;
  const double scaling_value = 0.1234;
  // Create some gibberish files that we are not interested in.
  for (unsigned int i = 0; i < arraysize(kGiberishFiles); ++i) {
    base::FilePath some_file = sensors_dir_.Append(kGiberishFiles[i]);
    CreateFile(some_file);
  }

  // Create a file with a scaling value.
  base::FilePath temp_sensor_scale_file =
      sensors_dir_.Append(kTestSensorFileNameScaling);
  CreateFile(temp_sensor_scale_file);
  // Write a scaling value to the file.
  WriteValueToFile(temp_sensor_scale_file, scaling_value);

  SensorDataLinux sensor_data;
  SensorDataLinux::ReaderFunctor empty_func;
  InitSensorDataForTest(rows, empty_func, &sensor_data);

  std::unique_ptr<SensorReader> reader = SensorReader::Create(sensor_data);
  EXPECT_FALSE(reader);
}

// Test a reader is still created if a file with a scaling value
// does not exist.
TEST_F(SensorReaderTest, ScalingFileDoesNotExist) {
  const size_t rows = 1;
  // Create a test sensor file, which must be found.
  base::FilePath temp_sensor_file1 = sensors_dir_.Append(kSensorFileNameTest1);
  CreateFile(temp_sensor_file1);

  SensorDataLinux sensor_data;
  SensorDataLinux::ReaderFunctor empty_func;
  InitSensorDataForTest(rows, empty_func, &sensor_data);

  std::unique_ptr<SensorReader> reader = SensorReader::Create(sensor_data);
  EXPECT_TRUE(reader);
}

// Test a reader is not created if a scaling file exists, but cannot be read
// from.
TEST_F(SensorReaderTest, CannotReadFromScalingFile) {
  const size_t rows = 1;
  // Create a test sensor file, which must be found.
  base::FilePath temp_sensor_file1 = sensors_dir_.Append(kSensorFileNameTest1);
  CreateFile(temp_sensor_file1);

  // Create a file with a scaling value, which must be found.
  base::FilePath temp_sensor_scale_file =
      sensors_dir_.Append(kTestSensorFileNameScaling);
  CreateFile(temp_sensor_scale_file);

  SensorDataLinux sensor_data;
  SensorDataLinux::ReaderFunctor empty_func;
  InitSensorDataForTest(rows, empty_func, &sensor_data);

  std::unique_ptr<SensorReader> reader = SensorReader::Create(sensor_data);
  EXPECT_FALSE(reader);
}

// Simulate a sensor, which has only one file to be read from.
TEST_F(SensorReaderTest, ReadValueFromOneFile) {
  const size_t rows = 1;
  const double value1 = 20;
  const double zero_value = 0;
  // Create a test sensor file, which must be found to be read from.
  base::FilePath temp_sensor_file = sensors_dir_.Append(kSensorFileNameTest1);
  CreateFile(temp_sensor_file);

  // Initialize sensor data for a reader.
  SensorDataLinux sensor_data;
  SensorDataLinux::ReaderFunctor empty_func;
  InitSensorDataForTest(rows, empty_func, &sensor_data);

  std::unique_ptr<SensorReader> reader = SensorReader::Create(sensor_data);
  EXPECT_TRUE(reader);

  // Write a value to the file.
  WriteValueToFile(temp_sensor_file, value1);

  // Fill SensorReading's first field with read value. Other fields must
  // be 0.
  SensorReading reading;
  EXPECT_TRUE(reader->ReadSensorReading(&reading));
  CheckSensorDataFields(reading, value1, zero_value, zero_value);
}

// Simulate a sensor, which has two files to be read from.
TEST_F(SensorReaderTest, ReadValuesFromTwoFiles) {
  const size_t rows = 2;
  const double value1 = 20;
  const double value2 = 50;
  const double zero_value = 0;
  // Create a test sensor file, which must be found.
  base::FilePath temp_sensor_file1 = sensors_dir_.Append(kSensorFileNameTest1);
  CreateFile(temp_sensor_file1);

  // Create another test sensor file, which must be found.
  base::FilePath temp_sensor_file2 = sensors_dir_.Append(kSensorFileNameTest2);
  CreateFile(temp_sensor_file2);

  // Initialize sensor data for a reader.
  SensorDataLinux sensor_data;
  SensorDataLinux::ReaderFunctor empty_func;
  InitSensorDataForTest(rows, empty_func, &sensor_data);

  std::unique_ptr<SensorReader> reader = SensorReader::Create(sensor_data);
  EXPECT_TRUE(reader);

  // Write a value to the file.
  WriteValueToFile(temp_sensor_file1, value1);
  WriteValueToFile(temp_sensor_file2, value2);

  // Fill SensorReading's two first fields with read value. Last field must
  // be 0.
  SensorReading reading;
  EXPECT_TRUE(reader->ReadSensorReading(&reading));
  CheckSensorDataFields(reading, value1, value2, zero_value);
}

// Simulate a sensor, which has the files to be read from.
// After read is successful, remove one of the files and try
// to read again. Reading must fail then.
TEST_F(SensorReaderTest, ReadValuesFromThreeFilesAndFail) {
  const size_t rows = 4;
  const double value1 = 20;
  const double value2 = 50;
  const double value3 = 80;
  const double zero_value = 0;
  // Create a test sensor file, which must be found.
  base::FilePath temp_sensor_file1 = sensors_dir_.Append(kSensorFileNameTest1);
  CreateFile(temp_sensor_file1);

  // Create another test sensor file, which must be found.
  base::FilePath temp_sensor_file2 = sensors_dir_.Append(kSensorFileNameTest2);
  CreateFile(temp_sensor_file2);

  // Create third test sensor file, which must be found.
  base::FilePath temp_sensor_file3 = sensors_dir_.Append(kSensorFileNameTest3);
  CreateFile(temp_sensor_file3);

  // Initialize sensor data for a reader.
  SensorDataLinux sensor_data;
  SensorDataLinux::ReaderFunctor empty_func;
  InitSensorDataForTest(rows, empty_func, &sensor_data);

  std::unique_ptr<SensorReader> reader = SensorReader::Create(sensor_data);
  EXPECT_TRUE(reader);

  // Write values to the files.
  WriteValueToFile(temp_sensor_file1, value1);
  WriteValueToFile(temp_sensor_file2, value2);
  WriteValueToFile(temp_sensor_file3, value3);

  // Fill SensorReading's values with data from files.
  SensorReading reading;
  EXPECT_TRUE(reader->ReadSensorReading(&reading));
  CheckSensorDataFields(reading, value1, value2, value3);

  SensorReading reading2;
  DeleteFile(temp_sensor_file2);
  EXPECT_FALSE(reader->ReadSensorReading(&reading2));
  CheckSensorDataFields(reading2, zero_value, zero_value, zero_value);
}

// Fill in SensorDataLinux with three arrays of files that must be found
// before creating a sensor reader. If even one file is not found,
// a sensor reader must not be created. As soon as all the files are found,
// check the reader is created.
TEST_F(SensorReaderTest, SensorReadFilesDoNotExist) {
  const size_t rows = 3;
  // Create a test sensor file, which must be found. Other
  // files will not be created and the test must fail to create a reader.
  base::FilePath temp_sensor_file1 = sensors_dir_.Append(kSensorFileNameTest1);
  CreateFile(temp_sensor_file1);

  // Initialize sensor data for a reader.
  SensorDataLinux sensor_data;
  SensorDataLinux::ReaderFunctor empty_func;
  InitSensorDataForTest(rows, empty_func, &sensor_data);

  std::unique_ptr<SensorReader> reader = SensorReader::Create(sensor_data);
  EXPECT_FALSE(reader);

  // Create one more file. The reader mustn't be created as long as it
  // expects three files to be found.
  base::FilePath temp_sensor_file2 = sensors_dir_.Append(kSensorFileNameTest2);
  CreateFile(temp_sensor_file2);

  reader.reset();
  reader = SensorReader::Create(sensor_data);
  EXPECT_FALSE(reader);

  // Create last file.
  base::FilePath temp_sensor_file3 = sensors_dir_.Append(kSensorFileNameTest3);
  CreateFile(temp_sensor_file3);

  reader.reset();
  reader = SensorReader::Create(sensor_data);
  EXPECT_TRUE(reader);
}

// Fill in SensorDataLinux with three arrays of files that must be found
// before creating a sensor reader. Create a file with a scaling value that
// must be applied. Pass a func to a SensorReader that will be used to
// apply scalings.
TEST_F(SensorReaderTest, CheckSensorReadingScalingApplied) {
  const size_t rows = 3;
  const double value1 = 20;
  const double value2 = 50;
  const double value3 = 80;
  const double scaling_value = 0.1234;
  // Create a file with a scaling value, which must be found.
  base::FilePath temp_sensor_scale_file =
      sensors_dir_.Append(kTestSensorFileNameScaling);
  CreateFile(temp_sensor_scale_file);

  // Create a test sensor file, which must be found. Other
  // files will not be created and the test must fail to create a reader.
  base::FilePath temp_sensor_file1 = sensors_dir_.Append(kSensorFileNameTest1);
  CreateFile(temp_sensor_file1);

  // Create one more file. The reader mustn't be created as long as it
  // expects three files to be found.
  base::FilePath temp_sensor_file2 = sensors_dir_.Append(kSensorFileNameTest2);
  CreateFile(temp_sensor_file2);

  // Create last file.
  base::FilePath temp_sensor_file3 = sensors_dir_.Append(kSensorFileNameTest3);
  CreateFile(temp_sensor_file3);

  // Write value to the files.
  WriteValueToFile(temp_sensor_file1, value1);
  WriteValueToFile(temp_sensor_file2, value2);
  WriteValueToFile(temp_sensor_file3, value3);
  WriteValueToFile(temp_sensor_scale_file, scaling_value);

  // Initialize sensor data for a reader.
  SensorDataLinux sensor_data;
  SensorDataLinux::ReaderFunctor apply_scaling_func =
      base::Bind([](double scaling_value_in_reader, SensorReading& reading) {
        reading.values[0] = scaling_value_in_reader * reading.values[0];
        reading.values[1] = -scaling_value_in_reader * reading.values[1];
        reading.values[2] = scaling_value_in_reader * reading.values[2];
      });
  InitSensorDataForTest(rows, apply_scaling_func, &sensor_data);

  std::unique_ptr<SensorReader> reader = SensorReader::Create(sensor_data);
  EXPECT_TRUE(reader);

  // Fill SensorReading's values with data from files.
  SensorReading reading;
  EXPECT_TRUE(reader->ReadSensorReading(&reading));
  CheckSensorDataFields(reading, value1 * scaling_value,
                        value2 * (-scaling_value), value3 * scaling_value);
}

}  // namespace device
