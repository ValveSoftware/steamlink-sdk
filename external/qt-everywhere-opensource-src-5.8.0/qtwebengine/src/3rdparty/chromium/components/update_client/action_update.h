// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/version.h"
#include "components/update_client/action.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"
#include "url/gurl.h"

namespace update_client {

class UpdateChecker;

// Defines a template method design pattern for ActionUpdate. This class
// implements the common code for updating a single CRX using either
// a differential or a full update algorithm.
// TODO(sorin): further refactor this class to enforce that there is a 1:1
// relationship between one instance of this class and one CRX id. In other
// words, make the CRX id and its associated CrxUpdateItem data structure
// a member of this class instead of passing them around as function parameters.
class ActionUpdate : public Action, protected ActionImpl {
 public:
  ActionUpdate();
  ~ActionUpdate() override;

  // Action overrides.
  void Run(UpdateContext* update_context, Callback callback) override;

 private:
  virtual bool IsBackgroundDownload(const CrxUpdateItem* item) = 0;
  virtual std::vector<GURL> GetUrls(const CrxUpdateItem* item) = 0;
  virtual std::string GetHash(const CrxUpdateItem* item) = 0;
  virtual void OnDownloadStart(CrxUpdateItem* item) = 0;
  virtual void OnDownloadSuccess(
      CrxUpdateItem* item,
      const CrxDownloader::Result& download_result) = 0;
  virtual void OnDownloadError(
      CrxUpdateItem* item,
      const CrxDownloader::Result& download_result) = 0;
  virtual void OnInstallStart(CrxUpdateItem* item) = 0;
  virtual void OnInstallSuccess(CrxUpdateItem* item) = 0;
  virtual void OnInstallError(CrxUpdateItem* item,
                              ComponentUnpacker::Error error,
                              int extended_error) = 0;

  // Called when progress is being made downloading a CRX. The progress may
  // not monotonically increase due to how the CRX downloader switches between
  // different downloaders and fallback urls.
  void DownloadProgress(const std::string& id,
                        const CrxDownloader::Result& download_result);

  // Called when the CRX package has been downloaded to a temporary location.
  void DownloadComplete(const std::string& id,
                        const CrxDownloader::Result& download_result);

  void Install(const std::string& id, const base::FilePath& crx_path);

  // TODO(sorin): refactor the public interface of ComponentUnpacker so
  // that these calls can run on the main thread.
  void DoInstallOnBlockingTaskRunner(UpdateContext* update_context,
                                     CrxUpdateItem* item,
                                     const base::FilePath& crx_path);

  void EndUnpackingOnBlockingTaskRunner(UpdateContext* update_context,
                                        CrxUpdateItem* item,
                                        const base::FilePath& crx_path,
                                        ComponentUnpacker::Error error,
                                        int extended_error);

  void DoneInstalling(const std::string& id,
                      ComponentUnpacker::Error error,
                      int extended_error);

  // Downloads updates for one CRX id only.
  std::unique_ptr<CrxDownloader> crx_downloader_;

  // Unpacks one CRX.
  scoped_refptr<ComponentUnpacker> unpacker_;

  DISALLOW_COPY_AND_ASSIGN(ActionUpdate);
};

class ActionUpdateDiff : public ActionUpdate {
 public:
  static std::unique_ptr<Action> Create();

 private:
  ActionUpdateDiff();
  ~ActionUpdateDiff() override;

  void TryUpdateFull();

  // ActionUpdate overrides.
  bool IsBackgroundDownload(const CrxUpdateItem* item) override;
  std::vector<GURL> GetUrls(const CrxUpdateItem* item) override;
  std::string GetHash(const CrxUpdateItem* item) override;
  void OnDownloadStart(CrxUpdateItem* item) override;
  void OnDownloadSuccess(CrxUpdateItem* item,
                         const CrxDownloader::Result& download_result) override;
  void OnDownloadError(CrxUpdateItem* item,
                       const CrxDownloader::Result& download_result) override;
  void OnInstallStart(CrxUpdateItem* item) override;
  void OnInstallSuccess(CrxUpdateItem* item) override;
  void OnInstallError(CrxUpdateItem* item,
                      ComponentUnpacker::Error error,
                      int extended_error) override;

  DISALLOW_COPY_AND_ASSIGN(ActionUpdateDiff);
};

class ActionUpdateFull : public ActionUpdate {
 public:
  static std::unique_ptr<Action> Create();

 private:
  ActionUpdateFull();
  ~ActionUpdateFull() override;

  // ActionUpdate overrides.
  bool IsBackgroundDownload(const CrxUpdateItem* item) override;
  std::vector<GURL> GetUrls(const CrxUpdateItem* item) override;
  std::string GetHash(const CrxUpdateItem* item) override;
  void OnDownloadStart(CrxUpdateItem* item) override;
  void OnDownloadSuccess(CrxUpdateItem* item,
                         const CrxDownloader::Result& download_result) override;
  void OnDownloadError(CrxUpdateItem* item,
                       const CrxDownloader::Result& download_result) override;
  void OnInstallStart(CrxUpdateItem* item) override;
  void OnInstallSuccess(CrxUpdateItem* item) override;
  void OnInstallError(CrxUpdateItem* item,
                      ComponentUnpacker::Error error,
                      int extended_error) override;

  DISALLOW_COPY_AND_ASSIGN(ActionUpdateFull);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_UPDATE_H_
