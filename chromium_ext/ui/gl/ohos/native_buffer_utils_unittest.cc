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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <functional>
#include <utility>

#include "arkweb/chromium_ext/ui/gl/ohos/native_buffer_utils.h"

#include "gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/gl/test/gl_test_support.h"

namespace gl {
namespace {
using PollMockFn = std::function<int(struct pollfd*, nfds_t, int)>;

PollMockFn& PollMock() {
  static PollMockFn mock;
  return mock;
}

class ScopedPollMock {
 public:
  explicit ScopedPollMock(PollMockFn mock) { PollMock() = std::move(mock); }
  ~ScopedPollMock() { PollMock() = PollMockFn(); }
};

base::ScopedFD MakeValidFd() {
  return base::ScopedFD(open("/dev/null", O_RDONLY));
}

EGLSyncKHR GL_BINDING_CALL FakeCreateSyncKHR(EGLDisplay,
                                             EGLenum,
                                             const EGLint* attribs) {
  if (attribs && attribs[1] >= 0) {
    close(attribs[1]);
  }
  static char kFakeSync;
  return reinterpret_cast<EGLSyncKHR>(&kFakeSync);
}

EGLint GL_BINDING_CALL FakeClientWaitSyncKHR(EGLDisplay,
                                              EGLSyncKHR,
                                              EGLint,
                                              EGLTimeKHR) {
  return 1;
}

EGLint GL_BINDING_CALL FakeWaitSyncKHR(EGLDisplay, EGLSyncKHR, EGLint) {
  return EGL_TRUE;
}

EGLBoolean GL_BINDING_CALL FakeDestroySyncKHR(EGLDisplay, EGLSyncKHR) {
  return EGL_TRUE;
}

class ScopedEGLFenceStubs {
 public:
  ScopedEGLFenceStubs() {
    create_ = gl::g_driver_egl.fn.eglCreateSyncKHRFn;
    client_wait_ = gl::g_driver_egl.fn.eglClientWaitSyncKHRFn;
    wait_ = gl::g_driver_egl.fn.eglWaitSyncKHRFn;
    destroy_ = gl::g_driver_egl.fn.eglDestroySyncKHRFn;
    gl::g_driver_egl.fn.eglCreateSyncKHRFn = &FakeCreateSyncKHR;
    gl::g_driver_egl.fn.eglClientWaitSyncKHRFn = &FakeClientWaitSyncKHR;
    gl::g_driver_egl.fn.eglWaitSyncKHRFn = &FakeWaitSyncKHR;
    gl::g_driver_egl.fn.eglDestroySyncKHRFn = &FakeDestroySyncKHR;
  }
  ~ScopedEGLFenceStubs() {
    gl::g_driver_egl.fn.eglCreateSyncKHRFn = create_;
    gl::g_driver_egl.fn.eglClientWaitSyncKHRFn = client_wait_;
    gl::g_driver_egl.fn.eglWaitSyncKHRFn = wait_;
    gl::g_driver_egl.fn.eglDestroySyncKHRFn = destroy_;
  }

 private:
  eglCreateSyncKHRProc create_;
  eglClientWaitSyncKHRProc client_wait_;
  eglWaitSyncKHRProc wait_;
  eglDestroySyncKHRProc destroy_;
};
}  // namespace

extern "C" int __real_poll(struct pollfd* fds, nfds_t nfds, int timeout);

extern "C" int __wrap_poll(struct pollfd* fds, nfds_t nfds, int timeout) {
  if (PollMock()) {
    return PollMock()(fds, nfds, timeout);
  }
  return __real_poll(fds, nfds, timeout);
}
}

namespace gl {
namespace ohos {
namespace {

class NativeBufferUtilsFenceTest : public testing::Test {
 public:
  void SetUp() override {
    gl::init::InitializeGLNoExtensionsOneOff(
        /*init_bindings=*/true, /*gpu_preference=*/gl::GpuPreference::kDefault);
    display_ = gl::GLTestSupport::InitializeGL(std::nullopt);
    ASSERT_NE(display_, nullptr);
    surf_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    ASSERT_NE(surf_, nullptr);
    gl_context_ = gl::init::CreateGLContext(nullptr, surf_.get(),
                                            gl::GLContextAttribs());
    ASSERT_NE(gl_context_, nullptr);
    ASSERT_TRUE(gl_context_->MakeCurrent(surf_.get()));
  }

 protected:
  scoped_refptr<gl::GLSurface> surf_;
  scoped_refptr<gl::GLContext> gl_context_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST(InsertEglFenceAndWaitTest, NoFenceFdReturnsTrue) {
  base::ScopedFD no_fence;
  EXPECT_TRUE(InsertEglFenceAndWait(std::move(no_fence)));
}

TEST_F(NativeBufferUtilsFenceTest, FenceCreationFailsWhenNoDisplay) {
  base::ScopedFD fd = MakeValidFd();
  ASSERT_GE(fd.get(), 0);
  gl_context_->ReleaseCurrent(surf_.get());
  EXPECT_FALSE(InsertEglFenceAndWait(std::move(fd)));
}

TEST_F(NativeBufferUtilsFenceTest, FenceCreationSucceeds) {
  ScopedEGLFenceStubs stubs;
  base::ScopedFD fd = MakeValidFd();
  ASSERT_GE(fd.get(), 0);
  EXPECT_TRUE(InsertEglFenceAndWait(std::move(fd)));
}

TEST(SyncFenceWaitTest, NoFenceFdReturnsTrue) {
  base::ScopedFD no_fence;
  EXPECT_TRUE(SyncFenceWait(std::move(no_fence)));
}

TEST(SyncFenceWaitTest, PollReadyNoErrorReturnsTrue) {
  ScopedPollMock m([](struct pollfd* fds, nfds_t, int) {
    fds->revents = POLLIN;
    return 1;
  });
  base::ScopedFD fd = MakeValidFd();
  ASSERT_GE(fd.get(), 0);
  EXPECT_TRUE(SyncFenceWait(std::move(fd)));
}

TEST(SyncFenceWaitTest, PollReventsPollerrReturnsFalse) {
  ScopedPollMock m([](struct pollfd* fds, nfds_t, int) {
    fds->revents = POLLERR;
    return 1;
  });
  base::ScopedFD fd = MakeValidFd();
  ASSERT_GE(fd.get(), 0);
  SyncFenceWait(std::move(fd));
}

TEST(SyncFenceWaitTest, PollTimeoutReturnsFalse) {
  ScopedPollMock m(
      [](struct pollfd*, nfds_t, int) { errno = 0; return 0; });
  base::ScopedFD fd = MakeValidFd();
  ASSERT_GE(fd.get(), 0);
  SyncFenceWait(std::move(fd));
}

TEST(SyncFenceWaitTest, PollErrorReturnsFalse) {
  ScopedPollMock m(
      [](struct pollfd*, nfds_t, int) { errno = EBADF; return -1; });
  base::ScopedFD fd = MakeValidFd();
  ASSERT_GE(fd.get(), 0);
  SyncFenceWait(std::move(fd));
}

TEST(SyncFenceWaitTest, PollEintrThenReadyReturnsTrue) {
  int calls = 0;
  ScopedPollMock m([&](struct pollfd* fds, nfds_t, int) {
    if (calls++ == 0) {
      errno = EINTR;
      return -1;
    }
    fds->revents = POLLIN;
    errno = 0;
    return 1;
  });
  base::ScopedFD fd = MakeValidFd();
  ASSERT_GE(fd.get(), 0);
  EXPECT_TRUE(SyncFenceWait(std::move(fd)));
}

TEST(SyncFenceWaitTest, PollEagainThenReadyReturnsTrue) {
  int calls = 0;
  ScopedPollMock m([&](struct pollfd* fds, nfds_t, int) {
    if (calls++ == 0) {
      errno = EAGAIN;
      return -1;
    }
    fds->revents = POLLIN;
    errno = 0;
    return 1;
  });
  base::ScopedFD fd = MakeValidFd();
  ASSERT_GE(fd.get(), 0);
  EXPECT_TRUE(SyncFenceWait(std::move(fd)));
}

}  // namespace
}  // namespace ohos
}  // namespace gl
