// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_PRIVATE_API_H_

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/image_writer_private.h"

namespace extensions {

class ImageWriterPrivateWriteFromUrlFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.writeFromUrl",
                             IMAGEWRITER_WRITEFROMURL)
  ImageWriterPrivateWriteFromUrlFunction();

 private:
  ~ImageWriterPrivateWriteFromUrlFunction() override;
  bool RunAsync() override;
  void OnWriteStarted(bool success, const std::string& error);
};

class ImageWriterPrivateWriteFromFileFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.writeFromFile",
                             IMAGEWRITER_WRITEFROMFILE)
  ImageWriterPrivateWriteFromFileFunction();

 private:
  ~ImageWriterPrivateWriteFromFileFunction() override;
  bool RunAsync() override;
  void OnWriteStarted(bool success, const std::string& error);
};

class ImageWriterPrivateCancelWriteFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.cancelWrite",
                             IMAGEWRITER_CANCELWRITE)
  ImageWriterPrivateCancelWriteFunction();

 private:
  ~ImageWriterPrivateCancelWriteFunction() override;
  bool RunAsync() override;
  void OnWriteCancelled(bool success, const std::string& error);
};

class ImageWriterPrivateDestroyPartitionsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.destroyPartitions",
                             IMAGEWRITER_DESTROYPARTITIONS)
  ImageWriterPrivateDestroyPartitionsFunction();

 private:
  ~ImageWriterPrivateDestroyPartitionsFunction() override;
  bool RunAsync() override;
  void OnDestroyComplete(bool success, const std::string& error);
};

class ImageWriterPrivateListRemovableStorageDevicesFunction
    : public ChromeAsyncExtensionFunction {
  public:
    DECLARE_EXTENSION_FUNCTION("imageWriterPrivate.listRemovableStorageDevices",
                               IMAGEWRITER_LISTREMOVABLESTORAGEDEVICES);
  ImageWriterPrivateListRemovableStorageDevicesFunction();

 private:
  ~ImageWriterPrivateListRemovableStorageDevicesFunction() override;
  bool RunAsync() override;
  void OnDeviceListReady(scoped_refptr<StorageDeviceList> device_list,
                         bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_PRIVATE_API_H_
