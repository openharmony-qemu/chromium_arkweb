/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "screenshot_capture_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "cef/libcef/browser/frame_host_impl.h"
#include "cef/ohos_cef_ext/include/arkweb_browser_ext.h"
#include "content/browser/renderer_host/render_frame_metadata_provider_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace OHOS::NWeb {

namespace {

struct SurfaceCropContext {
  double clip_x = 0;
  double clip_y = 0;
  double clip_width = 0;
  double clip_height = 0;
  gfx::PointF scroll_offset;
  double page_scale = 1.0;
  SkColor bg_color = SK_ColorWHITE;
  gfx::Rect requested_image_rect;
  gfx::Size requested_image_size;
};

std::string EncodeSkBitmapToBase64Png(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    return "";
  }

  std::optional<std::vector<uint8_t>> png_data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false);
  if (!png_data || png_data->empty()) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] Failed to encode bitmap to PNG";
    return "";
  }

  return base::Base64Encode(*png_data);
}

void ApplySurfaceCropAndEncode(
    ScreenshotCaptureUtil::CaptureCallback callback,
    const SurfaceCropContext& crop_ctx,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] Snapshot image is empty";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_SNAPSHOT_IMAGE_EMPTY, "snapshot image is empty"));
    return;
  }

  const SkBitmap* bitmap = image.ToSkBitmap();
  if (!bitmap || bitmap->drawsNothing()) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] Failed to get SkBitmap from image";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_SKBITMAP_GET_FAILED, "failed to get SkBitmap from image"));
    return;
  }

  const gfx::Rect& r = crop_ctx.requested_image_rect;
  const gfx::Rect source_bounds(0, 0, bitmap->width(), bitmap->height());
  const gfx::Rect valid_source = gfx::IntersectRects(r, source_bounds);

  SkBitmap cropped;
  cropped.allocN32Pixels(r.width(), r.height());
  cropped.eraseColor(crop_ctx.bg_color);

  if (!valid_source.IsEmpty()) {
    SkBitmap valid_pixels;
    SkIRect sk_valid = SkIRect::MakeXYWH(
        valid_source.x(), valid_source.y(),
        valid_source.width(), valid_source.height());
    bitmap->extractSubset(&valid_pixels, sk_valid);
    int dst_x = valid_source.x() - r.x();
    int dst_y = valid_source.y() - r.y();
    cropped.writePixels(valid_pixels.pixmap(), dst_x, dst_y);
  }

  if (!crop_ctx.requested_image_size.IsEmpty() &&
      (cropped.width() != crop_ctx.requested_image_size.width() ||
       cropped.height() != crop_ctx.requested_image_size.height())) {
    cropped = skia::ImageOperations::Resize(
        cropped, skia::ImageOperations::RESIZE_BEST,
        crop_ctx.requested_image_size.width(),
        crop_ctx.requested_image_size.height());
  }

  std::string base64_data = EncodeSkBitmapToBase64Png(cropped);
  if (base64_data.empty()) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] Failed to encode cropped bitmap";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_BITMAP_ENCODE_FAILED, "failed to encode cropped bitmap"));
    return;
  }

  LOG(DEBUG) << "[ScreenshotCaptureUtil] Surface crop capture success, data length=" << base64_data.length();
  std::move(callback).Run(CaptureResult(true, std::move(base64_data)));
}

void OnSnapshotReceivedForSurfaceCrop(
    ScreenshotCaptureUtil::CaptureCallback callback,
    SurfaceCropContext crop_ctx,
    const gfx::Image& image) {
  ApplySurfaceCropAndEncode(std::move(callback), crop_ctx, image);
}

void OnSnapshotReceivedSimple(
    ScreenshotCaptureUtil::CaptureCallback callback,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] Snapshot image is empty";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_SNAPSHOT_IMAGE_EMPTY, "snapshot image is empty"));
    return;
  }

  const SkBitmap* bitmap = image.ToSkBitmap();
  if (!bitmap || bitmap->drawsNothing()) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] Failed to get SkBitmap from image";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_SKBITMAP_GET_FAILED, "failed to get SkBitmap from image"));
    return;
  }

  std::string base64_data = EncodeSkBitmapToBase64Png(*bitmap);
  if (base64_data.empty()) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] Failed to encode bitmap";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_BITMAP_ENCODE_FAILED, "failed to encode bitmap"));
    return;
  }

  LOG(DEBUG) << "[ScreenshotCaptureUtil] Capture success, data length=" << base64_data.length();
  std::move(callback).Run(CaptureResult(true, std::move(base64_data)));
}

}  // namespace

void ScreenshotCaptureUtil::CaptureWithClip(
    CefRefPtr<ArkWebBrowserExt> browser,
    double x, double y,
    double width, double height,
    CaptureCallback callback) {
  if (!browser) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] browser is null";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_BROWSER_NULL, "browser is null"));
    return;
  }

  auto cef_frame = browser->GetMainFrame()
                     ? browser->GetMainFrame()->AsCefFrameHostImpl()
                     : nullptr;
  if (!cef_frame) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] failed to get cef frame";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_CEF_FRAME_NOT_FOUND, "cef frame not found"));
    return;
  }

  content::RenderFrameHost* rfh = cef_frame->GetRenderFrameHost();
  if (!rfh) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] render frame host is null";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_RENDER_FRAME_HOST_NULL, "render frame host is null"));
    return;
  }

  content::RenderWidgetHostImpl* widget_host =
      static_cast<content::RenderWidgetHostImpl*>(rfh->GetRenderWidgetHost());
  if (!widget_host) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] render widget host is null";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_RENDER_WIDGET_HOST_NULL, "render widget host is null"));
    return;
  }

  if (!widget_host->GetView()) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] render widget host view is null";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_RENDER_WIDGET_HOST_VIEW_NULL, "render widget host view is null"));
    return;
  }

  LOG(DEBUG) << "[ScreenshotCaptureUtil] Taking snapshot with clip: x=" << x
             << " y=" << y << " w=" << width << " h=" << height;

  auto* metadata_provider = widget_host->render_frame_metadata_provider();
  if (!metadata_provider) {
    LOG(ERROR) << "[ScreenshotCaptureUtil] No metadata provider, render not ready";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_RENDER_NOT_READY, "render not ready"));
    return;
  }

  const auto& metadata = metadata_provider->LastRenderFrameMetadata();
  gfx::PointF scroll_offset = metadata.root_scroll_offset.value_or(gfx::PointF());
  double page_scale = metadata.page_scale_factor;
  SkColor bg_color = metadata.root_background_color.toSkColor();

  LOG(DEBUG) << "[ScreenshotCaptureUtil] scroll_offset=(" << scroll_offset.x()
             << ", " << scroll_offset.y() << ") page_scale=" << page_scale;

  gfx::Size original_view_size = widget_host->GetView()->GetViewBounds().size();
  double device_scale_factor = widget_host->GetDeviceScaleFactor();

  double scroll_css_x = scroll_offset.x() / device_scale_factor;
  double scroll_css_y = scroll_offset.y() / device_scale_factor;

  double viewport_css_w = original_view_size.width() / page_scale;
  double viewport_css_h = original_view_size.height() / page_scale;

  gfx::RectF viewport_in_css(
      static_cast<float>(scroll_css_x),
      static_cast<float>(scroll_css_y),
      static_cast<float>(viewport_css_w),
      static_cast<float>(viewport_css_h));
  gfx::RectF clip_in_css(x, y, width, height);

  bool use_surface_crop = viewport_in_css.Intersects(clip_in_css);

  if (!use_surface_crop) {
    LOG(DEBUG) << "[ScreenshotCaptureUtil] Clip outside viewport, element not in view";
    std::move(callback).Run(CaptureResult(WebCommandErrorCode::SCREENSHOT_CAPTURE_ELEMENT_NOT_IN_VIEWPORT, "element not in viewport"));
    return;
  }

  LOG(DEBUG) << "[ScreenshotCaptureUtil] Using surface crop path";

  SurfaceCropContext crop_ctx;
  crop_ctx.clip_x = x;
  crop_ctx.clip_y = y;
  crop_ctx.clip_width = width;
  crop_ctx.clip_height = height;
  crop_ctx.scroll_offset = scroll_offset;
  crop_ctx.page_scale = page_scale;
  crop_ctx.bg_color = bg_color;

  double effective_dsf = device_scale_factor;
  double crop_x = (crop_ctx.clip_x * effective_dsf - scroll_offset.x()) * page_scale;
  double crop_y = (crop_ctx.clip_y * effective_dsf - scroll_offset.y()) * page_scale;
  double crop_w = crop_ctx.clip_width * effective_dsf * page_scale;
  double crop_h = crop_ctx.clip_height * effective_dsf * page_scale;

  crop_ctx.requested_image_rect = gfx::Rect(
      base::ClampRound(crop_x), base::ClampRound(crop_y),
      base::ClampRound(crop_w), base::ClampRound(crop_h));

  crop_ctx.requested_image_size = gfx::Size(
      base::ClampRound(crop_w), base::ClampRound(crop_h));

  LOG(DEBUG) << "[ScreenshotCaptureUtil] Crop rect: (" << crop_ctx.requested_image_rect.x()
             << ", " << crop_ctx.requested_image_rect.y() << ") "
             << crop_ctx.requested_image_rect.width() << "x" << crop_ctx.requested_image_rect.height();
  LOG(DEBUG) << "[ScreenshotCaptureUtil] Output size: " << crop_ctx.requested_image_size.width()
             << "x" << crop_ctx.requested_image_size.height();

  widget_host->GetSnapshotFromBrowser(
      base::BindOnce(&OnSnapshotReceivedForSurfaceCrop, std::move(callback), crop_ctx),
      /*from_surface=*/true);
}

}  // namespace OHOS::NWeb
