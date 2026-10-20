/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

// Keep this definition gate aligned with the declaration in
// mojo_renderer_factory.h: the ArkWeb overload is declared under
// `#elif BUILDFLAG(ARKWEB_MEDIA_HLS)` (the Android arm declares the upstream
// 2-argument overload instead, which this 5-argument definition never
// matches; this tree is never is_android).
#if BUILDFLAG(ARKWEB_MEDIA_HLS)
std::unique_ptr<MojoRenderer> MojoRendererFactory::CreateMediaPlayerRenderer(
    mojo::PendingRemote<mojom::MediaPlayerRendererClientExtension>
        client_extension_remote,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink,
    int player_id,
    const media::MediaPlayerUrlParams& params) {
  DCHECK(interface_factory_);
  mojo::PendingRemote<mojom::Renderer> renderer_remote;
  interface_factory_->CreateMediaPlayerRenderer(
      std::move(client_extension_remote),
      renderer_remote.InitWithNewPipeAndPassReceiver(),
      player_id,
      params);

  return std::make_unique<MojoRenderer>(media_task_runner, nullptr,
                                        video_renderer_sink,
                                        std::move(renderer_remote));
}
#endif  // BUILDFLAG(ARKWEB_MEDIA_HLS)

#if BUILDFLAG(ARKWEB_CUSTOM_VIDEO_PLAYER)
std::unique_ptr<MojoRenderer> MojoRendererFactory::CreateCustomMediaPlayerRenderer(
    mojo::PendingRemote<mojom::CustomMediaPlayerRendererClientExtension>
        client_extension_remote,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink,
    int player_id,
    const media::MediaPlayerUrlParams& params) {
  DCHECK(interface_factory_);
  mojo::PendingRemote<mojom::Renderer> renderer_remote;
  interface_factory_->CreateCustomMediaPlayerRenderer(
      std::move(client_extension_remote),
      renderer_remote.InitWithNewPipeAndPassReceiver(),
      player_id,
      params);

  return std::make_unique<MojoRenderer>(media_task_runner, nullptr,
                                        video_renderer_sink,
                                        std::move(renderer_remote));
}
#endif // ARKWEB_CUSTOM_VIDEO_PLAYER