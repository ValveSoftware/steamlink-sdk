// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/engine_image_serialization_processor.h"

#include <stddef.h>
#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/proto/blob_cache.pb.h"
#include "blimp/engine/renderer/blimp_engine_picture_cache.h"
#include "blimp/engine/renderer/blob_channel_sender_proxy.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/libwebp/webp/encode.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"

namespace blimp {
namespace engine {
namespace {

SkData* BlobCacheImageMetadataProtoAsSkData(
    const BlobCacheImageMetadata& proto) {
  int signed_size = proto.ByteSize();
  size_t unsigned_size = base::checked_cast<size_t>(signed_size);
  std::vector<uint8_t> serialized(unsigned_size);
  proto.SerializeWithCachedSizesToArray(serialized.data());
  return SkData::NewWithCopy(serialized.data(), serialized.size());
}

// For each pixel, un-premultiplies the alpha-channel for each of the RGB
// channels. As an example, for a channel value that before multiplication was
// 255, and after applying an alpha of 128, the premultiplied pixel would be
// 128. The un-premultiply step uses the alpha-channel to get back to 255. The
// alpha channel is kept unchanged.
void UnPremultiply(const unsigned char* in_pixels,
                   unsigned char* out_pixels,
                   size_t pixel_count) {
  const SkUnPreMultiply::Scale* table = SkUnPreMultiply::GetScaleTable();
  for (; pixel_count-- > 0; in_pixels += 4) {
    unsigned char alpha = in_pixels[3];
    if (alpha == 255) {  // Full opacity, just blindly copy.
      *out_pixels++ = in_pixels[0];
      *out_pixels++ = in_pixels[1];
      *out_pixels++ = in_pixels[2];
      *out_pixels++ = alpha;
    } else {
      SkUnPreMultiply::Scale scale = table[alpha];
      *out_pixels++ = SkUnPreMultiply::ApplyScale(scale, in_pixels[0]);
      *out_pixels++ = SkUnPreMultiply::ApplyScale(scale, in_pixels[1]);
      *out_pixels++ = SkUnPreMultiply::ApplyScale(scale, in_pixels[2]);
      *out_pixels++ = alpha;
    }
  }
  }

  bool PlatformPictureImport(const unsigned char* pixels,
                             WebPPicture* picture,
                             SkAlphaType alphaType) {
    // Each pixel uses 4 bytes (RGBA) which affects the stride per row.
    int row_stride = picture->width * 4;
    if (alphaType == kPremul_SkAlphaType) {
      // Need to unpremultiply each pixel, each pixel using 4 bytes (RGBA).
      size_t pixel_count = picture->height * picture->width;
      std::vector<unsigned char> unpremul_pixels(pixel_count * 4);
      UnPremultiply(pixels, unpremul_pixels.data(), pixel_count);
      if (SK_B32_SHIFT)  // Android
        return WebPPictureImportRGBA(picture, unpremul_pixels.data(),
                                     row_stride);
      return WebPPictureImportBGRA(picture, unpremul_pixels.data(), row_stride);
    }

    if (SK_B32_SHIFT)  // Android
      return WebPPictureImportRGBA(picture, pixels, row_stride);
    return WebPPictureImportBGRA(picture, pixels, row_stride);
  }

  }  // namespace

  EngineImageSerializationProcessor::EngineImageSerializationProcessor(
      std::unique_ptr<BlobChannelSenderProxy> blob_channel)
      : blob_channel_(std::move(blob_channel)) {
    DCHECK(blob_channel_);
    WebPMemoryWriterInit(&writer_state_);
}

EngineImageSerializationProcessor::~EngineImageSerializationProcessor() {
  WebPMemoryWriterClear(&writer_state_);
}

std::unique_ptr<cc::EnginePictureCache>
EngineImageSerializationProcessor::CreateEnginePictureCache() {
  return base::WrapUnique(new BlimpEnginePictureCache(this));
}

std::unique_ptr<cc::ClientPictureCache>
EngineImageSerializationProcessor::CreateClientPictureCache() {
  NOTREACHED();
  return nullptr;
}

bool EngineImageSerializationProcessor::onUseEncodedData(const void* data,
                                                         size_t len) {
  TRACE_EVENT1("blimp", "WebPImageEncoded::UsingEncodedData",
               "OriginalImageSize", len);
  // Encode all images regardless of their format, including WebP images.
  return false;
}

SkData* EngineImageSerializationProcessor::onEncode(const SkPixmap& pixmap) {
  TRACE_EVENT0("blimp", "WebImageEncoder::onEncode");

  // Ensure width and height are valid dimensions.
  DCHECK_GT(pixmap.width(), 0);
  DCHECK_LE(pixmap.width(), WEBP_MAX_DIMENSION);
  DCHECK_GT(pixmap.height(), 0);
  DCHECK_LE(pixmap.height(), WEBP_MAX_DIMENSION);

  // Send the image to the client via the BlobChannel if we know that the
  // client doesn't have it.
  const BlobId blob_id = CalculateBlobId(pixmap.addr(), pixmap.getSafeSize());
  if (!blob_channel_->IsInEngineCache(blob_id)) {
    blob_channel_->PutBlob(blob_id, EncodeImageAsBlob(pixmap));
    DCHECK(blob_channel_->IsInEngineCache(blob_id));
  }

  // Push the blob to the client, if the client doesn't already have it.
  if (!blob_channel_->IsInClientCache(blob_id)) {
    blob_channel_->DeliverBlob(blob_id);
  }

  // Construct a proto with the image metadata.
  BlobCacheImageMetadata proto;
  proto.set_id(blob_id);
  proto.set_width(pixmap.width());
  proto.set_height(pixmap.height());

  SkData* sk_data = BlobCacheImageMetadataProtoAsSkData(proto);
  DVLOG(3) << "Returning image ID " << BlobIdToString(blob_id);
  return sk_data;
}

scoped_refptr<BlobData> EngineImageSerializationProcessor::EncodeImageAsBlob(
    const SkPixmap& pixmap) {
  DVLOG(2) << "Encoding image color_type=" << pixmap.colorType()
           << ", alpha_type=" << pixmap.alphaType() << " " << pixmap.width()
           << "x" << pixmap.height();

  WebPConfig config;
  CHECK(WebPConfigInit(&config));

  WebPPicture picture;
  CHECK(WebPPictureInit(&picture));
  picture.width = pixmap.width();
  picture.height = pixmap.height();

  // Import picture from raw pixels.
  auto pixel_chars = static_cast<const unsigned char*>(pixmap.addr());
  CHECK(PlatformPictureImport(pixel_chars, &picture, pixmap.alphaType()));

  // Set up the writer parameters.
  writer_state_.size = 0;
  picture.custom_ptr = &writer_state_;
  picture.writer = &WebPMemoryWrite;

  // Setup the configuration for the output WebP picture. This is currently
  // the same as the default configuration for WebP, but since any change in
  // the WebP defaults would invalidate all caches they are hard coded.
  config.lossless = 0;
  config.quality = 75.0;  // between 0 (smallest file) and 100 (biggest).

  // TODO(nyquist): Move image encoding to a different thread when
  // asynchronous loading of images is possible. The encode work currently
  // blocks the render thread so we are dropping the method down to 0.
  // crbug.com/603643.
  config.method = 0;  // quality/speed trade-off (0=fast, 6=slower-better).

  TRACE_EVENT_BEGIN0("blimp", "WebPImageEncoder::onEncode WebPEncode");
  // Encode the picture using the given configuration.
  bool success = WebPEncode(&config, &picture);
  TRACE_EVENT_END1("blimp", "WebPImageEncoder::onEncode WebPEncode",
                   "EncodedImageSize", static_cast<int>(writer_state_.size));

  // Release the memory allocated by WebPPictureImport*(). This does not
  // free the memory used by the picture object itself.
  WebPPictureFree(&picture);

  if (!success) {
    LOG(FATAL) << "WebPPictureEncode() failed.";
    return nullptr;
  }

  scoped_refptr<BlobData> blob_data(new BlobData);
  blob_data->data.assign(reinterpret_cast<const char*>(writer_state_.mem),
                         writer_state_.size);
  return blob_data;
}

}  // namespace engine
}  // namespace blimp
