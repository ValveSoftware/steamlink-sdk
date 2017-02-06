// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "components/offline_pages/offline_event_logger.h"
#include "components/offline_pages/offline_page_archiver.h"
#include "components/offline_pages/offline_page_storage_manager.h"
#include "components/offline_pages/offline_page_types.h"

class GURL;
namespace base {
class Time;
}  // namespace base

namespace offline_pages {

struct ClientId;
struct OfflinePageItem;

// Service for saving pages offline, storing the offline copy and metadata, and
// retrieving them upon request.
//
// Example usage:
//   class ArchiverImpl : public OfflinePageArchiver {
//     // This is a class that knows how to create archiver
//     void CreateArchiver(...) override;
//     ...
//   }
//
//   // In code using the OfflinePagesModel to save a page:
//   std::unique_ptr<ArchiverImpl> archiver(new ArchiverImpl());
//   // Callback is of type SavePageCallback.
//   model->SavePage(url, std::move(archiver), callback);
//
// TODO(fgorski): Things to describe:
// * how to cancel requests and what to expect
class OfflinePageModel : public base::SupportsUserData {
 public:
  // Observer of the OfflinePageModel.
  class Observer {
   public:
    // Invoked when the model has finished loading.
    virtual void OfflinePageModelLoaded(OfflinePageModel* model) = 0;

    // Invoked when the model is being updated, due to adding, removing or
    // updating an offline page.
    virtual void OfflinePageModelChanged(OfflinePageModel* model) = 0;

    // Invoked when an offline copy related to |offline_id| was deleted.
    virtual void OfflinePageDeleted(int64_t offline_id,
                                    const ClientId& client_id) = 0;

   protected:
    virtual ~Observer() = default;
  };

  using CheckPagesExistOfflineResult =
      offline_pages::CheckPagesExistOfflineResult;
  using MultipleOfflinePageItemResult =
      offline_pages::MultipleOfflinePageItemResult;
  using DeletePageResult = offline_pages::DeletePageResult;
  using SavePageResult = offline_pages::SavePageResult;

  // Returns true if saving an offline page may be attempted for |url|.
  static bool CanSaveURL(const GURL& url);

  OfflinePageModel();
  ~OfflinePageModel() override;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Attempts to save a page addressed by |url| offline. Requires that the model
  // is loaded.  Generates a new offline id and returns it.
  virtual void SavePage(const GURL& url,
                        const ClientId& client_id,
                        std::unique_ptr<OfflinePageArchiver> archiver,
                        const SavePageCallback& callback) = 0;

  // Marks that the offline page related to the passed |offline_id| has been
  // accessed. Its access info, including last access time and access count,
  // will be updated. Requires that the model is loaded.
  virtual void MarkPageAccessed(int64_t offline_id) = 0;

  // Wipes out all the data by deleting all saved files and clearing the store.
  virtual void ClearAll(const base::Closure& callback) = 0;

  // Deletes pages based on |offline_ids|.
  virtual void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                                      const DeletePageCallback& callback) = 0;

  // Deletes offline pages matching the URL predicate.
  virtual void DeletePagesByURLPredicate(
      const UrlPredicate& predicate,
      const DeletePageCallback& callback) = 0;

  // Returns true via callback if there are offline pages in the given
  // |name_space|.
  virtual void HasPages(const std::string& name_space,
                        const HasPagesCallback& callback) = 0;

  // Returns via callback all GURLs in |urls| that are equal to the online URL
  // of any offline page.
  virtual void CheckPagesExistOffline(
      const std::set<GURL>& urls,
      const CheckPagesExistOfflineCallback& callback) = 0;

  // Gets all offline pages.
  virtual void GetAllPages(const MultipleOfflinePageItemCallback& callback) = 0;

  // Gets all offline ids where the offline page has the matching client id.
  virtual void GetOfflineIdsForClientId(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) = 0;

  // Gets all offline ids where the offline page has the matching client id.
  // Requires that the model is loaded.  May not return matching IDs depending
  // on the internal state of the model.
  //
  // This function is deprecated.  Use |GetOfflineIdsForClientId| instead.
  virtual const std::vector<int64_t> MaybeGetOfflineIdsForClientId(
      const ClientId& client_id) const = 0;

  // Returns zero or one offline pages associated with a specified |offline_id|.
  virtual void GetPageByOfflineId(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) = 0;

  // Returns an offline page associated with a specified |offline_id|. nullptr
  // is returned if not found.
  virtual const OfflinePageItem* MaybeGetPageByOfflineId(
      int64_t offline_id) const = 0;

  // Returns the offline page that is stored under |offline_url|, if any.
  virtual void GetPageByOfflineURL(
      const GURL& offline_url,
      const SingleOfflinePageItemCallback& callback) = 0;

  // Returns an offline page that is stored as |offline_url|. A nullptr is
  // returned if not found.
  //
  // This function is deprecated, and may return |nullptr| even if a page
  // exists, depending on the implementation details of OfflinePageModel.
  // Use |GetPageByOfflineURL| instead.
  virtual const OfflinePageItem* MaybeGetPageByOfflineURL(
      const GURL& offline_url) const = 0;

  // Returns the offline pages that are stored under |online_url|.
  virtual void GetPagesByOnlineURL(
      const GURL& online_url,
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Returns via callback an offline page saved for |online_url|, if any. The
  // best page is chosen based on creation date; a more recently created offline
  // page will be preferred over an older one. This API function does not
  // respect namespaces, as it is used to choose which page is rendered in a
  // tab. Today all namespaces are treated equally for the purposes of this
  // selection.
  virtual void GetBestPageForOnlineURL(
      const GURL& online_url,
      const SingleOfflinePageItemCallback callback) = 0;

  // Returns an offline page saved for |online_url|. A nullptr is returned if
  // not found.  See |GetBestPageForOnlineURL| for selection criteria.
  virtual const OfflinePageItem* MaybeGetBestPageForOnlineURL(
      const GURL& online_url) const = 0;

  // Checks that all of the offline pages have corresponding offline copies,
  // and all archived files have offline pages pointing to them.
  // If a page is discovered to be missing an offline copy, its offline page
  // metadata will be expired. If an archive file is discovered missing its
  // offline page, it will be deleted.
  virtual void CheckMetadataConsistency() = 0;

  // Marks pages with |offline_ids| as expired and deletes the associated
  // archive files.
  virtual void ExpirePages(const std::vector<int64_t>& offline_ids,
                           const base::Time& expiration_time,
                           const base::Callback<void(bool)>& callback) = 0;

  // Returns the policy controller.
  virtual ClientPolicyController* GetPolicyController() = 0;

  // TODO(dougarnett): Remove this and its uses.
  virtual bool is_loaded() const = 0;

  // Returns the logger. Ownership is retained by the model.
  virtual OfflineEventLogger* GetLogger() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
