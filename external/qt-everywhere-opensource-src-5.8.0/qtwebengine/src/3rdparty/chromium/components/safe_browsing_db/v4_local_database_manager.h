// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_LOCAL_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_LOCAL_DATABASE_MANAGER_H_

// A class that provides the interface between the SafeBrowsing protocol manager
// and database that holds the downloaded updates.

#include <memory>

#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/hit_report.h"
#include "components/safe_browsing_db/v4_database.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/safe_browsing_db/v4_update_protocol_manager.h"
#include "url/gurl.h"

using content::ResourceType;

namespace safe_browsing {

// Manages the local, on-disk database of updates downloaded from the
// SafeBrowsing service and interfaces with the protocol manager.
class V4LocalDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  // Construct V4LocalDatabaseManager.
  // Must be initialized by calling StartOnIOThread() before using.
  V4LocalDatabaseManager(const base::FilePath& base_path);

  //
  // SafeBrowsingDatabaseManager implementation
  //

  bool IsSupported() const override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CanCheckResourceType(content::ResourceType resource_type) const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool IsDownloadProtectionEnabled() const override;
  bool CheckBrowseUrl(const GURL& url, Client* client) override;
  void CancelCheck(Client* client) override;
  void StartOnIOThread(net::URLRequestContextGetter* request_context_getter,
                       const V4ProtocolConfig& config) override;
  void StopOnIOThread(bool shutdown) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  bool MatchCsdWhitelistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  bool MatchDownloadWhitelistUrl(const GURL& url) override;
  bool MatchDownloadWhitelistString(const std::string& str) override;
  bool MatchModuleWhitelistString(const std::string& str) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool IsMalwareKillSwitchOn() override;
  bool IsCsdWhitelistKillSwitchOn() override;

 private:
  ~V4LocalDatabaseManager() override;

  // The callback called each time the protocol manager downloads updates
  // successfully.
  void UpdateRequestCompleted(
      std::unique_ptr<ParsedServerResponse> parsed_server_response);

  void SetupUpdateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config);

  void SetupDatabase();

  void DatabaseReady(std::unique_ptr<V4Database> v4_database);

  // Called when the database has been updated and schedules the next update.
  void DatabaseUpdated();

  // The base directory under which to create the files that contain hashes.
  const base::FilePath base_path_;

  // Whether the service is running.
  bool enabled_;

  // Stores the current status of the lists to download from the SafeBrowsing
  // servers.
  // TODO(vakh): current_list_states_ doesn't really belong here.
  // It should come through the database, from the various V4Stores.
  base::hash_map<UpdateListIdentifier, std::string> current_list_states_;

  // The protocol manager that downloads the hash prefix updates.
  std::unique_ptr<V4UpdateProtocolManager> v4_update_protocol_manager_;

  // The database that manages the stores containing the hash prefix updates.
  // All writes to this variable must happen on the IO thread only.
  std::unique_ptr<V4Database> v4_database_;

  // Called when the V4Database has finished applying the latest update and is
  // ready to process next update.
  DatabaseUpdatedCallback db_updated_callback_;

  // The sequenced task runner for running safe browsing database operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  friend class base::RefCountedThreadSafe<V4LocalDatabaseManager>;
  DISALLOW_COPY_AND_ASSIGN(V4LocalDatabaseManager);
};  // class V4LocalDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_LOCAL_DATABASE_MANAGER_H_
