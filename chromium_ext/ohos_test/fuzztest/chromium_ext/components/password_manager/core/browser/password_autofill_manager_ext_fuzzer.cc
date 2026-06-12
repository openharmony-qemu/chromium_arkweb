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

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "arkweb/chromium_ext/components/password_manager/core/browser/password_autofill_manager_ext.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "url/gurl.h"

namespace password_manager {

static std::unique_ptr<blink::DummyPageHolder> g_page_holder = nullptr;
 
struct Environment {
    Environment() {
        base::CommandLine::Init(0, nullptr);
        blink_environment_.SetUp();
        g_page_holder = std::make_unique<blink::DummyPageHolder>();
    }
 
    ~Environment() {
        g_page_holder.reset();
    }
 
    content::BlinkTestEnvironmentWithIsolate blink_environment_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }
    static password_manager::Environment env;
    FuzzedDataProvider data_provider(data, size);

    StubPasswordManagerDriver driver;
    autofill::TestAutofillClient autofill_client;
    StubPasswordManagerClient password_client;

    GURL page_url(data_provider.ConsumeRandomLengthString());
    std::u16string username = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    std::u16string password = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());

    PasswordAutofillManagerExt manager_ext(&driver, &autofill_client, &password_client);

    (void)manager_ext.CanFillAccountSuggestion(page_url);
    (void)manager_ext.FillAccountSuggestion(page_url, username, password);

    PasswordForm form_on_page;
    form_on_page.signon_realm = data_provider.ConsumeRandomLengthString();
    form_on_page.url = GURL(data_provider.ConsumeRandomLengthString());
    form_on_page.action = GURL(data_provider.ConsumeRandomLengthString());
    form_on_page.username_element = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.password_element = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.username_value = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.password_value = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.submit_element = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.new_password_element = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.confirmation_password_element = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.new_password_value = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.display_name = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.affiliated_web_realm = data_provider.ConsumeRandomLengthString();
    form_on_page.app_display_name = data_provider.ConsumeRandomLengthString();
    form_on_page.keychain_identifier = data_provider.ConsumeRandomLengthString();
    form_on_page.icon_url = GURL(data_provider.ConsumeRandomLengthString());
    form_on_page.app_icon_url = GURL(data_provider.ConsumeRandomLengthString());
    form_on_page.change_password_url = GURL(data_provider.ConsumeRandomLengthString());
    form_on_page.sender_email = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.sender_name = base::UTF8ToUTF16(data_provider.ConsumeRandomLengthString());
    form_on_page.sender_profile_image_url = GURL(data_provider.ConsumeRandomLengthString());

    form_on_page.username_element_renderer_id = autofill::FieldRendererId(data_provider.ConsumeIntegral<uint64_t>());
    form_on_page.password_element_renderer_id = autofill::FieldRendererId(data_provider.ConsumeIntegral<uint64_t>());
    form_on_page.new_password_element_renderer_id = autofill::FieldRendererId(data_provider.ConsumeIntegral<uint64_t>());
    form_on_page.confirmation_password_element_renderer_id = autofill::FieldRendererId(data_provider.ConsumeIntegral<uint64_t>());

    form_on_page.only_for_fallback = data_provider.ConsumeBool();
    form_on_page.blocked_by_user = data_provider.ConsumeBool();
    form_on_page.skip_zero_click = data_provider.ConsumeBool();
    form_on_page.was_parsed_using_autofill_predictions = data_provider.ConsumeBool();
    form_on_page.accepts_webauthn_credentials = data_provider.ConsumeBool();
    form_on_page.form_has_autofilled_value = data_provider.ConsumeBool();
    form_on_page.sharing_notification_displayed = data_provider.ConsumeBool();
    form_on_page.actor_login_approved = data_provider.ConsumeBool();

    int scheme_value = data_provider.ConsumeIntegralInRange(0, static_cast<int>(PasswordForm::Scheme::kMaxValue) - 1);
    form_on_page.scheme = static_cast<PasswordForm::Scheme>(scheme_value);

    int type_value = data_provider.ConsumeIntegralInRange(0, static_cast<int>(PasswordForm::Type::kMaxValue) - 1);
    form_on_page.type = static_cast<PasswordForm::Type>(type_value);

    int store_value = data_provider.ConsumeIntegralInRange(0, static_cast<int>(PasswordForm::Store::kMaxValue) - 1);
    form_on_page.in_store = static_cast<PasswordForm::Store>(store_value);

    autofill::FormData form_data;
    form_data.unique_renderer_id = autofill::FormRendererId(data_provider.ConsumeIntegral<uint64_t>());
    form_on_page.form_data = form_data;

    (void)form_on_page.HasPasswordElement();
    (void)form_on_page.IsSingleUsername();
    (void)form_on_page.IsLikelyLoginForm();
    (void)form_on_page.IsLikelySignupForm();
    (void)form_on_page.IsLikelyChangePasswordForm();
    (void)form_on_page.IsLikelyResetPasswordForm();
    (void)form_on_page.HasNonEmptyPasswordValue();
    (void)form_on_page.HasUsernameElement();
    (void)form_on_page.HasNewPasswordElement();
    (void)form_on_page.IsFederatedCredential();
    (void)form_on_page.IsUsingAccountStore();
    (void)form_on_page.IsUsingProfileStore();
    (void)form_on_page.GetPasswordFormType();

    autofill::PasswordFormFillData result = CreatePasswordFormFillDataWithoutPasswordInfo(form_on_page);

    (void)result;

    return 0;
}

}  // namespace password_manager