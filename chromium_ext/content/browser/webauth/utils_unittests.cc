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

#include <memory>
#include "content/browser/webauth/utils.h"
#include <gtest/gtest.h>

#include <array>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "device/fido/attestation_statement.h"
#include "url/gurl.h"
#include "url/origin.h"

using namespace testing;

namespace content {

class UtilsTest : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
};

device::AuthenticatorData CreateAuthenticatorData() {
  std::array<uint8_t, device::kRpIdHashLength> rp_id_hash = {};
  return device::AuthenticatorData(
      rp_id_hash, /*user_present=*/false, /*user_verified=*/false,
      /*backup_eligible=*/false, /*backup_state=*/false,
      /*sign_counter=*/0, /*attested_credential_data=*/std::nullopt,
      /*extensions=*/std::nullopt);
}

TEST_F(UtilsTest, Convert001) {
    const blink::mojom::PublicKeyCredentialMediationRequirement mediation =
        blink::mojom::PublicKeyCredentialMediationRequirement::SILENT;
    device::CredentialMediationRequirement temp = device::CredentialMediationRequirement::kSilent;
    auto result = Convert(mediation);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, Convert002) {
    const blink::mojom::PublicKeyCredentialMediationRequirement mediation =
        blink::mojom::PublicKeyCredentialMediationRequirement::OPTIONAL;
    device::CredentialMediationRequirement temp = device::CredentialMediationRequirement::kOptional;
    auto result = Convert(mediation);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, Convert003) {
    const blink::mojom::PublicKeyCredentialMediationRequirement mediation =
        blink::mojom::PublicKeyCredentialMediationRequirement::REQUIRED;
    device::CredentialMediationRequirement temp = device::CredentialMediationRequirement::kRequired;
    auto result = Convert(mediation);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, Convert004) {
    const blink::mojom::PublicKeyCredentialMediationRequirement mediation =
        blink::mojom::PublicKeyCredentialMediationRequirement::CONDITIONAL;
    device::CredentialMediationRequirement temp = device::CredentialMediationRequirement::kConditional;
    auto result = Convert(mediation);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, Convert005) {
    const blink::mojom::PublicKeyCredentialMediationRequirement mediation =
        static_cast<blink::mojom::PublicKeyCredentialMediationRequirement>(-1);
    device::CredentialMediationRequirement temp = device::CredentialMediationRequirement::kSilent;
    auto result = Convert(mediation);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, Convert006) {
    const blink::mojom::Hint hint = blink::mojom::Hint::SECURITY_KEY;
    device::CredentialHint temp = device::CredentialHint::kSecurityKey;
    auto result = Convert(hint);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, Convert007) {
    const blink::mojom::Hint hint = blink::mojom::Hint::CLIENT_DEVICE;
    device::CredentialHint temp = device::CredentialHint::kClientDevice;
    auto result = Convert(hint);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, Convert008) {
    const blink::mojom::Hint hint = blink::mojom::Hint::HYBRID;
    device::CredentialHint temp = device::CredentialHint::kHintHybrid;
    auto result = Convert(hint);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, Convert009) {
    const blink::mojom::Hint hint = static_cast<blink::mojom::Hint>(-1);
    device::CredentialHint temp = device::CredentialHint::kHintHybrid;
    auto result = Convert(hint);
    EXPECT_EQ(result, temp);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreation) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1, 2, 3, 4};
  options->mediation =
      blink::mojom::PublicKeyCredentialMediationRequirement::CONDITIONAL;
  options->timeout = base::Seconds(3);
  options->hints = {blink::mojom::Hint::SECURITY_KEY,
                    blink::mojom::Hint::HYBRID};
  const url::Origin origin =
      url::Origin::Create(GURL("https://example.com/path"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  EXPECT_EQ(result.origin, "https://example.com");
  EXPECT_EQ(result.challenge, options->challenge);
  EXPECT_EQ(result.mediation,
            device::CredentialMediationRequirement::kConditional);
  EXPECT_EQ(result.timeout, base::Seconds(3));
  ASSERT_EQ(result.hints.size(), 2u);
  EXPECT_EQ(result.hints[0], device::CredentialHint::kSecurityKey);
  EXPECT_EQ(result.hints[1], device::CredentialHint::kHintHybrid);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonWithoutChallenge) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge.reset();
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  EXPECT_TRUE(result.origin.empty());
  EXPECT_TRUE(result.challenge.empty());
  EXPECT_EQ(result.mediation, device::CredentialMediationRequirement::kSilent);
  EXPECT_FALSE(result.extensions.has_value());
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonWithLargeBlobExtension) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{5, 6, 7};
  options->mediation =
      blink::mojom::PublicKeyCredentialMediationRequirement::REQUIRED;
  options->timeout = base::Seconds(5);
  options->hints = {blink::mojom::Hint::CLIENT_DEVICE};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->large_blob_read = true;
  options->extensions->large_blob_write = std::vector<uint8_t>{0xab, 0xcd};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  EXPECT_EQ(result.origin, "https://example.com");
  EXPECT_EQ(result.challenge, *options->challenge);
  EXPECT_EQ(result.mediation,
            device::CredentialMediationRequirement::kRequired);
  EXPECT_EQ(result.timeout, base::Seconds(5));
  ASSERT_EQ(result.hints.size(), 1u);
  EXPECT_EQ(result.hints[0], device::CredentialHint::kClientDevice);
  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> extension_value =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(extension_value.has_value());
  const base::Value::Dict* large_blob =
      extension_value->GetDict().FindDict("largeBlob");
  ASSERT_NE(large_blob, nullptr);
  EXPECT_EQ(large_blob->FindBool("read"), true);
  const std::string* write = large_blob->FindString("write");
  ASSERT_NE(write, nullptr);
  // New code uses Base64UrlEncodeOmitPadding instead of HexEncode
  EXPECT_EQ(*write, "q80");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonWithEmptyExtension) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();

  device::CtapRequestExtraCommon result = CreateCtapRequestExtraCommon(
      url::Origin::Create(GURL("https://example.com")), options);

  // New code returns nullopt when no extensions are set (empty dict)
  EXPECT_FALSE(result.extensions.has_value());
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonWithWriteOnlyExtension) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->large_blob_read = false;
  options->extensions->large_blob_write = std::vector<uint8_t>{0x01, 0xfe};

  device::CtapRequestExtraCommon result = CreateCtapRequestExtraCommon(
      url::Origin::Create(GURL("https://example.com")), options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> extension_value =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(extension_value.has_value());
  const base::Value::Dict* large_blob =
      extension_value->GetDict().FindDict("largeBlob");
  ASSERT_NE(large_blob, nullptr);
  // "read" is only set when large_blob_read is true; false means omitted
  EXPECT_FALSE(large_blob->FindBool("read").has_value());
  const std::string* write = large_blob->FindString("write");
  ASSERT_NE(write, nullptr);
  // New code uses Base64UrlEncodeOmitPadding instead of HexEncode
  EXPECT_EQ(*write, "Af4");
}

TEST_F(UtilsTest, CreateCtapMakeCredentialRequestExtraCopiesFormats) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {8, 9};
  options->attestation_formats = {"packed", "none"};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapMakeCredentialRequestExtra result =
      CreateCtapMakeCredentialRequestExtra(origin, options);

  EXPECT_EQ(result.common.origin, "https://example.com");
  EXPECT_EQ(result.common.challenge, options->challenge);
  EXPECT_EQ(result.attestation_formats, options->attestation_formats);
}

TEST_F(UtilsTest, CreateCtapGetAssertionRequestExtraCopiesCommonFields) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{10, 11};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapGetAssertionRequestExtra result =
      CreateCtapGetAssertionRequestExtra(origin, options);

  EXPECT_EQ(result.common.origin, "https://example.com");
  EXPECT_EQ(result.common.challenge, *options->challenge);
  EXPECT_FALSE(result.common.extensions.has_value());
}

TEST_F(UtilsTest, CreateMakeCredentialResponseCopiesExtraFields) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.client_data_json = {1, 2};
  extra.common.raw_id = {1, 2, 3};
  extra.common.authenticator_data = {4, 5};
  extra.attestation_object = {6, 7};
  extra.public_key = {8, 9};
  extra.public_key_algorithm = -7;
  response.response_extra = std::move(extra);
  response.transports = base::flat_set<device::FidoTransportProtocol>{
      device::FidoTransportProtocol::kUsbHumanInterfaceDevice};

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->info);
  EXPECT_EQ(result->info->client_data_json, (std::vector<uint8_t>{1, 2}));
  EXPECT_EQ(result->info->raw_id, (std::vector<uint8_t>{1, 2, 3}));
  EXPECT_EQ(result->info->id, "AQID");
  EXPECT_EQ(result->info->authenticator_data, (std::vector<uint8_t>{4, 5}));
  EXPECT_EQ(result->attestation_object, (std::vector<uint8_t>{6, 7}));
  EXPECT_EQ(result->public_key_der, (std::vector<uint8_t>{8, 9}));
  EXPECT_EQ(result->public_key_algo, -7);
  EXPECT_EQ(result->transports.size(), 1u);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseAllowsMissingTransports) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  response.response_extra = std::move(extra);
  response.transports.reset();

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->transports.empty());
  EXPECT_EQ(result->info->id, "AQ");
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesExtensionResults) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_data_json = {2, 3};
  extra.common.raw_id = {4, 5};
  extra.common.authenticator_data = {6, 7};
  extra.common.authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  // New code uses Base64UrlDecode instead of HexStringToBytes
  extra.common.client_extension_results =
      R"({"largeBlob":{"written":true,"blob":"Cgs"},"authData":"AQI"})";
  extra.signature = {8, 9};
  extra.user_handle = {10, 11};
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->info);
  EXPECT_EQ(result->info->client_data_json, (std::vector<uint8_t>{2, 3}));
  EXPECT_EQ(result->info->raw_id, (std::vector<uint8_t>{4, 5}));
  EXPECT_EQ(result->info->authenticator_data, (std::vector<uint8_t>{6, 7}));
  EXPECT_EQ(result->signature, (std::vector<uint8_t>{8, 9}));
  EXPECT_EQ(result->user_handle, (std::vector<uint8_t>{10, 11}));
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_large_blob);
  EXPECT_TRUE(result->extensions->echo_large_blob_written);
  EXPECT_TRUE(result->extensions->large_blob_written);
  ASSERT_TRUE(result->extensions->large_blob.has_value());
  EXPECT_EQ(*result->extensions->large_blob,
            (std::vector<uint8_t>{0x0a, 0x0b}));
  ASSERT_TRUE(result->extensions->digital_shield_data.has_value());
  EXPECT_EQ(*result->extensions->digital_shield_data,
            (std::vector<uint8_t>{0x01, 0x02}));
}

TEST_F(UtilsTest, CreateGetAssertionResponseIgnoresInvalidExtensionJson) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results = "not-json";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_FALSE(result->extensions->echo_large_blob);
  EXPECT_FALSE(result->extensions->echo_large_blob_written);
  EXPECT_FALSE(result->extensions->large_blob.has_value());
  EXPECT_FALSE(result->extensions->digital_shield_data.has_value());
}

TEST_F(UtilsTest, CreateGetAssertionResponseIgnoresNonDictionaryJson) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results = R"(["largeBlob"])";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->extensions->echo_large_blob);
  EXPECT_FALSE(result->extensions->digital_shield_data.has_value());
}

TEST_F(UtilsTest, CreateGetAssertionResponseHandlesMissingExtensionFields) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results =
      R"({"largeBlob":{"written":"not-a-bool"}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->extensions->echo_large_blob);
  EXPECT_FALSE(result->extensions->echo_large_blob_written);
  EXPECT_FALSE(result->extensions->large_blob.has_value());
  EXPECT_FALSE(result->extensions->digital_shield_data.has_value());
}

TEST_F(UtilsTest, CreateGetAssertionResponseIgnoresNonDictionaryLargeBlob) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results = R"({"largeBlob":false})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->extensions->echo_large_blob);
  EXPECT_FALSE(result->extensions->echo_large_blob_written);
}

// ===== New tests for BuildExtensionsDict in creation options =====

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithAppidExclude) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1, 2, 3};
  options->appid_exclude = "https://exclude.example.com";
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const std::string* appid_exclude =
      ext->GetDict().FindString("appidExclude");
  ASSERT_NE(appid_exclude, nullptr);
  EXPECT_EQ(*appid_exclude, "https://exclude.example.com");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithHmacCreateSecret) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->hmac_create_secret = true;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  auto hmac = ext->GetDict().FindBool("hmacCreateSecret");
  ASSERT_TRUE(hmac.has_value());
  EXPECT_TRUE(*hmac);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithProtectionPolicy) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->protection_policy =
      blink::mojom::ProtectionPolicy::UV_REQUIRED;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const std::string* policy =
      ext->GetDict().FindString("credentialProtectionPolicy");
  ASSERT_NE(policy, nullptr);
  EXPECT_EQ(*policy, "userVerificationRequired");
  const std::string* cred_protect = ext->GetDict().FindString("credProtect");
  ASSERT_NE(cred_protect, nullptr);
  EXPECT_EQ(*cred_protect, "userVerificationRequired");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithProtectionPolicyNone) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->protection_policy = blink::mojom::ProtectionPolicy::NONE;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const std::string* policy =
      ext->GetDict().FindString("credentialProtectionPolicy");
  ASSERT_NE(policy, nullptr);
  EXPECT_EQ(*policy, "userVerificationOptional");
  const std::string* cred_protect = ext->GetDict().FindString("credProtect");
  ASSERT_NE(cred_protect, nullptr);
  EXPECT_EQ(*cred_protect, "userVerificationOptional");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithEnforcePolicy) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->protection_policy =
      blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
  options->enforce_protection_policy = true;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const std::string* policy =
      ext->GetDict().FindString("credentialProtectionPolicy");
  ASSERT_NE(policy, nullptr);
  EXPECT_EQ(*policy, "userVerificationOptionalWithCredentialIDList");
  auto enforce = ext->GetDict().FindBool("enforceCredentialProtectionPolicy");
  ASSERT_TRUE(enforce.has_value());
  EXPECT_TRUE(*enforce);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithMinPinLength) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->min_pin_length_requested = true;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  auto min_pin = ext->GetDict().FindBool("minPinLength");
  ASSERT_TRUE(min_pin.has_value());
  EXPECT_TRUE(*min_pin);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithCredProps) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->cred_props = true;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  auto cred_props = ext->GetDict().FindBool("credProps");
  ASSERT_TRUE(cred_props.has_value());
  EXPECT_TRUE(*cred_props);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithLargeBlobRequired) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->large_blob_enable = device::LargeBlobSupport::kRequired;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* large_blob = ext->GetDict().FindDict("largeBlob");
  ASSERT_NE(large_blob, nullptr);
  const std::string* support = large_blob->FindString("support");
  ASSERT_NE(support, nullptr);
  EXPECT_EQ(*support, "required");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithLargeBlobPreferred) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->large_blob_enable = device::LargeBlobSupport::kPreferred;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* large_blob = ext->GetDict().FindDict("largeBlob");
  ASSERT_NE(large_blob, nullptr);
  const std::string* support = large_blob->FindString("support");
  ASSERT_NE(support, nullptr);
  EXPECT_EQ(*support, "preferred");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithLargeBlobNotRequested) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->large_blob_enable = device::LargeBlobSupport::kNotRequested;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  // kNotRequested should not produce a largeBlob extension
  EXPECT_FALSE(result.extensions.has_value());
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithCredBlob) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->cred_blob = std::vector<uint8_t>{0xaa, 0xbb};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const std::string* cred_blob = ext->GetDict().FindString("credBlob");
  ASSERT_NE(cred_blob, nullptr);
  EXPECT_EQ(*cred_blob, "qrs");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithPayment) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->is_payment_credential_creation = true;
  options->payment_browser_bound_key_parameters =
      std::vector<device::PublicKeyCredentialParams::CredentialInfo>{
          {device::CredentialType::kPublicKey, -7}};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* payment = ext->GetDict().FindDict("payment");
  ASSERT_NE(payment, nullptr);
  auto is_payment = payment->FindBool("isPayment");
  ASSERT_TRUE(is_payment.has_value());
  EXPECT_TRUE(*is_payment);
  const base::Value::List* params =
      payment->FindList("browserBoundPubKeyCredParams");
  ASSERT_NE(params, nullptr);
  ASSERT_EQ(params->size(), 1u);
  const base::Value::Dict& param_dict = (*params)[0].GetDict();
  EXPECT_EQ(*param_dict.FindString("type"), "public-key");
  EXPECT_EQ(*param_dict.FindInt("alg"), -7);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithPaymentOnlyParams) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->is_payment_credential_creation = false;
  options->payment_browser_bound_key_parameters =
      std::vector<device::PublicKeyCredentialParams::CredentialInfo>{
          {device::CredentialType::kPublicKey, -257}};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* payment = ext->GetDict().FindDict("payment");
  ASSERT_NE(payment, nullptr);
  auto is_payment = payment->FindBool("isPayment");
  ASSERT_TRUE(is_payment.has_value());
  EXPECT_FALSE(*is_payment);
  const base::Value::List* params =
      payment->FindList("browserBoundPubKeyCredParams");
  ASSERT_NE(params, nullptr);
  ASSERT_EQ(params->size(), 1u);
  EXPECT_EQ(*((*params)[0].GetDict().FindString("type")), "public-key");
  EXPECT_EQ(*((*params)[0].GetDict().FindInt("alg")), -257);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithRemoteDesktop) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->remote_desktop_client_override =
      blink::mojom::RemoteDesktopClientOverride::New();
  options->remote_desktop_client_override->origin =
      url::Origin::Create(GURL("https://remote.example.com"));
  options->remote_desktop_client_override->same_origin_with_ancestors = true;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* rdc =
      ext->GetDict().FindDict("remoteDesktopClientOverride");
  ASSERT_NE(rdc, nullptr);
  EXPECT_EQ(*rdc->FindString("origin"), "https://remote.example.com");
  EXPECT_EQ(*rdc->FindBool("sameOriginWithAncestors"), true);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithPrf) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->prf_enable = true;
  options->prf_input = blink::mojom::PRFValues::New();
  options->prf_input->first = {0x11, 0x22};
  options->prf_input->second = std::vector<uint8_t>{0x33, 0x44};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* prf = ext->GetDict().FindDict("prf");
  ASSERT_NE(prf, nullptr);
  const base::Value::Dict* eval = prf->FindDict("eval");
  ASSERT_NE(eval, nullptr);
  const std::string* first = eval->FindString("first");
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(*first, "ESI");
  const std::string* second = eval->FindString("second");
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(*second, "M0Q");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithoutPrfWhenDisabled) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->prf_enable = false;
  options->prf_input = blink::mojom::PRFValues::New();
  options->prf_input->first = {0x11, 0x22};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  // prf should not appear when prf_enable is false
  EXPECT_FALSE(result.extensions.has_value());
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithMultipleExtensions) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  options->appid_exclude = "https://exclude.example.com";
  options->hmac_create_secret = true;
  options->cred_props = true;
  options->large_blob_enable = device::LargeBlobSupport::kPreferred;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  EXPECT_NE(ext->GetDict().FindString("appidExclude"), nullptr);
  EXPECT_TRUE(ext->GetDict().FindBool("hmacCreateSecret").has_value());
  EXPECT_TRUE(ext->GetDict().FindBool("credProps").has_value());
  EXPECT_NE(ext->GetDict().FindDict("largeBlob"), nullptr);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForCreationWithNoExtensions) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();
  options->challenge = {1};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  EXPECT_FALSE(result.extensions.has_value());
}

// ===== New tests for BuildExtensionsDict in request options =====

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForRequestWithAppid) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->appid = "https://appid.example.com";
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const std::string* appid = ext->GetDict().FindString("appid");
  ASSERT_NE(appid, nullptr);
  EXPECT_EQ(*appid, "https://appid.example.com");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForRequestWithGetCredBlob) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->get_cred_blob = true;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  auto get_cred_blob = ext->GetDict().FindBool("getCredBlob");
  ASSERT_TRUE(get_cred_blob.has_value());
  EXPECT_TRUE(*get_cred_blob);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForRequestWithPrfEval) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->prf = true;
  auto prf_val = blink::mojom::PRFValues::New();
  prf_val->first = {0x11, 0x22};
  options->extensions->prf_inputs.push_back(std::move(prf_val));
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* prf = ext->GetDict().FindDict("prf");
  ASSERT_NE(prf, nullptr);
  const base::Value::Dict* eval = prf->FindDict("eval");
  ASSERT_NE(eval, nullptr);
  const std::string* first = eval->FindString("first");
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(*first, "ESI");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForRequestWithPrfEvalByCredential) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->prf = true;
  // PRF input with id → evalByCredential
  auto prf_with_id = blink::mojom::PRFValues::New();
  prf_with_id->id = std::vector<uint8_t>{0x05};
  prf_with_id->first = {0x11, 0x22};
  options->extensions->prf_inputs.push_back(std::move(prf_with_id));
  // PRF input without id → eval
  auto prf_eval = blink::mojom::PRFValues::New();
  prf_eval->first = {0x33, 0x44};
  options->extensions->prf_inputs.push_back(std::move(prf_eval));
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* prf = ext->GetDict().FindDict("prf");
  ASSERT_NE(prf, nullptr);
  const base::Value::Dict* eval = prf->FindDict("eval");
  ASSERT_NE(eval, nullptr);
  EXPECT_EQ(*eval->FindString("first"), "M0Q");
  const base::Value::Dict* eval_by_cred = prf->FindDict("evalByCredential");
  ASSERT_NE(eval_by_cred, nullptr);
  // The key is Base64UrlEncodeOmitPadding({0x05}) = "BQ"
  const base::Value::Dict* cred_entry = eval_by_cred->FindDict("BQ");
  ASSERT_NE(cred_entry, nullptr);
  EXPECT_EQ(*cred_entry->FindString("first"), "ESI");
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForRequestWithPrfDuplicateEval) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->prf = true;
  // Two PRF inputs without id → first goes to eval, second is skipped
  auto prf1 = blink::mojom::PRFValues::New();
  prf1->first = {0x11, 0x22};
  options->extensions->prf_inputs.push_back(std::move(prf1));
  auto prf2 = blink::mojom::PRFValues::New();
  prf2->first = {0x33, 0x44};
  options->extensions->prf_inputs.push_back(std::move(prf2));
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* prf = ext->GetDict().FindDict("prf");
  ASSERT_NE(prf, nullptr);
  const base::Value::Dict* eval = prf->FindDict("eval");
  ASSERT_NE(eval, nullptr);
  // Only the first eval should be kept
  EXPECT_EQ(*eval->FindString("first"), "ESI");
  // No evalByCredential since no inputs had id
  EXPECT_EQ(prf->FindDict("evalByCredential"), nullptr);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForRequestWithPaymentParams) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->payment_browser_bound_key_parameters =
      std::vector<device::PublicKeyCredentialParams::CredentialInfo>{
          {device::CredentialType::kPublicKey, -7},
          {device::CredentialType::kPublicKey, -257}};
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* payment = ext->GetDict().FindDict("payment");
  ASSERT_NE(payment, nullptr);
  const base::Value::List* params =
      payment->FindList("browserBoundPubKeyCredParams");
  ASSERT_NE(params, nullptr);
  ASSERT_EQ(params->size(), 2u);
  EXPECT_EQ(*((*params)[0].GetDict().FindString("type")), "public-key");
  EXPECT_EQ(*((*params)[0].GetDict().FindInt("alg")), -7);
  EXPECT_EQ(*((*params)[1].GetDict().FindString("type")), "public-key");
  EXPECT_EQ(*((*params)[1].GetDict().FindInt("alg")), -257);
}

TEST_F(UtilsTest, CreateCtapRequestExtraCommonForRequestWithRemoteDesktop) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();
  options->challenge = std::vector<uint8_t>{1};
  options->extensions =
      blink::mojom::AuthenticationExtensionsClientInputs::New();
  options->extensions->remote_desktop_client_override =
      blink::mojom::RemoteDesktopClientOverride::New();
  options->extensions->remote_desktop_client_override->origin =
      url::Origin::Create(GURL("https://rd.example.com"));
  options->extensions->remote_desktop_client_override->same_origin_with_ancestors =
      false;
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  device::CtapRequestExtraCommon result =
      CreateCtapRequestExtraCommon(origin, options);

  ASSERT_TRUE(result.extensions.has_value());
  std::optional<base::Value> ext =
      base::JSONReader::Read(*result.extensions, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ext.has_value());
  const base::Value::Dict* rdc =
      ext->GetDict().FindDict("remoteDesktopClientOverride");
  ASSERT_NE(rdc, nullptr);
  EXPECT_EQ(*rdc->FindString("origin"), "https://rd.example.com");
  EXPECT_EQ(*rdc->FindBool("sameOriginWithAncestors"), false);
}

// ===== New tests for CreateMakeCredentialResponse extension parsing =====

TEST_F(UtilsTest, CreateMakeCredentialResponseSetsAuthenticatorAttachment) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_EQ(result->authenticator_attachment,
            device::AuthenticatorAttachment::kPlatform);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseParsesHmacCreateSecret) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results =
      R"({"hmacCreateSecret":true})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_hmac_create_secret);
  EXPECT_TRUE(result->hmac_create_secret);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseParsesCredProps) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results =
      R"({"credProps":{"rk":true}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_cred_props);
  EXPECT_TRUE(result->has_cred_props_rk);
  EXPECT_TRUE(result->cred_props_rk);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseParsesCredPropsWithoutRk) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results = R"({"credProps":{}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_cred_props);
  EXPECT_FALSE(result->has_cred_props_rk);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseParsesLargeBlob) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results =
      R"({"largeBlob":{"supported":true}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_large_blob);
  EXPECT_TRUE(result->supports_large_blob);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseParsesCredBlob) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results = R"({"credBlob":true})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_cred_blob);
  EXPECT_TRUE(result->cred_blob);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseParsesPaymentResponse) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  // Base64Url of {0xde, 0xad, 0xbe, 0xef} = "3q2-7w"
  extra.common.client_extension_results =
      R"({"payment":{"browserBoundSignature":{"signature":"3q2-7w"}}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->payment);
  EXPECT_EQ(result->payment->browser_bound_signature,
            (std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}));
}

TEST_F(UtilsTest, CreateMakeCredentialResponsePaymentMissingSignature) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results =
      R"({"payment":{"browserBoundSignature":{}}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->payment);
}

TEST_F(UtilsTest, CreateMakeCredentialResponsePaymentMissingBoundSignature) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results = R"({"payment":{}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->payment);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseParsesPrfEnabled) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  // Base64Url of {0x11, 0x22} = "ESI"
  extra.common.client_extension_results =
      R"({"prf":{"enabled":true,"results":{"first":"ESI"}}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_prf);
  EXPECT_TRUE(result->prf);
  ASSERT_TRUE(result->prf_results);
  EXPECT_EQ(result->prf_results->first, (std::vector<uint8_t>{0x11, 0x22}));
}

TEST_F(UtilsTest, CreateMakeCredentialResponsePrfDisabled) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results =
      R"({"prf":{"enabled":false,"results":{"first":"ESI"}}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  // "enabled" field exists (even if false) → echo_prf=true, prf=false
  EXPECT_TRUE(result->echo_prf);
  EXPECT_FALSE(result->prf);
  ASSERT_TRUE(result->prf_results);
  EXPECT_EQ(result->prf_results->first, (std::vector<uint8_t>{0x11, 0x22}));
}

TEST_F(UtilsTest, CreateMakeCredentialResponsePrfWithSecond) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results =
      R"({"prf":{"enabled":true,"results":{"first":"ESI","second":"M0Q"}}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_prf);
  ASSERT_TRUE(result->prf_results);
  EXPECT_EQ(result->prf_results->first, (std::vector<uint8_t>{0x11, 0x22}));
  ASSERT_TRUE(result->prf_results->second);
  EXPECT_EQ(*result->prf_results->second,
            (std::vector<uint8_t>{0x33, 0x44}));
}

TEST_F(UtilsTest, CreateMakeCredentialResponsePrfMissingResults) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results =
      R"({"prf":{"enabled":true}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_prf);
  EXPECT_TRUE(result->prf);
  // prf_results is nullptr because "results" key is missing
  EXPECT_FALSE(result->prf_results);
}

TEST_F(UtilsTest, CreateMakeCredentialResponsePrfResultsMissingFirst) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results =
      R"({"prf":{"enabled":true,"results":{}}})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->echo_prf);
  EXPECT_FALSE(result->prf_results);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseIgnoresInvalidExtensionJson) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results = "not-json";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->echo_hmac_create_secret);
  EXPECT_FALSE(result->echo_cred_props);
  EXPECT_FALSE(result->echo_large_blob);
  EXPECT_FALSE(result->echo_prf);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseIgnoresNonDictExtensionJson) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results = R"(["hmacCreateSecret"])";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->echo_hmac_create_secret);
}

TEST_F(UtilsTest, CreateMakeCredentialResponseWithEmptyExtensionResults) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1};
  extra.common.client_extension_results = "{}";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->echo_hmac_create_secret);
  EXPECT_FALSE(result->echo_cred_props);
  EXPECT_FALSE(result->echo_large_blob);
  EXPECT_FALSE(result->echo_cred_blob);
  EXPECT_FALSE(result->echo_prf);
  EXPECT_FALSE(result->payment);
}

// ===== New tests for CreateGetAssertionResponse extension parsing =====

TEST_F(UtilsTest, CreateGetAssertionResponseParsesAppidExtension) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results = R"({"appid":true})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_appid_extension);
  EXPECT_TRUE(result->extensions->appid_extension);
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesAppidFalse) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results = R"({"appid":false})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_appid_extension);
  EXPECT_FALSE(result->extensions->appid_extension);
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesGetCredBlob) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  // Base64Url of {0xaa, 0xbb} = "qrs"
  extra.common.client_extension_results = R"({"getCredBlob":"qrs"})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  ASSERT_TRUE(result->extensions->get_cred_blob);
  EXPECT_EQ(*result->extensions->get_cred_blob,
            (std::vector<uint8_t>{0xaa, 0xbb}));
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesPaymentResponse) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  // Base64Url of {0xde, 0xad, 0xbe, 0xef} = "3q2-7w"
  extra.common.client_extension_results =
      R"({"payment":{"browserBoundSignature":{"signature":"3q2-7w"}}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  ASSERT_TRUE(result->extensions->payment);
  EXPECT_EQ(result->extensions->payment->browser_bound_signature,
            (std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}));
}

TEST_F(UtilsTest, CreateGetAssertionResponsePaymentMissingSignature) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results =
      R"({"payment":{"browserBoundSignature":{}}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_FALSE(result->extensions->payment);
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesPrfResults) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results =
      R"({"prf":{"results":{"first":"ESI","second":"M0Q"}}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_prf);
  ASSERT_TRUE(result->extensions->prf_results);
  EXPECT_EQ(result->extensions->prf_results->first,
            (std::vector<uint8_t>{0x11, 0x22}));
  ASSERT_TRUE(result->extensions->prf_results->second);
  EXPECT_EQ(*result->extensions->prf_results->second,
            (std::vector<uint8_t>{0x33, 0x44}));
}

TEST_F(UtilsTest, CreateGetAssertionResponsePrfMissingResults) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results = R"({"prf":{}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_prf);
  EXPECT_FALSE(result->extensions->prf_results);
}

TEST_F(UtilsTest, CreateGetAssertionResponsePrfResultsMissingFirst) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results =
      R"({"prf":{"results":{"second":"M0Q"}}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_prf);
  EXPECT_FALSE(result->extensions->prf_results);
}

TEST_F(UtilsTest, CreateGetAssertionResponsePrfResultsWithSecondOnly) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  // Only first is required, second is optional
  extra.common.client_extension_results =
      R"({"prf":{"results":{"first":"ESI"}}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_prf);
  ASSERT_TRUE(result->extensions->prf_results);
  EXPECT_EQ(result->extensions->prf_results->first,
            (std::vector<uint8_t>{0x11, 0x22}));
  EXPECT_FALSE(result->extensions->prf_results->second);
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesDigitalShieldData) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  // Base64Url of {0x01, 0x02} = "AQI"
  extra.common.client_extension_results = R"({"authData":"AQI"})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  ASSERT_TRUE(result->extensions->digital_shield_data);
  EXPECT_EQ(*result->extensions->digital_shield_data,
            (std::vector<uint8_t>{0x01, 0x02}));
}

TEST_F(UtilsTest, CreateGetAssertionResponseSetsAuthenticatorAttachment) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.authenticator_attachment =
      device::AuthenticatorAttachment::kCrossPlatform;
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  EXPECT_EQ(result->authenticator_attachment,
            device::AuthenticatorAttachment::kCrossPlatform);
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesMultipleExtensions) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  // Test largeBlob + appid + authData together
  extra.common.client_extension_results =
      R"({"largeBlob":{"written":true,"blob":"Cgs"},)"
      R"("appid":true,"authData":"AQI"})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_large_blob);
  EXPECT_TRUE(result->extensions->echo_large_blob_written);
  EXPECT_TRUE(result->extensions->large_blob_written);
  ASSERT_TRUE(result->extensions->large_blob);
  EXPECT_EQ(*result->extensions->large_blob,
            (std::vector<uint8_t>{0x0a, 0x0b}));
  EXPECT_TRUE(result->extensions->echo_appid_extension);
  EXPECT_TRUE(result->extensions->appid_extension);
  ASSERT_TRUE(result->extensions->digital_shield_data);
  EXPECT_EQ(*result->extensions->digital_shield_data,
            (std::vector<uint8_t>{0x01, 0x02}));
}

TEST_F(UtilsTest, CreateGetAssertionResponseEmptyExtensionResults) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results = "{}";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_FALSE(result->extensions->echo_large_blob);
  EXPECT_FALSE(result->extensions->echo_appid_extension);
  EXPECT_FALSE(result->extensions->echo_prf);
  EXPECT_FALSE(result->extensions->digital_shield_data.has_value());
  EXPECT_FALSE(result->extensions->get_cred_blob.has_value());
  EXPECT_FALSE(result->extensions->payment);
}

TEST_F(UtilsTest, CreateGetAssertionResponseIgnoresInvalidBase64InBlob) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_extension_results =
      R"({"largeBlob":{"blob":"!!!invalid"}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_large_blob);
  EXPECT_FALSE(result->extensions->large_blob.has_value());
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesLargeBlobWithoutWritten) {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  // largeBlob dict exists but no "written" field
  extra.common.client_extension_results =
      R"({"largeBlob":{"blob":"Cgs"}})";
  response.response_extra = std::move(extra);

  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(response);

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_large_blob);
  EXPECT_FALSE(result->extensions->echo_large_blob_written);
  ASSERT_TRUE(result->extensions->large_blob);
  EXPECT_EQ(*result->extensions->large_blob,
            (std::vector<uint8_t>{0x0a, 0x0b}));
}

// ===== All-fields tests for FillResponseExtensions =====

TEST_F(UtilsTest, CreateMakeCredentialResponseParsesAllExtensions) {
  device::AuthenticatorMakeCredentialResponse response(
      std::nullopt, device::AttestationObject(
                        CreateAuthenticatorData(),
                        std::make_unique<device::NoneAttestationStatement>()));
  AuthenticatorMakeCredentialResponseExtra extra;
  extra.common.raw_id = {1, 2, 3};
  extra.common.authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  extra.common.client_extension_results =
      R"({)"
      R"("hmacCreateSecret":true,)"
      R"("credProps":{"rk":true},)"
      R"("largeBlob":{"supported":true},)"
      R"("credBlob":false,)"
      R"("payment":{"browserBoundSignature":{"signature":"3q2-7w"}},)"
      R"("prf":{"enabled":true,"results":{"first":"ESI","second":"M0Q"}})"
      R"(})";
  response.response_extra = std::move(extra);

  blink::mojom::MakeCredentialAuthenticatorResponsePtr result =
      CreateMakeCredentialResponse(response);

  ASSERT_TRUE(result);
  // authenticator_attachment
  EXPECT_EQ(result->authenticator_attachment,
            device::AuthenticatorAttachment::kPlatform);
  // hmacCreateSecret
  EXPECT_TRUE(result->echo_hmac_create_secret);
  EXPECT_TRUE(result->hmac_create_secret);
  // credProps
  EXPECT_TRUE(result->echo_cred_props);
  EXPECT_TRUE(result->has_cred_props_rk);
  EXPECT_TRUE(result->cred_props_rk);
  // largeBlob
  EXPECT_TRUE(result->echo_large_blob);
  EXPECT_TRUE(result->supports_large_blob);
  // credBlob
  EXPECT_TRUE(result->echo_cred_blob);
  EXPECT_FALSE(result->cred_blob);
  // payment
  ASSERT_TRUE(result->payment);
  EXPECT_EQ(result->payment->browser_bound_signature,
            (std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}));
  // prf
  EXPECT_TRUE(result->echo_prf);
  EXPECT_TRUE(result->prf);
  ASSERT_TRUE(result->prf_results);
  EXPECT_EQ(result->prf_results->first, (std::vector<uint8_t>{0x11, 0x22}));
  ASSERT_TRUE(result->prf_results->second);
  EXPECT_EQ(*result->prf_results->second,
            (std::vector<uint8_t>{0x33, 0x44}));
}

device::AuthenticatorGetAssertionResponse
CreateAllExtensionsGetAssertionResponse() {
  device::AuthenticatorGetAssertionResponse response(
      CreateAuthenticatorData(), std::vector<uint8_t>{1}, std::nullopt);
  AuthenticatorGetAssertionResponseExtra extra;
  extra.common.client_data_json = {2, 3};
  extra.common.raw_id = {4, 5};
  extra.common.authenticator_data = {6, 7};
  extra.common.authenticator_attachment =
      device::AuthenticatorAttachment::kCrossPlatform;
  extra.common.client_extension_results =
      R"({)"
      R"("largeBlob":{"written":true,"blob":"Cgs"},)"
      R"("appid":false,)"
      R"("getCredBlob":"qrs",)"
      R"("payment":{"browserBoundSignature":{"signature":"3q2-7w"}},)"
      R"("prf":{"results":{"first":"ESI","second":"M0Q"}},)"
      R"("authData":"AQI")"
      R"(})";
  extra.signature = {8, 9};
  extra.user_handle = {10, 11};
  response.response_extra = std::move(extra);
  return response;
}

TEST_F(UtilsTest, CreateGetAssertionResponseParsesAllExtensions) {
  blink::mojom::GetAssertionAuthenticatorResponsePtr result =
      CreateGetAssertionResponse(CreateAllExtensionsGetAssertionResponse());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->info);
  // authenticator_attachment
  EXPECT_EQ(result->authenticator_attachment,
            device::AuthenticatorAttachment::kCrossPlatform);
  // largeBlob
  ASSERT_TRUE(result->extensions);
  EXPECT_TRUE(result->extensions->echo_large_blob);
  EXPECT_TRUE(result->extensions->echo_large_blob_written);
  EXPECT_TRUE(result->extensions->large_blob_written);
  ASSERT_TRUE(result->extensions->large_blob);
  EXPECT_EQ(*result->extensions->large_blob,
            (std::vector<uint8_t>{0x0a, 0x0b}));
  // appid
  EXPECT_TRUE(result->extensions->echo_appid_extension);
  EXPECT_FALSE(result->extensions->appid_extension);
  // getCredBlob
  ASSERT_TRUE(result->extensions->get_cred_blob);
  EXPECT_EQ(*result->extensions->get_cred_blob,
            (std::vector<uint8_t>{0xaa, 0xbb}));
  // payment
  ASSERT_TRUE(result->extensions->payment);
  EXPECT_EQ(result->extensions->payment->browser_bound_signature,
            (std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}));
  // prf
  EXPECT_TRUE(result->extensions->echo_prf);
  ASSERT_TRUE(result->extensions->prf_results);
  EXPECT_EQ(result->extensions->prf_results->first,
            (std::vector<uint8_t>{0x11, 0x22}));
  ASSERT_TRUE(result->extensions->prf_results->second);
  EXPECT_EQ(*result->extensions->prf_results->second,
            (std::vector<uint8_t>{0x33, 0x44}));
  // digital shield data (authData)
  ASSERT_TRUE(result->extensions->digital_shield_data);
  EXPECT_EQ(*result->extensions->digital_shield_data,
            (std::vector<uint8_t>{0x01, 0x02}));
}

}  // namespace content
