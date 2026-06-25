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

#ifndef OHOS_NWEB_SRC_CEF_DELEGATE_TEST_MOCK_BROWSER_HOST_H_
#define OHOS_NWEB_SRC_CEF_DELEGATE_TEST_MOCK_BROWSER_HOST_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "arkweb/build/features/features.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "cef/include/cef_base.h"
#include "cef/include/cef_browser.h"
#include "cef/include/cef_client.h"
#include "cef/include/cef_devtools_message_handler_delegate.h"
#include "cef/ohos_cef_ext/libcef/common/cef_open_devtools_ext_opt.h"
#include "cef/ohos_cef_ext/include/arkweb_browser_ext.h"
#include "include/cef_values.h"
#include "nweb_errors.h"
#include "nweb_value_callback.h"

namespace OHOS::NWeb {
namespace test {

// --- Test callback ---

class TestStringValueCallback : public NWebStringValueCallback {
 public:
  void OnReceiveValue(const std::string& value) override {
    value_ = value;
    called_ = true;
  }

  std::string value_;
  bool called_ = false;
};

// --- JSON helpers ---

inline int32_t ExtractCodeFromJson(const std::string& json) {
  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    return -9999;
  }
  auto code = val->GetDict().FindInt("code");
  return code.value_or(-9998);
}

inline std::string ExtractMessageFromJson(const std::string& json) {
  auto val = base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!val || !val->is_dict()) {
    return "";
  }
  const std::string* msg = val->GetDict().FindString("message");
  return msg ? *msg : "";
}

// --- Fake CefRegistration ---

class FakeRegistration : public CefRegistration {
 public:
  FakeRegistration() = default;

 private:
  IMPLEMENT_REFCOUNTING(FakeRegistration);
};

// --- MockBrowserHost ---
// Mocks all pure virtuals of ArkWebBrowserHostExt to avoid link errors.
// Only ExecuteDevToolsMethod, AddDevToolsMessageObserver, and
// ExecuteSelectCommand carry real test expectations.

class MockBrowserHost : public ArkWebBrowserHostExt {
 public:
  CefRefPtr<ArkWebBrowserHostExt> GetHost() override { return this; }
  // Use MOCK_METHOD only for methods we need to verify behavior
  MOCK_METHOD(void,
              ExecuteJavaScript,
              (const std::string&,
               CefRefPtr<CefJavaScriptResultCallback>,
               bool),
              (override));

  // Required methods from ArkWebBrowserHostExt
  // Note: Only pure virtual functions (= 0) MUST be implemented
  // Other virtual functions can be omitted if base class has default
  // implementation Non-virtual functions don't need to be implemented
  bool CreateBrowser(const CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient> client,
                     const CefString& url,
                     const CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue> extra_info,
                     CefRefPtr<CefRequestContext> request_context) {
    return false;
  }
  CefRefPtr<CefBrowser> CreateBrowserSync(
      const CefWindowInfo& windowInfo,
      CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefDictionaryValue> extra_info,
      CefRefPtr<CefRequestContext> request_context) {
    return nullptr;
  }
  MOCK_METHOD(void,
              SetDisallowSandboxFileAccessFromFileUrl,
              (bool),
              (override));
  MOCK_METHOD(void, GetRootBrowserAccessibilityManager, (void**), (override));
  MOCK_METHOD(void,
              ExecuteJavaScriptExt,
              (int, uint64_t, CefRefPtr<CefJavaScriptResultCallback>, bool),
              (override));
  MOCK_METHOD(void, SetNativeWindow, (cef_native_window_t), (override));
  MOCK_METHOD(void, SetWebDebuggingAccess, (bool), (override));
  MOCK_METHOD(void, WasOccluded, (bool), (override));
  MOCK_METHOD(void, SetIsOfflineWebComponent, (), (override));
  MOCK_METHOD(void, OnWindowShow, (), (override));
  MOCK_METHOD(void, OnWindowHide, (), (override));
  MOCK_METHOD(void, OnOnlineRenderToForeground, (), (override));
  MOCK_METHOD(void,
              SendTouchEventList,
              (const std::vector<CefTouchEvent>&),
              (override));
  MOCK_METHOD(CefRefPtr<CefBrowser>, GetBrowser, (), (override));
  MOCK_METHOD(void, CloseBrowser, (bool), (override));
  MOCK_METHOD(bool, TryCloseBrowser, (), (override));
  MOCK_METHOD(bool, IsReadyToBeClosed, (), (override));
  MOCK_METHOD(void, SetFocus, (bool), (override));
  MOCK_METHOD(CefWindowHandle, GetWindowHandle, (), (override));
  MOCK_METHOD(CefWindowHandle, GetOpenerWindowHandle, (), (override));
  MOCK_METHOD(int, GetOpenerIdentifier, (), (override));
  MOCK_METHOD(bool, HasView, (), (override));
  MOCK_METHOD(CefRefPtr<CefClient>, GetClient, (), (override));
  MOCK_METHOD(CefRefPtr<CefRequestContext>, GetRequestContext, (), (override));
  MOCK_METHOD(bool, CanZoom, (cef_zoom_command_t), (override));
  MOCK_METHOD(void, Zoom, (cef_zoom_command_t), (override));
  MOCK_METHOD(double, GetDefaultZoomLevel, (), (override));
  MOCK_METHOD(double, GetZoomLevel, (), (override));
  MOCK_METHOD(void, SetZoomLevel, (double), (override));
  MOCK_METHOD(void,
              RunFileDialog,
              (FileDialogMode,
               const CefString&,
               const CefString&,
               const std::vector<CefString>&,
               CefRefPtr<CefRunFileDialogCallback>),
              (override));
  MOCK_METHOD(void, StartDownload, (const CefString&), (override));
  MOCK_METHOD(void,
              DownloadImage,
              (const CefString&,
               bool,
               uint32_t,
               bool,
               CefRefPtr<CefDownloadImageCallback>),
              (override));
  MOCK_METHOD(void, Print, (), (override));
  MOCK_METHOD(void,
              PrintToPDF,
              (const CefString&,
               const CefPdfPrintSettings&,
               CefRefPtr<CefPdfPrintCallback>),
              (override));
  MOCK_METHOD(void,
              Find,
              (const CefString&,
               bool,
               bool,
               bool
#if BUILDFLAG(ARKWEB_FIND_IN_PAGE)
               ,
               bool
#endif
               ),
              (override));
  MOCK_METHOD(void, StopFinding, (bool), (override));
  MOCK_METHOD(void,
              ShowDevTools,
              (const CefWindowInfo&,
               CefRefPtr<CefClient>,
               const CefBrowserSettings&,
               const CefPoint&),
              (override));
  MOCK_METHOD(void, CloseDevTools, (), (override));
  MOCK_METHOD(bool, HasDevTools, (), (override));
  MOCK_METHOD(bool, SendDevToolsMessage, (const void*, size_t), (override));
  MOCK_METHOD(int,
              ExecuteDevToolsMethod,
              (int, const CefString&, CefRefPtr<CefDictionaryValue>),
              (override));
  MOCK_METHOD(CefRefPtr<CefRegistration>,
              AddDevToolsMessageObserver,
              (CefRefPtr<CefDevToolsMessageObserver>),
              (override));
  MOCK_METHOD(void,
              GetNavigationEntries,
              (CefRefPtr<CefNavigationEntryVisitor>, bool),
              (override));
  MOCK_METHOD(void, ReplaceMisspelling, (const CefString&), (override));
  MOCK_METHOD(void, AddWordToDictionary, (const CefString&), (override));
  MOCK_METHOD(bool, IsWindowRenderingDisabled, (), (override));
  MOCK_METHOD(void, WasResized, (), (override));
  MOCK_METHOD(void, WasHidden, (bool), (override));
  MOCK_METHOD(void, NotifyScreenInfoChanged, (), (override));
  MOCK_METHOD(void, Invalidate, (PaintElementType), (override));
  MOCK_METHOD(void, SendExternalBeginFrame, (), (override));
  MOCK_METHOD(void, SendKeyEvent, (const CefKeyEvent&), (override));
  MOCK_METHOD(void,
              SendMouseClickEvent,
              (const CefMouseEvent&, cef_mouse_button_type_t, bool, int),
              (override));
  MOCK_METHOD(void,
              SendMouseMoveEvent,
              (const CefMouseEvent&, bool),
              (override));
  MOCK_METHOD(void,
              SendMouseWheelEvent,
              (const CefMouseEvent&, int, int),
              (override));
  MOCK_METHOD(void, SendTouchEvent, (const CefTouchEvent&), (override));
  MOCK_METHOD(void, SendCaptureLostEvent, (), (override));
  MOCK_METHOD(void, NotifyMoveOrResizeStarted, (), (override));
  MOCK_METHOD(int, GetWindowlessFrameRate, (), (override));
  MOCK_METHOD(void, SetWindowlessFrameRate, (int), (override));
  MOCK_METHOD(void,
              ImeSetComposition,
              (const CefString&,
               const std::vector<CefCompositionUnderline>&,
               const CefRange&,
               const CefRange&),
              (override));
  MOCK_METHOD(void,
              ImeCommitText,
              (const CefString&, const CefRange&, int),
              (override));
  MOCK_METHOD(void, ImeFinishComposingText, (bool), (override));
  MOCK_METHOD(void, ImeCancelComposition, (), (override));
  MOCK_METHOD(void,
              DragTargetDragEnter,
              (CefRefPtr<CefDragData>,
               const CefMouseEvent&,
               DragOperationsMask),
              (override));
  MOCK_METHOD(void,
              DragTargetDragOver,
              (const CefMouseEvent&, DragOperationsMask),
              (override));
  MOCK_METHOD(void, DragTargetDragLeave, (), (override));
  MOCK_METHOD(void, DragTargetDrop, (const CefMouseEvent&), (override));
  MOCK_METHOD(void,
              DragSourceEndedAt,
              (int, int, DragOperationsMask),
              (override));
  MOCK_METHOD(void, DragSourceSystemDragEnded, (), (override));
  MOCK_METHOD(CefRefPtr<CefNavigationEntry>,
              GetVisibleNavigationEntry,
              (),
              (override));
  MOCK_METHOD(void, SetAccessibilityState, (cef_state_t), (override));
  MOCK_METHOD(void,
              SetAutoResizeEnabled,
              (bool, const CefSize&, const CefSize&),
              (override));
  MOCK_METHOD(void, SetAudioMuted, (bool), (override));
  MOCK_METHOD(bool, IsAudioMuted, (), (override));
  MOCK_METHOD(bool, IsFullscreen, (), (override));
  MOCK_METHOD(void, ExitFullscreen, (bool), (override));
  MOCK_METHOD(bool, CanExecuteChromeCommand, (int), (override));
  MOCK_METHOD(void,
              ExecuteChromeCommand,
              (int, cef_window_open_disposition_t),
              (override));
  MOCK_METHOD(bool, IsRenderProcessUnresponsive, (), (override));
  MOCK_METHOD(cef_runtime_style_t, GetRuntimeStyle, (), (override));
  MOCK_METHOD(void, SetPopupWindow, (cef_native_window_t), (override));
  MOCK_METHOD(void,
              CreateToPDF,
              (const CefPdfPrintSettings&, CefRefPtr<CefPdfValueCallback>),
              (override));
  MOCK_METHOD(void, EnableVideoAssistant, (bool), (override));
  MOCK_METHOD(void, EnableVideoAssistantAVCast, (bool), (override));
  MOCK_METHOD(void,
              ExecuteVideoAssistantFunction,
              (const CefString&),
              (override));
  MOCK_METHOD(bool, IsIframe, (), (override));
  MOCK_METHOD(void, ReloadFocusedFrame, (), (override));
  MOCK_METHOD(void, StopScreenCapture, (int32_t, const CefString&), (override));
  MOCK_METHOD(void, SetScreenCapturePickerShow, (), (override));
  MOCK_METHOD(void, DisableSessionReuse, (), (override));
  MOCK_METHOD(void,
              RegisterScreenCaptureDelegateListener,
              (CefRefPtr<CefScreenCaptureCallback>),
              (override));
  MOCK_METHOD(void, GetOverScrollOffsetValue, (float*, float*), (override));
  MOCK_METHOD(void, CustomWebMediaPlayer, (bool), (override));
  MOCK_METHOD(bool, GetWebDebuggingAccess, (), (override));
  MOCK_METHOD(void, GetImageForContextNode, (int), (override));
  MOCK_METHOD(void, GetImageFromCache, (const CefString&, int), (override));
  MOCK_METHOD(void, GetImageFromCacheEx, (const CefString&, int), (override));
  MOCK_METHOD(void, ExitFullScreen, (), (override));
  MOCK_METHOD(void, UpdateLocale, (const CefString&), (override));
  MOCK_METHOD(CefString, GetOriginalUrl, (), (override));
  MOCK_METHOD(void, PutNetworkAvailable, (bool), (override));
  MOCK_METHOD(int,
              PrerenderPage,
              (const CefString&, const CefString&),
              (override));
  MOCK_METHOD(void, CancelAllPrerendering, (), (override));
  MOCK_METHOD(void, RemoveCache, (bool), (override));
  MOCK_METHOD(void, PostTaskToUIThread, (CefRefPtr<CefTask>), (override));
  MOCK_METHOD(void, SetVirtualPixelRatio, (float), (override));
  MOCK_METHOD(float, GetVirtualPixelRatio, (), (override));
  MOCK_METHOD(void, SetWebPreferences, (const CefBrowserSettings&), (override));
  MOCK_METHOD(void, PutUserAgent, (const CefString&, bool), (override));
  MOCK_METHOD(CefString, DefaultUserAgent, (), (override));
  MOCK_METHOD(void, SetBackgroundColor, (int), (override));
  MOCK_METHOD(void, UpdateAdblockEasyListRules, (long), (override));
  MOCK_METHOD(void,
              RegisterArkJSfunction,
              (const CefString&,
               const std::vector<CefString>&,
               const std::vector<CefString>&,
               int32_t,
               const CefString&),
              (override));
  MOCK_METHOD(void,
              UnregisterArkJSfunction,
              (const CefString&, const std::vector<CefString>&),
              (override));
  MOCK_METHOD(void,
              CallH5Function,
              (int32_t,
               int32_t,
               const CefString&,
               const std::vector<CefRefPtr<CefValue>>&),
              (override));
  MOCK_METHOD(void,
              StoreWebArchive,
              (const CefString&,
               bool,
               CefRefPtr<CefStoreWebArchiveResultCallback>),
              (override));
  MOCK_METHOD(void, WasKeyboardResized, (), (override));
  MOCK_METHOD(void, SetEnableLowerFrameRate, (bool), (override));
  MOCK_METHOD(CefString, Title, (), (override));
  MOCK_METHOD(void,
              CreateWebMessagePorts,
              (std::vector<CefString>&),
              (override));
  MOCK_METHOD(void,
              PostWebMessage,
              (CefString&, std::vector<CefString>&, CefString&),
              (override));
  MOCK_METHOD(void, ClosePort, (const CefString&), (override));
  MOCK_METHOD(void, DestroyAllWebMessagePorts, (), (override));
  MOCK_METHOD(void,
              PostPortMessage,
              (const CefString&, CefRefPtr<CefValue>),
              (override));
  MOCK_METHOD(void,
              SetPortMessageCallback,
              (const CefString&, CefRefPtr<CefWebMessageReceiver>),
              (override));
  MOCK_METHOD(void, GetHitData, (int&, CefString&), (override));
  MOCK_METHOD(void, SetInitialScale, (float), (override));
  MOCK_METHOD(int, PageLoadProgress, (), (override));
  MOCK_METHOD(float, Scale, (), (override));
  MOCK_METHOD(void,
              LoadWithDataAndBaseUrl,
              (const CefString&,
               const CefString&,
               const CefString&,
               const CefString&,
               const CefString&),
              (override));
  MOCK_METHOD(void,
              LoadWithData,
              (const CefString&, const CefString&, const CefString&),
              (override));
  MOCK_METHOD(void,
              AddVisitedLinks,
              (const std::vector<CefString>&),
              (override));
  MOCK_METHOD(void,
              ResumeDownload,
              (const CefString& url,
               const CefString& full_path,
               int64_t received_bytes,
               int64_t total_bytes,
               const CefString& etag,
               const CefString& mime_type,
               const CefString& last_modified,
               const CefString& received_slices_string),
              (override));
  MOCK_METHOD(void, SetAudioResumeInterval, (int), (override));
  MOCK_METHOD(void, SetAudioExclusive, (bool), (override));
  MOCK_METHOD(void, SetAudioSessionType, (int), (override));
  MOCK_METHOD(void, CloseMedia, (), (override));
  MOCK_METHOD(void, StopMedia, (), (override));
  MOCK_METHOD(void, ResumeMedia, (), (override));
  MOCK_METHOD(void, PauseMedia, (), (override));
  MOCK_METHOD(int, GetMediaPlaybackState, (), (override));
  MOCK_METHOD(void, ScrollPageUpDown, (bool, bool, float), (override));
  MOCK_METHOD(CefRefPtr<CefBinaryValue>, GetWebState, (), (override));
  MOCK_METHOD(bool, RestoreWebState, (CefRefPtr<CefBinaryValue>), (override));
  MOCK_METHOD(void, ScrollTo, (float, float), (override));
  MOCK_METHOD(void, ScrollBy, (float, float), (override));
  MOCK_METHOD(void, SlideScroll, (float, float), (override));
  MOCK_METHOD(void, SetFileAccess, (bool), (override));
  MOCK_METHOD(void, SetBlockNetwork, (bool), (override));
  MOCK_METHOD(void, SetCacheMode, (int), (override));
  MOCK_METHOD(void, SetShouldFrameSubmissionBeforeDraw, (bool), (override));
  MOCK_METHOD(void, ZoomBy, (float, float, float), (override));
  MOCK_METHOD(void, SetWindowId, (int, int), (override));
  MOCK_METHOD(void, SetToken, (void*), (override));
  MOCK_METHOD(void,
              SetVirtualKeyBoardArg,
              (int32_t, int32_t, double),
              (override));
  MOCK_METHOD(bool, ShouldVirtualKeyboardOverlay, (), (override));
  MOCK_METHOD(void,
              JavaScriptOnDocumentStart,
              (const CefString&,
               const std::vector<CefString>&,
               (const std::vector<std::pair<CefString, CefString>>&),
               bool),
              (override));
  MOCK_METHOD(void, RemoveJavaScriptOnDocumentStart, (), (override));
  MOCK_METHOD(void,
              JavaScriptOnDocumentEnd,
              (const CefString&,
               const std::vector<CefString>&,
               (const std::vector<std::pair<CefString, CefString>>&),
               bool),
              (override));
  MOCK_METHOD(void, RemoveJavaScriptOnDocumentEnd, (), (override));
  MOCK_METHOD(void, OnDataDetectorSelectText, (), (override));
  MOCK_METHOD(std::string, GetDataDetectorSelectText, (), (override));
  MOCK_METHOD(void, SetBypassVsyncCondition, (int32_t), (override));
  MOCK_METHOD(void,
              JavaScriptOnHeadReady,
              (const CefString&,
               const std::vector<CefString>&,
               (const std::vector<std::pair<CefString, CefString>>&),
               bool),
              (override));
  MOCK_METHOD(void, RemoveJavaScriptOnHeadReady, (), (override));
  MOCK_METHOD(void, SetDrawRect, (int, int, int, int), (override));
  MOCK_METHOD(void, SetDrawMode, (int), (override));
  MOCK_METHOD(void,
              CreateWebPrintDocumentAdapter,
              (const CefString&, void**),
              (override));
  MOCK_METHOD(void,
              CreateWebPrintDocumentAdapterV2,
              (const CefString&, void**),
              (override));
  MOCK_METHOD(void, SetOverscrollMode, (int), (override));
  MOCK_METHOD(bool, Discard, (), (override));
  MOCK_METHOD(bool, Restore, (), (override));
  MOCK_METHOD(void, SetBrowserZoomLevel, (double), (override));
  MOCK_METHOD(int, GetTopControlsOffset, (), (override));
  MOCK_METHOD(int, GetShrinkViewportHeight, (), (override));
  MOCK_METHOD(void, OnEyeDropperResult, (bool, uint32_t), (override));
  MOCK_METHOD(void, SetPrintBackground, (bool), (override));
  MOCK_METHOD(bool, GetPrintBackground, (), (override));
#if BUILDFLAG(ARKWEB_INPUT_EVENTS)
  MOCK_METHOD(void, SetScrollable, (bool, int), (override));
  MOCK_METHOD(void, SetImeShow, (bool), (override));
  MOCK_METHOD(bool, IsElementExist, (CefString&), (override));
#endif
  MOCK_METHOD(void, StartCamera, (), (override));
  MOCK_METHOD(void, StopCamera, (), (override));
  MOCK_METHOD(void, CloseCamera, (), (override));
  MOCK_METHOD(CefString, GetLastJavascriptProxyCallingFrameUrl, (), (override));
  MOCK_METHOD(void, SetNWebId, (int), (override));
  MOCK_METHOD(bool, GetPendingSizeStatus, (), (override));
  MOCK_METHOD(void,
              PrecompileJavaScript,
              (const std::string&,
               const std::string&,
               CefRefPtr<CefCacheOptions>,
               CefRefPtr<CefPrecompileCallback>),
              (override));
  MOCK_METHOD(void,
              SetWakeLockHandler,
              (int32_t, CefRefPtr<CefSetLockCallback>),
              (override));
  MOCK_METHOD(CefRefPtr<CefDownloadItem>,
              GetDownloadItem,
              (uint32_t),
              (override));
  MOCK_METHOD(bool, NeedsReload, (), (override));
  MOCK_METHOD(bool, TerminateRenderProcess, (), (override));
  MOCK_METHOD(void,
              RegisterNativeJSProxy,
              (const CefString&,
               const std::vector<CefString>&,
               int32_t,
               bool,
               const CefString&),
              (override));
  MOCK_METHOD(void,
              SendTouchpadFlingEvent,
              (const CefMouseEvent&, double, double),
              (override));
  MOCK_METHOD(void, SendCancelFlingEvent, (const CefMouseEvent&), (override));
  MOCK_METHOD(void, SetFitContentMode, (int), (override));
  MOCK_METHOD(void, UpdateDrawRect, (), (override));
  MOCK_METHOD(void, OnTextSelected, (bool), (override));
  MOCK_METHOD(float, GetPageScaleFactor, (), (override));
  MOCK_METHOD(bool,
              WebPageSnapshot,
              (const char*, int, int, cef_web_snapshot_callback_t),
              (override));
  MOCK_METHOD(void, AdvanceFocusForIME, (int), (override));
  MOCK_METHOD(void, ScrollToWithAnime, (float, float, int32_t), (override));
  MOCK_METHOD(void, ScrollByWithAnime, (float, float, int32_t), (override));
  MOCK_METHOD(void, GetScrollOffset, (float*, float*), (override));
  MOCK_METHOD(void, GetOverScrollOffset, (float*, float*), (override));
  MOCK_METHOD(void, OnSafeInsetsChange, (int, int, int, int), (override));
  MOCK_METHOD(void, NotifyForNextTouchEvent, (), (override));
  MOCK_METHOD(void,
              SetGrantFileAccessDirs,
              (const std::vector<CefString>&, const std::vector<CefString>&),
              (override));
  MOCK_METHOD(void,
              SetAutofillCallback,
              (CefRefPtr<CefWebMessageReceiver>),
              (override));
  MOCK_METHOD(void, FillAutofillData, (CefRefPtr<CefValue>), (override));
  MOCK_METHOD(void, ScrollFocusedEditableNodeIntoView, (), (override));
  MOCK_METHOD(void, ProcessAutofillCancel, (const CefString&), (override));
  MOCK_METHOD(void,
              AutoFillWithIMFEvent,
              (bool, bool, bool, const CefString&),
              (override));

  // Required pure virtual methods from CefBrowser (via ArkWebBrowserHostExt
  // inheritance chain)
  MOCK_METHOD(bool, IsValid, (), (override));
  MOCK_METHOD(bool, CanGoBack, (), (override));
  MOCK_METHOD(void, GoBack, (), (override));
  MOCK_METHOD(bool, CanGoForward, (), (override));
  MOCK_METHOD(void, GoForward, (), (override));
  MOCK_METHOD(bool, IsLoading, (), (override));
  MOCK_METHOD(void, Reload, (), (override));
  MOCK_METHOD(void, ReloadIgnoreCache, (), (override));
  MOCK_METHOD(void, StopLoad, (), (override));
  MOCK_METHOD(int, GetIdentifier, (), (override));
  MOCK_METHOD(bool, IsSame, (CefRefPtr<CefBrowser>), (override));
  MOCK_METHOD(bool, IsPopup, (), (override));
  MOCK_METHOD(bool, HasDocument, (), (override));
  MOCK_METHOD(CefRefPtr<CefFrame>, GetMainFrame, (), (override));
  MOCK_METHOD(CefRefPtr<CefFrame>, GetFocusedFrame, (), (override));
  MOCK_METHOD(CefRefPtr<CefFrame>,
              GetFrameByIdentifier,
              (const CefString&),
              (override));
  MOCK_METHOD(CefRefPtr<CefFrame>,
              GetFrameByName,
              (const CefString&),
              (override));
  MOCK_METHOD(size_t, GetFrameCount, (), (override));
  MOCK_METHOD(void, GetFrameIdentifiers, (std::vector<CefString>&), (override));
  MOCK_METHOD(void, GetFrameNames, (std::vector<CefString>&), (override));
  MOCK_METHOD(bool, NeedToFireBeforeUnloadOrUnloadEvents, (), (override));
  MOCK_METHOD(void, DispatchBeforeUnload, (), (override));

  // Required pure virtual methods from ArkWebBrowserHostExt
  MOCK_METHOD(CefRefPtr<CefBrowserPermissionRequestDelegate>,
              GetPermissionRequestDelegate,
              (),
              (override));
  MOCK_METHOD(CefRefPtr<CefGeolocationAcess>,
              GetGeolocationPermissions,
              (),
              (override));
  MOCK_METHOD(bool, CanGoBackOrForward, (int), (override));
  MOCK_METHOD(void, GoBackOrForward, (int), (override));
  MOCK_METHOD(void, DeleteHistory, (), (override));
  MOCK_METHOD(void, PasswordSuggestionSelected, (int), (override));
  MOCK_METHOD(void, UpdateBrowserControlsState, (int, int, bool), (override));
  MOCK_METHOD(void, UpdateBrowserControlsHeight, (int, bool), (override));
  MOCK_METHOD(void,
              PrefetchPage,
              (const OHOS::NWeb::PrefetchOptions&),
              (override));
  MOCK_METHOD(void, ReloadOriginalUrl, (), (override));
  MOCK_METHOD(bool, CanStoreWebArchive, (), (override));
  MOCK_METHOD(void, SetBrowserUserAgentString, (const CefString&), (override));
  MOCK_METHOD(bool, ShouldShowLoadingUI, (), (override));
  MOCK_METHOD(void, SetForceEnableZoom, (bool), (override));
  MOCK_METHOD(bool, GetForceEnableZoom, (), (override));
  MOCK_METHOD(int, GetNWebId, (), (override));
  MOCK_METHOD(bool, GetSavePasswordAutomatically, (), (override));
  MOCK_METHOD(void, SetSavePasswordAutomatically, (bool), (override));
  MOCK_METHOD(void, SaveOrUpdatePassword, (bool), (override));
  MOCK_METHOD(bool, GetSavePassword, (), (override));
  MOCK_METHOD(void, SetSavePassword, (bool), (override));
  MOCK_METHOD(int, GetSecurityLevel, (), (override));
  MOCK_METHOD(void, EnableSafeBrowsing, (bool), (override));
  MOCK_METHOD(bool, IsSafeBrowsingEnabled, (), (override));
  MOCK_METHOD(void, EnableIntelligentTrackingPrevention, (bool), (override));
  MOCK_METHOD(bool, IsIntelligentTrackingPreventionEnabled, (), (override));
  MOCK_METHOD(bool, IsAdsBlockEnabled, (), (override));
  MOCK_METHOD(bool, IsAdsBlockEnabledForCurPage, (), (override));
  MOCK_METHOD(void, EnableAdsBlock, (bool), (override));
  MOCK_METHOD(int,
              SetUrlTrustListWithErrMsg,
              (const CefString&, bool, bool, CefString&),
              (override));
  MOCK_METHOD(void, EnableSafeBrowsingDetection, (bool, bool), (override));
  MOCK_METHOD(void,
              OnSafeBrowsingDetectionResult,
              (const SafeBrowsingDetectionResult& safeBrowsingDetectionResult),
              (override));
#if BUILDFLAG(ARKWEB_EXT_NAVIGATION)
  MOCK_METHOD(int, InsertBackForwardEntry, (int, const CefString&), (override));
  MOCK_METHOD(int,
              UpdateNavigationEntryUrl,
              (int, const CefString&),
              (override));
  MOCK_METHOD(void, ClearForwardList, (), (override));
#endif
  MOCK_METHOD(void, ExtensionSetTabId, (int), (override));
  MOCK_METHOD(int, ExtensionGetTabId, (), (override));
  MOCK_METHOD(uint32_t, GetAcceleratedWidget, (bool), (override));
  MOCK_METHOD(void, SetAdBlockEnabledForSite, (bool, int), (override));
  MOCK_METHOD(void,
              FindEx,
              (const CefString&, bool, bool, bool, bool),
              (override));
  MOCK_METHOD(void, SetFocusOnWeb, (), (override));
  MOCK_METHOD(void, UpdateSecurityLayer, (bool), (override));
  MOCK_METHOD(void, UpdateTextFieldStatus, (bool, bool), (override));
  MOCK_METHOD(CefString, GetCustomUserAgent, (), (override));
  MOCK_METHOD(void, GetLastHitData, (int&, CefString&), (override));
  MOCK_METHOD(std::string, GetSelectedTextFromContextParam, (), (override));
  MOCK_METHOD(bool, JudgeTextInputState, (), (override));
  MOCK_METHOD(void, SetNeedsReload, (bool), (override));
  MOCK_METHOD(void, SetOptimizeParserBudgetEnabled, (bool), (override));
  MOCK_METHOD(void, OnDestroyImageAnalyzerOverlay, (), (override));
  MOCK_METHOD(void, OnFoldStatusChanged, (uint32_t), (override));
  MOCK_METHOD(void, SetNativeEmbedMode, (bool), (override));
  MOCK_METHOD(void, SetNativeInnerWeb, (bool), (override));
  MOCK_METHOD(void,
              ScaleGestureChangeV2,
              (int, float, float, float, float),
              (override));
  MOCK_METHOD(std::string, GetCurrentLanguage, (), (override));
  MOCK_METHOD(void, MaximizeResize, (), (override));
  MOCK_METHOD(void, PutWebMediaAVSessionEnabled, (bool), (override));
  MOCK_METHOD(void, SetEnableHalfFrameRate, (bool), (override));
  MOCK_METHOD(bool, SetFocusByPosition, (float, float), (override));
  MOCK_METHOD(int32_t,
              ExecuteInputCommand,
              (const std::string&, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(int32_t,
              ExecuteSelectCommand,
              (const std::string&,
               const std::vector<int32_t>&,
               const std::vector<std::string>&),
              (override));
  MOCK_METHOD(void,
              SetPipNativeWindow,
              (int, int, int, cef_native_window_t),
              (override));
  MOCK_METHOD(void, SendPipEvent, (int, int, int, int), (override));
  MOCK_METHOD(void, ShowFreeCopyMenu, (), (override));
  MOCK_METHOD(bool, ShouldShowFreeCopyMenu, (), (override));
  MOCK_METHOD(void, SetBackForwardCacheOptions, (int32_t, int32_t), (override));
#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
  MOCK_METHOD(
      void,
      SetBlankScreenDetectionConfig,
      (bool, const std::vector<double>&, const std::vector<int32_t>&, int32_t),
      (override));
#endif
#if BUILDFLAG(ARKWEB_BGTASK)
  MOCK_METHOD(void, OnBrowserForeground, (), (override));
  MOCK_METHOD(void, OnBrowserBackground, (), (override));
#endif
  MOCK_METHOD(void,
              RunJavaScriptInFrames,
              (const std::string&,
               FrameInfos,
               bool,
               IsolatedWorld,
               CefRefPtr<CefJavaScriptResultCallback>),
              (override));
#if BUILDFLAG(ARKWEB_NWEB_EX)
  MOCK_METHOD(void,
              GetAllFrameInfos,
              (CefRefPtr<CefFrameInfosCallback>),
              (override));
  MOCK_METHOD(void,
              GetLastJavaScriptProxyCallingFrameInfo,
              (CefRefPtr<CefLastJavaScriptProxyCallingFrameInfoCallback>),
              (override));
#endif
#if BUILDFLAG(ARKWEB_AUTOLAYOUT)
  MOCK_METHOD(void, SetPageVisibility, (bool), (override));
#endif
#if BUILDFLAG(ARKWEB_READER_MODE)
  MOCK_METHOD(void, EnableReaderMode, (bool enabled), (override));
  MOCK_METHOD(void,
              Distill,
              (uint64_t, const DistillOptions&, CefRefPtr<CefDistillCallback>),
              (override));
  MOCK_METHOD(void, AbortDistill, (), (override));
#endif
#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
  MOCK_METHOD(void, GetFocusedFrameInfo, (int32_t&, CefString&), (override));
#endif
#if BUILDFLAG(ARKWEB_EXT_HTTPS_UPGRADES)
  MOCK_METHOD(void,
              LoadUrlWithParams,
              (const std::string&,
               const LoadUrlType&,
               const std::string&,
               const std::string&,
               const std::string&,
               const bool&,
               int32_t),
              (override));
  MOCK_METHOD(void, EnableHttpsUpgrades, (bool), (override));
#endif
#if BUILDFLAG(ARKWEB_DEVTOOLS)
  MOCK_METHOD(void,
              ShowDevToolsWith,
              (CefRefPtr<ArkWebBrowserHostExt>,
               CefRefPtr<CefDevToolsMessageHandlerDelegate>,
               const CefPoint&),
              (override));
  MOCK_METHOD(void,
              ShowDevToolsWithByPb,
              (CefRefPtr<ArkWebBrowserHostExt>,
               CefRefPtr<CefDevToolsMessageHandlerDelegate>,
               const CefPoint&,
               const CefOpenDevToolsExtOpt&),
              (override));
#endif
#if BUILDFLAG(ARKWEB_UNITTESTS)
  MOCK_METHOD(void, SetMediaResumeFromBFCachePage, (bool), (override));
  MOCK_METHOD(void, SetHasComposition, (bool), (override));
  MOCK_METHOD(bool, GetHasComposition, (), (override));
  MOCK_METHOD(void, SetEnableCustomVideoPlayer, (bool), (override));
  MOCK_METHOD(void, EnableAppLinking, (bool), (override));
  MOCK_METHOD(bool, IsAppLinkingEnabled, (), (const, override));
#endif
  MOCK_METHOD(int32_t, GetLastCommittedEntryPageTransition, (), (override));
#if BUILDFLAG(ARKWEB_SCREEN_OFFSET)
  MOCK_METHOD(void, NotifyScreenInfoChangedV2, (), (override));
#endif  // BUILDFLAG(ARKWEB_SCREEN_OFFSET)
#if BUILDFLAG(ARKWEB_DEVTOOLS)
  MOCK_METHOD(void, ViewSource, (), (override));
  MOCK_METHOD(bool, IsViewSourceEnabled, (), (override));
#endif

  IMPLEMENT_REFCOUNTING(MockBrowserHost);
};

}  // namespace test
}  // namespace OHOS::NWeb

#endif  // OHOS_NWEB_SRC_CEF_DELEGATE_TEST_MOCK_BROWSER_HOST_H_
