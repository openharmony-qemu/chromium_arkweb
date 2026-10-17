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
 
#if BUILDFLAG(ARKWEB_AI)
void RenderFrameImpl::CloseImageOverlaySelection() {
  if (GetFrameHost()) {
    GetFrameHost()->CloseImageOverlaySelection();
  }
}
#endif  // BUILDFLAG(ARKWEB_AI)

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)
void RenderFrameImpl::InitBlankScreenMonitor() {
  // Only initialize blank screen detection for main frame
  if (!is_main_frame_)
    return;

  // Gate on RendererPreferences.blank_screen_monitor_enabled.
  // RendererPreferences is synced from browser via SyncRendererPrefs() before
  // navigation commit, so the value is available here without any sync IPC.
  // When disabled, skip all initialization — no Mojo host, no compositor
  // frame analysis, no JS exception collection.
  bool enabled = false;
  if (auto* web_view = GetWebView()) {
    enabled = web_view->GetRendererPreferences().blank_screen_monitor_enabled;
  }
  if (!enabled) {
    LOG(DEBUG) << "[BlankScreenMonitor] Renderer: detection not enabled, skipping init";
    return;
  }

  // Get FrameSinkId
  auto* widget = GetLocalRootWebFrameWidget();
  if (!widget)
    return;

  auto frame_sink_id = widget->GetFrameSinkId();
  cached_blank_screen_frame_sink_id_ = frame_sink_id;

  // Get BlankScreenMonitorHost Mojo proxy.
  mojo::PendingRemote<blank_screen::mojom::BlankScreenMonitorHost> pending_host;
  GetBrowserInterfaceBroker().GetInterface(
      pending_host.InitWithNewPipeAndPassReceiver());

  auto remote = mojo::SharedRemote<blank_screen::mojom::BlankScreenMonitorHost>(
      std::move(pending_host));

  blank_screen::BlankScreenReporter::GetInstance().SetHost(
      frame_sink_id, std::move(remote));
  blank_screen::BlankScreenReporter::GetInstance().ResetForNewNavigation(
      frame_sink_id);

  // Enable compositor frame analysis.
  widget->SetArkWebFrameSinkId(frame_sink_id);

  LOG(DEBUG) << "[BlankScreenMonitor] Renderer: BlankScreenMonitor initialized for main frame, "
            << "frame_sink_id=" << frame_sink_id.ToString();
}
void RenderFrameImpl::DidCreateScriptContextForBlankScreen(
    v8::Local<v8::Context> context,
    int world_id) {
  // Only register V8 hooks in main frame main world
  if (!is_main_frame_ || world_id != ISOLATED_WORLD_ID_GLOBAL)
    return;
  auto* widget = GetLocalRootWebFrameWidget();
  if (!widget)
    return;
  blank_screen::JsExceptionCollector::GetInstance().InstallHooks(
      GetAgentGroupScheduler().Isolate(), widget->GetFrameSinkId());
}

void RenderFrameImpl::CleanupBlankScreenMonitor() {
  if (!is_main_frame_)
    return;

  if (!cached_blank_screen_frame_sink_id_.is_valid())
    return;

  blank_screen::BlankScreenReporter::GetInstance().RemoveFrameSink(cached_blank_screen_frame_sink_id_);
  blank_screen::JsExceptionCollector::GetInstance().RemoveFrameSink(cached_blank_screen_frame_sink_id_);
  LOG(DEBUG) << "[BlankScreenMonitor] Renderer: cleaned up BlankScreenMonitor for "
            << "frame_sink_id=" << cached_blank_screen_frame_sink_id_.ToString();
}

void RenderFrameImpl::ResetBlankScreenMonitorForNavigation() {
  if (!is_main_frame_)
    return;
  auto* widget = GetLocalRootWebFrameWidget();
  if (!widget)
    return;
  blank_screen::BlankScreenReporter::GetInstance().ResetForNewNavigation(
      widget->GetFrameSinkId());
  blank_screen::JsExceptionCollector::GetInstance().ResetForNewNavigation(
      widget->GetFrameSinkId());
}
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_MONITOR)

#if BUILDFLAG(ARKWEB_SAME_LAYER)
blink::WebNativeBridge* RenderFrameImpl::CreateWebNativeBridge(
    blink::WebNativeClient* client) {
    return media_factory_.CreateWebNativeBridge(client, GetLocalRootWebFrameWidget()->GetFrameSinkId(),
        agent_scheduling_group_->agent_group_scheduler().CompositorTaskRunner());
}

float RenderFrameImpl::DeviceScaleFactor() {
  return media_factory_.GetDeviceScaleFactor();
}
#endif

#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
bool RenderFrameImpl::IsOffscreen() {
  return is_offscreen_;
}
#endif

#if BUILDFLAG(ARKWEB_PASSWORD_AUTOFILL)
gfx::RectF RenderFrameImpl::ElementBoundsInWindow(
    const blink::WebElement& element) {
  return gfx::RectF(GetLocalRootWebFrameWidget()->BlinkSpaceToEnclosedDIPs(
      element.BoundsInWidget()));
}
#endif

#if BUILDFLAG(ARKWEB_MULTI_WINDOW)
bool RenderFrameImpl::GetNewWindowWebView(
    const GURL& target_url,
                                          blink::WebNavigationPolicy policy,
    bool allow_popup,
    const blink::WebWindowFeatures& features) {
  mojom::CreateNewWindowStatus status = mojom::CreateNewWindowStatus::kBlocked;
  auto* frame_host = GetFrameHost();
  if (!frame_host) {
    return false;
  }
  if (!frame_host->GetCreateNewWindow(
          target_url, NavigationPolicyToDisposition(policy), allow_popup,
          ConvertWebWindowFeaturesToMojoWindowFeatures(features), &status) ||
      status != mojom::CreateNewWindowStatus::kSuccess) {
    return false;
  }
  return true;
}
#endif

#if BUILDFLAG(ARKWEB_DRAG_DROP)
void RenderFrameImpl::ClearContextMenu() {
  // It does not postTask here because contextmenu popup windows should be
  // dismissed before drag start.
  GetFrameHost()->OnClearContextMenu();
}
#endif  // BUILDFLAG(ARKWEB_DRAG_DROP)

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
void RenderFrameImpl::SetZoomLevel(float magnify_delta,
                                   const gfx::Point& anchor) {
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    return;
  }
  web_frame_widget->SetZoomLevel(magnify_delta, anchor);
}

void RenderFrameImpl::SetOverscrollMode(int mode) {
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    return;
  }
  web_frame_widget->SetOverscrollMode(mode);
}

bool RenderFrameImpl::IsElementExist(std::string xPath) {
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    return false;
  }
  return web_frame_widget->IsElementExist(xPath);
}

int32_t RenderFrameImpl::ExecuteInputCommand(const std::string& xpath,
                                            const std::string& value,
                                            const std::string& event_type) {
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteInputCommand(xpath, value, event_type);
}

int32_t RenderFrameImpl::ExecuteSelectCommand(
    const std::string& xpath,
    const std::vector<int32_t>& indexes,
    const std::vector<std::string>& values) {
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteSelectCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteSelectCommand(xpath, indexes, values);
}

int32_t RenderFrameImpl::ExecuteFocusCommand(const std::string& xpath,
                                              bool suppress_keyboard) {
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteFocusCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteFocusCommand(xpath, suppress_keyboard);
}

int32_t RenderFrameImpl::ExecuteInputModifyCommand(const std::string& content) {
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputModifyCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteInputModifyCommand(content);
}

int32_t RenderFrameImpl::ExecuteInputInsertCommand(const std::string& content,
                                                    int32_t index) {
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputInsertCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteInputInsertCommand(content, index);
}

int32_t RenderFrameImpl::ExecuteAutoFillCommand(const std::string& xpath,
                                                 const std::string& content,
                                                 int32_t index,
                                                 int32_t mode) {
  auto* web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteAutoFillCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteAutoFillCommand(xpath, content, index, mode);
}

int32_t RenderFrameImpl::ExecuteClickCommand(const std::string& xpath) {
  auto* web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteClickCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteClickCommand(xpath);
}

int32_t RenderFrameImpl::ExecuteScrollCommand(const std::string& xpath,
                                                const std::string& align,
                                                int32_t offset) {
  auto* web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteScrollCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteScrollCommand(xpath, align, offset);
}

int32_t RenderFrameImpl::ExecuteInputSelectCommand(int32_t start_index,
                                                      int32_t finish_index) {
  auto* web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputSelectCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteInputSelectCommand(start_index, finish_index);
}

int32_t RenderFrameImpl::ExecuteInputSetCursorCommand(int32_t index) {
  auto* web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    LOG(ERROR) << "[WebCommandAction] ExecuteInputSetCursorCommand: web frame widget is null";
    return static_cast<int32_t>(OHOS::NWeb::WebCommandErrorCode::PAGE_NOT_READY);
  }
  return web_frame_widget->ExecuteInputSetCursorCommand(index);
}

#if BUILDFLAG(ARKWEB_GET_SCROLL_OFFSET)
gfx::Vector2dF RenderFrameImpl::GetOverScrollOffset() {
  gfx::Vector2dF overscroll_offset;
  overscroll_offset.set_x(0.0f);
  overscroll_offset.set_y(0.0f);
  auto web_frame_widget = GetLocalRootWebFrameWidget();
  if (!web_frame_widget) {
    return overscroll_offset;
  }
  return web_frame_widget->GetOverScrollOffset();
}
#endif
#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
#if BUILDFLAG(ARKWEB_MENU)
void RenderFrameImpl::MouseSelectMenuShow(bool show) {
  if (GetFrameHost()) {
    GetFrameHost()->MouseSelectMenuShow(show);
  }
}

void RenderFrameImpl::ChangeVisibilityOfQuickMenu() {
  if (GetFrameHost()) {
    GetFrameHost()->ChangeVisibilityOfQuickMenu();
  }
}

void RenderFrameImpl::HideQuickMenu() {
  if (GetFrameHost()) {
    GetFrameHost()->HideQuickMenu();
  }
}
#endif

#if BUILDFLAG(ARKWEB_PDF)
void RenderFrameImpl::OnPdfScrollAtBottom(const std::string& url) {
  if (GetFrameHost()) {
    GetFrameHost()->OnPdfScrollAtBottom(url);
  }
}

void RenderFrameImpl::OnPdfLoadEvent(int32_t result, const std::string& url) {
  if (GetFrameHost()) {
    GetFrameHost()->OnPdfLoadEvent(result, url);
  }
}

void RenderFrameImpl::SetIsPDF(bool is_pdf) {
  is_pdf_ = is_pdf;
}

bool RenderFrameImpl::IsPDF() {
  return is_pdf_;
}

#endif  // BUILDFLAG(ARKWEB_PDF)

#if BUILDFLAG(ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION)
bool RenderFrameImpl::IsVideoLoadOptimizationEnabled(const std::string& url) {
  if (auto* thread = RenderThreadImpl::current()) {
    return thread->IsVideoLoadOptimizationEnabled(url);
  }
  return false;
}

int RenderFrameImpl::GetVideoPreloadTimeDefault() const {
  if (auto* thread = RenderThreadImpl::current()) {
    return thread->GetVideoPreloadTimeDefault();
  }
  return INT_MAX;
}

int RenderFrameImpl::GetVideoMinCacheTimeDefault() const {
  if (auto* thread = RenderThreadImpl::current()) {
    return thread->GetVideoMinCacheTimeDefault();
  }
  return INT_MAX;
}

int RenderFrameImpl::GetVideoMaxCacheTimeDefault() const {
  if (auto* thread = RenderThreadImpl::current()) {
    return thread->GetVideoMaxCacheTimeDefault();
  }
  return INT_MAX;
}

int RenderFrameImpl::GetVideoMoovSizeDefault() const {
  if (auto* thread = RenderThreadImpl::current()) {
    return thread->GetVideoMoovSizeDefault();
  }
  return INT_MAX;
}

int RenderFrameImpl::GetVideoBitrateDefault() const {
  if (auto* thread = RenderThreadImpl::current()) {
    return thread->GetVideoBitrateDefault();
  }
  return INT_MAX;
}

bool RenderFrameImpl::SetNewsFeedPageFitted() {
  return media_factory_.SetNewsFeedPageFitted();
}
#endif // ARKWEB_EXT_VIDEO_LOAD_OPTIMIZATION

#if BUILDFLAG(ARKWEB_JS_ON_DOCUMENT_END)
void RenderFrameImpl::OnDocumentEndReady() {
  if (GetFrameHost()) {
    GetFrameHost()->OnDocumentEndReady();
  }
}
#endif

#if BUILDFLAG(ARKWEB_READER_MODE)
bool RenderFrameImpl::IsReaderModeEnabled() {
  return GetRendererPreferences().is_reader_mode_enabled;
}

bool RenderFrameImpl::IsCustomReaderModeEnabled() {
  return GetRendererPreferences().is_custom_reader_mode_enabled;
}

void RenderFrameImpl::ReportDistillableResult(
    const std::string& event_type,
    const std::string& value) {
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  if (!render_thread_impl) {
    LOG(ERROR) << "[Distiller] Get current render thread failed.";
    return;
  }

  mojom::RendererHost* renderer_host = render_thread_impl->GetRendererHost();
  if (!renderer_host) {
    LOG(ERROR) << "[Distiller] Get current render thread's render host failed.";
    return;
  }

  renderer_host->ReportDistillableResult(event_type, value);
}
#endif
