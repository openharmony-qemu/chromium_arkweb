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

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_FOR_INCLUDE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_FOR_INCLUDE_H_

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl_ext.h"
#include "content/public/browser/web_contents.h"

namespace content {

bool FlingScheduler::OnNestedScroll(float& delta_x, float& delta_y) {
  if (!host_) {
    return false;
  }

  WebContentsImpl* web_contents_impl =
      WebContentsImpl::FromRenderWidgetHostImpl(host_);

  if (!web_contents_impl) {
    return false;
  }

  RenderFrameHostImpl* main_frame = web_contents_impl->GetPrimaryMainFrame();

  bool ret = false;
  if (main_frame) {
    ret = main_frame->OnNestedScroll(delta_x, delta_y);
  }
  return ret;
}

bool FlingScheduler::OnNestedFling(float& velocity_x, float& velocity_y) {
  if (!host_) {
    return false;
  }

  WebContentsImpl* web_contents_impl =
      WebContentsImpl::FromRenderWidgetHostImpl(host_);

  if (!web_contents_impl) {
    return false;
  }

  RenderFrameHostImpl* main_frame = web_contents_impl->GetPrimaryMainFrame();

  bool ret = false;
  if (main_frame) {
    ret = main_frame->OnNestedFling(velocity_x, velocity_y);
  }
  return ret;
}
}  // namespace content

#endif
