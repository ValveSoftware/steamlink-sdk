// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/webcam_private/webcam.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/process_manager_observer.h"

class Profile;

namespace extensions {

class ProcessManager;

class WebcamPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<WebcamPrivateAPI>* GetFactoryInstance();

  // Convenience method to get the WebcamPrivateAPI for a BrowserContext.
  static WebcamPrivateAPI* Get(content::BrowserContext* context);

  explicit WebcamPrivateAPI(content::BrowserContext* context);
  ~WebcamPrivateAPI() override;

  Webcam* GetWebcam(const std::string& extension_id,
                    const std::string& device_id);

  bool OpenSerialWebcam(
      const std::string& extension_id,
      const std::string& device_path,
      const base::Callback<void(const std::string&, bool)>& callback);
  bool CloseWebcam(const std::string& extension_id,
                   const std::string& device_id);

 private:
  friend class BrowserContextKeyedAPIFactory<WebcamPrivateAPI>;

  void OnOpenSerialWebcam(
      const std::string& extension_id,
      const std::string& device_path,
      scoped_refptr<Webcam> webcam,
      const base::Callback<void(const std::string&, bool)>& callback,
      bool success);

  // Note: This function does not work for serial devices. Do not use this
  // function for serial devices.
  bool GetDeviceId(const std::string& extension_id,
                   const std::string& webcam_id,
                   std::string* device_id);
  std::string GetWebcamId(const std::string& extension_id,
                          const std::string& device_id);

  WebcamResource* FindWebcamResource(const std::string& extension_id,
                                     const std::string& webcam_id) const;
  bool RemoveWebcamResource(const std::string& extension_id,
                            const std::string& webcam_id);

  // BrowserContextKeyedAPI:
  static const char* service_name() {
    return "WebcamPrivateAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  content::BrowserContext* const browser_context_;
  std::unique_ptr<ApiResourceManager<WebcamResource>> webcam_resource_manager_;

  base::WeakPtrFactory<WebcamPrivateAPI> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateAPI);
};

template <>
void BrowserContextKeyedAPIFactory<WebcamPrivateAPI>
    ::DeclareFactoryDependencies();

class WebcamPrivateOpenSerialWebcamFunction : public AsyncExtensionFunction {
 public:
  WebcamPrivateOpenSerialWebcamFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.openSerialWebcam",
                             WEBCAMPRIVATE_OPENSERIALWEBCAM);

 protected:
  ~WebcamPrivateOpenSerialWebcamFunction() override;
  bool RunAsync() override;

 private:
  void OnOpenWebcam(const std::string& webcam_id, bool success);

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateOpenSerialWebcamFunction);
};

class WebcamPrivateCloseWebcamFunction : public AsyncExtensionFunction {
 public:
  WebcamPrivateCloseWebcamFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.closeWebcam",
                             WEBCAMPRIVATE_CLOSEWEBCAM);

 protected:
  ~WebcamPrivateCloseWebcamFunction() override;
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateCloseWebcamFunction);
};

class WebcamPrivateSetFunction : public AsyncExtensionFunction {
 public:
  WebcamPrivateSetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.set", WEBCAMPRIVATE_SET);

 protected:
  ~WebcamPrivateSetFunction() override;
  bool RunAsync() override;

 private:
  void OnSetWebcamParameters(bool success);

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateSetFunction);
};

class WebcamPrivateGetFunction : public AsyncExtensionFunction {
 public:
  WebcamPrivateGetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.get", WEBCAMPRIVATE_GET);

 protected:
  ~WebcamPrivateGetFunction() override;
  bool RunAsync() override;

 private:
  enum InquiryType {
    INQUIRY_PAN,
    INQUIRY_TILT,
    INQUIRY_ZOOM,
  };
  void OnGetWebcamParameters(InquiryType type, bool success, int value);

  int pan_;
  int tilt_;
  int zoom_;
  bool get_pan_;
  bool get_tilt_;
  bool get_zoom_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateGetFunction);
};

class WebcamPrivateResetFunction : public AsyncExtensionFunction {
 public:
  WebcamPrivateResetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.reset", WEBCAMPRIVATE_RESET);

 protected:
  ~WebcamPrivateResetFunction() override;
  bool RunAsync() override;

 private:
  void OnResetWebcam(bool success);

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateResetFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
