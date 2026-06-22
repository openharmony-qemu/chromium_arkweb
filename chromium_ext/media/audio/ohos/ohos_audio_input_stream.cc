// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on audio_input_stream_fuchsia.cc originally written by
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. 

#include "ohos_audio_input_stream.h"

#include "base/logging.h"
#include "media/audio/audio_device_description.h"
#include "ohos_audio_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/public/browser/web_contents.h"

namespace media {
namespace {
// ID of the virtual device used to share system audio
constexpr std::string_view kScreenSystemAudioDeviceId = "screen:systemAudio:-2:0";
constexpr double kVolumeEpsilon = 1e-6;
constexpr int kInvalidNWebId = -1;
}

class OHOSAudioInputStream::CaptureCallbackAdapter
    : public AudioCapturerSource::CaptureCallback {
 public:
    CaptureCallbackAdapter(AudioInputCallback* callback, base::WeakPtr<OHOSAudioInputStream> audio_input_stream)
        : callback_(callback), audio_input_stream_(audio_input_stream) {}

  void Capture(const AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               const AudioGlitchInfo& glitch_info,
               double volume) override {
    if (!paused_) {
      callback_->OnData(audio_source, audio_capture_time, volume, glitch_info);
    }
  }

  void OnCaptureError(AudioCapturerSource::ErrorCode code,
                      const std::string& message) override {
    callback_->OnError();
  }

  void OnCaptureMuted(bool is_muted) override {}
  void SetCapturePaused(bool paused) { paused_ = paused; }
  void OnCapturePaused(bool is_paused) override {
    LOG(INFO) << "CaptureCallbackAdapter::OnCapturePaused, is_paused: " << is_paused;
    if (audio_input_stream_) {
      audio_input_stream_->OnCapturerPaused(is_paused);
    }
  }
 private:
  bool paused_ = false;
  raw_ptr<AudioInputCallback> callback_ = nullptr;
  base::WeakPtr<OHOSAudioInputStream> audio_input_stream_;
};

OHOSAudioInputStream::OHOSAudioInputStream(OHOSAudioManager* manager,
                                           const AudioParameters& parameters)
    : manager_(manager), parameters_(parameters) {}

OHOSAudioInputStream::~OHOSAudioInputStream() = default;

AudioInputStream::OpenOutcome OHOSAudioInputStream::Open() {
  LOG(INFO) << "OHOSAudioInputStream::Open";
  return OpenOutcome::kSuccess;
}

void OHOSAudioInputStream::Start(AudioInputCallback* callback) {
  std::string device_id = manager_->GetSelectAudioDeviceId();
  LOG(INFO) << "OHOSAudioInputStream::Start, format: "
            << (int32_t)parameters_.format()
            << " , deviceid: "
            << device_id;

  nweb_id_ = GetNWebId(parameters_);
  manager_->AddInputStreamToNWebMap(nweb_id_, this);
  if (device_id == kScreenSystemAudioDeviceId) {
    base_capturer_source_ =
      base::MakeRefCounted<BaseAudioCapturerSource>(manager_->GetTaskRunner(), GetNWebId(parameters_));
    base_callback_adapter_ = std::make_unique<CaptureCallbackAdapter>(callback, weak_factory_.GetWeakPtr());
    base_capturer_source_->Initialize(parameters_, base_callback_adapter_.get());
    base_capturer_source_->Start();
  } else {
    capturer_source_ =
      base::MakeRefCounted<OHOSAudioCapturerSource>(manager_->GetTaskRunner());
    callback_adapter_ = std::make_unique<CaptureCallbackAdapter>(callback, weak_factory_.GetWeakPtr());
    capturer_source_->Initialize(parameters_, callback_adapter_.get());
    capturer_source_->Start();
    OnMicrophoneCaptureStateChanged(AudioCaptureState::ACTIVE);
  }
}

// LCOV_EXCL_START
void OHOSAudioInputStream::Stop() {
  LOG(INFO) << "OHOSAudioInputStream::Stop";
  if (capturer_source_) {
    capturer_source_->Stop();
    capturer_source_ = nullptr;
  }
  if (base_capturer_source_) {
    base_capturer_source_->Stop();
    base_capturer_source_ = nullptr;
  }
}

void OHOSAudioInputStream::Close() {
  LOG(INFO) << "OHOSAudioInputStream::Close";
  Stop();
  manager_->RemoveInputStreamFromNWebMap(nweb_id_, this);
  OnMicrophoneCaptureStateChanged(AudioCaptureState::NONE);
  manager_->ReleaseInputStream(this);
}
// LCOV_EXCL_STOP

void OHOSAudioInputStream::Pause() {
  LOG(INFO) << "OHOSAudioInputStream::Pause";
  if (capturer_source_) {
    capturer_source_->Pause();
  }
}

void OHOSAudioInputStream::Resume() {
  LOG(INFO) << "OHOSAudioInputStream::Resume";
  if (capturer_source_) {
    capturer_source_->Resume();
  }
}

double OHOSAudioInputStream::GetMaxVolume() {
  return 1.0;
}

void OHOSAudioInputStream::SetVolume(double volume) {
  LOG(DEBUG) << "OHOSAudioInputStream::SetVolume volume is:" << volume;
  if (capturer_source_) {
    capturer_source_->SetVolume(volume);
  }
  if (base_capturer_source_) {
    base_capturer_source_->SetVolume(volume);
  }
  volume_ = volume;
}

double OHOSAudioInputStream::GetVolume() {
  return volume_;
}

bool OHOSAudioInputStream::SetAutomaticGainControl(bool enabled) {
  if (capturer_source_) {
    capturer_source_->SetAutomaticGainControl(enabled);
  }
  if (base_capturer_source_) {
    base_capturer_source_->SetAutomaticGainControl(enabled);
  }
  automatic_gain_control_ = enabled;
  return true;
}

bool OHOSAudioInputStream::GetAutomaticGainControl() {
  return automatic_gain_control_;
}

bool OHOSAudioInputStream::IsMuted() {
  return volume_ < kVolumeEpsilon;
}

void OHOSAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  if (capturer_source_) {
    capturer_source_->SetOutputDeviceForAec(output_device_id);
  }
  if (base_capturer_source_) {
    base_capturer_source_->SetOutputDeviceForAec(output_device_id);
  }
}

int OHOSAudioInputStream::GetNWebId(const AudioParameters& parameters) {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    return GetNWebIdOnUIThread(parameters);
  }

  int nweb_id;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce([](const AudioParameters& params,
                        int* out_nweb_id,
                        base::WaitableEvent* out_event) {
        *out_nweb_id = GetNWebIdOnUIThread(params);
        out_event->Signal();
          }, parameters, &nweb_id, &event));

  event.Wait();
  return nweb_id;
}

int OHOSAudioInputStream::GetNWebIdOnUIThread(const AudioParameters& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* renderFrameHost =
      content::RenderFrameHost::FromID(params.render_process_id(),
                                       params.render_frame_id());
  if (!renderFrameHost) {
    LOG(ERROR) << "GetNWebIdOnUIThread RenderFrameHost not found for PID: "
               << params.render_process_id()
               << ", FrameID: " << params.render_frame_id();
  }

  content::WebContents* webContent = content::WebContents::FromRenderFrameHost(renderFrameHost);
  if (!webContent) {
    LOG(ERROR) << "GetNWebIdOnUIThread WebContent not associated with RenderFrameHost";
    return kInvalidNWebId;
  }

  return webContent->GetNWebId();
}

void OHOSAudioInputStream::ResumeMicrophone() {
  if (callback_adapter_) {
    callback_adapter_->SetCapturePaused(false);
    OnMicrophoneCaptureStateChanged(AudioCaptureState::ACTIVE);
  }
}

void OHOSAudioInputStream::PauseMicrophone() {
  if (callback_adapter_) {
    callback_adapter_->SetCapturePaused(true);
    OnMicrophoneCaptureStateChanged(AudioCaptureState::PAUSE);
  }
}

void OHOSAudioInputStream::OnMicrophoneCaptureStateChangedBind(AudioCaptureState audio_capture_state,
                                                               AudioCaptureState new_state,
                                                               const AudioParameters& parameters) {
  if (audio_capture_state == new_state) {
    return;
  }

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    auto ui_task_runner = content::GetUIThreadTaskRunner({});
    if (!ui_task_runner) {
      LOG(ERROR) << "OHOSAudioInputStream::OnMicrophoneCaptureStateChangedBind "
                    "ui_task_runner is nullptr";
      return;
    }
    ui_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce([](AudioCaptureState audio_capture_state,
                          AudioCaptureState new_state,
                          const AudioParameters& parameters,
                          base::WaitableEvent* out_event) {
            OHOSAudioInputStream::OnMicrophoneCaptureStateChangedBind(audio_capture_state, new_state, parameters);
        }, audio_capture_state, new_state, parameters, &event));
    return;
  }

  content::RenderFrameHost* renderFrameHost = content::RenderFrameHost::FromID(
      parameters.render_process_id(), parameters.render_frame_id());
  if (!renderFrameHost) {
    return;
  }
  content::WebContents* webContent =
      content::WebContents::FromRenderFrameHost(renderFrameHost);
  if (webContent) {
    webContent->OnMicrophoneCaptureStateChanged(
        static_cast<int>(audio_capture_state), static_cast<int>(new_state));
    LOG(INFO) << "OHOSAudioInputStream::OnMicrophoneCaptureStateChangedBind in "
                 "BrowserThread::UI";
  }
}

void OHOSAudioInputStream::OnMicrophoneCaptureStateChanged(AudioCaptureState new_state) {
    OHOSAudioInputStream::OnMicrophoneCaptureStateChangedBind(audio_capture_state_, new_state, parameters_);
    audio_capture_state_ = new_state;
}
void OHOSAudioInputStream::SetPauseStateChangedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  LOG(INFO) << "OHOSAudioInputStream::SetPauseStateChangedCallback";
  pause_state_callback_ = std::move(callback);
}

void OHOSAudioInputStream::OnCapturerPaused(bool is_paused) {
  if (pause_state_callback_) {
    LOG(INFO) << "OHOSAudioInputStream::OnCapturerPaused, is_paused: " << is_paused;
    pause_state_callback_.Run(is_paused);
  }
}
}  // namespace media
