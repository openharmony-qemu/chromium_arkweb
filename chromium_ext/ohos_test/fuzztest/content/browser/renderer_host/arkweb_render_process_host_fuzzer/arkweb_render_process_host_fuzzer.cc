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

#include <fuzzer/FuzzedDataProvider.h>

#include "base/lazy_instance.h"
#include "content/public/browser/render_process_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "arkweb/ohos_adapter_ndk/mock_ndk_api/include/mock_base_ohos_api.h"
#include "arkweb/ohos_nweb/src/nweb_resize_helper.h"
#include "content/public/common/content_switches.h"
#define private public
#include "arkweb/chromium_ext/content/browser/renderer_host/arkweb_render_process_host_impl_utils.h"
#undef private
using namespace content;
#define private public
#include "content/browser/renderer_host/render_process_host_impl.h"
#undef private

#if BUILDFLAG(ARKWEB_THEME_FONT)
const base::FilePath::CharType kAppThemePathA[] =
    FILE_PATH_LITERAL("/data/themes/a/app");
const base::FilePath::CharType kAppThemePathB[] =
    FILE_PATH_LITERAL("/data/themes/b/app");
const base::FilePath::CharType kAppThemeFontsDirName[] =
    FILE_PATH_LITERAL("fonts");
const base::FilePath::CharType kAppThemeFlagFileName[] =
    FILE_PATH_LITERAL("flag");
const base::FilePath::CharType kAppThemeFontsManifest[] =
    FILE_PATH_LITERAL("manifest.json");
#endif

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* fuzz_data, size_t fuzz_size) {
  if (fuzz_data == nullptr || fuzz_size == 0) {
    return 0;
  }

  FuzzedDataProvider fdp(fuzz_data, fuzz_size);
  uint8_t op = fdp.ConsumeIntegralInRange<uint8_t>(0, 49);

  switch (op) {
    case 0: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      content::RenderProcessHost::SetRenderProcessMode(content::RenderProcessMode::SINGLE_MODE);
      size_t maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
#endif
      break;
    }
    case 1: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      content::RenderProcessHost::SetRenderProcessMode(content::RenderProcessMode::MULTIPLE_MODE);
      g_max_renderer_count_override = 1;
      size_t maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
#endif
      break;
    }
    case 2: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      content::RenderProcessHost::SetRenderProcessMode(content::RenderProcessMode::MULTIPLE_MODE);
      g_max_renderer_count_override = 4;
      size_t maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
#endif
      break;
    }
    case 3: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      content::RenderProcessHost::SetRenderProcessMode(content::RenderProcessMode::MULTIPLE_MODE);
      g_max_renderer_count_override = 10;
      size_t maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
      g_max_renderer_count_override = 100;
      maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
#endif
      break;
    }
    case 4: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      auto* killer = content::DelayedRenderKiller::GetInstance();
      killer->rep_ = 5;
      bool needDebug = killer->NeedDebug();
      killer->rep_ = 0;
#endif
      break;
    }
    case 5: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      auto* killer = content::DelayedRenderKiller::GetInstance();
      killer->rep_ = 3;
      bool needDebug = killer->NeedDebug();
#endif
      break;
    }
    case 6: {
      ArkwebRenderProcessHostImplUtils::GetExistingBackgroundProcessHost();
      break;
    }
    case 7: {
      ArkwebRenderProcessHostImplUtils::GetExistingBackgroundProcessHost();
      break;
    }
    case 8: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = true;
      g_render_process_mode = content::RenderProcessMode::DEFAULT_MODE;
      auto result = content::RenderProcessHost::render_process_mode();
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = false;
#endif
      break;
    }
    case 9: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = true;
      g_render_process_mode = content::RenderProcessMode::SINGLE_MODE;
      auto result = content::RenderProcessHost::render_process_mode();
#endif
      break;
    }
    case 10: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = true;
      g_render_process_mode = content::RenderProcessMode::MULTIPLE_MODE;
      auto result = content::RenderProcessHost::render_process_mode();
#endif
      break;
    }
    case 11: {
#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
      int32_t status = 0;
      int32_t process_id = 1234;
      int32_t thread_id = 5678;
      int32_t role = 0;
      ArkwebRenderProcessHostImplUtils::ReportKeyThreadEx(status, process_id, thread_id, role);
#endif
      break;
    }
    case 12: {
#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
      int32_t status = 1;
      int32_t process_id = 1234;
      int32_t role = 1;
      std::vector<int32_t> thread_ids;
      thread_ids.push_back(1000);
      thread_ids.push_back(2000);
      ArkwebRenderProcessHostImplUtils::ReportKeyThreadIdsEx(status, process_id, thread_ids, role);
#endif
      break;
    }
    case 13: {
#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
      int32_t status = 2;
      int32_t process_id = 0;
      int32_t role = -1;
      std::vector<int32_t> thread_ids;
      ArkwebRenderProcessHostImplUtils::ReportKeyThreadIdsEx(status, process_id, thread_ids, role);
#endif
      break;
    }
    case 14: {
#if BUILDFLAG(ARKWEB_DFX_TRACING)
      int64_t block_time = 200;
      ArkwebRenderProcessHostImplUtils::ReportHisyevent(block_time, "ReportRenderInitBlock");
#endif
      break;
    }
    case 15: {
#if BUILDFLAG(ARKWEB_DFX_TRACING)
      OHOS::NWeb::NWebResizeHelper::GetInstance().SetDragResizeStart(false);
      ArkwebRenderProcessHostImplUtils::ReportHisyevent(200, "ReportDragBlank");
      ArkwebRenderProcessHostImplUtils::ReportHisyevent(200, "ReportRenderInitBlock");
      OHOS::NWeb::NWebResizeHelper::GetInstance().SetDragResizeStart(true);
      ArkwebRenderProcessHostImplUtils::ReportHisyevent(200, "ReportDragBlank");
#endif
      break;
    }
    case 16: {
#if BUILDFLAG(ARKWEB_DFX_TRACING)
      int64_t block_time = 0;
      ArkwebRenderProcessHostImplUtils::ReportHisyevent(block_time, "ReportDragBlank");
      block_time = 1000;
      ArkwebRenderProcessHostImplUtils::ReportHisyevent(block_time, "ReportDragBlank");
#endif
      break;
    }
    case 17: {
#if BUILDFLAG(ARKWEB_THEME_FONT)
      ArkwebRenderProcessHostImplUtils::g_theme_font_ = nullptr;
      bool valid = ArkwebRenderProcessHostImplUtils::IsThemeFontValid();
#endif
      break;
    }
    case 18: {
#if BUILDFLAG(ARKWEB_THEME_FONT)
      ArkwebRenderProcessHostImplUtils::g_theme_font_ = std::make_unique<ThemeFont>();
      bool valid = ArkwebRenderProcessHostImplUtils::IsThemeFontValid();
#endif
      break;
    }
    case 19: {
#if BUILDFLAG(ARKWEB_THEME_FONT)
      ThemeFont* font = ArkwebRenderProcessHostImplUtils::EnsureThemeFont();
#endif
      break;
    }
    case 20: {
#if BUILDFLAG(ARKWEB_THEME_FONT)
      ArkwebRenderProcessHostImplUtils::g_theme_font_ = nullptr;
      ThemeFont* font = ArkwebRenderProcessHostImplUtils::EnsureThemeFont();
#endif
      break;
    }
    case 21: {
#if BUILDFLAG(ARKWEB_I18N)
      std::string locale = "en-US";
      content::RenderProcessHost::OnLocaleChangedToRenderer(locale);
#endif
      break;
    }
    case 22: {
#if BUILDFLAG(ARKWEB_I18N)
      std::string locale = "zh-CN";
      content::RenderProcessHost::OnLocaleChangedToRenderer(locale);
#endif
      break;
    }
    case 23: {
#if BUILDFLAG(ARKWEB_I18N)
      std::string locale = "";
      content::RenderProcessHost::OnLocaleChangedToRenderer(locale);
#endif
      break;
    }
    case 24: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = true;
      g_max_renderer_count_override = 4;
      auto result = content::RenderProcessHost::render_process_mode();
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = false;
      g_max_renderer_count_override = 0;
#endif
      break;
    }
    case 25: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      size_t ignore_count = 1;
      size_t result = ArkwebRenderProcessHostImplUtils::GetProcessCountForLimitArkweb(ignore_count);
#endif
      break;
    }
    case 26: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      size_t ignore_count = 0;
      size_t result = ArkwebRenderProcessHostImplUtils::GetProcessCountForLimitArkweb(ignore_count);
#endif
      break;
    }
    case 27: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      size_t ignore_count = 100;
      size_t result = ArkwebRenderProcessHostImplUtils::GetProcessCountForLimitArkweb(ignore_count);
#endif
      break;
    }
    case 28: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      auto* killer = content::DelayedRenderKiller::GetInstance();
      killer->rep_ = 1;
      bool needDebug = killer->NeedDebug();
#endif
      break;
    }
    case 29: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      auto* killer = content::DelayedRenderKiller::GetInstance();
      killer->rep_ = 6;
      bool needDebug = killer->NeedDebug();
#endif
      break;
    }
    case 30: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = false;
      g_render_process_mode = content::RenderProcessMode::SINGLE_MODE;
      auto result = content::RenderProcessHost::render_process_mode();
#endif
      break;
    }
    case 31: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      content::RenderProcessHost::SetRenderProcessMode(content::RenderProcessMode::MULTIPLE_MODE);
      g_max_renderer_count_override = 5;
      size_t maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
#endif
      break;
    }
    case 32: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_SHARE)
      std::string token = "test_token_123";
      ArkwebRenderProcessHostImplUtils::GetProcessForSharedToken(token);
      ArkwebRenderProcessHostImplUtils::RegisteProcessForSharedToken(token, nullptr);
      ArkwebRenderProcessHostImplUtils::RemoveFromSharedRenderProcessMap(nullptr);
      ArkwebRenderProcessHostImplUtils::GetProcessForSharedToken(token);
#endif
      break;
    }
    case 33: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_SHARE)
      std::string token2 = "test_token_456";
      RenderProcessHost* host = nullptr;
      ArkwebRenderProcessHostImplUtils::RegisteProcessForSharedToken(token2, host);
      ArkwebRenderProcessHostImplUtils::GetProcessForSharedToken(token2);
      ArkwebRenderProcessHostImplUtils::RemoveFromSharedRenderProcessMap(host);
#endif
      break;
    }
    case 34: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE) && BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING) && BUILDFLAG(ARKWEB_DFX_TRACING)
      int64_t block_time = 300;
      std::string mode = "UnknownMode";
      ArkwebRenderProcessHostImplUtils::ReportHisyevent(block_time, mode);
#endif
      break;
    }
    case 35: {
#if BUILDFLAG(ARKWEB_THEME_FONT)
      ArkwebRenderProcessHostImplUtils::g_theme_font_ = std::make_unique<ThemeFont>();
      ArkwebRenderProcessHostImplUtils::g_theme_font_->flag_path = base::FilePath(FILE_PATH_LITERAL("/data/themes/a/app/flag"));
      ArkwebRenderProcessHostImplUtils::g_theme_font_->manifest_path = base::FilePath(FILE_PATH_LITERAL("/data/themes/a/app/fonts/manifest.json"));
      ArkwebRenderProcessHostImplUtils::g_theme_font_->font_path = base::FilePath(FILE_PATH_LITERAL("/data/themes/a/app/fonts/test.ttf"));
      base::File test_file(ArkwebRenderProcessHostImplUtils::g_theme_font_->font_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      test_file.Close();
      ArkwebRenderProcessHostImplUtils::g_theme_font_->font_files.clear();
      base::File valid_file(ArkwebRenderProcessHostImplUtils::g_theme_font_->font_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
      if (valid_file.IsValid()) {
        ArkwebRenderProcessHostImplUtils::g_theme_font_->font_files.push_back(std::move(valid_file));
      }
      bool valid = ArkwebRenderProcessHostImplUtils::IsThemeFontValid();
#endif
      break;
    }
    case 36: {
#if BUILDFLAG(ARKWEB_I18N)
      std::string locale = "fr-FR";
      content::RenderProcessHost::OnLocaleChangedToRenderer(locale);
      locale = "de-DE";
      content::RenderProcessHost::OnLocaleChangedToRenderer(locale);
#endif
      break;
    }
    case 37: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      auto* killer = content::DelayedRenderKiller::GetInstance();
      killer->rep_ = 0;
      bool needDebug = killer->NeedDebug();
#endif
      break;
    }
    case 38: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      auto* killer = content::DelayedRenderKiller::GetInstance();
      killer->rep_ = 10;
      bool needDebug = killer->NeedDebug();
#endif
      break;
    }
    case 39: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = false;
      g_render_process_mode = content::RenderProcessMode::DEFAULT_MODE;
      auto result = content::RenderProcessHost::render_process_mode();
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = false;
      result = content::RenderProcessHost::render_process_mode();
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = false;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = true;
      result = content::RenderProcessHost::render_process_mode();
#endif
      break;
    }
    case 40: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      size_t ignore_count = 5;
      size_t result = ArkwebRenderProcessHostImplUtils::GetProcessCountForLimitArkweb(ignore_count);
      ignore_count = 1000;
      result = ArkwebRenderProcessHostImplUtils::GetProcessCountForLimitArkweb(ignore_count);
#endif
      break;
    }
    case 41: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      content::RenderProcessHost::SetRenderProcessMode(content::RenderProcessMode::MULTIPLE_MODE);
      g_max_renderer_count_override = 20;
      size_t maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
      g_max_renderer_count_override = 0;
#endif
      break;
    }
    case 42: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      content::RenderProcessHost::SetRenderProcessMode(content::RenderProcessMode::MULTIPLE_MODE);
      g_max_renderer_count_override = 1000;
      size_t maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
      g_max_renderer_count_override = 0;
#endif
      break;
    }
    case 43: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      content::RenderProcessHost::SetRenderProcessMode(content::RenderProcessMode::SINGLE_MODE);
      size_t maxRendererProcessCount = ArkwebRenderProcessHostImplUtils::GetMaxRendererProcessCountEx();
#endif
      break;
    }
    case 44: {
#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
      int32_t status = -1;
      int32_t process_id = 0;
      int32_t thread_id = 0;
      int32_t role = 2;
      ArkwebRenderProcessHostImplUtils::ReportKeyThreadEx(status, process_id, thread_id, role);
#endif
      break;
    }
    case 45: {
#if BUILDFLAG(ARKWEB_PERFORMANCE_SCHEDULING)
      int32_t status = 1;
      int32_t process_id = 9999;
      int32_t role = 0;
      std::vector<int32_t> thread_ids;
      thread_ids.push_back(1);
      thread_ids.push_back(2);
      thread_ids.push_back(3);
      ArkwebRenderProcessHostImplUtils::ReportKeyThreadIdsEx(status, process_id, thread_ids, role);
#endif
      break;
    }
    case 46: {
#if BUILDFLAG(ARKWEB_DFX_TRACING)
      int64_t block_time = 5000;
      ArkwebRenderProcessHostImplUtils::ReportHisyevent(block_time, "ReportRenderInitBlock");
#endif
      break;
    }
    case 47: {
#if BUILDFLAG(ARKWEB_THEME_FONT)
      ThemeFont* font = nullptr;
      ArkwebRenderProcessHostImplUtils::g_theme_font_.reset();
      font = ArkwebRenderProcessHostImplUtils::EnsureThemeFont();
#endif
      break;
    }
    case 48: {
#if BUILDFLAG(ARKWEB_I18N)
      std::string locale = "ja-JP";
      content::RenderProcessHost::OnLocaleChangedToRenderer(locale);
      locale = "ko-KR";
      content::RenderProcessHost::OnLocaleChangedToRenderer(locale);
#endif
      break;
    }
    case 49: {
#if BUILDFLAG(ARKWEB_RENDER_PROCESS_MODE)
      base::ohos::SysInfoUtilsMock::mockIsPcDevice = true;
      base::ohos::SysInfoUtilsMock::mockIsTabletDevice = false;
      g_render_process_mode = content::RenderProcessMode::DEFAULT_MODE;
      auto result = content::RenderProcessHost::render_process_mode();
#endif
      break;
    }
    default:
      break;
  }

  return 0;
}
