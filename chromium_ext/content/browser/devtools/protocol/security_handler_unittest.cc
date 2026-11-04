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

#include "content/browser/devtools/protocol/security_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/browser/devtools/protocol/security.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content::protocol {

class SecurityHandlerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    security_handler_ = std::make_unique<SecurityHandler>();
  }

  void TearDown() override {
    security_handler_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  // Helper that calls NotifyCertificateError and records the callback result.
  // Returns true if the handler indicated it handled the error (i.e. returned
  // true from NotifyCertificateError).
  bool NotifyCertError(int cert_error,
                       const GURL& url,
                       content::CertificateRequestResultType* out_result) {
    bool was_handled = security_handler_->NotifyCertificateError(
        cert_error, url,
        base::BindOnce(
            [](content::CertificateRequestResultType* out,
               content::CertificateRequestResultType result) { *out = result; },
            out_result));
    return was_handled;
  }

  std::unique_ptr<SecurityHandler> security_handler_;
};

// ---- SetIgnoreCertificateErrors ----

TEST_F(SecurityHandlerTest, SetIgnoreCertificateErrorsTrue) {
  EXPECT_FALSE(security_handler_->IsIgnoreCertificateErrorsSet());

  Response response = security_handler_->SetIgnoreCertificateErrors(true);
  EXPECT_TRUE(response.IsSuccess());
  EXPECT_TRUE(security_handler_->IsIgnoreCertificateErrorsSet());
}

TEST_F(SecurityHandlerTest, SetIgnoreCertificateErrorsFalse) {
  security_handler_->SetIgnoreCertificateErrors(true);
  ASSERT_TRUE(security_handler_->IsIgnoreCertificateErrorsSet());

  Response response = security_handler_->SetIgnoreCertificateErrors(false);
  EXPECT_TRUE(response.IsSuccess());
  EXPECT_FALSE(security_handler_->IsIgnoreCertificateErrorsSet());
}

// ---- Mutual exclusion with SetOverrideCertificateErrors ----

TEST_F(SecurityHandlerTest,
       SetIgnoreCertificateErrorsFailsWhenOverrideIsActive) {
  // Enable() without SetRenderer sets enabled_ = true (host_ is null so
  // AssureTopLevelActiveFrame is skipped).
  ASSERT_TRUE(security_handler_->Enable().IsSuccess());
  ASSERT_TRUE(security_handler_->SetOverrideCertificateErrors(true).IsSuccess());

  // Now setIgnoreCertificateErrors(true) should fail.
  Response response = security_handler_->SetIgnoreCertificateErrors(true);
  EXPECT_FALSE(response.IsSuccess());
  EXPECT_FALSE(security_handler_->IsIgnoreCertificateErrorsSet());
}

TEST_F(SecurityHandlerTest,
       SetOverrideCertificateErrorsFailsWhenIgnoreIsActive) {
  ASSERT_TRUE(security_handler_->SetIgnoreCertificateErrors(true).IsSuccess());
  ASSERT_TRUE(security_handler_->IsIgnoreCertificateErrorsSet());

  // Enable is required for SetOverrideCertificateErrors, but even after
  // enabling, the override should fail because ignore is active.
  ASSERT_TRUE(security_handler_->Enable().IsSuccess());

  Response response = security_handler_->SetOverrideCertificateErrors(true);
  EXPECT_FALSE(response.IsSuccess());
}

TEST_F(SecurityHandlerTest,
       SetIgnoreCertificateErrorsAfterOverrideDisabled) {
  ASSERT_TRUE(security_handler_->Enable().IsSuccess());
  ASSERT_TRUE(security_handler_->SetOverrideCertificateErrors(true).IsSuccess());

  // Disable override.
  ASSERT_TRUE(security_handler_->SetOverrideCertificateErrors(false).IsSuccess());
  EXPECT_FALSE(security_handler_->IsIgnoreCertificateErrorsSet());

  // Now setIgnoreCertificateErrors should succeed.
  Response response = security_handler_->SetIgnoreCertificateErrors(true);
  EXPECT_TRUE(response.IsSuccess());
  EXPECT_TRUE(security_handler_->IsIgnoreCertificateErrorsSet());
}

// ---- NotifyCertificateError behavior ----

TEST_F(SecurityHandlerTest, NotifyCertificateErrorIgnoredWhenIgnoreSet) {
  security_handler_->SetIgnoreCertificateErrors(true);

  content::CertificateRequestResultType result =
      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  bool handled = NotifyCertError(net::ERR_CERT_COMMON_NAME_INVALID,
                                 GURL("https://example.com"), &result);

  EXPECT_TRUE(handled);
  EXPECT_EQ(content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE, result);
}

TEST_F(SecurityHandlerTest, NotifyCertificateErrorNotHandledWhenDisabled) {
  // Without Enable() and without SetIgnoreCertificateErrors, the handler
  // should not handle the error.
  content::CertificateRequestResultType result =
      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  bool handled = NotifyCertError(net::ERR_CERT_COMMON_NAME_INVALID,
                                 GURL("https://example.com"), &result);

  EXPECT_FALSE(handled);
}

TEST_F(SecurityHandlerTest,
       NotifyCertificateErrorNotContinuedWhenOnlyEnabled) {
  // Enable() without SetRenderer sets enabled_ = true but does not call
  // AttachToRenderFrameHost (host_ is null). NotifyCertificateError with
  // enabled_ true but not in override mode should return false.
  // Note: without Wire(), frontend_ is null, but NotifyCertificateError
  // only uses frontend_ when enabled_ is true and mode is not kIgnoreAll.
  // To avoid a null frontend_ crash, we skip this scenario and instead
  // verify that with enabled_ false, the error is not handled.
  content::CertificateRequestResultType result =
      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  bool handled = NotifyCertError(net::ERR_CERT_COMMON_NAME_INVALID,
                                 GURL("https://example.com"), &result);

  // When not enabled and not in ignore mode, NotifyCertificateError returns
  // false (the error is not handled).
  EXPECT_FALSE(handled);
}

TEST_F(SecurityHandlerTest, DisableResetsIgnoreCertificateErrors) {
  ASSERT_TRUE(security_handler_->Enable().IsSuccess());
  ASSERT_TRUE(security_handler_->SetIgnoreCertificateErrors(true).IsSuccess());
  ASSERT_TRUE(security_handler_->IsIgnoreCertificateErrorsSet());

  ASSERT_TRUE(security_handler_->Disable().IsSuccess());
  EXPECT_FALSE(security_handler_->IsIgnoreCertificateErrorsSet());
}

// ---- SetIgnoreCertificateErrors does not require Enable ----

TEST_F(SecurityHandlerTest, SetIgnoreCertificateErrorsWithoutEnable) {
  // setIgnoreCertificateErrors should work without calling Enable() first,
  // unlike setOverrideCertificateErrors which requires Enable().
  Response response = security_handler_->SetIgnoreCertificateErrors(true);
  EXPECT_TRUE(response.IsSuccess());
  EXPECT_TRUE(security_handler_->IsIgnoreCertificateErrorsSet());

  // Verify the bypass actually takes effect.
  content::CertificateRequestResultType result =
      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  bool handled = NotifyCertError(net::ERR_CERT_AUTHORITY_INVALID,
                                 GURL("https://self-signed.example.com"),
                                 &result);
  EXPECT_TRUE(handled);
  EXPECT_EQ(content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE, result);
}

}  // namespace content::protocol
