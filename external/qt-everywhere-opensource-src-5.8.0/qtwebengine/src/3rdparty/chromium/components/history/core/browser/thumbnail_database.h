// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_THUMBNAIL_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_THUMBNAIL_DATABASE_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/history/core/browser/history_types.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace base {
class FilePath;
class RefCountedMemory;
class Time;
}

namespace history {

class HistoryBackendClient;

// This database interface is owned by the history backend and runs on the
// history thread. It is a totally separate component from history partially
// because we may want to move it to its own thread in the future. The
// operations we will do on this database will be slow, but we can tolerate
// higher latency (it's OK for thumbnails to come in slower than the rest
// of the data). Moving this to a separate thread would not block potentially
// higher priority history operations.
class ThumbnailDatabase {
 public:
  explicit ThumbnailDatabase(HistoryBackendClient* backend_client);
  ~ThumbnailDatabase();

  // Must be called after creation but before any other methods are called.
  // When not INIT_OK, no other functions should be called.
  sql::InitStatus Init(const base::FilePath& db_name);

  // Computes and records various metrics for the database. Should only be
  // called once and only upon successful Init.
  void ComputeDatabaseMetrics();

  // Transactions on the database.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const {
    return db_.transaction_nesting();
  }
  void RollbackTransaction();

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Try to trim the cache memory used by the database.  If |aggressively| is
  // true try to trim all unused cache, otherwise trim by half.
  void TrimMemory(bool aggressively);

  // Favicon Bitmaps -----------------------------------------------------------

  // Returns true if there are favicon bitmaps for |icon_id|. If
  // |bitmap_id_sizes| is non NULL, sets it to a list of the favicon bitmap ids
  // and their associated pixel sizes for the favicon with |icon_id|.
  // The list contains results for the bitmaps which are cached in the
  // favicon_bitmaps table. The pixel sizes are a subset of the sizes in the
  // 'sizes' field of the favicons table for |icon_id|.
  bool GetFaviconBitmapIDSizes(
      favicon_base::FaviconID icon_id,
      std::vector<FaviconBitmapIDSize>* bitmap_id_sizes);

  // Returns true if there are any matched bitmaps for the given |icon_id|. All
  // matched results are returned if |favicon_bitmaps| is not NULL.
  bool GetFaviconBitmaps(favicon_base::FaviconID icon_id,
                         std::vector<FaviconBitmap>* favicon_bitmaps);

  // Gets the last updated time, bitmap data, and pixel size of the favicon
  // bitmap at |bitmap_id|. Returns true if successful.
  bool GetFaviconBitmap(FaviconBitmapID bitmap_id,
                        base::Time* last_updated,
                        base::Time* last_requested,
                        scoped_refptr<base::RefCountedMemory>* png_icon_data,
                        gfx::Size* pixel_size);

  // Adds a bitmap component at |pixel_size| for the favicon with |icon_id|.
  // Only favicons representing a .ico file should have multiple favicon bitmaps
  // per favicon.
  // |icon_data| is the png encoded data.
  // The |time| indicates the access time, and is used to detect when the
  // favicon should be refreshed.
  // |pixel_size| is the pixel dimensions of |icon_data|.
  // Returns the id of the added bitmap or 0 if unsuccessful.
  FaviconBitmapID AddFaviconBitmap(
      favicon_base::FaviconID icon_id,
      const scoped_refptr<base::RefCountedMemory>& icon_data,
      base::Time time,
      const gfx::Size& pixel_size);

  // Sets the bitmap data and the last updated time for the favicon bitmap at
  // |bitmap_id|.
  // Returns true if successful.
  bool SetFaviconBitmap(FaviconBitmapID bitmap_id,
                        scoped_refptr<base::RefCountedMemory> bitmap_data,
                        base::Time time);

  // Sets the last updated time for the favicon bitmap at |bitmap_id|.
  // Returns true if successful.
  bool SetFaviconBitmapLastUpdateTime(FaviconBitmapID bitmap_id,
                                      base::Time time);

  // Sets the last requested time for the favicon bitmap at |bitmap_id|.
  // Returns true if successful.
  bool SetFaviconBitmapLastRequestedTime(FaviconBitmapID bitmap_id,
                                         base::Time time);

  // Deletes the favicon bitmap with |bitmap_id|.
  // Returns true if successful.
  bool DeleteFaviconBitmap(FaviconBitmapID bitmap_id);

  // Favicons ------------------------------------------------------------------

  // Sets the the favicon as out of date. This will set |last_updated| for all
  // of the bitmaps for |icon_id| to be out of date.
  bool SetFaviconOutOfDate(favicon_base::FaviconID icon_id);

  // Returns the id of the entry in the favicon database with the specified url
  // and icon type. If |required_icon_type| contains multiple icon types and
  // there are more than one matched icon in database, only one icon will be
  // returned in the priority of TOUCH_PRECOMPOSED_ICON, TOUCH_ICON, and
  // FAVICON, and the icon type is returned in icon_type parameter if it is not
  // NULL.
  // Returns 0 if no entry exists for the specified url.
  favicon_base::FaviconID GetFaviconIDForFaviconURL(
      const GURL& icon_url,
      int required_icon_type,
      favicon_base::IconType* icon_type);

  // Gets the icon_url, icon_type and sizes for the specified |icon_id|.
  bool GetFaviconHeader(favicon_base::FaviconID icon_id,
                        GURL* icon_url,
                        favicon_base::IconType* icon_type);

  // Adds favicon with |icon_url|, |icon_type| and |favicon_sizes| to the
  // favicon db, returning its id.
  favicon_base::FaviconID AddFavicon(const GURL& icon_url,
                                     favicon_base::IconType icon_type);

  // Adds a favicon with a single bitmap. This call is equivalent to calling
  // AddFavicon and AddFaviconBitmap.
  favicon_base::FaviconID AddFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const scoped_refptr<base::RefCountedMemory>& icon_data,
      base::Time time,
      const gfx::Size& pixel_size);

  // Delete the favicon with the provided id. Returns false on failure
  bool DeleteFavicon(favicon_base::FaviconID id);

  // Icon Mapping --------------------------------------------------------------
  //
  // Returns true if there is a matched icon mapping for the given page and
  // icon type.
  // The matched icon mapping is returned in the icon_mapping parameter if it is
  // not NULL.

  // Returns true if there are icon mappings for the given page and icon types.
  // If |required_icon_types| contains multiple icon types and there is more
  // than one matched icon type in the database, icons of only a single type
  // will be returned in the priority of TOUCH_PRECOMPOSED_ICON, TOUCH_ICON,
  // and FAVICON.
  // The matched icon mappings are returned in the |mapping_data| parameter if
  // it is not NULL.
  bool GetIconMappingsForPageURL(const GURL& page_url,
                                 int required_icon_types,
                                 std::vector<IconMapping>* mapping_data);

  // Returns true if there is any matched icon mapping for the given page.
  // All matched icon mappings are returned in descent order of IconType if
  // mapping_data is not NULL.
  bool GetIconMappingsForPageURL(const GURL& page_url,
                                 std::vector<IconMapping>* mapping_data);

  // Adds a mapping between the given page_url and icon_id.
  // Returns the new mapping id if the adding succeeds, otherwise 0 is returned.
  IconMappingID AddIconMapping(const GURL& page_url,
                               favicon_base::FaviconID icon_id);

  // Deletes the icon mapping entries for the given page url.
  // Returns true if the deletion succeeded.
  bool DeleteIconMappings(const GURL& page_url);

  // Deletes the icon mapping with |mapping_id|.
  // Returns true if the deletion succeeded.
  bool DeleteIconMapping(IconMappingID mapping_id);

  // Checks whether a favicon is used by any URLs in the database.
  bool HasMappingFor(favicon_base::FaviconID id);

  // The class to enumerate icon mappings. Use InitIconMappingEnumerator to
  // initialize.
  class IconMappingEnumerator {
   public:
    IconMappingEnumerator();
    ~IconMappingEnumerator();

    // Get the next icon mapping, return false if no more are available.
    bool GetNextIconMapping(IconMapping* icon_mapping);

   private:
    friend class ThumbnailDatabase;

    // Used to query database and return the data for filling IconMapping in
    // each call of GetNextIconMapping().
    sql::Statement statement_;

    DISALLOW_COPY_AND_ASSIGN(IconMappingEnumerator);
  };

  // Return all icon mappings of the given |icon_type|.
  bool InitIconMappingEnumerator(favicon_base::IconType type,
                                 IconMappingEnumerator* enumerator);

  // Remove all data except that associated with the passed page urls.
  // Returns false in case of failure.  A nested transaction is used,
  // so failure causes any outer transaction to be rolled back.
  bool RetainDataForPageUrls(const std::vector<GURL>& urls_to_keep);

 private:
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, RetainDataForPageUrls);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest,
                           RetainDataForPageUrlsExpiresRetainedFavicons);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, Version3);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, Version4);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, Version5);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, Version6);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, Version7);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, Version8);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailDatabaseTest, WildSchema);

  // Open database on a given filename. If the file does not exist,
  // it is created.
  // |db| is the database to open.
  // |db_name| is a path to the database file.
  sql::InitStatus OpenDatabase(sql::Connection* db,
                               const base::FilePath& db_name);

  // Helper function to implement internals of Init().  This allows
  // Init() to retry in case of failure, since some failures run
  // recovery code.
  sql::InitStatus InitImpl(const base::FilePath& db_name);

  // Helper function to handle cleanup on upgrade failures.
  sql::InitStatus CantUpgradeToVersion(int cur_version);

  // Adds support for size in favicons table.
  bool UpgradeToVersion6();

  // Removes sizes column.
  bool UpgradeToVersion7();

  // Adds support for bitmap usage tracking.
  bool UpgradeToVersion8();

  // Returns true if the |favicons| database is missing a column.
  bool IsFaviconDBStructureIncorrect();

  sql::Connection db_;
  sql::MetaTable meta_table_;

  HistoryBackendClient* backend_client_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_THUMBNAIL_DATABASE_H_
