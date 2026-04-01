// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/ohos/ohos_media_player_renderer_client_factory.h"

#include "content/renderer/media/ohos/ohos_media_player_renderer_client.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"

namespace content {

OHOSMediaPlayerRendererClientFactory::OHOSMediaPlayerRendererClientFactory(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory,
    const GetNativeTextureWrapperCB& get_native_texture_wrapper_cb)
    : get_native_texture_wrapper_cb_(get_native_texture_wrapper_cb),
      compositor_task_runner_(compositor_task_runner),
      mojo_renderer_factory_(std::move(mojo_renderer_factory)) {}

OHOSMediaPlayerRendererClientFactory::~OHOSMediaPlayerRendererClientFactory() {}


// Note: GetRequiredMediaResourceType() method removed in Chromium 141
// MediaResource::Type enumeration no longer exists in the new architecture


// TODO(arkweb): Temporary fix to resolve compilation errors.
std::unique_ptr<media::Renderer>
OHOSMediaPlayerRendererClientFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::AudioRendererSink* audio_renderer_sink,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB request_surface_cb,
    const gfx::ColorSpace& target_color_space) {
  return nullptr;
}

std::unique_ptr<media::Renderer>
OHOSMediaPlayerRendererClientFactory::CreateOHOSMediaPlayerRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::AudioRendererSink* audio_renderer_sink,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB request_surface_cb,
    const gfx::ColorSpace& target_color_space,
    int player_id,
    const media::MediaPlayerUrlParams& params) {
  mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
      client_extension_remote;
  auto client_extension_receiver =
      client_extension_remote.InitWithNewPipeAndPassReceiver();
  std::unique_ptr<media::MojoRenderer> mojo_renderer =
      mojo_renderer_factory_->CreateMediaPlayerRenderer(
          std::move(client_extension_remote), media_task_runner,
          video_renderer_sink, player_id, params);

  media::ScopedNativeTextureWrapper native_texture_wrapper =
      get_native_texture_wrapper_cb_.Run();

  return std::make_unique<OHOSMediaPlayerRendererClient>(
      std::move(client_extension_receiver),
      media_task_runner, compositor_task_runner_,
      std::move(mojo_renderer),
      std::move(native_texture_wrapper),
      video_renderer_sink);
}

}  // namespace content
