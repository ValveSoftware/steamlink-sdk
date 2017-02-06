// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#import <AppKit/AppKit.h>
#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/gfx/image/image.h"

namespace ui {

namespace {

base::FilePath GetResourcesPakFilePath(NSString* name, NSString* mac_locale) {
  NSString *resource_path;
  // Some of the helper processes need to be able to fetch resources
  // (chrome_main.cc: SubprocessNeedsResourceBundle()). Fetch the same locale
  // as the already-running browser instead of using what NSBundle might pick
  // based on values at helper launch time.
  if ([mac_locale length]) {
    resource_path = [base::mac::FrameworkBundle() pathForResource:name
                                                           ofType:@"pak"
                                                      inDirectory:@""
                                                  forLocalization:mac_locale];
  } else {
    resource_path = [base::mac::FrameworkBundle() pathForResource:name
                                                           ofType:@"pak"];
  }

  if (!resource_path) {
    // Return just the name of the pack file.
    return base::FilePath(base::SysNSStringToUTF8(name) + ".pak");
  }

  return base::FilePath([resource_path fileSystemRepresentation]);
}

}  // namespace

void ResourceBundle::LoadCommonResources() {
  AddDataPackFromPath(GetResourcesPakFilePath(@"chrome_100_percent",
                        nil), SCALE_FACTOR_100P);

  // On Mac we load 1x and 2x resources and we let the UI framework decide
  // which one to use.
  if (IsScaleFactorSupported(SCALE_FACTOR_200P)) {
    AddDataPackFromPath(GetResourcesPakFilePath(@"chrome_200_percent", nil),
                        SCALE_FACTOR_200P);
  }
}

void ResourceBundle::LoadMaterialDesignResources() {
  if (!MaterialDesignController::IsModeMaterial()) {
    return;
  }

  // The Material Design data packs contain some of the same asset IDs as in
  // the non-Material Design data packs. Set aside the current packs and add the
  // Material Design packs so that they are searched first when a request for an
  // asset is made. The Material Design packs cannot be loaded in
  // LoadCommonResources() because the MaterialDesignController is not always
  // initialized at the time it is called.
  // TODO(shrike) - remove this method and restore loading of Material Design
  // packs to LoadCommonResources() when the MaterialDesignController goes away.
  std::vector<std::unique_ptr<ResourceHandle>> tmp_packs;
  for (auto it = data_packs_.begin(); it != data_packs_.end(); ++it) {
    std::unique_ptr<ResourceHandle> next_pack(*it);
    tmp_packs.push_back(std::move(next_pack));
  }
  data_packs_.weak_clear();

  AddMaterialDesignDataPackFromPath(
      GetResourcesPakFilePath(@"chrome_material_100_percent", nil),
      SCALE_FACTOR_100P);

  AddOptionalMaterialDesignDataPackFromPath(
      GetResourcesPakFilePath(@"chrome_material_200_percent", nil),
      SCALE_FACTOR_200P);

  // Add back the non-Material Design packs so that they serve as a fallback.
  for (auto it = tmp_packs.begin(); it != tmp_packs.end(); ++it) {
    data_packs_.push_back(std::move(*it));
  }
}

base::FilePath ResourceBundle::GetLocaleFilePath(const std::string& app_locale,
                                                 bool test_file_exists) {
  NSString* mac_locale = base::SysUTF8ToNSString(app_locale);

  // Mac OS X uses "_" instead of "-", so swap to get a Mac-style value.
  mac_locale = [mac_locale stringByReplacingOccurrencesOfString:@"-"
                                                     withString:@"_"];

  // On disk, the "en_US" resources are just "en" (http://crbug.com/25578).
  if ([mac_locale isEqual:@"en_US"])
    mac_locale = @"en";

  base::FilePath locale_file_path =
      GetResourcesPakFilePath(@"locale", mac_locale);

  if (delegate_) {
    locale_file_path =
        delegate_->GetPathForLocalePack(locale_file_path, app_locale);
  }

  // Don't try to load empty values or values that are not absolute paths.
  if (locale_file_path.empty() || !locale_file_path.IsAbsolute())
    return base::FilePath();

  if (test_file_exists && !base::PathExists(locale_file_path))
    return base::FilePath();

  return locale_file_path;
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  // Check to see if the image is already in the cache.
  {
    base::AutoLock lock(*images_and_fonts_lock_);
    if (images_.count(resource_id)) {
      if (!images_[resource_id].HasRepresentation(gfx::Image::kImageRepCocoa)) {
        DLOG(WARNING) << "ResourceBundle::GetNativeImageNamed() is returning a"
          << " cached gfx::Image that isn't backed by an NSImage. The image"
          << " will be converted, rather than going through the NSImage loader."
          << " resource_id = " << resource_id;
      }
      return images_[resource_id];
    }
  }

  gfx::Image image;
  if (delegate_)
    image = delegate_->GetNativeImageNamed(resource_id);

  if (image.IsEmpty()) {
    base::scoped_nsobject<NSImage> ns_image;
    // Material Design packs are meant to override the standard packs, so
    // search for the image in those packs first.
    for (size_t i = 0; i < data_packs_.size(); ++i) {
      if (!data_packs_[i]->HasOnlyMaterialDesignAssets())
        continue;
      scoped_refptr<base::RefCountedStaticMemory> data(
          data_packs_[i]->GetStaticMemory(resource_id));
      if (!data.get())
        continue;

      base::scoped_nsobject<NSData> ns_data(
          [[NSData alloc] initWithBytes:data->front() length:data->size()]);
      if (!ns_image.get()) {
        ns_image.reset([[NSImage alloc] initWithData:ns_data]);
      } else {
        NSImageRep* image_rep = [NSBitmapImageRep imageRepWithData:ns_data];
        if (image_rep)
          [ns_image addRepresentation:image_rep];
      }
    }

    if (ns_image.get()) {
      image = gfx::Image(ns_image.release());
    }
  }

  if (image.IsEmpty()) {
    base::scoped_nsobject<NSImage> ns_image;
    for (size_t i = 0; i < data_packs_.size(); ++i) {
      if (data_packs_[i]->HasOnlyMaterialDesignAssets())
        continue;
      scoped_refptr<base::RefCountedStaticMemory> data(
          data_packs_[i]->GetStaticMemory(resource_id));
      if (!data.get())
        continue;

      base::scoped_nsobject<NSData> ns_data(
          [[NSData alloc] initWithBytes:data->front() length:data->size()]);
      if (!ns_image.get()) {
        ns_image.reset([[NSImage alloc] initWithData:ns_data]);
      } else {
        NSImageRep* image_rep = [NSBitmapImageRep imageRepWithData:ns_data];
        if (image_rep)
          [ns_image addRepresentation:image_rep];
      }
    }

    if (!ns_image.get()) {
      LOG(WARNING) << "Unable to load image with id " << resource_id;
      NOTREACHED();  // Want to assert in debug mode.
      return GetEmptyImage();
    }

    image = gfx::Image(ns_image.release());
  }

  base::AutoLock lock(*images_and_fonts_lock_);

  // Another thread raced the load and has already cached the image.
  if (images_.count(resource_id))
    return images_[resource_id];

  images_[resource_id] = image;
  return images_[resource_id];
}

}  // namespace ui
