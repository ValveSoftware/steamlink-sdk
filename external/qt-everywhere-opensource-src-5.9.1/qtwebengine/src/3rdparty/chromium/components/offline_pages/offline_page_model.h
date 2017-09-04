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
#include "components/offline_pages/offline_page_model_query.h"
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
  // Controls how to search on differnt URLs for pages.
  enum class URLSearchMode {
    // Match against the last committed URL only.
    SEARCH_BY_FINAL_URL_ONLY,
    // Match against all stored URLs, including the last committed URL and
    // the original request URL.
    SEARCH_BY_ALL_URLS
  };

  // Describes the parameters to control how to save a page.
  struct SavePageParams {
    SavePageParams();
    SavePageParams(const SavePageParams& other);

    // The last committed URL of the page to save.
    GURL url;

    // The identification used by the client.
    ClientId client_id;

    // Used for the offline_id for the saved file if non-zero. If it is
    // kInvalidOfflineId, a new, random ID will be generated.
    int64_t proposed_offline_id;

    // The original URL of the page to save. Empty if no redirect occurs.
    GURL original_url;
  };

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

  static const int64_t kInvalidOfflineId = 0;

  // Attempts to save a page offline per |save_page_params|. Requires that the
  // model is loaded.  Generates a new offline id or uses the proposed offline
  // id in |save_page_params| and returns it.
  virtual void SavePage(const SavePageParams& save_page_params,
                        std::unique_ptr<OfflinePageArchiver> archiver,
                        const SavePageCallback& callback) = 0;

  // Marks that the offline page related to the passed |offline_id| has been
  // accessed. Its access info, including last access time and access count,
  // will be updated. Requires that the model is loaded.
  virtual void MarkPageAccessed(int64_t offline_id) = 0;

  // Deletes pages based on |offline_ids|.
  virtual void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                                      const DeletePageCallback& callback) = 0;

  // Deletes all pages associated with any of |client_ids|.
  virtual void DeletePagesByClientIds(const std::vector<ClientId>& client_ids,
                                      const DeletePageCallback& callback) = 0;

  virtual void GetPagesMatchingQuery(
      std::unique_ptr<OfflinePageModelQuery> query,
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Retrieves all pages associated with any of |client_ids|.
  virtual void GetPagesByClientIds(
      const std::vector<ClientId>& client_ids,
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Deletes cached offline pages matching the URL predicate.
  virtual void DeleteCachedPagesByURLPredicate(
      const UrlPredicate& predicate,
      const DeletePageCallback& callback) = 0;

  // Returns via callback all GURLs in |urls| that are equal to the online URL
  // of any offline page.
  virtual void CheckPagesExistOffline(
      const std::set<GURL>& urls,
      const CheckPagesExistOfflineCallback& callback) = 0;

  // Gets all offline pages.
  virtual void GetAllPages(const MultipleOfflinePageItemCallback& callback) = 0;

  // Gets all offline pages including expired ones.
  virtual void GetAllPagesWithExpired(
      const MultipleOfflinePageItemCallback& callback) = 0;

  // Gets all offline ids where the offline page has the matching client id.
  virtual void GetOfflineIdsForClientId(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) = 0;

  // Returns zero or one offline pages associated with a specified |offline_id|.
  virtual void GetPageByOfflineId(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) = 0;

  // Returns the offline pages that are related to |url|. |url_search_mode|
  // controls how the url match is done. See URLSearchMode for more details.
  virtual void GetPagesByURL(
      const GURL& url,
      URLSearchMode url_search_mode,
      const MultipleOfflinePageItemCallback& callback) = 0;

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
