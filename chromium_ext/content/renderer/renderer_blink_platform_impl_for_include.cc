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


namespace content {

#if BUILDFLAG(ARKWEB_HEIF_SUPPORT)
bool RendererBlinkPlatformImpl::DecodeHeifOnGpuSync(
    base::span<const uint8_t> heif_data,
    std::vector<uint8_t>* out_pixels,
    gfx::Size* out_size,
    std::string* out_error) {
  LOG(INFO) << "[HeifSupport] RendererBlinkPlatformImpl::DecodeHeifOnGpuSync";
  auto* render_thread = RenderThreadImpl::current();
  if (render_thread) {
    // Call directly on the main thread
    return DecodeHeifOnGpuSyncInternal(heif_data, out_pixels, out_size, out_error);
  }

  if (!main_thread_scheduler_) {
    *out_error = "Main thread scheduler not available";
    LOG(ERROR) << "[HeifSupport] " << *out_error;
    return false;
  }
  // Not on the main thread, needs to be posted to the main thread and wait.
  LOG(INFO) << "[HeifSupport] Not on main thread, posting to main thread";
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner = 
      main_thread_scheduler_->DeprecatedDefaultTaskRunner();
  
  if (!main_task_runner) {
    *out_error = "Main thread task runner not available";
    LOG(ERROR) << "[HeifSupport] " << *out_error;
    return false;
  }
  // Copy the data (because span might become invalid in other threads)
  std::vector<uint8_t> heif_data_copy(heif_data.begin(), heif_data.end());
  // Used to wait for the main thread to complete
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  
  bool success = false;
  // Post to the main thread for execution
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RendererBlinkPlatformImpl* self,
             std::vector<uint8_t> data,
             std::vector<uint8_t>* pixels,
             gfx::Size* size,
             std::string* error,
             bool* result,
             base::WaitableEvent* event) {
            LOG(INFO) << "[HeifSupport] Running on main thread now";
            *result = self->DecodeHeifOnGpuSyncInternal(
                base::span<const uint8_t>(data.data(), data.size()),
                pixels, size, error);
            event->Signal();
          },
          // SAFETY: Using Unretained is safe here because:
          // 1. This is a synchronous call - Wait() blocks below
          // 2. `this` is guaranteed to outlive the callback execution
          // 3. The calling thread holds a reference to `this`
          base::Unretained(this),
          std::move(heif_data_copy),
          out_pixels,
          out_size,
          out_error,
          &success,
          &done_event));
  
  // Waiting for the main thread to complete
  LOG(INFO) << "[HeifSupport] Waiting for main thread...";
  done_event.Wait();
  LOG(INFO) << "[HeifSupport] Main thread completed, success: " << success;
  
  return success;
}

bool RendererBlinkPlatformImpl::DecodeHeifOnGpuSyncInternal(
    base::span<const uint8_t> heif_data,
    std::vector<uint8_t>* out_pixels,
    gfx::Size* out_size,
    std::string* out_error) {
  
  LOG(INFO) << "[HeifSupport] DecodeHeifOnGpuSyncInternal called";
  
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread) {
    *out_error = "RenderThread not available";
    return false;
  }

  // Synchronously obtain GPU Channel
  scoped_refptr<gpu::GpuChannelHost> gpu_channel = 
      render_thread->EstablishGpuChannelSync();
  
  if (!gpu_channel) {
    *out_error = "GPU channel not available";
    return false;
  }

  // Convert data
  std::vector<uint8_t> buffer_data(heif_data.begin(), heif_data.end());
  mojo_base::BigBuffer buffer(buffer_data);

  LOG(INFO) << "[HeifSupport] Calling GPU DecodeHeif, data size: " << buffer_data.size();
  // Synchronously call GPU decoding
  return gpu_channel->GetGpuChannel().DecodeHeif(
      std::move(buffer),
      out_pixels,
      out_size,
      out_error);
}
#endif

}