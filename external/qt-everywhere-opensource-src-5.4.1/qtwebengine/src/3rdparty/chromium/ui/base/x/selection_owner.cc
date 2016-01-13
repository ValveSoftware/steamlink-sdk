// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_owner.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "base/logging.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"

namespace ui {

namespace {

const char kAtomPair[] = "ATOM_PAIR";
const char kMultiple[] = "MULTIPLE";
const char kSaveTargets[] = "SAVE_TARGETS";
const char kTargets[] = "TARGETS";

const char* kAtomsToCache[] = {
  kAtomPair,
  kMultiple,
  kSaveTargets,
  kTargets,
  NULL
};

// Gets the value of an atom pair array property. On success, true is returned
// and the value is stored in |value|.
bool GetAtomPairArrayProperty(XID window,
                              Atom property,
                              std::vector<std::pair<Atom,Atom> >* value) {
  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  unsigned long num_items = 0;
  unsigned char* properties = NULL;
  unsigned long remaining_bytes = 0;

  int result = XGetWindowProperty(gfx::GetXDisplay(),
                                  window,
                                  property,
                                  0,          // offset into property data to
                                              // read
                                  (~0L),      // entire array
                                  False,      // deleted
                                  AnyPropertyType,
                                  &type,
                                  &format,
                                  &num_items,
                                  &remaining_bytes,
                                  &properties);

  if (result != Success)
    return false;

  // GTK does not require |type| to be kAtomPair.
  if (format != 32 || num_items % 2 != 0) {
    XFree(properties);
    return false;
  }

  Atom* atom_properties = reinterpret_cast<Atom*>(properties);
  value->clear();
  for (size_t i = 0; i < num_items; i+=2)
    value->push_back(std::make_pair(atom_properties[i], atom_properties[i+1]));
  XFree(properties);
  return true;
}

}  // namespace

SelectionOwner::SelectionOwner(Display* x_display,
                               Window x_window,
                               Atom selection_name)
    : x_display_(x_display),
      x_window_(x_window),
      selection_name_(selection_name),
      atom_cache_(x_display_, kAtomsToCache) {
}

SelectionOwner::~SelectionOwner() {
  // If we are the selection owner, we need to release the selection so we
  // don't receive further events. However, we don't call ClearSelectionOwner()
  // because we don't want to do this indiscriminately.
  if (XGetSelectionOwner(x_display_, selection_name_) == x_window_)
    XSetSelectionOwner(x_display_, selection_name_, None, CurrentTime);
}

void SelectionOwner::RetrieveTargets(std::vector<Atom>* targets) {
  for (SelectionFormatMap::const_iterator it = format_map_.begin();
       it != format_map_.end(); ++it) {
    targets->push_back(it->first);
  }
}

void SelectionOwner::TakeOwnershipOfSelection(
    const SelectionFormatMap& data) {
  XSetSelectionOwner(x_display_, selection_name_, x_window_, CurrentTime);

  if (XGetSelectionOwner(x_display_, selection_name_) == x_window_) {
    // The X server agrees that we are the selection owner. Commit our data.
    format_map_ = data;
  }
}

void SelectionOwner::ClearSelectionOwner() {
  XSetSelectionOwner(x_display_, selection_name_, None, CurrentTime);
  format_map_ = SelectionFormatMap();
}

void SelectionOwner::OnSelectionRequest(const XSelectionRequestEvent& event) {
  // Incrementally build our selection. By default this is a refusal, and we'll
  // override the parts indicating success in the different cases.
  XEvent reply;
  reply.xselection.type = SelectionNotify;
  reply.xselection.requestor = event.requestor;
  reply.xselection.selection = event.selection;
  reply.xselection.target = event.target;
  reply.xselection.property = None;  // Indicates failure
  reply.xselection.time = event.time;

  if (event.target == atom_cache_.GetAtom(kMultiple)) {
    // The contents of |event.property| should be a list of
    // <target,property> pairs.
    std::vector<std::pair<Atom,Atom> > conversions;
    if (GetAtomPairArrayProperty(event.requestor,
                                 event.property,
                                 &conversions)) {
      std::vector<Atom> conversion_results;
      for (size_t i = 0; i < conversions.size(); ++i) {
        bool conversion_successful = ProcessTarget(conversions[i].first,
                                                   event.requestor,
                                                   conversions[i].second);
        conversion_results.push_back(conversions[i].first);
        conversion_results.push_back(
            conversion_successful ? conversions[i].second : None);
      }

      // Set the property to indicate which conversions succeeded. This matches
      // what GTK does.
      XChangeProperty(
          x_display_,
          event.requestor,
          event.property,
          atom_cache_.GetAtom(kAtomPair),
          32,
          PropModeReplace,
          reinterpret_cast<const unsigned char*>(&conversion_results.front()),
          conversion_results.size());

      reply.xselection.property = event.property;
    }
  } else {
    if (ProcessTarget(event.target, event.requestor, event.property))
      reply.xselection.property = event.property;
  }

  // Send off the reply.
  XSendEvent(x_display_, event.requestor, False, 0, &reply);
}

void SelectionOwner::OnSelectionClear(const XSelectionClearEvent& event) {
  DLOG(ERROR) << "SelectionClear";

  // TODO(erg): If we receive a SelectionClear event while we're handling data,
  // we need to delay clearing.
}

bool SelectionOwner::ProcessTarget(Atom target,
                                   ::Window requestor,
                                   ::Atom property) {
  Atom multiple_atom = atom_cache_.GetAtom(kMultiple);
  Atom save_targets_atom = atom_cache_.GetAtom(kSaveTargets);
  Atom targets_atom = atom_cache_.GetAtom(kTargets);

  if (target == multiple_atom || target == save_targets_atom)
    return false;

  if (target == targets_atom) {
    // We have been asked for TARGETS. Send an atom array back with the data
    // types we support.
    std::vector<Atom> targets;
    targets.push_back(targets_atom);
    targets.push_back(save_targets_atom);
    targets.push_back(multiple_atom);
    RetrieveTargets(&targets);

    XChangeProperty(x_display_, requestor, property, XA_ATOM, 32,
                    PropModeReplace,
                    reinterpret_cast<unsigned char*>(&targets.front()),
                    targets.size());
    return true;
  } else {
    // Try to find the data type in map.
    SelectionFormatMap::const_iterator it = format_map_.find(target);
    if (it != format_map_.end()) {
      XChangeProperty(x_display_, requestor, property, target, 8,
                      PropModeReplace,
                      const_cast<unsigned char*>(
                          reinterpret_cast<const unsigned char*>(
                              it->second->front())),
                      it->second->size());
      return true;
    }
    // I would put error logging here, but GTK ignores TARGETS and spams us
    // looking for its own internal types.
  }
  return false;
}

}  // namespace ui

