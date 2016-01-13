// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <X11/extensions/Xfixes.h>
#include <X11/Xatom.h>
#include <list>
#include <set>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_requestor.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

namespace {

const char kClipboard[] = "CLIPBOARD";
const char kClipboardManager[] = "CLIPBOARD_MANAGER";
const char kMimeTypeFilename[] = "chromium/filename";
const char kMimeTypePepperCustomData[] = "chromium/x-pepper-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
const char kSaveTargets[] = "SAVE_TARGETS";
const char kTargets[] = "TARGETS";

const char* kAtomsToCache[] = {
  kClipboard,
  kClipboardManager,
  Clipboard::kMimeTypePNG,
  kMimeTypeFilename,
  kMimeTypeMozillaURL,
  kMimeTypeWebkitSmartPaste,
  kSaveTargets,
  kString,
  kTargets,
  kText,
  kUtf8String,
  NULL
};

///////////////////////////////////////////////////////////////////////////////

// Uses the XFixes API to provide sequence numbers for GetSequenceNumber().
class SelectionChangeObserver : public ui::PlatformEventObserver {
 public:
  static SelectionChangeObserver* GetInstance();

  uint64 clipboard_sequence_number() const {
    return clipboard_sequence_number_;
  }
  uint64 primary_sequence_number() const { return primary_sequence_number_; }

 private:
  friend struct DefaultSingletonTraits<SelectionChangeObserver>;

  SelectionChangeObserver();
  virtual ~SelectionChangeObserver();

  // ui::PlatformEventObserver:
  virtual void WillProcessEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const ui::PlatformEvent& event) OVERRIDE {}

  int event_base_;
  Atom clipboard_atom_;
  uint64 clipboard_sequence_number_;
  uint64 primary_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(SelectionChangeObserver);
};

SelectionChangeObserver::SelectionChangeObserver()
    : event_base_(-1),
      clipboard_atom_(None),
      clipboard_sequence_number_(0),
      primary_sequence_number_(0) {
  int ignored;
  if (XFixesQueryExtension(gfx::GetXDisplay(), &event_base_, &ignored)) {
    clipboard_atom_ = XInternAtom(gfx::GetXDisplay(), kClipboard, false);
    XFixesSelectSelectionInput(gfx::GetXDisplay(), GetX11RootWindow(),
                               clipboard_atom_,
                               XFixesSetSelectionOwnerNotifyMask |
                               XFixesSelectionWindowDestroyNotifyMask |
                               XFixesSelectionClientCloseNotifyMask);
    // This seems to be semi-optional. For some reason, registering for any
    // selection notify events seems to subscribe us to events for both the
    // primary and the clipboard buffers. Register anyway just to be safe.
    XFixesSelectSelectionInput(gfx::GetXDisplay(), GetX11RootWindow(),
                               XA_PRIMARY,
                               XFixesSetSelectionOwnerNotifyMask |
                               XFixesSelectionWindowDestroyNotifyMask |
                               XFixesSelectionClientCloseNotifyMask);

    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  }
}

SelectionChangeObserver::~SelectionChangeObserver() {
  // We are a singleton; we will outlive the event source.
}

SelectionChangeObserver* SelectionChangeObserver::GetInstance() {
  return Singleton<SelectionChangeObserver>::get();
}

void SelectionChangeObserver::WillProcessEvent(const ui::PlatformEvent& event) {
  if (event->type == event_base_ + XFixesSelectionNotify) {
    XFixesSelectionNotifyEvent* ev =
        reinterpret_cast<XFixesSelectionNotifyEvent*>(event);
    if (ev->selection == clipboard_atom_) {
      clipboard_sequence_number_++;
    } else if (ev->selection == XA_PRIMARY) {
      primary_sequence_number_++;
    } else {
      DLOG(ERROR) << "Unexpected selection atom: " << ev->selection;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

// Represents a list of possible return types. Copy constructable.
class TargetList {
 public:
  typedef std::vector< ::Atom> AtomVector;

  TargetList(const AtomVector& target_list, X11AtomCache* atom_cache);

  const AtomVector& target_list() { return target_list_; }

  bool ContainsText() const;
  bool ContainsFormat(const Clipboard::FormatType& format_type) const;
  bool ContainsAtom(::Atom atom) const;

 private:
  AtomVector target_list_;
  X11AtomCache* atom_cache_;
};

TargetList::TargetList(const AtomVector& target_list,
                       X11AtomCache* atom_cache)
    : target_list_(target_list),
      atom_cache_(atom_cache) {
}

bool TargetList::ContainsText() const {
  std::vector< ::Atom> atoms = GetTextAtomsFrom(atom_cache_);
  for (std::vector< ::Atom>::const_iterator it = atoms.begin();
       it != atoms.end(); ++it) {
    if (ContainsAtom(*it))
      return true;
  }

  return false;
}

bool TargetList::ContainsFormat(
    const Clipboard::FormatType& format_type) const {
  ::Atom atom = atom_cache_->GetAtom(format_type.ToString().c_str());
  return ContainsAtom(atom);
}

bool TargetList::ContainsAtom(::Atom atom) const {
  return find(target_list_.begin(), target_list_.end(), atom)
      != target_list_.end();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

// I would love for the FormatType to really be a wrapper around an X11 ::Atom,
// but there are a few problems. Chromeos unit tests spawn a new X11 server for
// each test, so Atom numeric values don't persist across tests. We could still
// maybe deal with that if we didn't have static accessor methods everywhere.

Clipboard::FormatType::FormatType() {
}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {
}

Clipboard::FormatType::~FormatType() {
}

std::string Clipboard::FormatType::Serialize() const {
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::operator<(const FormatType& other) const {
  return data_ < other.data_;
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return data_ == other.data_;
}

///////////////////////////////////////////////////////////////////////////////
// Clipboard::AuraX11Details

// Private implementation of our X11 integration. Keeps X11 headers out of the
// majority of chrome, which break badly.
class Clipboard::AuraX11Details : public PlatformEventDispatcher {
 public:
  AuraX11Details();
  virtual ~AuraX11Details();

  X11AtomCache* atom_cache() { return &atom_cache_; }

  // Returns the X11 type that we pass to various XSelection functions for the
  // given type.
  ::Atom LookupSelectionForClipboardType(ClipboardType type) const;

  // Returns the X11 type that we pass to various XSelection functions for
  // CLIPBOARD_TYPE_COPY_PASTE.
  ::Atom GetCopyPasteSelection() const;

  // Returns the object which is responsible for communication on |type|.
  SelectionRequestor* GetSelectionRequestorForClipboardType(ClipboardType type);

  // Finds the SelectionFormatMap for the incoming selection atom.
  const SelectionFormatMap& LookupStorageForAtom(::Atom atom);

  // As we need to collect all the data types before we tell X11 that we own a
  // particular selection, we create a temporary clipboard mapping that
  // InsertMapping writes to. Then we commit it in TakeOwnershipOfSelection,
  // where we save it in one of the clipboard data slots.
  void CreateNewClipboardData();

  // Inserts a mapping into clipboard_data_.
  void InsertMapping(const std::string& key,
                     const scoped_refptr<base::RefCountedMemory>& memory);

  // Moves the temporary |clipboard_data_| to the long term data storage for
  // |type|.
  void TakeOwnershipOfSelection(ClipboardType type);

  // Returns the first of |types| offered by the current selection holder in
  // |data_out|, or returns NULL if none of those types are available.
  //
  // If the selection holder is us, this call is synchronous and we pull
  // the data out of |clipboard_selection_| or |primary_selection_|. If the
  // selection holder is some other window, we spin up a nested message loop
  // and do the asynchronous dance with whatever application is holding the
  // selection.
  ui::SelectionData RequestAndWaitForTypes(ClipboardType type,
                                           const std::vector< ::Atom>& types);

  // Retrieves the list of possible data types the current clipboard owner has.
  //
  // If the selection holder is us, this is synchronous, otherwise this runs a
  // blocking message loop.
  TargetList WaitAndGetTargetsList(ClipboardType type);

  // Returns a list of all text atoms that we handle.
  std::vector< ::Atom> GetTextAtoms() const;

  // Returns a vector with a |format| converted to an X11 atom.
  std::vector< ::Atom> GetAtomsForFormat(const Clipboard::FormatType& format);

  // Clears a certain clipboard type, whether we own it or not.
  void Clear(ClipboardType type);

  // If we own the CLIPBOARD selection, requests the clipboard manager to take
  // ownership of it.
  void StoreCopyPasteDataAndWait();

 private:
  // PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const PlatformEvent& event) OVERRIDE;

  // Our X11 state.
  Display* x_display_;
  ::Window x_root_window_;

  // Input-only window used as a selection owner.
  ::Window x_window_;

  X11AtomCache atom_cache_;

  // Objects which request and receive selection data.
  SelectionRequestor clipboard_requestor_;
  SelectionRequestor primary_requestor_;
  SelectionRequestor clipboard_manager_requestor_;

  // Temporary target map that we write to during DispatchObects.
  SelectionFormatMap clipboard_data_;

  // Objects which offer selection data to other windows.
  SelectionOwner clipboard_owner_;
  SelectionOwner primary_owner_;

  DISALLOW_COPY_AND_ASSIGN(AuraX11Details);
};

Clipboard::AuraX11Details::AuraX11Details()
    : x_display_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(x_display_)),
      x_window_(XCreateWindow(
          x_display_, x_root_window_,
          -100, -100, 10, 10,  // x, y, width, height
          0,                   // border width
          CopyFromParent,      // depth
          InputOnly,
          CopyFromParent,      // visual
          0,
          NULL)),
      atom_cache_(x_display_, kAtomsToCache),
      clipboard_requestor_(x_display_, x_window_,
                           atom_cache_.GetAtom(kClipboard), this),
      primary_requestor_(x_display_, x_window_, XA_PRIMARY, this),
      clipboard_manager_requestor_(x_display_, x_window_,
                                   atom_cache_.GetAtom(kClipboardManager),
                                   this),
      clipboard_owner_(x_display_, x_window_, atom_cache_.GetAtom(kClipboard)),
      primary_owner_(x_display_, x_window_, XA_PRIMARY) {
  // We don't know all possible MIME types at compile time.
  atom_cache_.allow_uncached_atoms();

  XStoreName(x_display_, x_window_, "Chromium clipboard");
  XSelectInput(x_display_, x_window_, PropertyChangeMask);

  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

Clipboard::AuraX11Details::~AuraX11Details() {
  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

  XDestroyWindow(x_display_, x_window_);
}

::Atom Clipboard::AuraX11Details::LookupSelectionForClipboardType(
    ClipboardType type) const {
  if (type == CLIPBOARD_TYPE_COPY_PASTE)
    return GetCopyPasteSelection();

  return XA_PRIMARY;
}

::Atom Clipboard::AuraX11Details::GetCopyPasteSelection() const {
  return atom_cache_.GetAtom(kClipboard);
}

const SelectionFormatMap& Clipboard::AuraX11Details::LookupStorageForAtom(
    ::Atom atom) {
  if (atom == XA_PRIMARY)
    return primary_owner_.selection_format_map();

  DCHECK_EQ(GetCopyPasteSelection(), atom);
  return clipboard_owner_.selection_format_map();
}

ui::SelectionRequestor*
Clipboard::AuraX11Details::GetSelectionRequestorForClipboardType(
    ClipboardType type) {
  if (type == CLIPBOARD_TYPE_COPY_PASTE)
    return &clipboard_requestor_;
  else
    return &primary_requestor_;
}

void Clipboard::AuraX11Details::CreateNewClipboardData() {
  clipboard_data_ = SelectionFormatMap();
}

void Clipboard::AuraX11Details::InsertMapping(
    const std::string& key,
    const scoped_refptr<base::RefCountedMemory>& memory) {
  ::Atom atom_key = atom_cache_.GetAtom(key.c_str());
  clipboard_data_.Insert(atom_key, memory);
}

void Clipboard::AuraX11Details::TakeOwnershipOfSelection(ClipboardType type) {
  if (type == CLIPBOARD_TYPE_COPY_PASTE)
    return clipboard_owner_.TakeOwnershipOfSelection(clipboard_data_);
  else
    return primary_owner_.TakeOwnershipOfSelection(clipboard_data_);
}

SelectionData Clipboard::AuraX11Details::RequestAndWaitForTypes(
    ClipboardType type,
    const std::vector< ::Atom>& types) {
  ::Atom selection_name = LookupSelectionForClipboardType(type);
  if (XGetSelectionOwner(x_display_, selection_name) == x_window_) {
    // We can local fastpath instead of playing the nested message loop game
    // with the X server.
    const SelectionFormatMap& format_map = LookupStorageForAtom(selection_name);

    for (std::vector< ::Atom>::const_iterator it = types.begin();
         it != types.end(); ++it) {
      SelectionFormatMap::const_iterator format_map_it = format_map.find(*it);
      if (format_map_it != format_map.end())
        return SelectionData(format_map_it->first, format_map_it->second);
    }
  } else {
    TargetList targets = WaitAndGetTargetsList(type);
    SelectionRequestor* receiver = GetSelectionRequestorForClipboardType(type);

    std::vector< ::Atom> intersection;
    ui::GetAtomIntersection(types, targets.target_list(), &intersection);
    return receiver->RequestAndWaitForTypes(intersection);
  }

  return SelectionData();
}

TargetList Clipboard::AuraX11Details::WaitAndGetTargetsList(
    ClipboardType type) {
  ::Atom selection_name = LookupSelectionForClipboardType(type);
  std::vector< ::Atom> out;
  if (XGetSelectionOwner(x_display_, selection_name) == x_window_) {
    // We can local fastpath and return the list of local targets.
    const SelectionFormatMap& format_map = LookupStorageForAtom(selection_name);

    for (SelectionFormatMap::const_iterator it = format_map.begin();
         it != format_map.end(); ++it) {
      out.push_back(it->first);
    }
  } else {
    scoped_refptr<base::RefCountedMemory> data;
    size_t out_data_items = 0;
    ::Atom out_type = None;

    SelectionRequestor* receiver = GetSelectionRequestorForClipboardType(type);
    if (receiver->PerformBlockingConvertSelection(atom_cache_.GetAtom(kTargets),
                                                  &data,
                                                  NULL,
                                                  &out_data_items,
                                                  &out_type)) {
      // Some apps return an |out_type| of "TARGETS". (crbug.com/377893)
      if (out_type == XA_ATOM || out_type == atom_cache_.GetAtom(kTargets)) {
        const ::Atom* atom_array =
            reinterpret_cast<const ::Atom*>(data->front());
        for (size_t i = 0; i < out_data_items; ++i)
          out.push_back(atom_array[i]);
      }
    } else {
      // There was no target list. Most Java apps doesn't offer a TARGETS list,
      // even though they AWT to. They will offer individual text types if you
      // ask. If this is the case we attempt to make sense of the contents as
      // text. This is pretty unfortunate since it means we have to actually
      // copy the data to see if it is available, but at least this path
      // shouldn't be hit for conforming programs.
      std::vector< ::Atom> types = GetTextAtoms();
      for (std::vector< ::Atom>::const_iterator it = types.begin();
           it != types.end(); ++it) {
        ::Atom type = None;
        if (receiver->PerformBlockingConvertSelection(*it,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      &type) &&
            type == *it) {
          out.push_back(*it);
        }
      }
    }
  }

  return TargetList(out, &atom_cache_);
}

std::vector< ::Atom> Clipboard::AuraX11Details::GetTextAtoms() const {
  return GetTextAtomsFrom(&atom_cache_);
}

std::vector< ::Atom> Clipboard::AuraX11Details::GetAtomsForFormat(
    const Clipboard::FormatType& format) {
  std::vector< ::Atom> atoms;
  atoms.push_back(atom_cache_.GetAtom(format.ToString().c_str()));
  return atoms;
}

void Clipboard::AuraX11Details::Clear(ClipboardType type) {
  if (type == CLIPBOARD_TYPE_COPY_PASTE)
    clipboard_owner_.ClearSelectionOwner();
  else
    primary_owner_.ClearSelectionOwner();
}

void Clipboard::AuraX11Details::StoreCopyPasteDataAndWait() {
  ::Atom selection = GetCopyPasteSelection();
  if (XGetSelectionOwner(x_display_, selection) != x_window_)
    return;

  ::Atom clipboard_manager_atom = atom_cache_.GetAtom(kClipboardManager);
  if (XGetSelectionOwner(x_display_, clipboard_manager_atom) == None)
    return;

  const SelectionFormatMap& format_map = LookupStorageForAtom(selection);
  if (format_map.size() == 0)
    return;
  std::vector<Atom> targets = format_map.GetTypes();

  base::TimeTicks start = base::TimeTicks::Now();
  clipboard_manager_requestor_.PerformBlockingConvertSelectionWithParameter(
      atom_cache_.GetAtom(kSaveTargets), targets);
  UMA_HISTOGRAM_TIMES("Clipboard.X11StoreCopyPasteDuration",
                      base::TimeTicks::Now() - start);
}

bool Clipboard::AuraX11Details::CanDispatchEvent(const PlatformEvent& event) {
  return event->xany.window == x_window_;
}

uint32_t Clipboard::AuraX11Details::DispatchEvent(const PlatformEvent& xev) {
  switch (xev->type) {
    case SelectionRequest: {
      if (xev->xselectionrequest.selection == XA_PRIMARY) {
        primary_owner_.OnSelectionRequest(xev->xselectionrequest);
      } else {
        // We should not get requests for the CLIPBOARD_MANAGER selection
        // because we never take ownership of it.
        DCHECK_EQ(GetCopyPasteSelection(), xev->xselectionrequest.selection);
        clipboard_owner_.OnSelectionRequest(xev->xselectionrequest);
      }
      break;
    }
    case SelectionNotify: {
      ::Atom selection = xev->xselection.selection;
      if (selection == XA_PRIMARY)
        primary_requestor_.OnSelectionNotify(xev->xselection);
      else if (selection == GetCopyPasteSelection())
        clipboard_requestor_.OnSelectionNotify(xev->xselection);
      else if (selection == atom_cache_.GetAtom(kClipboardManager))
        clipboard_manager_requestor_.OnSelectionNotify(xev->xselection);
      break;
    }
    case SelectionClear: {
      if (xev->xselectionclear.selection == XA_PRIMARY) {
        primary_owner_.OnSelectionClear(xev->xselectionclear);
      } else {
        // We should not get requests for the CLIPBOARD_MANAGER selection
        // because we never take ownership of it.
        DCHECK_EQ(GetCopyPasteSelection(), xev->xselection.selection);
        clipboard_owner_.OnSelectionClear(xev->xselectionclear);
        }
      break;
    }
    default:
      break;
  }

  return POST_DISPATCH_NONE;
}

///////////////////////////////////////////////////////////////////////////////
// Clipboard

Clipboard::Clipboard()
    : aurax11_details_(new AuraX11Details) {
  DCHECK(CalledOnValidThread());
}

Clipboard::~Clipboard() {
  DCHECK(CalledOnValidThread());

  aurax11_details_->StoreCopyPasteDataAndWait();
}

void Clipboard::WriteObjects(ClipboardType type, const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardType(type));

  aurax11_details_->CreateNewClipboardData();
  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }
  aurax11_details_->TakeOwnershipOfSelection(type);

  if (type == CLIPBOARD_TYPE_COPY_PASTE) {
    ObjectMap::const_iterator text_iter = objects.find(CBF_TEXT);
    if (text_iter != objects.end()) {
      aurax11_details_->CreateNewClipboardData();
      const ObjectMapParams& params_vector = text_iter->second;
      if (params_vector.size()) {
        const ObjectMapParam& char_vector = params_vector[0];
        if (char_vector.size())
          WriteText(&char_vector.front(), char_vector.size());
      }
      aurax11_details_->TakeOwnershipOfSelection(CLIPBOARD_TYPE_SELECTION);
    }
  }
}

bool Clipboard::IsFormatAvailable(const FormatType& format,
                                  ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardType(type));

  TargetList target_list = aurax11_details_->WaitAndGetTargetsList(type);
  if (format.Equals(GetPlainTextFormatType()) ||
      format.Equals(GetUrlFormatType())) {
    return target_list.ContainsText();
  }
  return target_list.ContainsFormat(format);
}

void Clipboard::Clear(ClipboardType type) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardType(type));
  aurax11_details_->Clear(type);
}

void Clipboard::ReadAvailableTypes(ClipboardType type,
                                   std::vector<base::string16>* types,
                                   bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  TargetList target_list = aurax11_details_->WaitAndGetTargetsList(type);

  types->clear();

  if (target_list.ContainsText())
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (target_list.ContainsFormat(GetHtmlFormatType()))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  if (target_list.ContainsFormat(GetRtfFormatType()))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (target_list.ContainsFormat(GetBitmapFormatType()))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  *contains_filenames = false;

  SelectionData data(aurax11_details_->RequestAndWaitForTypes(
      type, aurax11_details_->GetAtomsForFormat(GetWebCustomDataFormatType())));
  if (data.IsValid())
    ReadCustomDataTypes(data.GetData(), data.GetSize(), types);
}

void Clipboard::ReadText(ClipboardType type, base::string16* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(aurax11_details_->RequestAndWaitForTypes(
      type, aurax11_details_->GetTextAtoms()));
  if (data.IsValid()) {
    std::string text = data.GetText();
    *result = base::UTF8ToUTF16(text);
  }
}

void Clipboard::ReadAsciiText(ClipboardType type, std::string* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(aurax11_details_->RequestAndWaitForTypes(
      type, aurax11_details_->GetTextAtoms()));
  if (data.IsValid())
    result->assign(data.GetText());
}

// TODO(estade): handle different charsets.
// TODO(port): set *src_url.
void Clipboard::ReadHTML(ClipboardType type,
                         base::string16* markup,
                         std::string* src_url,
                         uint32* fragment_start,
                         uint32* fragment_end) const {
  DCHECK(CalledOnValidThread());
  markup->clear();
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  SelectionData data(aurax11_details_->RequestAndWaitForTypes(
      type, aurax11_details_->GetAtomsForFormat(GetHtmlFormatType())));
  if (data.IsValid()) {
    *markup = data.GetHtml();

    *fragment_start = 0;
    DCHECK(markup->length() <= kuint32max);
    *fragment_end = static_cast<uint32>(markup->length());
  }
}

void Clipboard::ReadRTF(ClipboardType type, std::string* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(aurax11_details_->RequestAndWaitForTypes(
      type, aurax11_details_->GetAtomsForFormat(GetRtfFormatType())));
  if (data.IsValid())
    data.AssignTo(result);
}

SkBitmap Clipboard::ReadImage(ClipboardType type) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(aurax11_details_->RequestAndWaitForTypes(
      type, aurax11_details_->GetAtomsForFormat(GetBitmapFormatType())));
  if (data.IsValid()) {
    SkBitmap bitmap;
    if (gfx::PNGCodec::Decode(data.GetData(), data.GetSize(), &bitmap))
      return SkBitmap(bitmap);
  }

  return SkBitmap();
}

void Clipboard::ReadCustomData(ClipboardType clipboard_type,
                               const base::string16& type,
                               base::string16* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(aurax11_details_->RequestAndWaitForTypes(
      clipboard_type,
      aurax11_details_->GetAtomsForFormat(GetWebCustomDataFormatType())));
  if (data.IsValid())
    ReadCustomDataForType(data.GetData(), data.GetSize(), type, result);
}

void Clipboard::ReadBookmark(base::string16* title, std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(erg): This was left NOTIMPLEMENTED() in the gtk port too.
  NOTIMPLEMENTED();
}

void Clipboard::ReadData(const FormatType& format, std::string* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(aurax11_details_->RequestAndWaitForTypes(
      CLIPBOARD_TYPE_COPY_PASTE, aurax11_details_->GetAtomsForFormat(format)));
  if (data.IsValid())
    data.AssignTo(result);
}

uint64 Clipboard::GetSequenceNumber(ClipboardType type) {
  DCHECK(CalledOnValidThread());
  if (type == CLIPBOARD_TYPE_COPY_PASTE)
    return SelectionChangeObserver::GetInstance()->clipboard_sequence_number();
  else
    return SelectionChangeObserver::GetInstance()->primary_sequence_number();
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  std::string str(text_data, text_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&str));

  aurax11_details_->InsertMapping(kMimeTypeText, mem);
  aurax11_details_->InsertMapping(kText, mem);
  aurax11_details_->InsertMapping(kString, mem);
  aurax11_details_->InsertMapping(kUtf8String, mem);
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  // TODO(estade): We need to expand relative links with |url_data|.
  static const char* html_prefix = "<meta http-equiv=\"content-type\" "
                                   "content=\"text/html; charset=utf-8\">";
  std::string data = html_prefix;
  data += std::string(markup_data, markup_len);
  // Some programs expect NULL-terminated data. See http://crbug.com/42624
  data += '\0';

  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&data));
  aurax11_details_->InsertMapping(kMimeTypeHTML, mem);
}

void Clipboard::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(GetRtfFormatType(), rtf_data, data_len);
}

void Clipboard::WriteBookmark(const char* title_data,
                              size_t title_len,
                              const char* url_data,
                              size_t url_len) {
  // Write as a mozilla url (UTF16: URL, newline, title).
  base::string16 url = base::UTF8ToUTF16(std::string(url_data, url_len) + "\n");
  base::string16 title = base::UTF8ToUTF16(std::string(title_data, title_len));

  std::vector<unsigned char> data;
  ui::AddString16ToVector(url, &data);
  ui::AddString16ToVector(title, &data);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&data));

  aurax11_details_->InsertMapping(kMimeTypeMozillaURL, mem);
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void Clipboard::WriteWebSmartPaste() {
  std::string empty;
  aurax11_details_->InsertMapping(
      kMimeTypeWebkitSmartPaste,
      scoped_refptr<base::RefCountedMemory>(
          base::RefCountedString::TakeString(&empty)));
}

void Clipboard::WriteBitmap(const SkBitmap& bitmap) {
  // Encode the bitmap as a PNG for transport.
  std::vector<unsigned char> output;
  if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output)) {
    aurax11_details_->InsertMapping(kMimeTypePNG,
                                    base::RefCountedBytes::TakeVector(
                                        &output));
  }
}

void Clipboard::WriteData(const FormatType& format,
                          const char* data_data,
                          size_t data_len) {
  // We assume that certain mapping types are only written by trusted code.
  // Therefore we must upkeep their integrity.
  if (format.Equals(GetBitmapFormatType()))
    return;

  std::vector<unsigned char> bytes(data_data, data_data + data_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));
  aurax11_details_->InsertMapping(format.ToString(), mem);
}

// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetUrlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeURIList));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetUrlWFormatType() {
  return GetUrlFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeText));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeFilename));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameWFormatType() {
  return Clipboard::GetFilenameFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeHTML));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeRTF));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePNG));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebkitSmartPaste));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomData));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePepperCustomData));
  return type;
}

}  // namespace ui
