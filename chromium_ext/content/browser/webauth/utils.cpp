// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/utils.h"

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/webauthn/core/browser/common_utils.h"
#include "device/fido/ohos/ohos_authenticator.h"

namespace content {

namespace {

std::string ToString(const blink::mojom::ProtectionPolicy& protection_policy)
{
    switch (protection_policy) {
        case blink::mojom::ProtectionPolicy::NONE: {
            return "userVerificationOptional";
        }
        case blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED: {
            return "userVerificationOptionalWithCredentialIDList";
        }
        case blink::mojom::ProtectionPolicy::UV_REQUIRED: {
            return "userVerificationRequired";
        }
        case blink::mojom::ProtectionPolicy::UNSPECIFIED: {
            return "";
        }
    }
    return "";
}

std::string ToString(const device::CredentialType& type)
{
    if (type != device::CredentialType::kPublicKey) {
        LOG(WARNING) << "invalid CredentialType[" << static_cast<int>(type) << "]";
    }
    return "public-key";
}

std::string ToString(const device::LargeBlobSupport& large_blob_support)
{
    switch (large_blob_support) {
        case device::LargeBlobSupport::kNotRequested:
            return "";
        case device::LargeBlobSupport::kRequired:
            return "required";
        case device::LargeBlobSupport::kPreferred:
            return "preferred";
    }
    return "";
}

blink::mojom::AuthenticationExtensionsPaymentResponsePtr ExtractPaymentResponse(
    const base::Value::Dict& payment_dict)
{
    const base::Value::Dict* browser_bound_signature =
        payment_dict.FindDict("browserBoundSignature");
    if (!browser_bound_signature) {
        return nullptr;
    }
    const std::string* signature = browser_bound_signature->FindString("signature");
    if (!signature) {
        return nullptr;
    }
    auto decoded_signature = base::Base64UrlDecode(
        *signature, base::Base64UrlDecodePolicy::IGNORE_PADDING);
    if (!decoded_signature) {
        LOG(WARNING) << "invalid payment, invalid signature";
        return nullptr;
    }
    blink::mojom::AuthenticationExtensionsPaymentResponsePtr ret =
        blink::mojom::AuthenticationExtensionsPaymentResponse::New();
    ret->browser_bound_signature = std::move(*decoded_signature);
    return ret;
}

blink::mojom::PRFValuesPtr ExtractPrfValue(const base::Value::Dict& prf_dict)
{
    const base::Value::Dict* results_dict = prf_dict.FindDict("results");
    if (!results_dict) {
        return nullptr;
    }

    const std::string* first_str = results_dict->FindString("first");
    if (!first_str) {
        LOG(WARNING) << "invalid prf_results, no first field";
        return nullptr;
    }

    auto decoded_first = base::Base64UrlDecode(
        *first_str, base::Base64UrlDecodePolicy::IGNORE_PADDING);
    if (!decoded_first) {
        LOG(WARNING) << "invalid prf_results, invalid first field";
        return nullptr;
    }

    blink::mojom::PRFValuesPtr prf_results = blink::mojom::PRFValues::New();
    prf_results->first = std::move(*decoded_first);

    const std::string* second_str = results_dict->FindString("second");
    if (second_str) {
        prf_results->second = base::Base64UrlDecode(
            *second_str, base::Base64UrlDecodePolicy::IGNORE_PADDING);
        if (!prf_results->second) {
            LOG(WARNING) << "invalid prf.results.second field";
        }
    }
    return prf_results;
}

base::Value::Dict ConvertToDict(
    const blink::mojom::RemoteDesktopClientOverride& input)
{
    base::Value::Dict dict;
    dict.Set("origin", input.origin.Serialize());
    dict.Set("sameOriginWithAncestors", input.same_origin_with_ancestors);
    return dict;
}

base::Value::Dict ConvertToDict(const blink::mojom::PRFValues& prf_input)
{
    base::Value::Dict prf_dict;
    std::string first = webauthn::Base64UrlEncodeOmitPadding(
        prf_input.first);
    prf_dict.Set("first", std::move(first));
    if (prf_input.second) {
        std::string second = webauthn::Base64UrlEncodeOmitPadding(
            *prf_input.second);
        prf_dict.Set("second", std::move(second));
    }
    return prf_dict;
}

base::Value::Dict ConvertToDict(
    const std::vector<blink::mojom::PRFValuesPtr>& prf_inputs)
{
    bool found_eval = false;
    base::Value::Dict prf;
    base::Value::Dict eval_by_credential;
    for (const auto& prf_input : prf_inputs) {
        if (!prf_input) {
            continue;
        }
        base::Value::Dict prf_dict = ConvertToDict(*prf_input);
        if (prf_input->id) {
            std::string id = webauthn::Base64UrlEncodeOmitPadding(*prf_input->id);
            eval_by_credential.Set(id, std::move(prf_dict));
            continue;
        }
        if (found_eval) {
            LOG(WARNING) << "prf: duplicate eval ignored, only one eval is allowed per request";
            continue;
        }
        prf.Set("eval", std::move(prf_dict));
        found_eval = true;
    }
    if (!eval_by_credential.empty()) {
        prf.Set("evalByCredential", std::move(eval_by_credential));
    }
    return prf;
}

using PublicKeyCredentialParameters = ::device::PublicKeyCredentialParams::CredentialInfo;

base::Value::List ConvertToList(
    const std::vector<PublicKeyCredentialParameters>& params)
{
    base::Value::List params_list = base::Value::List::with_capacity(params.size());
    for (const auto& param : params) {
        base::Value::Dict param_dict;
        param_dict.Set("type", ToString(param.type));
        param_dict.Set("alg", param.algorithm);
        params_list.Append(std::move(param_dict));
    }
    return params_list;
}

base::Value::Dict ConvertToDict(
    const bool is_payment_credential_creation,
    const std::optional<std::vector<PublicKeyCredentialParameters>>& params)
{
    base::Value::Dict payment;
    payment.Set("isPayment", is_payment_credential_creation);
    if (params) {
        payment.Set("browserBoundPubKeyCredParams", ConvertToList(*params));
    }
    return payment;
}

// Builds the extensions JSON dict for creation (make credential) requests.
// JSON keys follow WebAuthn spec extension names:
//   appidExclude, hmacCreateSecret, credentialProtectionPolicy/credProtect,
//   enforceCredentialProtectionPolicy, minPinLength, credProps,
//   largeBlob.support, credBlob, payment, remoteDesktopClientOverride, prf.eval.
base::Value::Dict BuildExtensionsDict(
    const blink::mojom::PublicKeyCredentialCreationOptions& options)
{
    base::Value::Dict extensions_dict;
    // appidExclude: https://w3c.github.io/webauthn/#sctn-appid-exclude
    if (options.appid_exclude) {
        extensions_dict.Set("appidExclude", *options.appid_exclude);
    }
    // hmacCreateSecret: https://w3c.github.io/webauthn/#sctn-hmac-create-secret
    if (options.hmac_create_secret) {
        extensions_dict.Set("hmacCreateSecret", options.hmac_create_secret);
    }
    // credentialProtectionPolicy / credProtect:
    // https://w3c.github.io/webauthn/#sctn-credProtect
    if (options.protection_policy != blink::mojom::ProtectionPolicy::UNSPECIFIED) {
        extensions_dict.Set("credentialProtectionPolicy",
            ToString(options.protection_policy));
        // OHOS FIDO2 requires credProtect as credentialProtectionPolicy in extension.
        extensions_dict.Set("credProtect", ToString(options.protection_policy));
    }
    if (options.enforce_protection_policy) {
        extensions_dict.Set("enforceCredentialProtectionPolicy",
            options.enforce_protection_policy);
    }
    // minPinLength: https://w3c.github.io/webauthn/#sctn-minPinLength
    if (options.min_pin_length_requested) {
        extensions_dict.Set("minPinLength", options.min_pin_length_requested);
    }
    // credProps: https://w3c.github.io/webauthn/#sctn-credProps
    if (options.cred_props) {
        extensions_dict.Set("credProps", options.cred_props);
    }
    // largeBlob: https://w3c.github.io/webauthn/#sctn-large-blob
    if (options.large_blob_enable != device::LargeBlobSupport::kNotRequested) {
        base::Value::Dict large_blob_dict;
        large_blob_dict.Set("support", ToString(options.large_blob_enable));
        extensions_dict.Set("largeBlob", std::move(large_blob_dict));
    }
    // credBlob: https://w3c.github.io/webauthn/#sctn-credBlob
    if (options.cred_blob) {
        extensions_dict.Set("credBlob", webauthn::Base64UrlEncodeOmitPadding(
            *options.cred_blob));
    }
    // payment:
    // https://w3c.github.io/secure-payment-confirmation/#sctn-payment-extension-registration
    if (options.is_payment_credential_creation ||
        options.payment_browser_bound_key_parameters) {
        extensions_dict.Set("payment",
            ConvertToDict(options.is_payment_credential_creation,
                          options.payment_browser_bound_key_parameters));
    }
    // remoteDesktopClientOverride:
    // https://github.com/w3c/webauthn/blob/main/explainers/remote-desktop-support.md
    if (options.remote_desktop_client_override) {
        extensions_dict.Set("remoteDesktopClientOverride",
            ConvertToDict(*options.remote_desktop_client_override));
    }
    // prf: https://w3c.github.io/webauthn/#sctn-prf-extension
    if (options.prf_enable && options.prf_input) {
        base::Value::Dict prf_dict;
        prf_dict.Set("eval", ConvertToDict(*options.prf_input));
        extensions_dict.Set("prf", std::move(prf_dict));
    }
    return extensions_dict;
}

// Builds the extensions JSON dict for request (get assertion) options.
// JSON keys follow WebAuthn spec extension names:
//   largeBlob.read/write, prf.eval/evalByCredential, appid, getCredBlob,
//   payment.browserBoundPubKeyCredParams, remoteDesktopClientOverride.
base::Value::Dict BuildExtensionsDict(
    const blink::mojom::AuthenticationExtensionsClientInputs& extensions)
{
    base::Value::Dict extensions_dict;

    // largeBlob: https://w3c.github.io/webauthn/#sctn-large-blob
    base::Value::Dict dict_large_blob;
    if (extensions.large_blob_write.has_value()) {
        std::string encoded = webauthn::Base64UrlEncodeOmitPadding(
            *extensions.large_blob_write);
        dict_large_blob.Set("write", std::move(encoded));
    }
    if (extensions.large_blob_read) {
        dict_large_blob.Set("read", extensions.large_blob_read);
    }
    if (extensions.large_blob_read || extensions.large_blob_write) {
        extensions_dict.Set("largeBlob", std::move(dict_large_blob));
    }

    // prf: https://w3c.github.io/webauthn/#sctn-prf-extension
    if (extensions.prf) {
        extensions_dict.Set("prf", ConvertToDict(extensions.prf_inputs));
    }

    // appid: https://w3c.github.io/webauthn/#sctn-appid
    if (extensions.appid) {
        extensions_dict.Set("appid", *extensions.appid);
    }

    // getCredBlob: https://w3c.github.io/webauthn/#sctn-credBlob
    if (extensions.get_cred_blob) {
        extensions_dict.Set("getCredBlob", extensions.get_cred_blob);
    }

    // payment:
    // https://w3c.github.io/secure-payment-confirmation/#sctn-payment-extension-registration
    if (extensions.payment_browser_bound_key_parameters) {
        base::Value::Dict payment;
        payment.Set("browserBoundPubKeyCredParams",
            ConvertToList(*extensions.payment_browser_bound_key_parameters));
        extensions_dict.Set("payment", std::move(payment));
    }

    // remoteDesktopClientOverride:
    // https://github.com/w3c/webauthn/blob/main/explainers/remote-desktop-support.md
    if (extensions.remote_desktop_client_override) {
        extensions_dict.Set("remoteDesktopClientOverride",
            ConvertToDict(*extensions.remote_desktop_client_override));
    }

    return extensions_dict;
}

// Parses extension outputs from authenticator make-credential response.
// JSON keys follow WebAuthn spec extension names:
//   hmacCreateSecret, credProps.rk, largeBlob.supported, credBlob,
//   payment, prf.enabled/results.
void FillResponseExtensions(
    const base::Value::Dict& root_dict,
    blink::mojom::MakeCredentialAuthenticatorResponse& response)
{
    // hmacCreateSecret: https://w3c.github.io/webauthn/#sctn-hmac-create-secret
    auto hmac_create_secret = root_dict.FindBool("hmacCreateSecret");
    if (hmac_create_secret) {
        response.echo_hmac_create_secret = true;
        response.hmac_create_secret = *hmac_create_secret;
    }

    // credProps: https://w3c.github.io/webauthn/#sctn-credProps
    const base::Value::Dict* cred_props_dict = root_dict.FindDict("credProps");
    if (cred_props_dict) {
        response.echo_cred_props = true;
        const auto rk = cred_props_dict->FindBool("rk");
        if (rk) {
            response.has_cred_props_rk = true;
            response.cred_props_rk = *rk;
        }
    }
    // largeBlob: https://w3c.github.io/webauthn/#sctn-large-blob
    const base::Value::Dict* large_blob_dict = root_dict.FindDict("largeBlob");
    if (large_blob_dict) {
        response.echo_large_blob = true;
        auto supported = large_blob_dict->FindBool("supported");
        if (supported) {
            response.supports_large_blob = *supported;
        }
    }
    // credBlob: https://w3c.github.io/webauthn/#sctn-credBlob
    auto cred_blob = root_dict.FindBool("credBlob");
    if (cred_blob) {
        response.echo_cred_blob = true;
        response.cred_blob = *cred_blob;
    }
    // payment:
    // https://w3c.github.io/secure-payment-confirmation/#sctn-payment-extension-registration
    const base::Value::Dict* payment_dict = root_dict.FindDict("payment");
    if (payment_dict) {
        response.payment = ExtractPaymentResponse(*payment_dict);
    }
    // prf: https://w3c.github.io/webauthn/#sctn-prf-extension
    const base::Value::Dict* prf_dict = root_dict.FindDict("prf");
    if (prf_dict) {
        auto enabled_op = prf_dict->FindBool("enabled");
        if (enabled_op) {
            response.echo_prf = true;
            response.prf = *enabled_op;
            response.prf_results = ExtractPrfValue(*prf_dict);
        }
    }
}

// Parses extension outputs from authenticator get-assertion response.
// JSON keys follow WebAuthn spec extension names:
//   largeBlob.written/blob, appid, getCredBlob, payment, prf.results,
//   authData (OHOS digital shield extension).
void FillResponseExtensions(
    const base::Value::Dict& outer_dict,
    blink::mojom::AuthenticationExtensionsClientOutputs& outputs)
{
    // largeBlob: https://w3c.github.io/webauthn/#sctn-large-blob
    const base::Value* large_blob_val = outer_dict.Find("largeBlob");
    if (large_blob_val != nullptr && large_blob_val->is_dict()) {
        outputs.echo_large_blob = true;
        const base::Value::Dict& inner_dict = large_blob_val->GetDict();
        const base::Value* written = inner_dict.Find("written");
        if (written != nullptr && written->is_bool()) {
            outputs.echo_large_blob_written = true;
            outputs.large_blob_written = written->GetBool();
        }
        const std::string* blob = inner_dict.FindString("blob");
        if (blob != nullptr) {
            outputs.large_blob = base::Base64UrlDecode(
                *blob, base::Base64UrlDecodePolicy::IGNORE_PADDING);
            if (!outputs.large_blob) {
                LOG(WARNING) << "invalid largeBlob.blob";
            }
        }
    }

    // appid: https://w3c.github.io/webauthn/#sctn-appid
    const auto appid = outer_dict.FindBool("appid");
    if (appid) {
        outputs.echo_appid_extension = true;
        outputs.appid_extension = *appid;
    }

    // getCredBlob: https://w3c.github.io/webauthn/#sctn-credBlob
    const std::string* get_cred_blob = outer_dict.FindString("getCredBlob");
    if (get_cred_blob) {
        outputs.get_cred_blob = base::Base64UrlDecode(
            *get_cred_blob, base::Base64UrlDecodePolicy::IGNORE_PADDING);
        if (!outputs.get_cred_blob) {
            LOG(WARNING) << "invalid getCredBlob";
        }
    }

    // payment:
    // https://w3c.github.io/secure-payment-confirmation/#sctn-payment-extension-registration
    const base::Value::Dict* payment_dict = outer_dict.FindDict("payment");
    if (payment_dict) {
        outputs.payment = ExtractPaymentResponse(*payment_dict);
    }

    // prf: https://w3c.github.io/webauthn/#sctn-prf-extension
    const base::Value::Dict* prf_dict = outer_dict.FindDict("prf");
    if (prf_dict) {
        outputs.echo_prf = true;
        outputs.prf_results = ExtractPrfValue(*prf_dict);
    }

    // authData: OHOS digital shield extension.
    const std::string* auth_data_hex = outer_dict.FindString("authData");
    if (auth_data_hex != nullptr) {
        outputs.digital_shield_data = base::Base64UrlDecode(
                *auth_data_hex, base::Base64UrlDecodePolicy::IGNORE_PADDING);
        if (!outputs.digital_shield_data) {
            LOG(WARNING) << "invalid authData";
        }
    }
}

} // namespace

device::CredentialMediationRequirement Convert(
    const blink::mojom::PublicKeyCredentialMediationRequirement& mediation)
{
    switch (mediation) {
        case blink::mojom::PublicKeyCredentialMediationRequirement::SILENT:
            return device::CredentialMediationRequirement::kSilent;
        case blink::mojom::PublicKeyCredentialMediationRequirement::OPTIONAL:
            return device::CredentialMediationRequirement::kOptional;
        case blink::mojom::PublicKeyCredentialMediationRequirement::REQUIRED:
            return device::CredentialMediationRequirement::kRequired;
        case blink::mojom::PublicKeyCredentialMediationRequirement::CONDITIONAL:
            return device::CredentialMediationRequirement::kConditional;
    }
    return device::CredentialMediationRequirement::kSilent;
}

device::CredentialHint Convert(const blink::mojom::Hint& hint)
{
    switch (hint) {
        case blink::mojom::Hint::SECURITY_KEY:
            return device::CredentialHint::kSecurityKey;
        case blink::mojom::Hint::CLIENT_DEVICE:
            return device::CredentialHint::kClientDevice;
        case blink::mojom::Hint::HYBRID:
            return device::CredentialHint::kHintHybrid;
    }
    return device::CredentialHint::kHintHybrid;
}

device::CtapRequestExtraCommon CreateCtapRequestExtraCommon(
    const url::Origin& caller_origin,
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options)
{
    device::CtapRequestExtraCommon ret;
    ret.origin = caller_origin.Serialize();
    ret.challenge = options->challenge;
    ret.mediation = Convert(options->mediation);
    if (options->timeout) {
        ret.timeout = *options->timeout;
    }
    ret.hints.reserve(options->hints.size());
    for (const auto& hint : options->hints) {
        ret.hints.push_back(Convert(hint));
    }
    base::Value::Dict extensions_dict = BuildExtensionsDict(*options);
    ret.extensions = std::nullopt;
    if (!extensions_dict.empty()) {
        std::string str;
        if (base::JSONWriter::Write(extensions_dict, &str)) {
            ret.extensions = std::move(str);
        } else {
            LOG(ERROR) << "convert extensions failed.";
        }
    }
    return ret;
}

device::CtapRequestExtraCommon CreateCtapRequestExtraCommon(
    const url::Origin& caller_origin,
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options)
{
    device::CtapRequestExtraCommon ret;
    ret.origin = caller_origin.Serialize();
    if (options->challenge.has_value()) {
        ret.challenge = *options->challenge;
    } else {
        LOG(ERROR) << "Challenge is null, challenge_url not supported in current implementation";
        return device::CtapRequestExtraCommon();
    }
    ret.mediation = Convert(options->mediation);
    if (options->timeout) {
        ret.timeout = *options->timeout;
    }
    ret.hints.reserve(options->hints.size());
    for (const auto& hint : options->hints) {
        ret.hints.push_back(Convert(hint));
    }
    ret.extensions = std::nullopt;
    if (options->extensions) {
        base::Value::Dict extensions_dict =
            BuildExtensionsDict(*options->extensions);
        if (!extensions_dict.empty()) {
            std::string str;
            if (base::JSONWriter::Write(extensions_dict, &str)) {
                ret.extensions = std::move(str);
            } else {
                LOG(ERROR) << "convert extensions failed.";
            }
        }
    }
    return ret;
}

device::CtapMakeCredentialRequestExtra CreateCtapMakeCredentialRequestExtra(
    const url::Origin& caller_origin,
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options)
{
    device::CtapMakeCredentialRequestExtra ret;
    ret.common = CreateCtapRequestExtraCommon(caller_origin, options);
    ret.attestation_formats = options->attestation_formats;
    return ret;
}

device::CtapGetAssertionRequestExtra CreateCtapGetAssertionRequestExtra(
    const url::Origin& caller_origin,
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options)
{
    device::CtapGetAssertionRequestExtra ret;
    ret.common = CreateCtapRequestExtraCommon(caller_origin, options);
    return ret;
}

blink::mojom::MakeCredentialAuthenticatorResponsePtr CreateMakeCredentialResponse(
    const device::AuthenticatorMakeCredentialResponse& response_data)
{
    LOG(INFO) << "CreateMakeCredentialResponse response_extra";
    auto response = blink::mojom::MakeCredentialAuthenticatorResponse::New();
    auto common_info = blink::mojom::CommonCredentialInfo::New();

    common_info->client_data_json =
        response_data.response_extra->common.client_data_json;
    common_info->raw_id = response_data.response_extra->common.raw_id;
    common_info->id = webauthn::Base64UrlEncodeOmitPadding(common_info->raw_id);
    common_info->authenticator_data =
        response_data.response_extra->common.authenticator_data;
    response->info = std::move(common_info);
    response->authenticator_attachment =
        response_data.response_extra->common.authenticator_attachment;
    response->attestation_object =
        response_data.response_extra->attestation_object;
    if (response_data.transports) {
        response->transports.assign(
            response_data.transports->begin(),
            response_data.transports->end());
    }
    response->public_key_der = response_data.response_extra->public_key;
    response->public_key_algo =
        response_data.response_extra->public_key_algorithm;

    std::optional<base::Value> json_val = base::JSONReader::Read(
        response_data.response_extra->common.client_extension_results,
        base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (!json_val.has_value() || !json_val->is_dict()) {
        return response;
    }

    const base::Value::Dict& root_dict = json_val->GetDict();
    FillResponseExtensions(root_dict, *response);
    return response;
}

blink::mojom::GetAssertionAuthenticatorResponsePtr CreateGetAssertionResponse(
    const device::AuthenticatorGetAssertionResponse& response_data)
{
    LOG(INFO) << "CreateGetAssertionResponse response_extra";
    auto response = blink::mojom::GetAssertionAuthenticatorResponse::New();
    auto common_info = blink::mojom::CommonCredentialInfo::New();

    common_info->client_data_json =
        response_data.response_extra->common.client_data_json;
    common_info->raw_id = response_data.response_extra->common.raw_id;
    common_info->id = webauthn::Base64UrlEncodeOmitPadding(common_info->raw_id);
    common_info->authenticator_data =
        response_data.response_extra->common.authenticator_data;
    response->info = std::move(common_info);

    response->authenticator_attachment =
        response_data.response_extra->common.authenticator_attachment;
    response->signature = response_data.response_extra->signature;
    response->user_handle = response_data.response_extra->user_handle;
    response->extensions =
        blink::mojom::AuthenticationExtensionsClientOutputs::New();

    std::optional<base::Value> json_val = base::JSONReader::Read(
        response_data.response_extra->common.client_extension_results, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (!json_val.has_value() || !json_val->is_dict()) {
        return response;
    }
    const base::Value::Dict& outer_dict = json_val->GetDict();
    FillResponseExtensions(outer_dict, *response->extensions);
    return response;
}

void GetClientCapabilitiesFromOhosWebAuthnApi(
    blink::mojom::Authenticator::GetClientCapabilitiesCallback callback)
{
    device::OhosAuthenticator::GetClientCapabilities(base::BindOnce(
        [](const std::vector<std::pair<std::string, bool>>& capabilities) {
            std::vector<blink::mojom::WebAuthnClientCapabilityPtr> result;
            result.reserve(capabilities.size());
            for (const auto& capability : capabilities) {
                result.push_back(blink::mojom::WebAuthnClientCapability::New(
                    capability.first, capability.second));
            }
            return result;
        }).Then(std::move(callback)));
}

} // namespace content
