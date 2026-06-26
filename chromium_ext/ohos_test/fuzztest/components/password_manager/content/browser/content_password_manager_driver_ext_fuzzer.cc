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

/*
 * Fuzzer for content_password_manager_driver_ext.cc
 *
 * Target: arkweb/chromium_ext/components/password_manager/content/browser/content_password_manager_driver_ext.cc
 */

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/arkweb_password_autofill_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/bad_message.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/core/embedder/embedder.h"
#include "net/base/mock_network_change_notifier.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/rect_f.h"

#define private public
#include "arkweb/chromium_ext/components/password_manager/content/browser/content_password_manager_driver_ext.h"
#undef private

namespace password_manager {
namespace {

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  MockPasswordManagerClient(const MockPasswordManagerClient&) = delete;
  MockPasswordManagerClient& operator=(const MockPasswordManagerClient&) =
      delete;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(autofill::LogManager*, GetCurrentLogManager, (), (override));
  MOCK_METHOD(PasswordManager*, GetPasswordManager, (), (const override));
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
  MOCK_METHOD(void,
              CheckSafeBrowsingReputation,
              (const GURL&, const GURL&),
              (override));
#endif
};

class FuzzerEnv {
 public:
  FuzzerEnv()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        content_client_initializer_(),
        web_contents_factory_() {
    ui::DeviceDataManager::CreateInstance();
    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>(
        content::ImageTransportFactory::GetInstance()->GetContextFactory());
    aura_test_helper_->SetUp();
    network_change_notifier_ =
        net::test::MockNetworkChangeNotifier::Create();
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    web_contents_ = web_contents_factory_.CreateWebContents(
        browser_context_.get());
    rfh_ = static_cast<content::TestRenderFrameHost*>(
        web_contents_->GetPrimaryMainFrame());
    content::GpuDataManagerImpl::GetInstance();
    if (rfh_) {
      content::RenderFrameHost::AllowInjectingJavaScript();
      rfh_->InitializeRenderFrameIfNeeded();
    }
  }

  content::TestRenderFrameHost* rfh() { return rfh_; }

 private:
  struct CommandLineInitializer {
    CommandLineInitializer() { base::CommandLine::Init(0, nullptr); }
  };
  struct MojoInitializer {
    MojoInitializer() {
      content::ForceCreateNetworkServiceDirectlyForTesting();
      content::StoragePartitionImpl::ForceInProcessStorageServiceForTesting();
      mojo::core::Init(
          mojo::core::Configuration{.is_broker_process = true});
    }
  };
  CommandLineInitializer cmdline_init_;
  MojoInitializer mojo_init_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestContentClientInitializer content_client_initializer_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  content::TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;
  content::WebContents* web_contents_ = nullptr;
  content::TestRenderFrameHost* rfh_ = nullptr;
};

autofill::InputFillRequestData CreateInputFillRequestData(
    FuzzedDataProvider* fdp) {
    autofill::InputFillRequestData data;
    data.field_renderer_id = autofill::FieldRendererId(fdp->ConsumeIntegral<uint32_t>());
    data.is_focused = fdp->ConsumeBool();
    data.type = (fdp->ConsumeBool() ? autofill::mojom::OhosInputElementType::kUsernameType 
                                    : autofill::mojom::OhosInputElementType::kPasswordType);
    
    float x = fdp->ConsumeFloatingPoint<float>();
    float y = fdp->ConsumeFloatingPoint<float>();
    float width = fdp->ConsumeFloatingPoint<float>();
    float height = fdp->ConsumeFloatingPoint<float>();
    data.bounds = gfx::RectF(x, y, width, height);
    
    std::string value_str = fdp->ConsumeBytesAsString(100);
    data.value = std::u16string(value_str.begin(), value_str.end());
    
    std::string placeholder_str = fdp->ConsumeBytesAsString(100);
    data.placeholder = std::u16string(placeholder_str.begin(), placeholder_str.end());
    
    data.autocomplete_attr = fdp->ConsumeBytesAsString(50);
    
    return data;
}

void TestOnRequestAutofill(
    ContentPasswordManagerDriverExt* driver,
    FuzzedDataProvider* fdp) {
    if (!driver || !fdp) return;

    autofill::FormRendererId form_id(fdp->ConsumeIntegral<uint32_t>());
    autofill::mojom::OhosPasswordFormAutofillState state = 
        static_cast<autofill::mojom::OhosPasswordFormAutofillState>(
            fdp->ConsumeIntegralInRange<uint32_t>(0, 2));
    
    autofill::InputFillRequestData username_data = CreateInputFillRequestData(fdp);
    autofill::InputFillRequestData password_data = CreateInputFillRequestData(fdp);

    driver->OnRequestAutofill(form_id, state, username_data, password_data);
}

void TestPreSuppressKeyboardIfNeed(
    ContentPasswordManagerDriverExt* driver,
    FuzzedDataProvider* fdp) {
    if (!driver || !fdp) return;

    autofill::FormRendererId form_id(fdp->ConsumeIntegral<uint32_t>());
    autofill::mojom::OhosPasswordFormAutofillState state = 
        static_cast<autofill::mojom::OhosPasswordFormAutofillState>(
            fdp->ConsumeIntegralInRange<uint32_t>(0, 2));
    
    autofill::InputFillRequestData username_data = CreateInputFillRequestData(fdp);
    autofill::InputFillRequestData password_data = CreateInputFillRequestData(fdp);

    driver->PreSuppressKeyboardIfNeed(form_id, state, username_data, password_data);
}

void TestFillAccountSuggestionWithURL(
    ContentPasswordManagerDriverExt* driver,
    FuzzedDataProvider* fdp) {
    if (!driver || !fdp) return;

    GURL page_url(fdp->ConsumeBytesAsString(100));
    std::string username_str = fdp->ConsumeBytesAsString(50);
    std::u16string username(username_str.begin(), username_str.end());
    std::string password_str = fdp->ConsumeBytesAsString(50);
    std::u16string password(password_str.begin(), password_str.end());

    driver->FillAccountSuggestion(page_url, username, password);
}



void TestAutofillSurfaceClosed(
    ContentPasswordManagerDriverExt* driver,
    FuzzedDataProvider* fdp) {
    if (!driver || !fdp) return;

    bool show_virtual_keyboard = fdp->ConsumeBool();
    driver->AutofillSurfaceClosed(show_virtual_keyboard);
}

void TestSendParsedPasswordFormToRenderer(
    ContentPasswordManagerDriverExt* driver,
    FuzzedDataProvider* fdp) {
    if (!driver || !fdp) return;

    autofill::PasswordFormFillData parsed_form_data_without_password;
    parsed_form_data_without_password.form_renderer_id =
        autofill::FormRendererId(fdp->ConsumeIntegral<uint32_t>());
    parsed_form_data_without_password.url = GURL(fdp->ConsumeBytesAsString(100));
    parsed_form_data_without_password.username_element_renderer_id = autofill::FieldRendererId(
        fdp->ConsumeIntegral<uint32_t>());
    parsed_form_data_without_password.password_element_renderer_id = autofill::FieldRendererId(
        fdp->ConsumeIntegral<uint32_t>());

    driver->SendParsedPasswordFormToRenderer(parsed_form_data_without_password);
}

enum class TestScenario {
    ON_REQUEST_AUTOFILL = 0,
    PRE_SUPPRESS_KEYBOARD_IF_NEED = 1,
    FILL_ACCOUNT_SUGGESTION_WITH_URL = 2,
    AUTOFILL_SURFACE_CLOSED = 3,
    SEND_PARSED_PASSWORD_FORM_TO_RENDERER = 4,
    MAX_SCENARIO
};

void RunFuzzer(FuzzedDataProvider* fdp, ContentPasswordManagerDriverExt* driver) {
    if (!driver || !fdp) return;

    auto scenario = fdp->ConsumeIntegralInRange<uint8_t>(
        static_cast<uint8_t>(TestScenario::ON_REQUEST_AUTOFILL),
        static_cast<uint8_t>(TestScenario::MAX_SCENARIO) - 1);

    switch (scenario) {
        case static_cast<uint8_t>(TestScenario::ON_REQUEST_AUTOFILL):
            TestOnRequestAutofill(driver, fdp);
            break;
        case static_cast<uint8_t>(TestScenario::PRE_SUPPRESS_KEYBOARD_IF_NEED):
            TestPreSuppressKeyboardIfNeed(driver, fdp);
            break;
        case static_cast<uint8_t>(TestScenario::FILL_ACCOUNT_SUGGESTION_WITH_URL):
            TestFillAccountSuggestionWithURL(driver, fdp);
            break;
        case static_cast<uint8_t>(TestScenario::AUTOFILL_SURFACE_CLOSED):
            TestAutofillSurfaceClosed(driver, fdp);
            break;
        case static_cast<uint8_t>(TestScenario::SEND_PARSED_PASSWORD_FORM_TO_RENDERER):
            TestSendParsedPasswordFormToRenderer(driver, fdp);
            break;
        default:
            TestFillAccountSuggestionWithURL(driver, fdp);
            break;
    }
}

}  // namespace
}  // namespace password_manager

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 1) {
        return 0;
    }
    static password_manager::FuzzerEnv* env = new password_manager::FuzzerEnv();
    FuzzedDataProvider fdp(data, size);
    
    content::TestRenderFrameHost* rfh = env->rfh();
    if (!rfh) {
        return 0;
    }
    
    static password_manager::MockPasswordManagerClient mock_client;
    password_manager::ContentPasswordManagerDriverExt driver(rfh, &mock_client);
    password_manager::RunFuzzer(&fdp, &driver);
    return 0;
}
