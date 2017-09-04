// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_URL_INDEX_H_
#define MEDIA_BLINK_URL_INDEX_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/blink/lru.h"
#include "media/blink/media_blink_export.h"
#include "media/blink/multibuffer.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "url/gurl.h"

namespace media {

const int64_t kPositionNotSpecified = -1;

class UrlData;

// A multibuffer for loading media resources which knows
// how to create MultiBufferDataProviders to load data
// into the cache.
class MEDIA_BLINK_EXPORT ResourceMultiBuffer
    : NON_EXPORTED_BASE(public MultiBuffer) {
 public:
  ResourceMultiBuffer(UrlData* url_data_, int block_shift);
  ~ResourceMultiBuffer() override;

  // MultiBuffer implementation.
  std::unique_ptr<MultiBuffer::DataProvider> CreateWriter(
      const BlockId& pos) override;
  bool RangeSupported() const override;
  void OnEmpty() override;

 protected:
  // Do not access from destructor, it is a pointer to the
  // object that contains us.
  UrlData* url_data_;
};

class UrlIndex;

// All the data & metadata for a single resource.
// Data is cached using a MultiBuffer instance.
class MEDIA_BLINK_EXPORT UrlData : public base::RefCounted<UrlData> {
 public:
  // Keep in sync with WebMediaPlayer::CORSMode.
  enum CORSMode { CORS_UNSPECIFIED, CORS_ANONYMOUS, CORS_USE_CREDENTIALS };
  typedef std::pair<GURL, CORSMode> KeyType;

  // Accessors
  const GURL& url() const { return url_; }

  // Cross-origin access mode
  CORSMode cors_mode() const { return cors_mode_; }

  // Are HTTP range requests supported?
  bool range_supported() const { return range_supported_; }

  // True if we found a reason why this URL won't be stored in the
  // HTTP disk cache.
  bool cacheable() const { return cacheable_; }

  // Last used time.
  base::Time last_used() const { return last_used_; }

  // Last modified time.
  base::Time last_modified() const { return last_modified_; }

  const std::string& etag() const { return etag_; }

  // Expiration time.
  base::Time valid_until() const { return valid_until_; }

  // The key used by UrlIndex to find this UrlData.
  KeyType key() const;

  // Length of data associated with url or |kPositionNotSpecified|
  int64_t length() const { return length_; }

  // Returns the number of blocks cached for this resource.
  size_t CachedSize();

  // Returns our url_index, or nullptr if it's been destroyed.
  UrlIndex* url_index() const { return url_index_.get(); }

  // Notifies the url index that this is currently used.
  // The url <-> URLData mapping will be eventually be invalidated if
  // this is not called regularly.
  void Use();

  // Call this before we add some data to the multibuffer().
  // If the multibuffer is empty, the data origin is set from
  // |origin| and returns true. If not, it compares |origin|
  // to the previous origin and returns wheather they match or not.
  bool ValidateDataOrigin(const GURL& origin);

  // Setters.
  void set_length(int64_t length);
  void set_cacheable(bool cacheable);
  void set_valid_until(base::Time valid_until);
  void set_range_supported();
  void set_last_modified(base::Time last_modified);
  void set_etag(const std::string& etag);

  // A redirect has occured (or we've found a better UrlData for the same
  // resource).
  void RedirectTo(const scoped_refptr<UrlData>& to);

  // Fail, tell all clients that a failure has occured.
  void Fail();

  // Callback for receving notifications when a redirect occurs.
  typedef base::Callback<void(const scoped_refptr<UrlData>&)> RedirectCB;

  // Register a callback to be called when a redirect occurs.
  // Callbacks are cleared when a redirect occurs, so clients must call
  // OnRedirect again if they wish to continue receiving callbacks.
  void OnRedirect(const RedirectCB& cb);

  // Returns true it is valid to keep using this to access cached data.
  // A single media player instance may choose to ignore this for resources
  // that have already been opened.
  bool Valid() const;

  // Virtual so we can override it for testing.
  virtual ResourceMultiBuffer* multibuffer();

  // Accessor
  blink::WebFrame* frame() const { return frame_; }

 protected:
  UrlData(const GURL& url,
          CORSMode cors_mode,
          const base::WeakPtr<UrlIndex>& url_index);
  virtual ~UrlData();

 private:
  friend class ResourceMultiBuffer;
  friend class UrlIndex;
  friend class base::RefCounted<UrlData>;

  void OnEmpty();
  void MergeFrom(const scoped_refptr<UrlData>& other);

  // Url we represent, note that there may be multiple UrlData for
  // the same url.
  const GURL url_;

  // Origin of the data, should only be different from the url_.GetOrigin()
  // when service workers are involved.
  GURL data_origin_;
  bool have_data_origin_;

  // Cross-origin access mode.
  const CORSMode cors_mode_;

  base::WeakPtr<UrlIndex> url_index_;

  // Length of resource this url points to. (in bytes)
  int64_t length_;

  // Does the server support ranges?
  bool range_supported_;

  // Set to false if we have reason to beleive the chrome disk cache
  // will not cache this url.
  bool cacheable_;

  // Last time some media time used this resource.
  // Note that we use base::Time rather than base::TimeTicks because
  // TimeTicks will stop advancing when a machine goes to sleep.
  // base::Time can go backwards, jump hours at a time and be generally
  // unpredictable, but it doesn't stop, which is preferable here.
  // (False negatives are better than false positivies.)
  base::Time last_used_;

  // Expiration time according to http headers.
  base::Time valid_until_;

  // Last modification time according to http headers.
  base::Time last_modified_;

  // Etag from HTTP reply.
  std::string etag_;

  ResourceMultiBuffer multibuffer_;
  std::vector<RedirectCB> redirect_callbacks_;

  blink::WebFrame* frame_;

  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(UrlData);
};

// The UrlIndex lets you look up UrlData instances by url.
class MEDIA_BLINK_EXPORT UrlIndex {
 public:
  explicit UrlIndex(blink::WebFrame*);
  UrlIndex(blink::WebFrame*, int block_shift);
  virtual ~UrlIndex();

  // Look up an UrlData in the index and return it. If none is found,
  // create a new one. Note that newly created UrlData entries are NOT
  // added to the index, instead you must call TryInsert on them after
  // initializing relevant parameters, like whether it support
  // ranges and it's last modified time.
  scoped_refptr<UrlData> GetByUrl(const GURL& gurl,
                                  UrlData::CORSMode cors_mode);

  // Add the given UrlData to the index if possible. If a better UrlData
  // is already present in the index, return it instead. (If not, we just
  // return the given UrlData.) Please make sure to initialize all the data
  // that can be gathered from HTTP headers in |url_data| before calling this.
  // In particular, the following fields are important:
  //   o range_supported: Entries which do not support ranges cannot be
  //     shared and are not added to the index.
  //   o valid_until, last_used: Entries have to be valid to be inserted
  //     into the index, this means that they have to have been recently
  //     used or have an Expires: header that says when they stop being valid.
  //   o last_modified: Expired cache entries can be re-used if last_modified
  //     matches.
  // TODO(hubbe): Add etag support.
  scoped_refptr<UrlData> TryInsert(const scoped_refptr<UrlData>& url_data);

  blink::WebFrame* frame() const { return frame_; }
  int block_shift() const { return block_shift_; }

 private:
  friend class UrlData;
  friend class ResourceMultiBuffer;
  void RemoveUrlDataIfEmpty(const scoped_refptr<UrlData>& url_data);

  // Virtual so we can override it in tests.
  virtual scoped_refptr<UrlData> NewUrlData(const GURL& url,
                                            UrlData::CORSMode cors_mode);

  std::map<UrlData::KeyType, scoped_refptr<UrlData>> by_url_;
  blink::WebFrame* frame_;
  scoped_refptr<MultiBuffer::GlobalLRU> lru_;

  // log2 of block size in multibuffer cache. Defaults to kBlockSizeShift.
  // Currently only changed for testing purposes.
  const int block_shift_;

 protected:
  base::WeakPtrFactory<UrlIndex> weak_factory_;
};

}  // namespace media
#endif  // MEDIA_BLINK_URL_INDEX_H_
