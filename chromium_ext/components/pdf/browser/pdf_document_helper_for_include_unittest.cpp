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

// 恢复自 132_trunk 的 6def5f99ba（"add UT for pdf_document_helper_for_include"）。
// 144 的 PDFDocumentHelperClient 接口已变，Mock 已适配：
//   - OnPDFHasUnsupportedFeature 已删除（移除 mock）
//   - OnSaveURL(WebContents*) 改为无参 OnSaveURL()
//   - OnSearchifyStateChange 改名 OnSearchifyStarted(RFH*)（两用例未调用，移除）
// 用例体（HideHandleAndQuickMenu / ResetResponsePendingInputEvent）逻辑零修改。

#include "content/public/browser/render_frame_host_receiver_set.h"

#define private public
#include "components/pdf/browser/pdf_document_helper.h"
#undef private
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <limits>

#include "base/test/scoped_logging_settings.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace pdf {
namespace {
using namespace testing;

class MockPDFDocumentHelperClient : public PDFDocumentHelperClient {
public:
    MOCK_METHOD(void,
                UpdateContentRestrictions,
                (content::RenderFrameHost* render_frame_host,
                 int content_restrictions),
                (override));

    MOCK_METHOD(void,
                OnSaveURL,
                (),
                (override));

    MOCK_METHOD(void,
                SetPluginCanSave,
                (content::RenderFrameHost* render_frame_host,
                bool can_save),
                (override));

    MOCK_METHOD(void,
                OnDidScroll,
                (const gfx::SelectionBound& start,
                const gfx::SelectionBound& end),
                (override));
};

class FakePdfListener : public mojom::PdfListener {
 public:
  MOCK_METHOD(void, SetCaretPosition, (const gfx::PointF&), (override));
  MOCK_METHOD(void, MoveRangeSelectionExtent, (const gfx::PointF&), (override));
  MOCK_METHOD(void,
              SetSelectionBounds,
              (const gfx::PointF&, const gfx::PointF&),
              (override));
  MOCK_METHOD(void, GetPdfBytes, (uint32_t, GetPdfBytesCallback), (override));
  MOCK_METHOD(void, GetPageText, (int32_t, GetPageTextCallback), (override));
#if BUILDFLAG(ARKWEB_PDF)
  MOCK_METHOD(void, ClearTextSelection, (), (override));
  MOCK_METHOD(void, OnScaleChanged, (), (override));
#endif  // BUILDFLAG(ARKWEB_PDF)
  MOCK_METHOD(void,
              GetMostVisiblePageIndex,
              (GetMostVisiblePageIndexCallback),
              (override));
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  MOCK_METHOD(void,
              GetSaveDataBufferHandlerForDrive,
              (mojom::SaveRequestType,
               GetSaveDataBufferHandlerForDriveCallback),
              (override));
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
};

class PDFDocumentHelperTest : public content::RenderViewHostTestHarness {
protected:
    void SetUp() override;
    void TearDown() override;
    std::unique_ptr<logging::ScopedLoggingSettings> logging_settings_;
    static std::string log_buffer_;

    // 日志处理器：把日志写入字符串缓冲区
    static bool LogHandler(
                           logging::LogSeverity severity,
                           const char* file,
                           int line,
                           size_t message_start,
                           const std::string& str) {
                              log_buffer_ += str + "\n";
                              return true;
                           }
};

std::string PDFDocumentHelperTest::log_buffer_ = "";

void PDFDocumentHelperTest::SetUp() {
    content::RenderViewHostTestHarness::SetUp();
    // 保存当前日志设置，测试结束后自动恢复
    logging_settings_ = std::make_unique<logging::ScopedLoggingSettings>();

    // 设置日志处理器：将所有日志输出到内存中
    logging::SetLogMessageHandler(&PDFDocumentHelperTest::LogHandler);

    // 清空旧日志
    log_buffer_.clear();
}

void PDFDocumentHelperTest::TearDown() {
    content::RenderViewHostTestHarness::TearDown();
    // 恢复原始日志设置（由ScopedLoggingSettings析构完成）
    logging_settings_.reset();

    // 恢复默认日志处理器
    logging::SetLogMessageHandler(nullptr);
}

TEST_F(PDFDocumentHelperTest, HideHandleAndQuickMenu) {
    content::BrowserContext* ptr = GetBrowserContext();
    ASSERT_NE(ptr, nullptr);

    content::RenderFrameHostTester* tester =
    content::RenderFrameHostTester::For(main_rfh());
    ASSERT_NE(tester, nullptr);
    tester->InitializeRenderFrameIfNeeded();
    content::RenderFrameHost* subframe1 = tester->AppendChild("subframe1");

    //create pdf_helper
    std::unique_ptr<PDFDocumentHelperClient> mock_client_ = std::make_unique<MockPDFDocumentHelperClient>();
    PDFDocumentHelper::CreateForCurrentDocument(subframe1, std::move(mock_client_));
    auto helper = PDFDocumentHelper::GetForCurrentDocument(subframe1);

    helper->HideHandleAndQuickMenu(true);
    EXPECT_THAT(log_buffer_, testing::HasSubstr("PDF touch_selection_controller_client_manager_ is null"));
}

TEST_F(PDFDocumentHelperTest, ResetResponsePendingInputEvent) {
    EXPECT_TRUE(log_buffer_.empty());
    content::RenderFrameHostTester* tester =
    content::RenderFrameHostTester::For(main_rfh());
    ASSERT_NE(tester, nullptr);
    tester->InitializeRenderFrameIfNeeded();
    content::RenderFrameHost* subframe1 = tester->AppendChild("subframe1");

    //create pdf_helper
    std::unique_ptr<PDFDocumentHelperClient> mock_client_ = std::make_unique<MockPDFDocumentHelperClient>();
    PDFDocumentHelper::CreateForCurrentDocument(subframe1, std::move(mock_client_));
    auto helper = PDFDocumentHelper::GetForCurrentDocument(subframe1);

    helper->ResetResponsePendingInputEvent();
    EXPECT_THAT(log_buffer_, testing::HasSubstr("PDF touch_selection_controller_client_manager_ is null"));
}

TEST_F(PDFDocumentHelperTest, SafeScaleHandlesNormalAndOverflowValues) {
  content::RenderFrameHostTester* tester =
      content::RenderFrameHostTester::For(main_rfh());
  ASSERT_NE(tester, nullptr);
  tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = tester->AppendChild("subframe");
  auto mock_client = std::make_unique<MockPDFDocumentHelperClient>();
  PDFDocumentHelper::CreateForCurrentDocument(subframe, std::move(mock_client));
  auto* helper = PDFDocumentHelper::GetForCurrentDocument(subframe);
  ASSERT_NE(helper, nullptr);

  EXPECT_EQ(helper->SafeScale(10, 1.5f), 15);
  EXPECT_EQ(helper->SafeScale(std::numeric_limits<int32_t>::max(), 2.0f),
            std::numeric_limits<int32_t>::max());
  EXPECT_EQ(helper->SafeScale(std::numeric_limits<int32_t>::min(), 2.0f),
            std::numeric_limits<int32_t>::min());
}

TEST_F(PDFDocumentHelperTest, SetSelectionBoundsVisibility) {
  content::RenderFrameHostTester* tester =
      content::RenderFrameHostTester::For(main_rfh());
  ASSERT_NE(tester, nullptr);
  tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = tester->AppendChild("subframe");
  auto mock_client = std::make_unique<MockPDFDocumentHelperClient>();
  PDFDocumentHelper::CreateForCurrentDocument(subframe, std::move(mock_client));
  auto* helper = PDFDocumentHelper::GetForCurrentDocument(subframe);
  ASSERT_NE(helper, nullptr);

  gfx::SelectionBound start;
  gfx::SelectionBound end;
  start.set_visible(true);
  end.set_visible(false);
  helper->has_selection_ = false;
  helper->SetSelectionBoundsVisibility(start, end);
  EXPECT_TRUE(start.visible());
  EXPECT_FALSE(end.visible());

  helper->has_selection_ = true;
  helper->SetIsLeftHandleVisible(false);
  helper->SetIsRightHandleVisible(false);
  EXPECT_FALSE(helper->is_right_visible_.load());
  helper->SetIsRightHandleVisible(true);
  helper->SetSelectionBoundsVisibility(start, end);
  EXPECT_FALSE(start.visible());
  EXPECT_TRUE(end.visible());

  helper->SetIsLeftHandleVisible(false);
  helper->SetIsRightHandleVisible(true);
  EXPECT_FALSE(helper->is_left_visible_.load());
  EXPECT_TRUE(helper->is_right_visible_.load());
}

TEST_F(PDFDocumentHelperTest, OnScaleChangedUpdatesScaleWithoutRemoteClient) {
  content::RenderFrameHostTester* tester =
      content::RenderFrameHostTester::For(main_rfh());
  ASSERT_NE(tester, nullptr);
  tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = tester->AppendChild("subframe");
  auto mock_client = std::make_unique<MockPDFDocumentHelperClient>();
  PDFDocumentHelper::CreateForCurrentDocument(subframe, std::move(mock_client));
  auto* helper = PDFDocumentHelper::GetForCurrentDocument(subframe);
  ASSERT_NE(helper, nullptr);

  helper->OnScaleChanged(2.5f);

  EXPECT_FLOAT_EQ(helper->page_scale_factor_, 2.5f);
  EXPECT_THAT(log_buffer_,
              testing::HasSubstr("PDF remote_pdf_client_ is null"));
}

TEST_F(PDFDocumentHelperTest, NullSelectionManagerMethodsReturnSafely) {
  content::RenderFrameHostTester* tester =
      content::RenderFrameHostTester::For(main_rfh());
  ASSERT_NE(tester, nullptr);
  tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = tester->AppendChild("subframe");
  auto mock_client = std::make_unique<MockPDFDocumentHelperClient>();
  PDFDocumentHelper::CreateForCurrentDocument(subframe, std::move(mock_client));
  auto* helper = PDFDocumentHelper::GetForCurrentDocument(subframe);
  ASSERT_NE(helper, nullptr);

  helper->ConvertAndUpdateSelectionBounds(gfx::Rect(1, 2, 3, 4));
  helper->UpdateQuickMenu();
  helper->SetIsPdfDocument(true);
  helper->ClearTextSelection();

  EXPECT_THAT(log_buffer_,
              testing::HasSubstr(
                  "PDF touch_selection_controller_client_manager_ is null"));
  EXPECT_THAT(log_buffer_,
              testing::HasSubstr("PDF remote_pdf_client_ is null"));
}

TEST_F(PDFDocumentHelperTest, UpdateScaleFactorUsesValidMainFrameScale) {
  content::RenderFrameHostTester* tester =
      content::RenderFrameHostTester::For(main_rfh());
  ASSERT_NE(tester, nullptr);
  tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = tester->AppendChild("subframe");
  auto mock_client = std::make_unique<MockPDFDocumentHelperClient>();
  PDFDocumentHelper::CreateForCurrentDocument(subframe, std::move(mock_client));
  auto* helper = PDFDocumentHelper::GetForCurrentDocument(subframe);
  ASSERT_NE(helper, nullptr);
  helper->page_scale_factor_ = 2.5f;

  helper->UpdateScaleFactor();

  EXPECT_GT(helper->page_scale_factor_, 0.0f);
}

TEST_F(PDFDocumentHelperTest, RemoteClientReceivesClearAndScaleChanged) {
  content::RenderFrameHostTester* tester =
      content::RenderFrameHostTester::For(main_rfh());
  ASSERT_NE(tester, nullptr);
  tester->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe = tester->AppendChild("subframe");
  auto mock_client = std::make_unique<MockPDFDocumentHelperClient>();
  PDFDocumentHelper::CreateForCurrentDocument(subframe, std::move(mock_client));
  auto* helper = PDFDocumentHelper::GetForCurrentDocument(subframe);
  ASSERT_NE(helper, nullptr);
  testing::StrictMock<FakePdfListener> listener;
  mojo::Receiver<mojom::PdfListener> receiver(&listener);
  helper->SetListener(receiver.BindNewPipeAndPassRemote());
  EXPECT_CALL(listener, ClearTextSelection()).Times(1);
  EXPECT_CALL(listener, OnScaleChanged()).Times(1);

  helper->ClearTextSelection();
  helper->OnScaleChanged(3.0f);
  receiver.FlushForTesting();

  EXPECT_FLOAT_EQ(helper->page_scale_factor_, 3.0f);
}
}  // namespace
}  // namespace pdf
