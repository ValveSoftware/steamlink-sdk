// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_icon_image.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "ui/base/layout.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"

// The ImageSkia provided by extensions::IconImage contains ImageSkiaReps that
// are computed and updated using the following algorithm (if no default icon
// was supplied, transparent icon is considered the default):
// - |LoadImageForScaleFactors()| searches the extension for an icon of an
//   appropriate size. If the extension doesn't have a icon resource needed for
//   the image representation, the default icon's representation for the
//   requested scale factor is returned by ImageSkiaSource.
// - If the extension has the resource, IconImage tries to load it using
//   ImageLoader.
// - |ImageLoader| is asynchronous.
//  - ImageSkiaSource will initially return transparent image resource of the
//    desired size.
//  - The image will be updated with an appropriate image representation when
//    the |ImageLoader| finishes. The image representation is chosen the same
//    way as in the synchronous case. The observer is notified of the image
//    change, unless the added image representation is transparent (in which
//    case the image had already contained the appropriate image
//    representation).

namespace {

extensions::ExtensionResource GetExtensionIconResource(
    const extensions::Extension& extension,
    const ExtensionIconSet& icons,
    int size,
    ExtensionIconSet::MatchType match_type) {
  const std::string& path = icons.Get(size, match_type);
  return path.empty() ? extensions::ExtensionResource()
                      : extension.GetResource(path);
}

class BlankImageSource : public gfx::CanvasImageSource {
 public:
  explicit BlankImageSource(const gfx::Size& size_in_dip)
      : CanvasImageSource(size_in_dip, /*is_opaque =*/ false) {
  }
  ~BlankImageSource() override {}

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawColor(SkColorSetARGB(0, 0, 0, 0));
  }

  DISALLOW_COPY_AND_ASSIGN(BlankImageSource);
};

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// IconImage::Source

class IconImage::Source : public gfx::ImageSkiaSource {
 public:
  Source(IconImage* host, const gfx::Size& size_in_dip);
  ~Source() override;

  void ResetHost();

 private:
  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  // Used to load images, possibly asynchronously. NULLed out when the IconImage
  // is destroyed.
  IconImage* host_;

  // Image whose representations will be used until |host_| loads the real
  // representations for the image.
  gfx::ImageSkia blank_image_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

IconImage::Source::Source(IconImage* host, const gfx::Size& size_in_dip)
    : host_(host),
      blank_image_(new BlankImageSource(size_in_dip), size_in_dip) {
}

IconImage::Source::~Source() {
}

void IconImage::Source::ResetHost() {
  host_ = NULL;
}

gfx::ImageSkiaRep IconImage::Source::GetImageForScale(float scale) {
  gfx::ImageSkiaRep representation;
  if (host_) {
    representation =
        host_->LoadImageForScaleFactor(ui::GetSupportedScaleFactor(scale));
  }

  if (!representation.is_null())
    return representation;

  return blank_image_.GetRepresentation(scale);
}

////////////////////////////////////////////////////////////////////////////////
// IconImage

IconImage::IconImage(
    content::BrowserContext* context,
    const Extension* extension,
    const ExtensionIconSet& icon_set,
    int resource_size_in_dip,
    const gfx::ImageSkia& default_icon,
    Observer* observer)
    : browser_context_(context),
      extension_(extension),
      icon_set_(icon_set),
      resource_size_in_dip_(resource_size_in_dip),
      source_(NULL),
      default_icon_(gfx::ImageSkiaOperations::CreateResizedImage(
          default_icon,
          skia::ImageOperations::RESIZE_BEST,
          gfx::Size(resource_size_in_dip, resource_size_in_dip))),
      weak_ptr_factory_(this) {
  if (observer)
    AddObserver(observer);
  gfx::Size resource_size(resource_size_in_dip, resource_size_in_dip);
  source_ = new Source(this, resource_size);
  image_skia_ = gfx::ImageSkia(source_, resource_size);
  image_ = gfx::Image(image_skia_);

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_REMOVED,
                 content::NotificationService::AllSources());
}

void IconImage::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IconImage::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

IconImage::~IconImage() {
  FOR_EACH_OBSERVER(Observer, observers_, OnExtensionIconImageDestroyed(this));
  source_->ResetHost();
}

gfx::ImageSkiaRep IconImage::LoadImageForScaleFactor(
    ui::ScaleFactor scale_factor) {
  // Do nothing if extension is unloaded.
  if (!extension_)
    return gfx::ImageSkiaRep();

  const float scale = ui::GetScaleForScaleFactor(scale_factor);
  const int resource_size_in_pixel =
      static_cast<int>(resource_size_in_dip_ * scale);

  extensions::ExtensionResource resource;

  // Find extension resource for non bundled component extensions.
  resource =
      GetExtensionIconResource(*extension_, icon_set_, resource_size_in_pixel,
                               ExtensionIconSet::MATCH_BIGGER);

  // If resource is not found by now, try matching smaller one.
  if (resource.empty()) {
    resource =
        GetExtensionIconResource(*extension_, icon_set_, resource_size_in_pixel,
                                 ExtensionIconSet::MATCH_SMALLER);
  }

  // If there is no resource found, return default icon.
  if (resource.empty())
    return default_icon_.GetRepresentation(scale);

  std::vector<ImageLoader::ImageRepresentation> info_list;
  info_list.push_back(ImageLoader::ImageRepresentation(
      resource, ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
      gfx::ScaleToFlooredSize(
          gfx::Size(resource_size_in_dip_, resource_size_in_dip_), scale),
      scale_factor));

  extensions::ImageLoader* loader =
      extensions::ImageLoader::Get(browser_context_);
  loader->LoadImagesAsync(extension_.get(), info_list,
                          base::Bind(&IconImage::OnImageLoaded,
                                     weak_ptr_factory_.GetWeakPtr(), scale));

  return gfx::ImageSkiaRep();
}

void IconImage::OnImageLoaded(float scale, const gfx::Image& image_in) {
  const gfx::ImageSkia* image =
      image_in.IsEmpty() ? &default_icon_ : image_in.ToImageSkia();

  // Maybe default icon was not set.
  if (image->isNull())
    return;

  gfx::ImageSkiaRep rep = image->GetRepresentation(scale);
  DCHECK(!rep.is_null());
  DCHECK_EQ(scale, rep.scale());

  // Remove fractional scale image representations as they may have become
  // stale here. These images are generated by ImageSkia on request from
  // supported scales like 1x, 2x, etc.
  // TODO(oshima)
  // A better approach might be to set the |image_| member using the ImageSkia
  // copy from the image passed in and set the |image_skia_| member using
  // image_.ToImageSkia(). However that does not work correctly as the
  // ImageSkia from the image does not contain a source which breaks requests
  // for scaled images.
  std::vector<gfx::ImageSkiaRep> reps = image_skia_.image_reps();
  for (const auto& image_rep : reps) {
    if (!ui::IsSupportedScale(image_rep.scale()))
      image_skia_.RemoveRepresentation(image_rep.scale());
  }

  image_skia_.RemoveRepresentation(scale);
  image_skia_.AddRepresentation(rep);

  // Update the image to use the updated image skia.
  // It's a shame we have to do this because it means that all the other
  // representations stored on |image_| will be deleted, but unfortunately
  // there's no way to combine the storage of two images.
  image_ = gfx::Image(image_skia_);

  FOR_EACH_OBSERVER(Observer, observers_, OnExtensionIconImageChanged(this));
}

void IconImage::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(type, extensions::NOTIFICATION_EXTENSION_REMOVED);

  const Extension* extension = content::Details<const Extension>(details).ptr();

  if (extension_.get() == extension)
    extension_ = nullptr;
}

}  // namespace extensions
