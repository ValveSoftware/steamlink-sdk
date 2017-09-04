// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/clipboard/DataObject.h"

#include "core/clipboard/DataObjectItem.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DataObjectTest : public ::testing::Test {
 public:
  DataObjectTest() : m_dataObject(DataObject::create()) {}

 protected:
  Persistent<DataObject> m_dataObject;
};

TEST_F(DataObjectTest, addItemWithFilenameAndNoTitle) {
  String filePath = testing::blinkRootDir();
  filePath.append("/Source/core/clipboard/DataObjectTest.cpp");

  m_dataObject->addFilename(filePath, String(), String());
  EXPECT_EQ(1U, m_dataObject->length());

  DataObjectItem* item = m_dataObject->item(0);
  EXPECT_EQ(DataObjectItem::FileKind, item->kind());

  Blob* blob = item->getAsFile();
  ASSERT_TRUE(blob->isFile());
  File* file = toFile(blob);
  EXPECT_TRUE(file->hasBackingFile());
  EXPECT_EQ(File::IsUserVisible, file->getUserVisibility());
  EXPECT_EQ(filePath, file->path());
}

TEST_F(DataObjectTest, addItemWithFilenameAndTitle) {
  String filePath = testing::blinkRootDir();
  filePath.append("/Source/core/clipboard/DataObjectTest.cpp");

  m_dataObject->addFilename(filePath, "name.cpp", String());
  EXPECT_EQ(1U, m_dataObject->length());

  DataObjectItem* item = m_dataObject->item(0);
  EXPECT_EQ(DataObjectItem::FileKind, item->kind());

  Blob* blob = item->getAsFile();
  ASSERT_TRUE(blob->isFile());
  File* file = toFile(blob);
  EXPECT_TRUE(file->hasBackingFile());
  EXPECT_EQ(File::IsUserVisible, file->getUserVisibility());
  EXPECT_EQ(filePath, file->path());
  EXPECT_EQ("name.cpp", file->name());
}

TEST_F(DataObjectTest, fileSystemId) {
  String filePath = testing::blinkRootDir();
  filePath.append("/Source/core/clipboard/DataObjectTest.cpp");
  KURL url;

  m_dataObject->addFilename(filePath, String(), String());
  m_dataObject->addFilename(filePath, String(), "fileSystemIdForFilename");
  m_dataObject->add(
      File::createForFileSystemFile(url, FileMetadata(), File::IsUserVisible),
      "fileSystemIdForFileSystemFile");

  ASSERT_EQ(3U, m_dataObject->length());

  {
    DataObjectItem* item = m_dataObject->item(0);
    EXPECT_FALSE(item->hasFileSystemId());
  }

  {
    DataObjectItem* item = m_dataObject->item(1);
    EXPECT_TRUE(item->hasFileSystemId());
    EXPECT_EQ("fileSystemIdForFilename", item->fileSystemId());
  }

  {
    DataObjectItem* item = m_dataObject->item(2);
    EXPECT_TRUE(item->hasFileSystemId());
    EXPECT_EQ("fileSystemIdForFileSystemFile", item->fileSystemId());
  }
}

}  // namespace blink
