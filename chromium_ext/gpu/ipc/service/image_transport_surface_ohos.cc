// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

// static
scoped_refptr<gl::Presenter> ImageTransportSurface::CreatePresenter(
    scoped_refptr<SharedContextState> context_state,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SurfaceHandle surface_handle) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  // Under stub/mock bindings there is no real GL surface to present to; mirror
  // the upstream Android factory and return nullptr so host-side GL tests that
  // exercise this path do not dereference a null |context_state->display()|.
  if (gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
      gl::GetGLImplementation() == gl::kGLImplementationStubGL) {
    return nullptr;
  }
#if BUILDFLAG(IS_OZONE)
  return gl::init::CreateSurfacelessViewGLSurface(context_state->display(), surface_handle);
#else
  return nullptr;
#endif
}

// static
scoped_refptr<gl::GLSurface> ImageTransportSurface::CreateNativeGLSurface(
    gl::GLDisplay* display,
    SurfaceHandle surface_handle,
    gl::GLSurfaceFormat format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  return gl::init::CreateViewGLSurface(display, surface_handle);
}

}  // namespace gpu
