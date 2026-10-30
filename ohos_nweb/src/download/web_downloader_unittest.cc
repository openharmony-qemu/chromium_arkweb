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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>

#include "arkweb/build/features/features.h"
#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#include "ohos_nweb/src/capi/nweb_c_api.h"
#include "ohos_nweb/src/capi/nweb_download_delegate_callback.h"
#include "ohos_nweb/src/capi/nweb_download_item.h"

// These functions are implemented in web_downloader.cc but are not declared in
// nweb_c_api.h. Declare them here so the tests can exercise the null /
// with-value branches of the C API wrappers.
extern "C" {
void WebDownloader_PutDownloadCallback(int32_t nweb_id,
                                       NWebDownloadDelegateCallback* callback);
void WebDownloader_ResumeDownload(int32_t nweb_id,
                                  const NWebDownloadItem* download_item);
}

using namespace testing;

namespace {

// Invalid nweb id that never maps to a registered NWebImpl, so
// NWebImpl::FromID() returns nullptr and the "no nweb" branches are covered.
const int32_t kInvalidNWebId = 0x7fffff00;

// Global callback invoked by the delegate callback functors set through
// WebDownloader_SetDownloadBeforeStart / WebDownloader_SetDownloadDidUpdate.
bool g_before_start_called = false;
bool g_did_update_called = false;

void OnBeforeStart(NWebDownloadItem* /*item*/,
                   WebBeforeDownloadCallbackWrapper* /*wrapper*/) {
  g_before_start_called = true;
}

void OnDidUpdate(NWebDownloadItem* /*item*/,
                 WebDownloadItemCallbackWrapper* /*wrapper*/) {
  g_did_update_called = true;
}

NWebDownloadItem* CreateItem() {
  NWebDownloadItem* item = nullptr;
  WebDownloadItem_CreateWebDownloadItem(&item);
  return item;
}

}  // namespace

class WebDownloaderTest : public testing::Test {
 protected:
  void SetUp() override {
    g_before_start_called = false;
    g_did_update_called = false;
  }
};

// WebDownloader_PutDownloadCallback: nweb lookup failure returns early.
TEST_F(WebDownloaderTest, PutDownloadCallback_NoNWeb_DoesNotCrash) {
  NWebDownloadDelegateCallback callback = {};
  EXPECT_NO_FATAL_FAILURE(
      WebDownloader_PutDownloadCallback(kInvalidNWebId, &callback));
  EXPECT_NO_FATAL_FAILURE(
      WebDownloader_PutDownloadCallback(kInvalidNWebId, nullptr));
}

// WebDownloader_ResumeDownload: nweb lookup failure returns early.
TEST_F(WebDownloaderTest, ResumeDownload_NoNWeb_DoesNotCrash) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloader_ResumeDownload(kInvalidNWebId, item));
  WebDownloadItem_Destroy(item);
  EXPECT_NO_FATAL_FAILURE(
      WebDownloader_ResumeDownload(kInvalidNWebId, nullptr));
}

// WebDownloader_ResumeDownloadStatic: null item returns early; non-null item
// flows through (no NWebImpl lookup required).
TEST_F(WebDownloaderTest, ResumeDownloadStatic_NullItem_ReturnsEarly) {
  EXPECT_NO_FATAL_FAILURE(WebDownloader_ResumeDownloadStatic(nullptr));
}

TEST_F(WebDownloaderTest, ResumeDownloadStatic_WithItem_DoesNotCrash) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloader_ResumeDownloadStatic(item));
  WebDownloadItem_Destroy(item);
}

// WebDownloader_CreateDownloadDelegateCallback.
TEST_F(WebDownloaderTest, CreateDownloadDelegateCallback_NullOut_NoOp) {
  EXPECT_NO_FATAL_FAILURE(
      WebDownloader_CreateDownloadDelegateCallback(nullptr));
}

TEST_F(WebDownloaderTest, CreateDownloadDelegateCallback_WithOut_Allocates) {
  NWebDownloadDelegateCallback* callback = nullptr;
  WebDownloader_CreateDownloadDelegateCallback(&callback);
  ASSERT_TRUE(callback != nullptr);
  delete callback;
}

// WebDownloader_SetDownloadBeforeStart / SetDownloadDidUpdate.
TEST_F(WebDownloaderTest, SetDownloadBeforeStart_NullCallback_NoOp) {
  EXPECT_NO_FATAL_FAILURE(
      WebDownloader_SetDownloadBeforeStart(nullptr, OnBeforeStart));
}

TEST_F(WebDownloaderTest, SetDownloadBeforeStart_WithCallback_SetsFunctor) {
  NWebDownloadDelegateCallback callback = {};
  WebDownloader_SetDownloadBeforeStart(&callback, OnBeforeStart);
  ASSERT_TRUE(callback.downloadBeforeStart != nullptr);
  NWebDownloadItem* item = CreateItem();
  callback.downloadBeforeStart(item, nullptr);
  EXPECT_TRUE(g_before_start_called);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetDownloadDidUpdate_NullCallback_NoOp) {
  EXPECT_NO_FATAL_FAILURE(
      WebDownloader_SetDownloadDidUpdate(nullptr, OnDidUpdate));
}

TEST_F(WebDownloaderTest, SetDownloadDidUpdate_WithCallback_SetsFunctor) {
  NWebDownloadDelegateCallback callback = {};
  WebDownloader_SetDownloadDidUpdate(&callback, OnDidUpdate);
  ASSERT_TRUE(callback.downloadDidUpdate != nullptr);
  NWebDownloadItem* item = CreateItem();
  callback.downloadDidUpdate(item, nullptr);
  EXPECT_TRUE(g_did_update_called);
  WebDownloadItem_Destroy(item);
}

// WebDownload_Continue: null wrapper no-op; wrapper with null callback safe.
TEST_F(WebDownloaderTest, Continue_NullWrapper_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownload_Continue(nullptr, "/tmp/down.bin"));
}

TEST_F(WebDownloaderTest, Continue_WithWrapper_NullCallback_Safe) {
  NWebBeforeDownloadCallbackWrapper wrapper;
  EXPECT_NO_FATAL_FAILURE(WebDownload_Continue(&wrapper, "/tmp/down.bin"));
}

// WebDownload_CancelBeforeDownload / PauseBeforeDownload / ResumeBeforeDownload
// (gated by ARKWEB_EX_DOWNLOAD, enabled in this build).
TEST_F(WebDownloaderTest, CancelBeforeDownload_NullWrapper_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownload_CancelBeforeDownload(nullptr));
}

TEST_F(WebDownloaderTest, CancelBeforeDownload_WithWrapper_NullCallback_Safe) {
  NWebBeforeDownloadCallbackWrapper wrapper;
  EXPECT_NO_FATAL_FAILURE(WebDownload_CancelBeforeDownload(&wrapper));
}

TEST_F(WebDownloaderTest, PauseBeforeDownload_NullWrapper_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownload_PauseBeforeDownload(nullptr));
}

TEST_F(WebDownloaderTest, PauseBeforeDownload_WithWrapper_NullCallback_Safe) {
  NWebBeforeDownloadCallbackWrapper wrapper;
  EXPECT_NO_FATAL_FAILURE(WebDownload_PauseBeforeDownload(&wrapper));
}

TEST_F(WebDownloaderTest, ResumeBeforeDownload_NullWrapper_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownload_ResumeBeforeDownload(nullptr));
}

TEST_F(WebDownloaderTest, ResumeBeforeDownload_WithWrapper_NullCallback_Safe) {
  NWebBeforeDownloadCallbackWrapper wrapper;
  EXPECT_NO_FATAL_FAILURE(WebDownload_ResumeBeforeDownload(&wrapper));
}

// WebDownload_Cancel / Pause / Resume.
TEST_F(WebDownloaderTest, Cancel_NullWrapper_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownload_Cancel(nullptr));
}

TEST_F(WebDownloaderTest, Cancel_WithWrapper_NullCallback_Safe) {
  NWebDownloadItemCallbackWrapper wrapper;
  EXPECT_NO_FATAL_FAILURE(WebDownload_Cancel(&wrapper));
}

TEST_F(WebDownloaderTest, Pause_NullWrapper_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownload_Pause(nullptr));
}

TEST_F(WebDownloaderTest, Pause_WithWrapper_NullCallback_Safe) {
  NWebDownloadItemCallbackWrapper wrapper;
  EXPECT_NO_FATAL_FAILURE(WebDownload_Pause(&wrapper));
}

TEST_F(WebDownloaderTest, Resume_NullWrapper_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownload_Resume(nullptr));
}

TEST_F(WebDownloaderTest, Resume_WithWrapper_NullCallback_Safe) {
  NWebDownloadItemCallbackWrapper wrapper;
  EXPECT_NO_FATAL_FAILURE(WebDownload_Resume(&wrapper));
}

// WebDownload_GetItemState: nweb lookup failure returns MAX_DOWNLOAD_STATE.
TEST_F(WebDownloaderTest, GetItemState_NoNWeb_ReturnsMaxState) {
  EXPECT_EQ(WebDownload_GetItemState(kInvalidNWebId, 1L),
            NWebDownloadItemState::MAX_DOWNLOAD_STATE);
}

// WebDownload_GetItemStateByGuidV2: null guid returns MAX_DOWNLOAD_STATE.
TEST_F(WebDownloaderTest, GetItemStateByGuidV2_NullGuid_ReturnsMaxState) {
  EXPECT_EQ(WebDownload_GetItemStateByGuidV2(nullptr),
            NWebDownloadItemState::MAX_DOWNLOAD_STATE);
}

// GetItemStateByGuidV2 with a non-null guid flows into the static lookup.
TEST_F(WebDownloaderTest, GetItemStateByGuidV2_WithGuid_DoesNotCrash) {
  EXPECT_NO_FATAL_FAILURE(WebDownload_GetItemStateByGuidV2("unknown-guid-1"));
}

// WebDownload_GetItemStateByGuid: static lookup by guid. With an unknown guid
// the download manager reports MAX_DOWNLOAD_STATE (no active download).
TEST_F(WebDownloaderTest, GetItemStateByGuid_UnknownGuid_DoesNotCrash) {
  EXPECT_NO_FATAL_FAILURE(
      WebDownload_GetItemStateByGuid(std::string("unknown-guid-1")));
}

// WebDownloadItem_CreateWebDownloadItem / Destroy.
TEST_F(WebDownloaderTest, CreateWebDownloadItem_NullOut_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_CreateWebDownloadItem(nullptr));
}

TEST_F(WebDownloaderTest, CreateWebDownloadItem_WithOut_Allocates) {
  NWebDownloadItem* item = nullptr;
  WebDownloadItem_CreateWebDownloadItem(&item);
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, Destroy_NullItem_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_Destroy(nullptr));
}

// WebDownloadItem_Set* string setters: null item / null value no-op, with
// values populate the struct fields.
TEST_F(WebDownloaderTest, SetFullPath_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetFullPath(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetFullPath(nullptr, "/tmp/a.bin"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetFullPath_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetFullPath(item, "/tmp/a.bin");
  ASSERT_TRUE(item->full_path != nullptr);
  EXPECT_STREQ(item->full_path, "/tmp/a.bin");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetReceivedBytes_NullItem_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetReceivedBytes(nullptr, 100));
}

TEST_F(WebDownloaderTest, SetReceivedBytes_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetReceivedBytes(item, 100);
  EXPECT_EQ(item->received_bytes, 100);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetTotalBytes_NullItem_NoOp) {
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetTotalBytes(nullptr, 1000));
}

TEST_F(WebDownloaderTest, SetTotalBytes_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetTotalBytes(item, 1000);
  EXPECT_EQ(item->total_bytes, 1000);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetReceivedSlices_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetReceivedSlices(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetReceivedSlices(nullptr, "0-10"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetReceivedSlices_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetReceivedSlices(item, "0-10");
  ASSERT_TRUE(item->received_slices != nullptr);
  EXPECT_STREQ(item->received_slices, "0-10");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetETag_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetETag(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetETag(nullptr, "etag-1"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetETag_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetETag(item, "etag-1");
  ASSERT_TRUE(item->etag != nullptr);
  EXPECT_STREQ(item->etag, "etag-1");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetLastModified_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetLastModified(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(
      WebDownloadItem_SetLastModified(nullptr, "2026-01-01"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetLastModified_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetLastModified(item, "2026-01-01");
  ASSERT_TRUE(item->last_modified != nullptr);
  EXPECT_STREQ(item->last_modified, "2026-01-01");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetMimeType_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetMimeType(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(
      WebDownloadItem_SetMimeType(nullptr, "application/zip"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetMimeType_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetMimeType(item, "application/zip");
  ASSERT_TRUE(item->mime_type != nullptr);
  EXPECT_STREQ(item->mime_type, "application/zip");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetUrl_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetUrl(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetUrl(nullptr, "https://a.b/c"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetUrl_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetUrl(item, "https://a.b/c");
  ASSERT_TRUE(item->url != nullptr);
  EXPECT_STREQ(item->url, "https://a.b/c");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetOriginalUrl_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetOriginalUrl(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(
      WebDownloadItem_SetOriginalUrl(nullptr, "https://a.b/o"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetOriginalUrl_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetOriginalUrl(item, "https://a.b/o");
  ASSERT_TRUE(item->original_url != nullptr);
  EXPECT_STREQ(item->original_url, "https://a.b/o");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetReferrerUrl_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetReferrerUrl(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(
      WebDownloadItem_SetReferrerUrl(nullptr, "https://a.b/r"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetReferrerUrl_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetReferrerUrl(item, "https://a.b/r");
  ASSERT_TRUE(item->referrer_url != nullptr);
  EXPECT_STREQ(item->referrer_url, "https://a.b/r");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetGuid_NullItemOrValue_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetGuid(item, nullptr));
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetGuid(nullptr, "guid-1"));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetGuid_WithValue_SetsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetGuid(item, "guid-1");
  ASSERT_TRUE(item->guid != nullptr);
  EXPECT_STREQ(item->guid, "guid-1");
  WebDownloadItem_Destroy(item);
}

// WebDownloadItem_SetUrlChain.
TEST_F(WebDownloaderTest, SetUrlChain_NullItem_NoOp) {
  char* chain[] = {const_cast<char*>("https://a.b/1")};
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetUrlChain(nullptr, chain, 1));
}

TEST_F(WebDownloaderTest, SetUrlChain_NullChainOrNonPositiveSize_NoOp) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetUrlChain(item, nullptr, 1));
  EXPECT_NO_FATAL_FAILURE(WebDownloadItem_SetUrlChain(item, nullptr, 0));
  EXPECT_EQ(item->url_chain_size, 0);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SetUrlChain_WithValues_SetsChain) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  char* chain[] = {const_cast<char*>("https://a.b/1"), nullptr,
                   const_cast<char*>("https://a.b/3")};
  WebDownloadItem_SetUrlChain(item, chain, 3);
  ASSERT_TRUE(item->url_chain != nullptr);
  EXPECT_EQ(item->url_chain_size, 3);
  EXPECT_STREQ(item->url_chain[0], "https://a.b/1");
  EXPECT_EQ(item->url_chain[1], nullptr);
  EXPECT_STREQ(item->url_chain[2], "https://a.b/3");
  WebDownloadItem_Destroy(item);
}

// WebDownloadItem_Get* getters: null item returns default, non-null returns
// the struct field value.
TEST_F(WebDownloaderTest, Guid_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_Guid(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, Guid_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetGuid(item, "guid-1");
  EXPECT_STREQ(WebDownloadItem_Guid(item), "guid-1");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, GetDownloadItemId_NullItem_ReturnsMinusOne) {
  EXPECT_EQ(WebDownloadItem_GetDownloadItemId(nullptr), -1L);
}

TEST_F(WebDownloaderTest, GetDownloadItemId_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->download_item_id = 42;
  EXPECT_EQ(WebDownloadItem_GetDownloadItemId(item), 42L);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, GetState_NullItem_ReturnsMaxState) {
  EXPECT_EQ(WebDownloadItem_GetState(nullptr),
            NWebDownloadItemState::MAX_DOWNLOAD_STATE);
}

TEST_F(WebDownloaderTest, GetState_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->state = NWebDownloadItemState::COMPLETE;
  EXPECT_EQ(WebDownloadItem_GetState(item), NWebDownloadItemState::COMPLETE);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, CurrentSpeed_NullItem_ReturnsZero) {
  EXPECT_EQ(WebDownloadItem_CurrentSpeed(nullptr), 0);
}

TEST_F(WebDownloaderTest, CurrentSpeed_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->current_speed = 2048;
  EXPECT_EQ(WebDownloadItem_CurrentSpeed(item), 2048);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, PercentComplete_NullItem_ReturnsZero) {
  EXPECT_EQ(WebDownloadItem_PercentComplete(nullptr), 0);
}

TEST_F(WebDownloaderTest, PercentComplete_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->percent_complete = 50;
  EXPECT_EQ(WebDownloadItem_PercentComplete(item), 50);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, TotalBytes_NullItem_ReturnsZero) {
  EXPECT_EQ(WebDownloadItem_TotalBytes(nullptr), 0);
}

TEST_F(WebDownloaderTest, TotalBytes_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->total_bytes = 4096;
  EXPECT_EQ(WebDownloadItem_TotalBytes(item), 4096);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, ReceivedBytes_NullItem_ReturnsZero) {
  EXPECT_EQ(WebDownloadItem_ReceivedBytes(nullptr), 0);
}

TEST_F(WebDownloaderTest, ReceivedBytes_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->received_bytes = 1024;
  EXPECT_EQ(WebDownloadItem_ReceivedBytes(item), 1024);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, FullPath_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_FullPath(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, FullPath_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetFullPath(item, "/tmp/a.bin");
  EXPECT_STREQ(WebDownloadItem_FullPath(item), "/tmp/a.bin");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, Url_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_Url(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, Url_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetUrl(item, "https://a.b/c");
  EXPECT_STREQ(WebDownloadItem_Url(item), "https://a.b/c");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, OriginalUrl_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_OriginalUrl(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, OriginalUrl_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetOriginalUrl(item, "https://a.b/o");
  EXPECT_STREQ(WebDownloadItem_OriginalUrl(item), "https://a.b/o");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, ReferrerUrl_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_ReferrerUrl(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, ReferrerUrl_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetReferrerUrl(item, "https://a.b/r");
  EXPECT_STREQ(WebDownloadItem_ReferrerUrl(item), "https://a.b/r");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, UrlChain_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_UrlChain(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, UrlChain_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  char* chain[] = {const_cast<char*>("https://a.b/1")};
  WebDownloadItem_SetUrlChain(item, chain, 1);
  ASSERT_TRUE(WebDownloadItem_UrlChain(item) != nullptr);
  EXPECT_STREQ(WebDownloadItem_UrlChain(item)[0], "https://a.b/1");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, UrlChainSize_NullItem_ReturnsZero) {
  EXPECT_EQ(WebDownloadItem_UrlChainSize(nullptr), 0);
}

TEST_F(WebDownloaderTest, UrlChainSize_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  char* chain[] = {const_cast<char*>("https://a.b/1")};
  WebDownloadItem_SetUrlChain(item, chain, 1);
  EXPECT_EQ(WebDownloadItem_UrlChainSize(item), 1);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, SuggestedFileName_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_SuggestedFileName(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, SuggestedFileName_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->suggested_file_name = strdup("file.bin");
  EXPECT_STREQ(WebDownloadItem_SuggestedFileName(item), "file.bin");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, ContentDisposition_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_ContentDisposition(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, ContentDisposition_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->content_disposition = strdup("attachment");
  EXPECT_STREQ(WebDownloadItem_ContentDisposition(item), "attachment");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, ETag_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_ETag(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, ETag_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetETag(item, "etag-1");
  EXPECT_STREQ(WebDownloadItem_ETag(item), "etag-1");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, MimeType_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_MimeType(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, MimeType_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetMimeType(item, "application/zip");
  EXPECT_STREQ(WebDownloadItem_MimeType(item), "application/zip");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, IsPaused_NullItem_ReturnsFalse) {
  EXPECT_FALSE(WebDownloadItem_IsPaused(nullptr));
}

TEST_F(WebDownloaderTest, IsPaused_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->is_paused = 1;
  EXPECT_TRUE(WebDownloadItem_IsPaused(item));
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, Method_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_Method(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, Method_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->method = strdup("GET");
  EXPECT_STREQ(WebDownloadItem_Method(item), "GET");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, LastErrorCode_NullItem_ReturnsZero) {
  EXPECT_EQ(WebDownloadItem_LastErrorCode(nullptr), 0);
}

TEST_F(WebDownloaderTest, LastErrorCode_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->last_error_code = -6;
  EXPECT_EQ(WebDownloadItem_LastErrorCode(item), -6);
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, ReceivedSlices_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_ReceivedSlices(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, ReceivedSlices_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetReceivedSlices(item, "0-10");
  EXPECT_STREQ(WebDownloadItem_ReceivedSlices(item), "0-10");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, LastModified_NullItem_ReturnsNull) {
  EXPECT_EQ(WebDownloadItem_LastModified(nullptr), nullptr);
}

TEST_F(WebDownloaderTest, LastModified_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  WebDownloadItem_SetLastModified(item, "2026-01-01");
  EXPECT_STREQ(WebDownloadItem_LastModified(item), "2026-01-01");
  WebDownloadItem_Destroy(item);
}

TEST_F(WebDownloaderTest, NWebId_NullItem_ReturnsZero) {
  EXPECT_EQ(WebDownloadItem_NWebId(nullptr), 0);
}

TEST_F(WebDownloaderTest, NWebId_WithItem_ReturnsField) {
  NWebDownloadItem* item = CreateItem();
  ASSERT_TRUE(item != nullptr);
  item->nweb_id = 7;
  EXPECT_EQ(WebDownloadItem_NWebId(item), 7);
  WebDownloadItem_Destroy(item);
}

// Wrapper destroy helpers.
TEST_F(WebDownloaderTest, DestroyBeforeDownloadCallbackWrapper_Null_NoOp) {
  EXPECT_NO_FATAL_FAILURE(DestroyBeforeDownloadCallbackWrapper(nullptr));
}

TEST_F(WebDownloaderTest,
       DestroyBeforeDownloadCallbackWrapper_WithWrapper_Frees) {
  NWebBeforeDownloadCallbackWrapper* wrapper =
      new NWebBeforeDownloadCallbackWrapper();
  EXPECT_NO_FATAL_FAILURE(DestroyBeforeDownloadCallbackWrapper(wrapper));
}

TEST_F(WebDownloaderTest, DestroyDownloadItemCallbackWrapper_Null_NoOp) {
  EXPECT_NO_FATAL_FAILURE(DestroyDownloadItemCallbackWrapper(nullptr));
}

TEST_F(WebDownloaderTest,
       DestroyDownloadItemCallbackWrapper_WithWrapper_Frees) {
  NWebDownloadItemCallbackWrapper* wrapper =
      new NWebDownloadItemCallbackWrapper();
  EXPECT_NO_FATAL_FAILURE(DestroyDownloadItemCallbackWrapper(wrapper));
}
