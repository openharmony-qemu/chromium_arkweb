// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/arkweb_local_frame_mojo_handler_ext.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"


namespace blink {

using ::testing::_;

class MockCallbackHelper {
public:
    MOCK_METHOD(void, JavaScriptCallback, (::base::Value result));
    MOCK_METHOD(void, GenerateCodeCacheCallback, (int32_t status));
    MOCK_METHOD(void, GetImageCallback,
                (uint32_t id, ::base::ReadOnlySharedMemoryRegion region));
    MOCK_METHOD(void, GetImageByXPathCallback,
                (uint32_t size, ::base::ReadOnlySharedMemoryRegion region,
                 uint32_t width, uint32_t height));
};

class ArkWebLocalFrameMojoHandlerExtTest : public RenderingTest {
protected:
    void SetUp() override {
        EnableCompositing();
        RenderingTest::SetUp();
        ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
        web_view_helper_.Initialize();
        dummy_page_holder_ = std::make_unique<DummyPageHolder>();
        local_frame_ = &dummy_page_holder_->GetFrame();
        handler_ = MakeGarbageCollected<ArkWebLocalFrameMojoHandlerExt>(*local_frame_);
        mock_callback_ = std::make_unique<MockCallbackHelper>();
    }

    mojo::ScopedHandle CreateMockHandle(const std::string& script_content) {
        base::FilePath file_path = temp_dir_.GetPath().AppendASCII("script.js");
        base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
        if (!file.IsValid()) return mojo::ScopedHandle();

        file.WriteAtCurrentPos(script_content.data(), script_content.size());
        file.Close();

        base::File read_file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
        if (!read_file.IsValid()) return mojo::ScopedHandle();

        base::ScopedPlatformFile scoped_handle(read_file.TakePlatformFile());

        mojo::ScopedHandle script_handle = 
                            mojo::WrapPlatformFile(std::move(scoped_handle));

        return script_handle;
    }

    void JavaScriptExecuteRequestExtTest(
                mojo::ScopedHandle handle_fd,
                const uint64_t scriptLength,
                bool wants_result,
                LocalFrameMojoHandler::JavaScriptExecuteRequestCallback callback)
    {
        handler_->JavaScriptExecuteRequestExt(std::move(handle_fd), scriptLength,
                                        wants_result, std::move(callback));
        return;
    }

    void GenerateCodeCacheTest(
                const String& url,
                const String& script,
                mojom::blink::CacheOptionsPtr cache_options,
               LocalFrameMojoHandler:: GenerateCodeCacheCallback callback)
    {
        handler_->GenerateCodeCache(url, script,
                            std::move(cache_options), std::move(callback));
        return;
    }

    void GetImageFromCacheTest(
                const String& url,
                LocalFrameMojoHandler::GetImageFromCacheCallback callback)
    {
        handler_->GetImageFromCache(url, std::move(callback));
        return;
    }

    void DetectBlankScreenTest(
                const String& url,
                bool detectionEnable,
                const Vector<double>& detectionTiming,
                const Vector<int32_t>& detectionMethods,
                int32_t contentfulNodesCountThreshold)
    {
        handler_->DetectBlankScreen(url, detectionEnable, detectionTiming, detectionMethods,
                                    contentfulNodesCountThreshold);
    }

    void GetImageByXPathTest(
                int32_t taskid,
                const String& xpath,
                LocalFrameMojoHandler::GetImageByXPathCallback callback)
    {
        handler_->GetImageByXPath(taskid, xpath, std::move(callback));
    }

    void GetImageByXPathOnHandler(
                ArkWebLocalFrameMojoHandlerExt* h,
                int32_t taskid,
                const String& xpath,
                LocalFrameMojoHandler::GetImageByXPathCallback callback)
    {
        CHECK(h);
        h->GetImageByXPath(taskid, xpath, std::move(callback));
    }

    void GetAllImageTest(
                int32_t taskid,
                const String& url,
                LocalFrameMojoHandler::GetAllImageCallback callback)
    {
        handler_->GetAllImage(taskid, url, std::move(callback));
    }

    void TearDown() override {
        dummy_page_holder_.reset();
    }

    std::unique_ptr<DummyPageHolder> dummy_page_holder_;
    Persistent<LocalFrame> local_frame_;
    Persistent<ArkWebLocalFrameMojoHandlerExt> handler_;
    std::unique_ptr<MockCallbackHelper> mock_callback_;
    frame_test_helpers::WebViewHelper web_view_helper_;
    base::ScopedTempDir temp_dir_;
};

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GetImageFromCache_ValidUrl) {

    auto callback = base::BindOnce(&MockCallbackHelper::GetImageCallback,
                                    base::Unretained(mock_callback_.get()));

    EXPECT_CALL(*mock_callback_, GetImageCallback(
        testing::Eq(0u),
        testing::A<base::ReadOnlySharedMemoryRegion>()
    )).Times(1);

    GetImageFromCacheTest(
        "https://example.com/image.png",
        std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GetImageFromCache_EmptyUrl) {
    auto callback = base::BindOnce(&MockCallbackHelper::GetImageCallback,
                                    base::Unretained(mock_callback_.get()));

    EXPECT_CALL(*mock_callback_, GetImageCallback(
        testing::Eq(0u),
        testing::A<base::ReadOnlySharedMemoryRegion>()
    )).Times(1);

    GetImageFromCacheTest(
        "",
        std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GenerateCodeCache_Success) {
    auto cache_options = mojom::blink::CacheOptions::New();
    EXPECT_CALL(*mock_callback_, GenerateCodeCacheCallback(testing::Eq(0)))
        .Times(1);
    
    auto callback = base::BindOnce(&MockCallbackHelper::GenerateCodeCacheCallback,
                                    base::Unretained(mock_callback_.get()));
    
    GenerateCodeCacheTest(
        "https://example.com/script.js",
        "console.log('Hello World');",
        std::move(cache_options),
        std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GenerateCodeCache_EmptyScript) {
    auto cache_options = mojom::blink::CacheOptions::New();
    EXPECT_CALL(*mock_callback_, GenerateCodeCacheCallback(testing::Eq(0)))
        .Times(1);
    
    auto callback = base::BindOnce(&MockCallbackHelper::GenerateCodeCacheCallback,
                                    base::Unretained(mock_callback_.get()));
    
    GenerateCodeCacheTest(
        "https://example.com/script.js",
        "",
        std::move(cache_options),
        std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GenerateCodeCache_EmptyUrl) {
    auto cache_options = mojom::blink::CacheOptions::New();
    EXPECT_CALL(*mock_callback_, GenerateCodeCacheCallback(testing::Eq(0)))
        .Times(1);
    
    auto callback = base::BindOnce(&MockCallbackHelper::GenerateCodeCacheCallback,
                                    base::Unretained(mock_callback_.get()));
    
    GenerateCodeCacheTest(
        "",
        "console.log('Hello World');",
        std::move(cache_options),
        std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, JavaScriptExecuteRequestExt_Success) {
    const std::string kTestScript = "1 + 2;";
    const uint64_t kScriptLength = kTestScript.size();
    
    mojo::ScopedHandle script_handle = CreateMockHandle(kTestScript);
    auto callback = base::BindOnce(&MockCallbackHelper::JavaScriptCallback,
                                    base::Unretained(mock_callback_.get()));

    JavaScriptExecuteRequestExtTest(
        std::move(script_handle), 
        kScriptLength, 
        true,  // wants_result
        std::move(callback));
    
    base::RunLoop().RunUntilIdle();
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, JavaScriptExecuteRequestExt_NoResult) {
    const std::string kTestScript = "console.log('Hello');";
    const uint64_t kScriptLength = kTestScript.size();
    
    mojo::ScopedHandle script_handle = CreateMockHandle(kTestScript);

    auto callback = base::BindOnce(&MockCallbackHelper::JavaScriptCallback,
                                    base::Unretained(mock_callback_.get()));

    JavaScriptExecuteRequestExtTest(
        std::move(script_handle), 
        kScriptLength, 
        false,  // wants_result
        std::move(callback));
    
    base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)
TEST_F(ArkWebLocalFrameMojoHandlerExtTest, DetectBlankScreen_EmptyVectors) {
    Vector<double> timing;
    Vector<int32_t> methods;
    ASSERT_NO_FATAL_FAILURE(
        DetectBlankScreenTest("https://example.com", true,  timing, methods, 5));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, DetectBlankScreen_WithTimingOnly) {
    Vector<double> timing = {1.0, 2.0, 3.0};
    Vector<int32_t> methods;
    ASSERT_NO_FATAL_FAILURE(
        DetectBlankScreenTest("https://example.com", true, timing, methods, 5));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, DetectBlankScreen_WithMethodsOnly) {
    Vector<double> timing;
    Vector<int32_t> methods = {0, 1};
    ASSERT_NO_FATAL_FAILURE(
        DetectBlankScreenTest("https://example.com", true, timing, methods, 5));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, DetectBlankScreen_WithBothNonEmpty) {
    Vector<double> timing = {1.0, 2.0};
    Vector<int32_t> methods = {0};
    ASSERT_NO_FATAL_FAILURE(
        DetectBlankScreenTest("https://example.com", true, timing, methods, 5));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, DetectBlankScreen_WithDetectionEnableOnly) {
    Vector<double> timing = {1.0, 2.0};
    Vector<int32_t> methods = {0};
    ASSERT_NO_FATAL_FAILURE(
        DetectBlankScreenTest("https://example.com", false, timing, methods, 5));
}
#endif  // BUILDFLAG(ARKWEB_BLANK_SCREEN_DETECTION)

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GetImageByXPath_NoMatch) {
    EXPECT_CALL(*mock_callback_, GetImageByXPathCallback(
        testing::Eq(0u),
        testing::A<base::ReadOnlySharedMemoryRegion>(),
        testing::Eq(0u),
        testing::Eq(0u)
    )).Times(1);

    auto callback = base::BindOnce(&MockCallbackHelper::GetImageByXPathCallback,
                                    base::Unretained(mock_callback_.get()));
    GetImageByXPathTest(1, "/html/body/img[1]", std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GetImageByXPath_InvalidXPath) {
    EXPECT_CALL(*mock_callback_, GetImageByXPathCallback(
        testing::Eq(0u),
        testing::A<base::ReadOnlySharedMemoryRegion>(),
        testing::Eq(0u),
        testing::Eq(0u)
    )).Times(1);

    auto callback = base::BindOnce(&MockCallbackHelper::GetImageByXPathCallback,
                                    base::Unretained(mock_callback_.get()));
    GetImageByXPathTest(1, "///invalid[[[", std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GetImageByXPath_EmptyXPath) {
    EXPECT_CALL(*mock_callback_, GetImageByXPathCallback(
        testing::Eq(0u),
        testing::A<base::ReadOnlySharedMemoryRegion>(),
        testing::Eq(0u),
        testing::Eq(0u)
    )).Times(1);

    auto callback = base::BindOnce(&MockCallbackHelper::GetImageByXPathCallback,
                                    base::Unretained(mock_callback_.get()));
    GetImageByXPathTest(1, "", std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GetAllImage_ValidUrl) {
    EXPECT_CALL(*mock_callback_, GetImageCallback(
        testing::Eq(0u),
        testing::A<base::ReadOnlySharedMemoryRegion>()
    )).Times(1);

    auto callback = base::BindOnce(&MockCallbackHelper::GetImageCallback,
                                    base::Unretained(mock_callback_.get()));
    GetAllImageTest(1, "https://example.com/image.png", std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GetAllImage_EmptyUrl) {
    EXPECT_CALL(*mock_callback_, GetImageCallback(
        testing::Eq(0u),
        testing::A<base::ReadOnlySharedMemoryRegion>()
    )).Times(1);

    auto callback = base::BindOnce(&MockCallbackHelper::GetImageCallback,
                                    base::Unretained(mock_callback_.get()));
    GetAllImageTest(1, "", std::move(callback));
}

TEST_F(ArkWebLocalFrameMojoHandlerExtTest, GetImageByXPath_WithRealElement) {
    SetBodyInnerHTML(R"HTML(
        <div id="target" style="width:100px;height:100px;background:red;">text</div>
    )HTML");
    UpdateAllLifecyclePhasesForTest();

    Persistent<ArkWebLocalFrameMojoHandlerExt> rendering_handler =
        MakeGarbageCollected<ArkWebLocalFrameMojoHandlerExt>(GetFrame());
    CHECK(rendering_handler);

    EXPECT_CALL(*mock_callback_, GetImageByXPathCallback(_, _, _, _)).Times(1);

    auto callback = base::BindOnce(&MockCallbackHelper::GetImageByXPathCallback,
                                    base::Unretained(mock_callback_.get()));
    GetImageByXPathOnHandler(rendering_handler.Get(), 1,
                             "//*[@id='target']", std::move(callback));
}

}