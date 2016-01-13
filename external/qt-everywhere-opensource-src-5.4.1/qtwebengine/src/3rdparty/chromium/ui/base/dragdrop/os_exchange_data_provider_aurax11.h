// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_

#include <X11/Xlib.h>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include <map>

#include "base/files/file_path.h"
#include "base/pickle.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_requestor.h"
#include "ui/base/x/selection_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "url/gurl.h"

namespace ui {

class Clipboard;
class OSExchangeDataProviderAuraX11Test;

// OSExchangeData::Provider implementation for aura on linux.
class UI_BASE_EXPORT OSExchangeDataProviderAuraX11
    : public OSExchangeData::Provider,
      public ui::PlatformEventDispatcher {
 public:
  // |x_window| is the window the cursor is over, and |selection| is the set of
  // data being offered.
  OSExchangeDataProviderAuraX11(::Window x_window,
                                const SelectionFormatMap& selection);

  // Creates a Provider for sending drag information. This creates its own,
  // hidden X11 window to own send data.
  OSExchangeDataProviderAuraX11();

  virtual ~OSExchangeDataProviderAuraX11();

  // After all the Set* methods have built up the data we're offering, call
  // this to take ownership of the XdndSelection clipboard.
  void TakeOwnershipOfSelection() const;

  // Retrieves a list of types we're offering. Noop if we haven't taken the
  // selection.
  void RetrieveTargets(std::vector<Atom>* targets) const;

  // Makes a copy of the format map currently being offered.
  SelectionFormatMap GetFormatMap() const;

  const base::FilePath& file_contents_name() const {
    return file_contents_name_;
  }

  // Overridden from OSExchangeData::Provider:
  virtual Provider* Clone() const OVERRIDE;
  virtual void MarkOriginatedFromRenderer() OVERRIDE;
  virtual bool DidOriginateFromRenderer() const OVERRIDE;
  virtual void SetString(const base::string16& data) OVERRIDE;
  virtual void SetURL(const GURL& url, const base::string16& title) OVERRIDE;
  virtual void SetFilename(const base::FilePath& path) OVERRIDE;
  virtual void SetFilenames(const std::vector<FileInfo>& filenames) OVERRIDE;
  virtual void SetPickledData(const OSExchangeData::CustomFormat& format,
                              const Pickle& pickle) OVERRIDE;
  virtual bool GetString(base::string16* data) const OVERRIDE;
  virtual bool GetURLAndTitle(OSExchangeData::FilenameToURLPolicy policy,
                              GURL* url,
                              base::string16* title) const OVERRIDE;
  virtual bool GetFilename(base::FilePath* path) const OVERRIDE;
  virtual bool GetFilenames(std::vector<FileInfo>* filenames) const OVERRIDE;
  virtual bool GetPickledData(const OSExchangeData::CustomFormat& format,
                              Pickle* pickle) const OVERRIDE;
  virtual bool HasString() const OVERRIDE;
  virtual bool HasURL(OSExchangeData::FilenameToURLPolicy policy) const
      OVERRIDE;
  virtual bool HasFile() const OVERRIDE;
  virtual bool HasCustomFormat(const OSExchangeData::CustomFormat& format) const
      OVERRIDE;

  virtual void SetFileContents(const base::FilePath& filename,
                               const std::string& file_contents) OVERRIDE;

  virtual void SetHtml(const base::string16& html,
                       const GURL& base_url) OVERRIDE;
  virtual bool GetHtml(base::string16* html, GURL* base_url) const OVERRIDE;
  virtual bool HasHtml() const OVERRIDE;
  virtual void SetDragImage(const gfx::ImageSkia& image,
                            const gfx::Vector2d& cursor_offset) OVERRIDE;
  virtual const gfx::ImageSkia& GetDragImage() const OVERRIDE;
  virtual const gfx::Vector2d& GetDragImageOffset() const OVERRIDE;

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const PlatformEvent& event) OVERRIDE;

 private:
  friend class OSExchangeDataProviderAuraX11Test;
  typedef std::map<OSExchangeData::CustomFormat, Pickle>  PickleData;

  // Returns true if |formats_| contains a string format and the string can be
  // parsed as a URL.
  bool GetPlainTextURL(GURL* url) const;

  // Returns the targets in |format_map_|.
  std::vector< ::Atom> GetTargets() const;

  // Drag image and offset data.
  gfx::ImageSkia drag_image_;
  gfx::Vector2d drag_image_offset_;

  // Our X11 state.
  Display* x_display_;
  ::Window x_root_window_;

  // In X11, because the IPC parts of drag operations are implemented by
  // XSelection, we require an x11 window to receive drag messages on. The
  // OSExchangeDataProvider system is modeled on the Windows implementation,
  // which does not require a window. We only sometimes have a valid window
  // available (in the case of drag receiving). Other times, we need to create
  // our own xwindow just to receive events on it.
  const bool own_window_;

  ::Window x_window_;

  X11AtomCache atom_cache_;

  // A representation of data. This is either passed to us from the other
  // process, or built up through a sequence of Set*() calls. It can be passed
  // to |selection_owner_| when we take the selection.
  SelectionFormatMap format_map_;

  // Auxilary data for the X Direct Save protocol.
  base::FilePath file_contents_name_;

  // Takes a snapshot of |format_map_| and offers it to other windows.
  mutable SelectionOwner selection_owner_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderAuraX11);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_
