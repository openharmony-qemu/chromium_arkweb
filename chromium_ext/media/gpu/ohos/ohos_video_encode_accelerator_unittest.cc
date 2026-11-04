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

#include "media/gpu/ohos/ohos_video_encode_accelerator.h"

#include <functional>
#include <memory>
#include <type_traits>

#include "arkweb/build/features/features.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_log.h"
#include "media/base/ohos/ohos_media_codec_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
using namespace OHOS::NWeb;

namespace {

class FakeMediaLog : public MediaLog {
 public:
  FakeMediaLog() = default;
};

class FakeClient final : public OHOSVideoEncodeAccelerator::Client {
 public:
  ~FakeClient() = default;

  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override {
    require_bitstream_buffers_count_++;
  }

  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) override {
    bitstream_buffer_ready_count_++;
    last_metadata_ = metadata;
  }

  void NotifyErrorStatus(const EncoderStatus& status) override {
    notify_error_status_count_++;
    last_status_ = status;
  }

  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override {
    notify_encoder_info_change_count_++;
  }

  int require_bitstream_buffers_count_ = 0;
  int bitstream_buffer_ready_count_ = 0;
  int notify_error_status_count_ = 0;
  int notify_encoder_info_change_count_ = 0;
  EncoderStatus last_status_;
  BitstreamBufferMetadata last_metadata_{0, false, base::Microseconds(0)};
};

class FakeOHOSMediaCodecBridge final : public OHOSMediaCodecBridge {
 public:
  ~FakeOHOSMediaCodecBridge() = default;

  using DequeueCallback = std::function<
      CodecCodeAdapter(uint32_t&, BufferInfo&, BufferFlag&, OhosBuffer&)>;
  using FillSurfaceCallback =
      std::function<CodecCodeAdapter(scoped_refptr<VideoFrame>, const int64_t)>;

  DequeueCallback dequeue_callback;
  FillSurfaceCallback fill_surface_callback;

  CodecCodeAdapter Release() override { return CodecCodeAdapter::OK; }

  CodecCodeAdapter DequeueOutputBuffer(uint32_t& index,
                                       BufferInfo& info,
                                       BufferFlag& flag,
                                       OhosBuffer& buffer) override {
    if (dequeue_callback) {
      return dequeue_callback(index, info, flag, buffer);
    }
    return CodecCodeAdapter::ERROR;
  }

  CodecCodeAdapter CreateVideoCodecByMime(const std::string mimetype) override {
    return CodecCodeAdapter::OK;
  }

  CodecCodeAdapter CreateVideoCodecByName(const std::string name) override {
    return CodecCodeAdapter::OK;
  }

  CodecCodeAdapter Configure(
      const CodecConfigPara& config,
      scoped_refptr<base::SequencedTaskRunner> codec_task_runner) override {
    return CodecCodeAdapter::OK;
  }

  CodecCodeAdapter Prepare() override { return CodecCodeAdapter::OK; }
  CodecCodeAdapter Start() override { return CodecCodeAdapter::OK; }
  CodecCodeAdapter Stop() override { return CodecCodeAdapter::OK; }
  CodecCodeAdapter Reset() override { return CodecCodeAdapter::OK; }
  CodecCodeAdapter CreateInputSurface() override {
    return CodecCodeAdapter::OK;
  }

  CodecCodeAdapter ReleaseOutputBuffer(uint32_t index, bool render) override {
    return CodecCodeAdapter::OK;
  }

  void ClearKeyFrameCache() override {}

  CodecCodeAdapter RequestKeyFrameSoon() override {
    return CodecCodeAdapter::OK;
  }

  CodecCodeAdapter FillSurfaceBuffer(scoped_refptr<VideoFrame> frame,
                                     const int64_t timestamp_ms) override {
    if (fill_surface_callback) {
      return fill_surface_callback(frame, timestamp_ms);
    }
    return CodecCodeAdapter::ERROR;
  }
};

}  // namespace

class OHOSVideoEncodeAcceleratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    vea_ = new OHOSVideoEncodeAccelerator();
    client_ = std::make_unique<FakeClient>();
    media_log_ = std::make_unique<FakeMediaLog>();
    frame_size_ = gfx::Size(1280, 720);
  }

 protected:
  void TearDown() override {
    if (vea_) {
      // Destroy() calls delete this; after that vea_ is already freed.
      // For tests that don't call Destroy(), we need to delete here.
      // Use explicit destructor call + operator delete to avoid vtable
      // dispatch issues under LTO where the deleting destructor may be
      // incorrectly folded.
      vea_->~OHOSVideoEncodeAccelerator();
      operator delete(vea_);
      vea_ = nullptr;
    }
    client_.reset();
    media_log_.reset();
  }

  // Destroy the current VEA and create a fresh one. Used by tests that
  // need a clean VEA state.
  void CreateFreshVEA() {
    if (vea_) {
      vea_->~OHOSVideoEncodeAccelerator();
      operator delete(vea_);
    }
    vea_ = new OHOSVideoEncodeAccelerator();
  }

  // TaskEnvironment is required for SequencedTaskRunner::GetCurrentDefault()
  // which is called by MaybeStartIOTimer() and related timer operations.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  // Helper methods to access private members via friend declarations.
  // TEST_F creates a derived class of the fixture, and friend access does not
  // propagate to derived classes. These helpers are defined in the fixture
  // (which IS the friend), so they can access private members, and the
  // derived test class can call these public helpers.
  int32_t& NumBuffersAtCodec() { return vea_->num_buffers_at_codec_; }
  auto& PendingFramesRef() { return vea_->pending_frames_; }
  void CallMaybeStartIOTimer() { vea_->MaybeStartIOTimer(); }
  void CallMaybeStopIOTimer() { vea_->MaybeStopIOTimer(); }
  bool IsIOTimerRunning() { return vea_->io_timer_.IsRunning(); }
  auto& AvailableBitstreamBuffers() {
    return vea_->available_bitstream_buffers_;
  }
  bool& ErrorOccurred() { return vea_->error_occurred_; }
  void SetMediaCodec(std::unique_ptr<OHOSMediaCodecBridge> codec) {
    vea_->media_codec_ = std::move(codec);
  }
  void SetLog(std::unique_ptr<MediaLog> log) { vea_->log_ = std::move(log); }
  void SetClientPtrFactory(
      std::unique_ptr<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>
          cpf) {
    vea_->client_ptr_factory_ = std::move(cpf);
  }
  auto& FrameTimestampMap() { return vea_->frame_timestamp_map_; }
  gfx::Size& FrameSize() { return vea_->frame_size_; }
  void CallDoIOTask() { vea_->DoIOTask(); }
  void CallQueueInput() { vea_->QueueInput(); }
  void CallDequeueOutput() { vea_->DequeueOutput(); }
  void CallNotifyErrorStatus(EncoderStatus status) {
    vea_->NotifyErrorStatus(std::move(status));
  }

  OHOSVideoEncodeAccelerator* vea_ = nullptr;
  std::unique_ptr<FakeClient> client_;
  std::unique_ptr<MediaLog> media_log_;
  gfx::Size frame_size_;
};

TEST_F(OHOSVideoEncodeAcceleratorTest, OHOSVideoEncodeAccelerator001) {
  EXPECT_TRUE(vea_);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, OHOSVideoEncodeAccelerator002) {
  EXPECT_TRUE(vea_);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, GetSupportedProfiles) {
  VideoEncodeAccelerator::SupportedProfiles profiles =
      vea_->GetSupportedProfiles();
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Initialize001) {
  VideoEncodeAccelerator::Config config = VideoEncodeAccelerator::Config();
  config.output_profile = VP8PROFILE_ANY;
  ASSERT_EQ(vea_->Initialize(config, client_.get(), std::move(media_log_)),
            EncoderStatus::Codes::kEncoderUnsupportedProfile);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Initialize002) {
  VideoEncodeAccelerator::Config config = VideoEncodeAccelerator::Config();
  config.output_profile = H264PROFILE_BASELINE;
  // Zero bitrate causes Configure to fail with kEncoderUnsupportedConfig on
  // the real codec. This test exercises the kEncoderUnsupportedConfig branch.
  ASSERT_EQ(vea_->Initialize(config, client_.get(), std::move(media_log_)),
            EncoderStatus::Codes::kEncoderUnsupportedConfig);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Initialize003) {
  VideoEncodeAccelerator::Config config = VideoEncodeAccelerator::Config();
  config.output_profile = H264PROFILE_MAIN;
  config.input_visible_size = gfx::Size(1920, 1080);
  config.bitrate = Bitrate::ConstantBitrate(static_cast<uint32_t>(500000));
  ASSERT_EQ(vea_->Initialize(config, client_.get(), std::move(media_log_)),
            EncoderStatus::Codes::kOk);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, MaybeStartIOTimer) {
  NumBuffersAtCodec() = 0;
  PendingFramesRef() = {};
  CallMaybeStartIOTimer();
  EXPECT_FALSE(IsIOTimerRunning());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, MaybeStopIOTimer) {
  NumBuffersAtCodec() = 0;
  PendingFramesRef() = {};
  CallMaybeStopIOTimer();
  EXPECT_FALSE(IsIOTimerRunning());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, MaybeStopIOTimer001) {
  NumBuffersAtCodec() = 1;
  PendingFramesRef() = {};
  CallMaybeStopIOTimer();
  EXPECT_FALSE(IsIOTimerRunning());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, MaybeStopIOTimer002) {
  VideoPixelFormat format = VideoPixelFormat::PIXEL_FORMAT_I420;
  const gfx::Size coded_size = gfx::Size(640, 480);
  const gfx::Rect visible_rect = gfx::Rect(640, 480);
  const gfx::Size natural_size = gfx::Size(640, 480);
  base::TimeDelta timestamp = base::TimeDelta::FromInternalValue(0);
  scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateFrame(
      format, coded_size, visible_rect, natural_size, timestamp);
  bool frame_state = true;
  base::Time frame_time = base::Time::Now();
  NumBuffersAtCodec() = 1;
  PendingFramesRef().push(
      std::make_tuple(video_frame, frame_state, frame_time));
  CallMaybeStopIOTimer();
  EXPECT_FALSE(IsIOTimerRunning());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, MaybeStopIOTimer003) {
  VideoPixelFormat format = VideoPixelFormat::PIXEL_FORMAT_I420;
  const gfx::Size coded_size = gfx::Size(640, 480);
  const gfx::Rect visible_rect = gfx::Rect(640, 480);
  const gfx::Size natural_size = gfx::Size(640, 480);
  base::TimeDelta timestamp = base::TimeDelta::FromInternalValue(0);
  scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateFrame(
      format, coded_size, visible_rect, natural_size, timestamp);
  bool frame_state = true;
  base::Time frame_time = base::Time::Now();
  NumBuffersAtCodec() = 0;
  PendingFramesRef().push(
      std::make_tuple(video_frame, frame_state, frame_time));
  CallMaybeStopIOTimer();
  EXPECT_FALSE(IsIOTimerRunning());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, UseOutputBitstreamBuffer) {
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(1024);
  unsigned int id = 1;
  BitstreamBuffer buffer(id, region.Duplicate(), region.GetSize());
  vea_->UseOutputBitstreamBuffer(std::move(buffer));
  EXPECT_EQ(AvailableBitstreamBuffers().size(), id);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, RequestEncodingParametersChange) {
  Bitrate bitrate = Bitrate::ConstantBitrate(1000000u);
  uint32_t framerate = 30;
  std::optional<gfx::Size> size;
  vea_->RequestEncodingParametersChange(bitrate, framerate, size);
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DoIOTask) {
  CallDoIOTask();
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, QueueInput) {
  CallQueueInput();
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput) {
  CallDequeueOutput();
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Encode) {}

TEST_F(OHOSVideoEncodeAcceleratorTest, NotifyErrorStatus) {}

TEST_F(OHOSVideoEncodeAcceleratorTest, QueueInput001) {
  ErrorOccurred() = false;
  CallQueueInput();
  EXPECT_FALSE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, QueueInput002) {
  ErrorOccurred() = true;
  CallQueueInput();
  EXPECT_TRUE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, QueueInput003) {
  const gfx::Size coded_size;
  const gfx::Rect visible_rect;
  const gfx::Size natural_size;
  base::TimeDelta timestamp = base::TimeDelta::FromInternalValue(1000);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(VideoPixelFormat::PIXEL_FORMAT_I422A, coded_size,
                              visible_rect, natural_size, timestamp);
  bool force_keyframe = false;
  base::Time current_time = base::Time::Now();
  PendingFramesRef().emplace(std::move(frame), force_keyframe, current_time);
  ErrorOccurred() = true;
  CallQueueInput();
  EXPECT_TRUE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, QueueInput004) {
  const gfx::Size coded_size(1920, 1080);
  const gfx::Rect visible_rect(0, 0, 1920, 1080);
  const gfx::Size natural_size(1920, 1080);
  base::TimeDelta timestamp = base::TimeDelta::FromInternalValue(1000);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(VideoPixelFormat::PIXEL_FORMAT_I422A, coded_size,
                              visible_rect, natural_size, timestamp);
  bool force_keyframe = true;
  base::Time current_time = base::Time::Now();
  PendingFramesRef().emplace(std::move(frame), force_keyframe, current_time);
  EXPECT_FALSE(PendingFramesRef().empty());
  ErrorOccurred() = false;
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  // FillSurfaceBuffer returns ERROR by default -> QueueInput returns early,
  // num_buffers_at_codec_ stays 0.
  SetMediaCodec(std::move(media_codec));
  CallQueueInput();
  EXPECT_FALSE(ErrorOccurred());
  EXPECT_EQ(NumBuffersAtCodec(), 0);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, QueueInput005) {
  const gfx::Size coded_size(1920, 1080);
  const gfx::Rect visible_rect(0, 0, 1920, 1080);
  const gfx::Size natural_size(1920, 1080);
  base::TimeDelta timestamp = base::TimeDelta::FromInternalValue(1000);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(VideoPixelFormat::PIXEL_FORMAT_I422A, coded_size,
                              visible_rect, natural_size, timestamp);
  bool force_keyframe = false;
  base::Time current_time = base::Time::Now();
  PendingFramesRef().emplace(std::move(frame), force_keyframe, current_time);
  ErrorOccurred() = false;
  EXPECT_FALSE(PendingFramesRef().empty());
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  // FillSurfaceBuffer returns ERROR by default -> QueueInput returns early.
  SetMediaCodec(std::move(media_codec));
  CallQueueInput();
  EXPECT_FALSE(ErrorOccurred());
  EXPECT_EQ(NumBuffersAtCodec(), 0);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, QueueInput006) {
  const gfx::Size coded_size(1920, 1080);
  const gfx::Rect visible_rect(0, 0, 1920, 1080);
  const gfx::Size natural_size(1920, 1080);
  base::TimeDelta timestamp = base::TimeDelta::FromInternalValue(1000);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(VideoPixelFormat::PIXEL_FORMAT_I422A, coded_size,
                              visible_rect, natural_size, timestamp);
  bool force_keyframe = false;
  base::Time current_time = base::Time::Now();
  PendingFramesRef().emplace(std::move(frame), force_keyframe, current_time);
  ErrorOccurred() = false;
  EXPECT_FALSE(PendingFramesRef().empty());
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  media_codec->fill_surface_callback = [](scoped_refptr<VideoFrame>,
                                          const int64_t) {
    return CodecCodeAdapter::OK;
  };
  SetMediaCodec(std::move(media_codec));
  CallQueueInput();
  EXPECT_FALSE(ErrorOccurred());
  EXPECT_EQ(NumBuffersAtCodec(), 1);
  EXPECT_TRUE(PendingFramesRef().empty());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput001) {
  ErrorOccurred() = true;
  NumBuffersAtCodec() = 0;
  ASSERT_EQ(NumBuffersAtCodec(), 0);
  CallDequeueOutput();
  ASSERT_TRUE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput002) {
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 0;
  ASSERT_EQ(NumBuffersAtCodec(), 0);
  CallDequeueOutput();
  ASSERT_FALSE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput003) {
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 1;
  ASSERT_EQ(NumBuffersAtCodec(), 1);
  CallDequeueOutput();
  ASSERT_FALSE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput004) {
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 0;
  ASSERT_EQ(NumBuffersAtCodec(), 0);
  CallDequeueOutput();
  ASSERT_FALSE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput005) {
  CreateFreshVEA();
  int32_t id = 1;
  size_t size = 10;
  uint64_t offset = 0;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  base::TimeDelta presentation_timestamp = kNoTimestamp;
  BitstreamBuffer bitstreambuffer(id, std::move(region), size, offset,
                                  presentation_timestamp);
  AvailableBitstreamBuffers().push_back(std::move(bitstreambuffer));
  EXPECT_FALSE(AvailableBitstreamBuffers().empty());
  ErrorOccurred() = false;
  ASSERT_FALSE(ErrorOccurred());
  NumBuffersAtCodec() = 1;
  ASSERT_EQ(NumBuffersAtCodec(), 1);
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  media_codec->dequeue_callback = [](uint32_t& index, BufferInfo& info,
                                     BufferFlag& flag, OhosBuffer& buffer) {
    return CodecCodeAdapter::RETRY;
  };
  SetMediaCodec(std::move(media_codec));
  CallDequeueOutput();
  ASSERT_FALSE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput006) {
  CreateFreshVEA();
  int32_t id = 1;
  size_t size = 10;
  uint64_t offset = 0;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  base::TimeDelta presentation_timestamp = kNoTimestamp;
  BitstreamBuffer bitstreambuffer(id, std::move(region), size, offset,
                                  presentation_timestamp);
  AvailableBitstreamBuffers().push_back(std::move(bitstreambuffer));
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 1;
  // DequeueOutputBuffer returns ERROR by default -> NotifyErrorStatus called.
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  SetMediaCodec(std::move(media_codec));
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));
  auto client = std::make_unique<FakeClient>();
  auto client_ptr_factory =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(client_ptr_factory));
  EXPECT_FALSE(AvailableBitstreamBuffers().empty());
  EXPECT_FALSE(NumBuffersAtCodec() == 0);
  CallDequeueOutput();
  ASSERT_TRUE(ErrorOccurred());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput007) {
  CreateFreshVEA();
  int32_t id = 1;
  size_t size = 10;
  uint64_t offset = 0;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  base::TimeDelta presentation_timestamp = kNoTimestamp;
  BitstreamBuffer bitstreambuffer(id, std::move(region), size, offset,
                                  presentation_timestamp);
  AvailableBitstreamBuffers().push_back(std::move(bitstreambuffer));
  EXPECT_FALSE(AvailableBitstreamBuffers().empty());
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 1;
  ASSERT_EQ(NumBuffersAtCodec(), 1);
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  // Return OK but with no matching timestamp in frame_timestamp_map_.
  // DequeueOutput will find no match and return early without error.
  media_codec->dequeue_callback = [](uint32_t& index, BufferInfo& info,
                                     BufferFlag& flag, OhosBuffer& buffer) {
    index = 0;
    return CodecCodeAdapter::OK;
  };
  SetMediaCodec(std::move(media_codec));
  CallDequeueOutput();
  ASSERT_FALSE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput008) {
  CreateFreshVEA();
  int32_t id = 1;
  size_t size = 512;
  uint64_t offset = 0;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  BitstreamBuffer bitstreambuffer(id, std::move(region), size, offset,
                                  base::Microseconds(0));
  AvailableBitstreamBuffers().push_back(std::move(bitstreambuffer));
  FrameTimestampMap().insert({base::Microseconds(0), base::Microseconds(100)});
  EXPECT_FALSE(AvailableBitstreamBuffers().empty());
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 1;
  ASSERT_EQ(NumBuffersAtCodec(), 1);
  std::vector<uint8_t> encoded(100, 0xAB);
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  // Return OK with info.size > bitstream_buffer.size() -> "Encoded buffer
  // too large" error.
  media_codec->dequeue_callback = [&encoded](
                                      uint32_t& index, BufferInfo& info_param,
                                      BufferFlag& flag, OhosBuffer& buffer) {
    index = 0;
    info_param.presentationTimeUs = 0;
    info_param.size = 1024;
    buffer.addr = encoded.data();
    buffer.bufferSize = 100;
    return CodecCodeAdapter::OK;
  };
  SetMediaCodec(std::move(media_codec));
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));
  auto client = std::make_unique<FakeClient>();
  auto client_ptr_factory =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(client_ptr_factory));
  CallDequeueOutput();
  ASSERT_TRUE(ErrorOccurred());
  ASSERT_NE(vea_, nullptr);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Destroy001) {
  CreateFreshVEA();
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  SetMediaCodec(std::move(media_codec));
  ASSERT_NE(vea_, nullptr);
  vea_->Destroy();
  // Destroy() calls delete this; release vea_ to prevent double-free in
  // TearDown.
  vea_ = nullptr;
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Destroy002) {
  CreateFreshVEA();
  vea_->Destroy();
  // Destroy() calls delete this; release vea_ to prevent double-free in
  // TearDown.
  vea_ = nullptr;
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput_happypath_keyframe) {
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));
  auto client = std::make_unique<FakeClient>();
  auto cpf =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(cpf));

  const size_t size = 512;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  BitstreamBuffer buf(/*id=*/1, std::move(region), size, /*offset=*/0,
                      base::Microseconds(0));
  AvailableBitstreamBuffers().push_back(std::move(buf));
  FrameTimestampMap().insert(
      {base::Microseconds(0), base::Microseconds(12345)});
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 1;

  std::vector<uint8_t> encoded(100, 0xAB);
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  media_codec->dequeue_callback = [&encoded](
                                      uint32_t& index, BufferInfo& info_param,
                                      BufferFlag& flag, OhosBuffer& buffer) {
    index = 0;
    info_param.presentationTimeUs = 0;
    info_param.size = 100;
    flag = BufferFlag::CODEC_BUFFER_FLAG_SYNC_FRAME;
    buffer.addr = encoded.data();
    buffer.bufferSize = 100;
    return CodecCodeAdapter::OK;
  };
  SetMediaCodec(std::move(media_codec));

  CallDequeueOutput();

  EXPECT_FALSE(ErrorOccurred());
  EXPECT_EQ(NumBuffersAtCodec(), 0);
  EXPECT_TRUE(AvailableBitstreamBuffers().empty());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput_happypath_nonkeyframe) {
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));
  auto client = std::make_unique<FakeClient>();
  auto cpf =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(cpf));

  const size_t size = 512;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  BitstreamBuffer buf(/*id=*/1, std::move(region), size, /*offset=*/0,
                      base::Microseconds(0));
  AvailableBitstreamBuffers().push_back(std::move(buf));
  FrameTimestampMap().insert(
      {base::Microseconds(0), base::Microseconds(12345)});
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 1;

  std::vector<uint8_t> encoded(100, 0xAB);
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  media_codec->dequeue_callback = [&encoded](
                                      uint32_t& index, BufferInfo& info_param,
                                      BufferFlag& flag, OhosBuffer& buffer) {
    index = 0;
    info_param.presentationTimeUs = 0;
    info_param.size = 100;
    flag = BufferFlag::CODEC_BUFFER_FLAG_NONE;
    buffer.addr = encoded.data();
    buffer.bufferSize = 100;
    return CodecCodeAdapter::OK;
  };
  SetMediaCodec(std::move(media_codec));

  CallDequeueOutput();

  EXPECT_FALSE(ErrorOccurred());
  EXPECT_EQ(NumBuffersAtCodec(), 0);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput_mapping_invalid) {
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));

  auto client = std::make_unique<FakeClient>();
  auto cpf =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(cpf));

  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(10);
  BitstreamBuffer buf(/*id=*/1, std::move(region), /*size=*/512, /*offset=*/0,
                      base::Microseconds(0));
  AvailableBitstreamBuffers().push_back(std::move(buf));
  FrameTimestampMap().insert(
      {base::Microseconds(0), base::Microseconds(12345)});
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 1;

  std::vector<uint8_t> encoded(100, 0xAB);
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  media_codec->dequeue_callback = [&encoded](
                                      uint32_t& index, BufferInfo& info_param,
                                      BufferFlag& flag, OhosBuffer& buffer) {
    index = 0;
    info_param.presentationTimeUs = 0;
    info_param.size = 100;
    flag = BufferFlag::CODEC_BUFFER_FLAG_NONE;
    buffer.addr = encoded.data();
    buffer.bufferSize = 100;
    return CodecCodeAdapter::OK;
  };
  SetMediaCodec(std::move(media_codec));

  CallDequeueOutput();

  EXPECT_TRUE(ErrorOccurred());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput_shortcircuit_num_zero) {
  const size_t size = 512;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  BitstreamBuffer buf(/*id=*/1, std::move(region), size, /*offset=*/0,
                      base::Microseconds(0));
  AvailableBitstreamBuffers().push_back(std::move(buf));

  ErrorOccurred() = false;
  NumBuffersAtCodec() = 0;

  CallDequeueOutput();

  EXPECT_FALSE(ErrorOccurred());
  EXPECT_EQ(NumBuffersAtCodec(), 0);
  EXPECT_EQ(AvailableBitstreamBuffers().size(), 1u);
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Encode_wrong_format) {
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));

  auto client = std::make_unique<FakeClient>();
  auto cpf =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(cpf));

  const gfx::Size coded_size(640, 480);
  const gfx::Rect visible_rect(0, 0, 640, 480);
  const gfx::Size natural_size(640, 480);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_I422A, coded_size, visible_rect,
      natural_size, base::TimeDelta::FromInternalValue(1000));

  vea_->Encode(frame, /*force_keyframe=*/false);

  EXPECT_TRUE(ErrorOccurred());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Encode_wrong_size) {
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));

  auto client = std::make_unique<FakeClient>();
  auto cpf =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(cpf));

  const gfx::Size coded_size(640, 480);
  const gfx::Rect visible_rect(0, 0, 640, 480);
  const gfx::Size natural_size(640, 480);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_I420, coded_size, visible_rect,
      natural_size, base::TimeDelta::FromInternalValue(1000));

  vea_->Encode(frame, /*force_keyframe=*/false);

  EXPECT_TRUE(ErrorOccurred());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Encode_success) {
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));
  auto client = std::make_unique<FakeClient>();
  auto cpf =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(cpf));

  FrameSize() = gfx::Size(640, 480);
  const gfx::Size coded_size(640, 480);
  const gfx::Rect visible_rect(0, 0, 640, 480);
  const gfx::Size natural_size(640, 480);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_I420, coded_size, visible_rect,
      natural_size, base::TimeDelta::FromInternalValue(1000));

  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  media_codec->fill_surface_callback = [](scoped_refptr<VideoFrame>,
                                          const int64_t) {
    return CodecCodeAdapter::OK;
  };
  SetMediaCodec(std::move(media_codec));

  vea_->Encode(frame, /*force_keyframe=*/false);

  EXPECT_FALSE(ErrorOccurred());
  EXPECT_TRUE(PendingFramesRef().empty());
  EXPECT_EQ(NumBuffersAtCodec(), 1);

  NumBuffersAtCodec() = 0;
  CallMaybeStopIOTimer();
}

TEST_F(OHOSVideoEncodeAcceleratorTest, IOTimer_start_stop) {
  NumBuffersAtCodec() = 1;
  PendingFramesRef() = {};
  CallMaybeStartIOTimer();
  ASSERT_TRUE(IsIOTimerRunning());

  NumBuffersAtCodec() = 0;
  CallMaybeStopIOTimer();
  ASSERT_FALSE(IsIOTimerRunning());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, IOTimer_already_running) {
  NumBuffersAtCodec() = 1;
  PendingFramesRef() = {};
  CallMaybeStartIOTimer();
  ASSERT_TRUE(IsIOTimerRunning());

  CallMaybeStartIOTimer();
  EXPECT_TRUE(IsIOTimerRunning());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, Destroy_with_running_timer) {
  CreateFreshVEA();
  NumBuffersAtCodec() = 1;
  PendingFramesRef() = {};
  CallMaybeStartIOTimer();
  ASSERT_TRUE(IsIOTimerRunning());

  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  SetMediaCodec(std::move(media_codec));

  vea_->Destroy();
  // Destroy() calls delete this; release vea_ to prevent double-free in
  // TearDown.
  vea_ = nullptr;
}

TEST_F(OHOSVideoEncodeAcceleratorTest, NotifyErrorStatus_already_errored) {
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));

  auto client = std::make_unique<FakeClient>();
  auto cpf =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(cpf));

  ErrorOccurred() = true;
  CallNotifyErrorStatus(
      {EncoderStatus::Codes::kEncoderFailedEncode, "already errored"});

  EXPECT_TRUE(ErrorOccurred());
}

TEST_F(OHOSVideoEncodeAcceleratorTest, DequeueOutput_memcpy_failure) {
  auto log = std::make_unique<FakeMediaLog>();
  SetLog(std::move(log));
  auto client = std::make_unique<FakeClient>();
  auto cpf =
      std::make_unique<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>(
          client.get());
  SetClientPtrFactory(std::move(cpf));

  const size_t size = 512;
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(size);
  BitstreamBuffer buf(/*id=*/1, std::move(region), size, /*offset=*/0,
                      base::Microseconds(0));
  AvailableBitstreamBuffers().push_back(std::move(buf));
  FrameTimestampMap().insert(
      {base::Microseconds(0), base::Microseconds(12345)});
  ErrorOccurred() = false;
  NumBuffersAtCodec() = 1;

  // Set up a dequeue callback that returns OK but provides a null buffer
  // address, which will cause memcpy_s to fail.
  auto media_codec = std::make_unique<FakeOHOSMediaCodecBridge>();
  media_codec->dequeue_callback = [](uint32_t& index, BufferInfo& info_param,
                                     BufferFlag& flag, OhosBuffer& buffer) {
    index = 0;
    info_param.presentationTimeUs = 0;
    info_param.size = 100;
    flag = BufferFlag::CODEC_BUFFER_FLAG_NONE;
    buffer.addr = nullptr;
    buffer.bufferSize = 100;
    return CodecCodeAdapter::OK;
  };
  SetMediaCodec(std::move(media_codec));

  CallDequeueOutput();

  // memcpy_s with nullptr source should fail -> NotifyErrorStatus called.
  EXPECT_TRUE(ErrorOccurred());
}

}  // namespace media
