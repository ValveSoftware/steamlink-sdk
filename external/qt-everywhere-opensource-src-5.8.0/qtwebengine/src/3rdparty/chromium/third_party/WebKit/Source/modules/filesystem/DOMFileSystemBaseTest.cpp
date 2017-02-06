// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/filesystem/DOMFileSystemBase.h"

#include "core/fileapi/File.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace blink {

class DOMFileSystemBaseTest : public ::testing::Test {
public:
    DOMFileSystemBaseTest()
    {
        m_filePath = testing::blinkRootDir();
        m_filePath.append("/Source/modules/filesystem/DOMFileSystemBaseTest.cpp");
        getFileMetadata(m_filePath, m_fileMetadata);
        m_fileMetadata.platformPath = m_filePath;
    }

protected:
    String m_filePath;
    FileMetadata m_fileMetadata;
};


TEST_F(DOMFileSystemBaseTest, externalFilesystemFilesAreUserVisible)
{
    KURL rootUrl = DOMFileSystemBase::createFileSystemRootURL("http://chromium.org/", FileSystemTypeExternal);

    File* file = DOMFileSystemBase::createFile(m_fileMetadata, rootUrl, FileSystemTypeExternal, "DOMFileSystemBaseTest.cpp");
    EXPECT_TRUE(file);
    EXPECT_TRUE(file->hasBackingFile());
    EXPECT_EQ(File::IsUserVisible, file->getUserVisibility());
    EXPECT_EQ("DOMFileSystemBaseTest.cpp", file->name());
    EXPECT_EQ(m_filePath, file->path());
}

TEST_F(DOMFileSystemBaseTest, temporaryFilesystemFilesAreNotUserVisible)
{
    KURL rootUrl = DOMFileSystemBase::createFileSystemRootURL("http://chromium.org/", FileSystemTypeTemporary);

    File* file = DOMFileSystemBase::createFile(m_fileMetadata, rootUrl, FileSystemTypeTemporary, "UserVisibleName.txt");
    EXPECT_TRUE(file);
    EXPECT_TRUE(file->hasBackingFile());
    EXPECT_EQ(File::IsNotUserVisible, file->getUserVisibility());
    EXPECT_EQ("UserVisibleName.txt", file->name());
    EXPECT_EQ(m_filePath, file->path());
}

TEST_F(DOMFileSystemBaseTest, persistentFilesystemFilesAreNotUserVisible)
{
    KURL rootUrl = DOMFileSystemBase::createFileSystemRootURL("http://chromium.org/", FileSystemTypePersistent);

    File* file = DOMFileSystemBase::createFile(m_fileMetadata, rootUrl, FileSystemTypePersistent, "UserVisibleName.txt");
    EXPECT_TRUE(file);
    EXPECT_TRUE(file->hasBackingFile());
    EXPECT_EQ(File::IsNotUserVisible, file->getUserVisibility());
    EXPECT_EQ("UserVisibleName.txt", file->name());
    EXPECT_EQ(m_filePath, file->path());
}

} // namespace blink
