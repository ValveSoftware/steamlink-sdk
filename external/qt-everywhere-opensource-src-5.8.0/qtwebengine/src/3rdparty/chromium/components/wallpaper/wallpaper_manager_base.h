// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_WALLPAPER_MANAGER_BASE_H_
#define COMPONENTS_WALLPAPER_WALLPAPER_MANAGER_BASE_H_

#include <stddef.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/wallpaper/wallpaper_export.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace base {
class CommandLine;
class SequencedTaskRunner;
}

namespace user_manager {
class User;
class UserImage;
}

namespace wallpaper {

// This object is passed between several threads while wallpaper is being
// loaded. It will notify callback when last reference to it is removed
// (thus indicating that the last load action has finished).
class WALLPAPER_EXPORT MovableOnDestroyCallback {
 public:
  explicit MovableOnDestroyCallback(const base::Closure& callback);

  ~MovableOnDestroyCallback();

 private:
  base::Closure callback_;
};

using MovableOnDestroyCallbackHolder =
    std::unique_ptr<MovableOnDestroyCallback>;

struct WALLPAPER_EXPORT WallpaperInfo {
  WallpaperInfo();
  WallpaperInfo(const std::string& in_location,
                WallpaperLayout in_layout,
                user_manager::User::WallpaperType in_type,
                const base::Time& in_date);
  ~WallpaperInfo();

  // Either file name of migrated wallpaper including first directory level
  // (corresponding to user wallpaper_files_id) or online wallpaper URL.
  std::string location;
  WallpaperLayout layout;
  user_manager::User::WallpaperType type;
  base::Time date;
  bool operator==(const WallpaperInfo& other) {
    return (location == other.location) && (layout == other.layout) &&
           (type == other.type);
  }
};

class WallpaperManagerBrowserTest;

// Name of wallpaper sequence token.
WALLPAPER_EXPORT extern const char kWallpaperSequenceTokenName[];

// File path suffices of resized small or large wallpaper.
// TODO(bshe): Use the same sub folder system as custom wallpapers use.
// crbug.com/174928
WALLPAPER_EXPORT extern const char kSmallWallpaperSuffix[];
WALLPAPER_EXPORT extern const char kLargeWallpaperSuffix[];

// Directory names of custom wallpapers.
WALLPAPER_EXPORT extern const char kSmallWallpaperSubDir[];
WALLPAPER_EXPORT extern const char kLargeWallpaperSubDir[];
WALLPAPER_EXPORT extern const char kOriginalWallpaperSubDir[];
WALLPAPER_EXPORT extern const char kThumbnailWallpaperSubDir[];

// The width and height of small/large resolution wallpaper. When screen size is
// smaller than |kSmallWallpaperMaxWidth| and |kSmallWallpaperMaxHeight|, the
// small resolution wallpaper should be used. Otherwise, use the large
// resolution wallpaper.
WALLPAPER_EXPORT extern const int kSmallWallpaperMaxWidth;
WALLPAPER_EXPORT extern const int kSmallWallpaperMaxHeight;
WALLPAPER_EXPORT extern const int kLargeWallpaperMaxWidth;
WALLPAPER_EXPORT extern const int kLargeWallpaperMaxHeight;

// The width and height of wallpaper thumbnails.
WALLPAPER_EXPORT extern const int kWallpaperThumbnailWidth;
WALLPAPER_EXPORT extern const int kWallpaperThumbnailHeight;

// A dictionary that maps usernames to wallpaper properties.
WALLPAPER_EXPORT extern const char kUsersWallpaperInfo[];

// A dictionary pref that maps usernames to file paths to their wallpapers.
// Deprecated. Will remove this const char after done migration.
WALLPAPER_EXPORT extern const char kUserWallpapers[];

// A dictionary pref that maps usernames to wallpaper properties.
WALLPAPER_EXPORT extern const char kUserWallpapersProperties[];

class WallpaperFilesId;

// This singleton class maintains wallpapers for users.
class WALLPAPER_EXPORT WallpaperManagerBase {
 public:
  enum WallpaperResolution {
    WALLPAPER_RESOLUTION_LARGE,
    WALLPAPER_RESOLUTION_SMALL
  };
  class CustomizedWallpaperRescaledFiles {
   public:
    CustomizedWallpaperRescaledFiles(const base::FilePath& path_downloaded,
                                     const base::FilePath& path_rescaled_small,
                                     const base::FilePath& path_rescaled_large);
    bool AllSizesExist() const;

    // Closure will hold unretained pointer to this object. So caller must
    // make sure that the closure will be destoyed before this object.
    // Closure must be called on BlockingPool.
    base::Closure CreateCheckerClosure();

    const base::FilePath& path_downloaded() const;
    const base::FilePath& path_rescaled_small() const;
    const base::FilePath& path_rescaled_large() const;

    bool downloaded_exists() const;
    bool rescaled_small_exists() const;
    bool rescaled_large_exists() const;

   private:
    // Must be called on BlockingPool.
    void CheckCustomizedWallpaperFilesExist();

    const base::FilePath path_downloaded_;
    const base::FilePath path_rescaled_small_;
    const base::FilePath path_rescaled_large_;

    bool downloaded_exists_;
    bool rescaled_small_exists_;
    bool rescaled_large_exists_;

    DISALLOW_COPY_AND_ASSIGN(CustomizedWallpaperRescaledFiles);
  };

  // For testing.
  class TestApi {
   public:
    explicit TestApi(WallpaperManagerBase* wallpaper_manager);
    virtual ~TestApi();

    bool GetWallpaperFromCache(const AccountId& account_id,
                               gfx::ImageSkia* image);

    bool GetPathFromCache(const AccountId& account_id, base::FilePath* path);

    void SetWallpaperCache(const AccountId& account_id,
                           const base::FilePath& path,
                           const gfx::ImageSkia& image);

    void ClearDisposableWallpaperCache();

   private:
    WallpaperManagerBase* wallpaper_manager_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnWallpaperAnimationFinished(const AccountId& account_id) = 0;
    virtual void OnUpdateWallpaperForTesting() {}
    virtual void OnPendingListEmptyForTesting() {}
  };

  // set path IDs for used directories
  static void SetPathIds(int dir_user_data_enum,
                         int dir_chromeos_wallpapers_enum,
                         int dir_chromeos_custom_wallpapers_enum);

  // Returns custom wallpaper directory by appending corresponding |sub_dir|.
  static base::FilePath GetCustomWallpaperDir(const char* sub_dir);

  // Registers wallpaper manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Resizes |image| to a resolution which is nearest to |preferred_width| and
  // |preferred_height| while respecting the |layout| choice. |output_skia| is
  // optional (may be NULL). Returns true on success.
  static bool ResizeImage(const gfx::ImageSkia& image,
                          WallpaperLayout layout,
                          int preferred_width,
                          int preferred_height,
                          scoped_refptr<base::RefCountedBytes>* output,
                          gfx::ImageSkia* output_skia);

  // Resizes |image| to a resolution which is nearest to |preferred_width| and
  // |preferred_height| while respecting the |layout| choice and saves the
  // resized wallpaper to |path|. |output_skia| is optional (may be
  // NULL). Returns true on success.
  static bool ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                                     const base::FilePath& path,
                                     WallpaperLayout layout,
                                     int preferred_width,
                                     int preferred_height,
                                     gfx::ImageSkia* output_skia);

  // Returns custom wallpaper path. Append |sub_dir|, |wallpaper_files_id| and
  // |file|
  // to custom wallpaper directory.
  static base::FilePath GetCustomWallpaperPath(
      const char* sub_dir,
      const WallpaperFilesId& wallpaper_files_id,
      const std::string& file);

  WallpaperManagerBase();
  virtual ~WallpaperManagerBase();

  // Returns the appropriate wallpaper resolution for all root windows.
  virtual WallpaperResolution GetAppropriateResolution() = 0;

  virtual void SetCommandLineForTesting(base::CommandLine* command_line);

  // Adds PowerManagerClient, TimeZoneSettings and CrosSettings observers.
  virtual void AddObservers() = 0;

  // Loads wallpaper asynchronously if the current wallpaper is not the
  // wallpaper of logged in user.
  virtual void EnsureLoggedInUserWallpaperLoaded();

  // Gets wallpaper information of logged in user.
  virtual bool GetLoggedInUserWallpaperInfo(WallpaperInfo* info);

  // Initializes wallpaper. If logged in, loads user's wallpaper. If not logged
  // in, uses a solid color wallpaper. If logged in as a stub user, uses an
  // empty wallpaper.
  virtual void InitializeWallpaper() = 0;

  // Removes all |account_id| related wallpaper info and saved wallpapers.
  virtual void RemoveUserWallpaperInfo(const AccountId& account_id) = 0;

  // Saves custom wallpaper to file, post task to generate thumbnail and updates
  // local state preferences. If |update_wallpaper| is false, don't change
  // wallpaper but only update cache.
  virtual void SetCustomWallpaper(const AccountId& account_id,
                                  const WallpaperFilesId& wallpaper_files_id,
                                  const std::string& file,
                                  WallpaperLayout layout,
                                  user_manager::User::WallpaperType type,
                                  const gfx::ImageSkia& image,
                                  bool update_wallpaper) = 0;

  // Use given files as new default wallpaper.
  // Reloads current wallpaper, if old default was loaded.
  // Current value of default_wallpaper_image_ is destroyed.
  // Sets default_wallpaper_image_ either to |small_wallpaper_image| or
  // |large_wallpaper_image| depending on GetAppropriateResolution().
  virtual void SetDefaultWallpaperPath(
      const base::FilePath& customized_default_wallpaper_file_small,
      std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
      const base::FilePath& customized_default_wallpaper_file_large,
      std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) = 0;

  // Sets wallpaper to default wallpaper (asynchronously with zero delay).
  virtual void SetDefaultWallpaperNow(const AccountId& account_id) = 0;

  // Sets wallpaper to default wallpaper (asynchronously with default delay).
  virtual void SetDefaultWallpaperDelayed(const AccountId& account_id) = 0;

  // Sets selected wallpaper information for |account_id| and saves it to Local
  // State if |is_persistent| is true.
  virtual void SetUserWallpaperInfo(const AccountId& account_id,
                                    const WallpaperInfo& info,
                                    bool is_persistent) = 0;

  // Sets |account_id|'s wallpaper (asynchronously with zero delay).
  virtual void SetUserWallpaperNow(const AccountId& account_id);

  // Sets |account_id|'s wallpaper (asynchronously with default delay).
  virtual void SetUserWallpaperDelayed(const AccountId& account_id);

  // Sets wallpaper to |image| (asynchronously with zero delay). If
  // |update_wallpaper| is false, skip change wallpaper but only update cache.
  virtual void SetWallpaperFromImageSkia(const AccountId& account_id,
                                         const gfx::ImageSkia& image,
                                         WallpaperLayout layout,
                                         bool update_wallpaper) = 0;

  // Updates current wallpaper. It may switch the size of wallpaper based on the
  // current display's resolution. (asynchronously with zero delay)
  virtual void UpdateWallpaper(bool clear_cache);

  // Adds given observer to the list.
  virtual void AddObserver(Observer* observer);

  // Removes given observer from the list.
  virtual void RemoveObserver(Observer* observer);

  // Returns whether a wallpaper policy is enforced for |account_id|.
  virtual bool IsPolicyControlled(const AccountId& account_id) const;

  // Called when a wallpaper policy has been set for |account_id|.  Blocks user
  // from changing the wallpaper.
  virtual void OnPolicySet(const std::string& policy,
                           const AccountId& account_id);

  // Called when the wallpaper policy has been cleared for |account_id|.
  virtual void OnPolicyCleared(const std::string& policy,
                               const AccountId& account_id);

  // Called when the policy-set wallpaper has been fetched.  Initiates decoding
  // of the JPEG |data| with a callback to SetPolicyControlledWallpaper().
  virtual void OnPolicyFetched(const std::string& policy,
                               const AccountId& account_id,
                               std::unique_ptr<std::string> data) = 0;

  // This is called from CustomizationDocument.
  // |resized_directory| is the directory where resized versions are stored and
  // must be writable.
  virtual void SetCustomizedDefaultWallpaper(
      const GURL& wallpaper_url,
      const base::FilePath& downloaded_file,
      const base::FilePath& resized_directory);

  // Returns queue size.
  virtual size_t GetPendingListSizeForTesting() const = 0;

  // Ruturns files identifier for the |account_id|.
  virtual WallpaperFilesId GetFilesId(const AccountId& account_id) const = 0;

 protected:
  friend class TestApi;
  friend class WallpaperManagerBrowserTest;
  friend class WallpaperManagerBrowserTestDefaultWallpaper;
  friend class WallpaperManagerPolicyTest;

  // The |CustomWallpaperElement| contains |first| the path of the image which
  // is currently being loaded and or in progress of being loaded and |second|
  // the image itself.
  typedef std::pair<base::FilePath, gfx::ImageSkia> CustomWallpaperElement;
  typedef std::map<AccountId, CustomWallpaperElement> CustomWallpaperMap;

  // Saves original custom wallpaper to |path| (absolute path) on filesystem
  // and starts resizing operation of the custom wallpaper if necessary.
  static void SaveCustomWallpaper(const WallpaperFilesId& wallpaper_files_id,
                                  const base::FilePath& path,
                                  WallpaperLayout layout,
                                  std::unique_ptr<gfx::ImageSkia> image);

  // Moves custom wallpapers from user email directory to
  // |wallpaper_files_id| directory.
  static void MoveCustomWallpapersOnWorker(
      const AccountId& account_id,
      const WallpaperFilesId& wallpaper_files_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
      base::WeakPtr<WallpaperManagerBase> weak_ptr);

  // Gets |account_id|'s custom wallpaper at |wallpaper_path|. Falls back on
  // original custom wallpaper. When |update_wallpaper| is true, sets wallpaper
  // to the loaded wallpaper. Must run on wallpaper sequenced worker thread.
  static void GetCustomWallpaperInternal(
      const AccountId& account_id,
      const WallpaperInfo& info,
      const base::FilePath& wallpaper_path,
      bool update_wallpaper,
      const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
      MovableOnDestroyCallbackHolder on_finish,
      base::WeakPtr<WallpaperManagerBase> weak_ptr);

  // Resize and save customized default wallpaper.
  static void ResizeCustomizedDefaultWallpaper(
      std::unique_ptr<gfx::ImageSkia> image,
      const CustomizedWallpaperRescaledFiles* rescaled_files,
      bool* success,
      gfx::ImageSkia* small_wallpaper_image,
      gfx::ImageSkia* large_wallpaper_image);

  // Initialize wallpaper for the specified user to default and saves this
  // settings in local state.
  virtual void InitInitialUserWallpaper(const AccountId& account_id,
                                        bool is_persistent);

  // Gets encoded wallpaper from cache. Returns true if success.
  virtual bool GetWallpaperFromCache(const AccountId& account_id,
                                     gfx::ImageSkia* image);

  // Gets path of encoded wallpaper from cache. Returns true if success.
  virtual bool GetPathFromCache(const AccountId& account_id,
                                base::FilePath* path);

  // The number of wallpapers have loaded. For test only.
  virtual int loaded_wallpapers_for_test() const;

  // Cache some (or all) logged in users' wallpapers to memory at login
  // screen. It should not compete with first wallpaper loading when boot
  // up/initialize login WebUI page.
  // There are two ways the first wallpaper might be loaded:
  // 1. Loaded on boot. Login WebUI waits for it.
  // 2. When flag --disable-boot-animation is passed. Login WebUI is loaded
  // right away and in 500ms after. Wallpaper started to load.
  // For case 2, should_cache_wallpaper_ is used to indicate if we need to
  // cache wallpapers on wallpaper animation finished. The cache operation
  // should be only executed once.
  virtual void CacheUsersWallpapers();

  // Caches |account_id|'s wallpaper to memory.
  virtual void CacheUserWallpaper(const AccountId& account_id);

  // Clears disposable ONLINE and CUSTOM wallpaper cache. At multi profile
  // world, logged in users' wallpaper cache is not disposable.
  virtual void ClearDisposableWallpaperCache();

  // Deletes all |account_id| related custom wallpapers and directories.
  virtual void DeleteUserWallpapers(const AccountId& account_id,
                                    const std::string& path_to_file);

  // Gets the CommandLine representing the current process's command line.
  virtual base::CommandLine* GetCommandLine();

  // Initialize wallpaper of registered device after device policy is trusted.
  // Note that before device is enrolled, it proceeds with untrusted setting.
  virtual void InitializeRegisteredDeviceWallpaper() = 0;

  // Loads |account_id|'s wallpaper. When |update_wallpaper| is true, sets
  // wallpaper to the loaded wallpaper.
  virtual void LoadWallpaper(const AccountId& account_id,
                             const WallpaperInfo& info,
                             bool update_wallpaper,
                             MovableOnDestroyCallbackHolder on_finish);

  // Called when the original custom wallpaper is moved to the new place.
  // Updates the corresponding user wallpaper info.
  virtual void MoveCustomWallpapersSuccess(
      const AccountId& account_id,
      const wallpaper::WallpaperFilesId& wallpaper_files_id);

  // Moves custom wallpaper to a new place. Email address was used as directory
  // name in the old system, this is not safe. New directory system uses
  // wallpaper_files_id instead of e-mail. This must be called after
  // wallpaper_files_id is ready.
  virtual void MoveLoggedInUserCustomWallpaper();

  // Gets wallpaper information of |account_id| from Local State or memory.
  // Returns
  // false if wallpaper information is not found.
  virtual bool GetUserWallpaperInfo(const AccountId& account_id,
                                    WallpaperInfo* info) const = 0;

  // Sets wallpaper to the decoded wallpaper if |update_wallpaper| is true.
  // Otherwise, cache wallpaper to memory if not logged in.  (Takes a UserImage
  // because that's the callback interface provided by UserImageLoader.)
  virtual void OnWallpaperDecoded(
      const AccountId& account_id,
      WallpaperLayout layout,
      bool update_wallpaper,
      MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage> user_image) = 0;

  // Creates new PendingWallpaper request (or updates currently pending).
  virtual void ScheduleSetUserWallpaper(const AccountId& account_id,
                                        bool delayed) = 0;

  // Sets wallpaper to default.
  virtual void DoSetDefaultWallpaper(
      const AccountId& account_id,
      MovableOnDestroyCallbackHolder on_finish) = 0;

  // Starts to load wallpaper at |wallpaper_path|. If |wallpaper_path| is
  // already loaded for that user, do nothing. Must be called on UI thread.
  virtual void StartLoad(const AccountId& account_id,
                         const WallpaperInfo& info,
                         bool update_wallpaper,
                         const base::FilePath& wallpaper_path,
                         MovableOnDestroyCallbackHolder on_finish) = 0;

  // After completed load operation, update average load time.
  virtual void SaveLastLoadTime(const base::TimeDelta elapsed);

  // Notify all registered observers.
  virtual void NotifyAnimationFinished();

  // Calculate delay for next wallpaper load.
  // It is usually average wallpaper load time.
  // If last wallpaper load happened long ago, timeout should be reduced by
  // the time passed after last wallpaper load. So usual user experience results
  // in zero delay.
  virtual base::TimeDelta GetWallpaperLoadDelay() const;

  // This is called after we check that supplied default wallpaper files exist.
  virtual void SetCustomizedDefaultWallpaperAfterCheck(
      const GURL& wallpaper_url,
      const base::FilePath& downloaded_file,
      std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files) = 0;

  // Starts rescaling of customized wallpaper.
  virtual void OnCustomizedDefaultWallpaperDecoded(
      const GURL& wallpaper_url,
      std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
      std::unique_ptr<user_manager::UserImage> user_image);

  // Check the result of ResizeCustomizedDefaultWallpaper and finally
  // apply Customized Default Wallpaper.
  virtual void OnCustomizedDefaultWallpaperResized(
      const GURL& wallpaper_url,
      std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
      std::unique_ptr<bool> success,
      std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
      std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) = 0;

  // Init |*default_*_wallpaper_file_| from given command line and
  // clear |default_wallpaper_image_|.
  virtual void SetDefaultWallpaperPathsFromCommandLine(
      base::CommandLine* command_line) = 0;

  // Sets wallpaper to decoded default.
  virtual void OnDefaultWallpaperDecoded(
      const base::FilePath& path,
      const WallpaperLayout layout,
      std::unique_ptr<user_manager::UserImage>* result,
      MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage> user_image) = 0;

  // Start decoding given default wallpaper.
  virtual void StartLoadAndSetDefaultWallpaper(
      const base::FilePath& path,
      const WallpaperLayout layout,
      MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage>* result_out) = 0;

  // Returns wallpaper subdirectory name for current resolution.
  virtual const char* GetCustomWallpaperSubdirForCurrentResolution();

  // Init default_wallpaper_image_ with 1x1 image of default color.
  virtual void CreateSolidDefaultWallpaper();

  // The number of loaded wallpapers.
  int loaded_wallpapers_for_test_;

  base::ThreadChecker thread_checker_;

  // Sequence token associated with wallpaper operations.
  base::SequencedWorkerPool::SequenceToken sequence_token_;

  // Wallpaper sequenced task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Logged-in user wallpaper information.
  WallpaperInfo current_user_wallpaper_info_;

  // If non-NULL, used in place of the real command line.
  base::CommandLine* command_line_for_testing_;

  // Caches wallpapers of users. Accessed only on UI thread.
  CustomWallpaperMap wallpaper_cache_;

  // The last selected user on user pod row.
  AccountId last_selected_user_ = EmptyAccountId();

  bool should_cache_wallpaper_;

  base::ObserverList<Observer> observers_;

  // These members are for the scheduler:

  // When last load attempt finished.
  base::Time last_load_finished_at_;

  // last N wallpaper loads times.
  std::deque<base::TimeDelta> last_load_times_;

  base::FilePath default_small_wallpaper_file_;
  base::FilePath default_large_wallpaper_file_;

  base::FilePath guest_small_wallpaper_file_;
  base::FilePath guest_large_wallpaper_file_;

  base::FilePath child_small_wallpaper_file_;
  base::FilePath child_large_wallpaper_file_;

  // Current decoded default image is stored in cache.
  std::unique_ptr<user_manager::UserImage> default_wallpaper_image_;

  base::WeakPtrFactory<WallpaperManagerBase> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperManagerBase);
};

}  // namespace wallpaper

#endif  // COMPONENTS_WALLPAPER_WALLPAPER_MANAGER_BASE_H_
