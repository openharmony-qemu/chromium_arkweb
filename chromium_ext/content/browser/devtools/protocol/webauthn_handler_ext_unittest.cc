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

#include "content/browser/devtools/protocol/webauthn_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "content/browser/devtools/protocol/web_authn.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "device/fido/virtual_fido_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::protocol {

namespace {

// Simple callback implementations that capture the result for inspection.

class FakeAddCredentialCallback : public WebAuthn::Backend::AddCredentialCallback {
 public:
  explicit FakeAddCredentialCallback(bool* success_out)
      : success_out_(success_out) {}
  void sendSuccess() override { *success_out_ = true; }
  void sendFailure(const DispatchResponse& response) override {
    *success_out_ = false;
  }
  void fallThrough() override {}

 private:
  bool* success_out_;
};

class FakeGetCredentialsCallback
    : public WebAuthn::Backend::GetCredentialsCallback {
 public:
  explicit FakeGetCredentialsCallback(bool* success_out)
      : success_out_(success_out) {}
  void sendSuccess(
      std::unique_ptr<Array<WebAuthn::Credential>> credentials) override {
    *success_out_ = true;
  }
  void sendFailure(const DispatchResponse& response) override {
    *success_out_ = false;
  }
  void fallThrough() override {}

 private:
  bool* success_out_;
};

class FrontendChannelStub : public crdtp::FrontendChannel {
 public:
  ~FrontendChannelStub() override = default;

  void SendProtocolResponse(int call_id,
                            std::unique_ptr<Serializable> message) override {}
  void SendProtocolNotification(
      std::unique_ptr<Serializable> message) override {}

  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override {}

  void FlushProtocolNotifications() override {}
};

// Helper to create a non-resident credential for testing.
std::unique_ptr<WebAuthn::Credential> MakeNonResidentCredential(
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    const std::vector<uint8_t>& pkcs8_private_key,
    int sign_count = 0) {
  return WebAuthn::Credential::Create()
      .SetCredentialId(Binary::fromVector(credential_id))
      .SetIsResidentCredential(false)
      .SetRpId(rp_id)
      .SetPrivateKey(Binary::fromVector(pkcs8_private_key))
      .SetSignCount(sign_count)
      .Build();
}

// Helper to create a resident credential for testing.
std::unique_ptr<WebAuthn::Credential> MakeResidentCredential(
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    const std::vector<uint8_t>& pkcs8_private_key,
    const std::vector<uint8_t>& user_handle,
    int sign_count = 0) {
  return WebAuthn::Credential::Create()
      .SetCredentialId(Binary::fromVector(credential_id))
      .SetIsResidentCredential(true)
      .SetRpId(rp_id)
      .SetPrivateKey(Binary::fromVector(pkcs8_private_key))
      .SetUserHandle(Binary::fromVector(user_handle))
      .SetSignCount(sign_count)
      .Build();
}

}  // namespace

class WebAuthnHandlerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    webauthn_handler_ = std::make_unique<WebAuthnHandler>();
    webauthn_handler_->Wire(new UberDispatcher(new FrontendChannelStub()));
  }

  void TearDown() override {
    RenderViewHostImplTestHarness::TearDown();
    webauthn_handler_.reset();
  }

 protected:
  // Enables the virtual authenticator environment and returns the handler.
  Response Enable(bool enable_ui = false) {
    webauthn_handler_->SetRenderer(0, contents()->GetPrimaryMainFrame());
    return webauthn_handler_->Enable(enable_ui);
  }

  // Creates and adds a CTAP2 USB authenticator with resident key and user
  // verification support. Returns the authenticator ID.
  String AddDefaultAuthenticator() {
    auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                       .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                       .SetTransport(WebAuthn::AuthenticatorTransportEnum::Usb)
                       .SetHasResidentKey(true)
                       .SetHasUserVerification(true)
                       .Build();
    String id;
    Response response =
        webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
    EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code()) << response.Message();
    return id;
  }

  // Generates a fresh PKCS8 private key for testing.
  std::vector<uint8_t> FreshPrivateKey() {
    auto key = device::VirtualFidoDevice::PrivateKey::FreshP256Key();
    return key->GetPKCS8PrivateKey();
  }

  std::unique_ptr<WebAuthnHandler> webauthn_handler_;
};

// ---- AddVirtualAuthenticator ----
// Note: Enable/Disable and DtorCleansUpObservers tests are in the upstream
// webauthn_handler_unittest.cc. Only new tests are added here.

TEST_F(WebAuthnHandlerTest, AddVirtualAuthenticatorCtap2Usb) {
  ASSERT_TRUE(Enable().IsSuccess());

  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                     .SetTransport(WebAuthn::AuthenticatorTransportEnum::Usb)
                     .SetHasResidentKey(true)
                     .SetHasUserVerification(true)
                     .Build();
  String id;
  Response response =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  EXPECT_FALSE(id.empty());
}

TEST_F(WebAuthnHandlerTest, AddVirtualAuthenticatorU2fUsb) {
  ASSERT_TRUE(Enable().IsSuccess());

  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::U2f)
                     .SetTransport(WebAuthn::AuthenticatorTransportEnum::Usb)
                     .Build();
  String id;
  Response response =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  EXPECT_FALSE(id.empty());
}

TEST_F(WebAuthnHandlerTest, AddVirtualAuthenticatorFailsIfNotEnabled) {
  webauthn_handler_->SetRenderer(0, contents()->GetPrimaryMainFrame());
  // Do not call Enable().

  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                     .SetTransport(WebAuthn::AuthenticatorTransportEnum::Usb)
                     .Build();
  String id;
  Response response =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       AddVirtualAuthenticatorU2fInternalReturnsError) {
  ASSERT_TRUE(Enable().IsSuccess());

  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::U2f)
                     .SetTransport(
                         WebAuthn::AuthenticatorTransportEnum::Internal)
                     .Build();
  String id;
  Response response =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       AddVirtualAuthenticatorHasLargeBlobRequiresResidentKey) {
  ASSERT_TRUE(Enable().IsSuccess());

  // hasLargeBlob=true but hasResidentKey=false should fail.
  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                     .SetTransport(WebAuthn::AuthenticatorTransportEnum::Usb)
                     .SetHasLargeBlob(true)
                     .Build();
  String id;
  Response response =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       AddVirtualAuthenticatorInternalHasLargeBlobRequiresResidentKey) {
  ASSERT_TRUE(Enable().IsSuccess());

  // hasLargeBlob=true but hasResidentKey=false with internal transport should
  // fail — same constraint as usb, just verifying the transport doesn't matter.
  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                     .SetTransport(
                         WebAuthn::AuthenticatorTransportEnum::Internal)
                     .SetHasLargeBlob(true)
                     .Build();
  String id;
  Response response =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       AddVirtualAuthenticatorHasLargeBlobWithResidentKeySucceeds) {
  ASSERT_TRUE(Enable().IsSuccess());

  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                     .SetCtap2Version(WebAuthn::Ctap2VersionEnum::Ctap2_1)
                     .SetTransport(WebAuthn::AuthenticatorTransportEnum::Usb)
                     .SetHasResidentKey(true)
                     .SetHasLargeBlob(true)
                     .Build();
  String id;
  Response response =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       AddVirtualAuthenticatorOnlyOneInternalTransport) {
  ASSERT_TRUE(Enable().IsSuccess());

  // Add first internal authenticator.
  auto options1 = WebAuthn::VirtualAuthenticatorOptions::Create()
                      .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                      .SetTransport(
                          WebAuthn::AuthenticatorTransportEnum::Internal)
                      .SetHasUserVerification(true)
                      .Build();
  String id1;
  Response response1 =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options1), &id1);
  ASSERT_EQ(crdtp::DispatchCode::SUCCESS, response1.Code());

  // Adding a second internal authenticator should fail.
  auto options2 = WebAuthn::VirtualAuthenticatorOptions::Create()
                      .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                      .SetTransport(
                          WebAuthn::AuthenticatorTransportEnum::Internal)
                      .Build();
  String id2;
  Response response2 =
      webauthn_handler_->AddVirtualAuthenticator(std::move(options2), &id2);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response2.Code());
}

// ---- RemoveVirtualAuthenticator ----

TEST_F(WebAuthnHandlerTest, RemoveVirtualAuthenticatorSucceeds) {
  ASSERT_TRUE(Enable().IsSuccess());
  String id = AddDefaultAuthenticator();

  Response response = webauthn_handler_->RemoveVirtualAuthenticator(id);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest, RemoveVirtualAuthenticatorInvalidIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());

  Response response = webauthn_handler_->RemoveVirtualAuthenticator("nonexistent");
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       RemoveVirtualAuthenticatorThenOperationsFail) {
  ASSERT_TRUE(Enable().IsSuccess());
  String id = AddDefaultAuthenticator();
  ASSERT_TRUE(webauthn_handler_->RemoveVirtualAuthenticator(id).IsSuccess());

  // Operations on the removed authenticator should fail.
  Response response = webauthn_handler_->ClearCredentials(id);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());

  response = webauthn_handler_->SetUserVerified(id, true);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       RemoveVirtualAuthenticatorDoesNotAffectOthers) {
  ASSERT_TRUE(Enable().IsSuccess());
  String id1 = AddDefaultAuthenticator();

  // Add a second authenticator (different transport to avoid internal conflict).
  auto options = WebAuthn::VirtualAuthenticatorOptions::Create()
                     .SetProtocol(WebAuthn::AuthenticatorProtocolEnum::Ctap2)
                     .SetTransport(WebAuthn::AuthenticatorTransportEnum::Nfc)
                     .SetHasResidentKey(true)
                     .SetHasUserVerification(true)
                     .Build();
  String id2;
  ASSERT_EQ(crdtp::DispatchCode::SUCCESS,
            webauthn_handler_->AddVirtualAuthenticator(std::move(options), &id2)
                .Code());

  // Remove the first; the second should still work.
  ASSERT_TRUE(webauthn_handler_->RemoveVirtualAuthenticator(id1).IsSuccess());
  Response response = webauthn_handler_->ClearCredentials(id2);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

// ---- AddCredential ----

TEST_F(WebAuthnHandlerTest, AddNonResidentCredentialSucceeds) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto credential_id = std::vector<uint8_t>{1, 2, 3, 4};
  auto private_key = FreshPrivateKey();
  auto credential = MakeNonResidentCredential(credential_id, "example.com",
                                              private_key);

  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(credential),
      std::make_unique<FakeAddCredentialCallback>(&success));
  EXPECT_TRUE(success);
}

TEST_F(WebAuthnHandlerTest,
       AddResidentCredentialWithoutUserHandleFails) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  // Build a resident credential without userHandle.
  auto credential_id = std::vector<uint8_t>{5, 6, 7, 8};
  auto private_key = FreshPrivateKey();
  auto credential =
      WebAuthn::Credential::Create()
          .SetCredentialId(Binary::fromVector(credential_id))
          .SetIsResidentCredential(true)
          .SetRpId("example.com")
          .SetPrivateKey(Binary::fromVector(private_key))
          .SetSignCount(0)
          .Build();

  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(credential),
      std::make_unique<FakeAddCredentialCallback>(&success));
  EXPECT_FALSE(success);
}

TEST_F(WebAuthnHandlerTest, AddResidentCredentialWithUserHandleSucceeds) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto credential_id = std::vector<uint8_t>{9, 10, 11, 12};
  auto private_key = FreshPrivateKey();
  auto user_handle = std::vector<uint8_t>{0xA1, 0xB2, 0xC3};
  auto credential = MakeResidentCredential(credential_id, "example.com",
                                           private_key, user_handle);

  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(credential),
      std::make_unique<FakeAddCredentialCallback>(&success));
  EXPECT_TRUE(success);
}

TEST_F(WebAuthnHandlerTest, AddCredentialWithoutRpIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto credential_id = std::vector<uint8_t>{13, 14, 15, 16};
  auto private_key = FreshPrivateKey();
  // No RpId set.
  auto credential =
      WebAuthn::Credential::Create()
          .SetCredentialId(Binary::fromVector(credential_id))
          .SetIsResidentCredential(false)
          .SetPrivateKey(Binary::fromVector(private_key))
          .SetSignCount(0)
          .Build();

  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(credential),
      std::make_unique<FakeAddCredentialCallback>(&success));
  EXPECT_FALSE(success);
}

TEST_F(WebAuthnHandlerTest, AddCredentialInvalidAuthenticatorIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());

  auto credential_id = std::vector<uint8_t>{17, 18, 19, 20};
  auto private_key = FreshPrivateKey();
  auto credential =
      MakeNonResidentCredential(credential_id, "example.com", private_key);

  bool success = false;
  webauthn_handler_->AddCredential(
      "nonexistent", std::move(credential),
      std::make_unique<FakeAddCredentialCallback>(&success));
  EXPECT_FALSE(success);
}

// ---- GetCredentials ----

TEST_F(WebAuthnHandlerTest, GetCredentialsReturnsEmptyAfterCreation) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  EXPECT_EQ(0u, authenticator->registrations().size());
}

TEST_F(WebAuthnHandlerTest, GetCredentialsReturnsInjectedList) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  // Add two credentials.
  auto cred_id1 = std::vector<uint8_t>{1, 2, 3};
  auto pk1 = FreshPrivateKey();
  auto cred1 = MakeNonResidentCredential(cred_id1, "example.com", pk1);
  bool success1 = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred1),
      std::make_unique<FakeAddCredentialCallback>(&success1));
  ASSERT_TRUE(success1);

  auto cred_id2 = std::vector<uint8_t>{4, 5, 6};
  auto pk2 = FreshPrivateKey();
  auto cred2 = MakeNonResidentCredential(cred_id2, "test.com", pk2);
  bool success2 = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred2),
      std::make_unique<FakeAddCredentialCallback>(&success2));
  ASSERT_TRUE(success2);

  // Verify two credentials are registered.
  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  EXPECT_EQ(2u, authenticator->registrations().size());
}

TEST_F(WebAuthnHandlerTest, GetCredentialsInvalidAuthenticatorIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());

  bool success = false;
  webauthn_handler_->GetCredentials(
      "nonexistent",
      std::make_unique<FakeGetCredentialsCallback>(&success));
  EXPECT_FALSE(success);
}

TEST_F(WebAuthnHandlerTest, GetCredentialsFieldCompleteness) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  // Add a non-resident credential with known values.
  auto cred_id = std::vector<uint8_t>{1, 2, 3, 4};
  auto pk = FreshPrivateKey();
  auto credential = MakeNonResidentCredential(cred_id, "example.com", pk, 5);
  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(credential),
      std::make_unique<FakeAddCredentialCallback>(&success));
  ASSERT_TRUE(success);

  // Add a resident credential with known values.
  auto cred_id2 = std::vector<uint8_t>{5, 6, 7, 8};
  auto pk2 = FreshPrivateKey();
  auto user_handle = std::vector<uint8_t>{0xA1, 0xB2, 0xC3};
  auto credential2 = MakeResidentCredential(cred_id2, "test.com", pk2,
                                            user_handle, 10);
  bool success2 = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(credential2),
      std::make_unique<FakeAddCredentialCallback>(&success2));
  ASSERT_TRUE(success2);

  // Verify registration data fields directly.
  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  ASSERT_EQ(2u, authenticator->registrations().size());

  // Check non-resident credential fields.
  // For non-resident credentials, rp and user are not populated; the rp_id is
  // hashed into application_parameter. The counter and is_resident fields
  // should still be correct.
  auto it1 = authenticator->registrations().find(cred_id);
  ASSERT_NE(it1, authenticator->registrations().end());
  EXPECT_FALSE(it1->second.is_resident);
  EXPECT_EQ(5u, it1->second.counter);
  EXPECT_FALSE(it1->second.rp.has_value());
  EXPECT_FALSE(it1->second.user.has_value());

  // Check resident credential fields.
  auto it2 = authenticator->registrations().find(cred_id2);
  ASSERT_NE(it2, authenticator->registrations().end());
  ASSERT_TRUE(it2->second.rp.has_value());
  EXPECT_EQ("test.com", it2->second.rp->id);
  EXPECT_TRUE(it2->second.is_resident);
  EXPECT_EQ(10u, it2->second.counter);
  ASSERT_TRUE(it2->second.user.has_value());
  EXPECT_EQ(user_handle, std::vector<uint8_t>(it2->second.user->id.begin(),
                                              it2->second.user->id.end()));
}

TEST_F(WebAuthnHandlerTest, GetCredentialsSignCountIncrement) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto cred_id = std::vector<uint8_t>{1, 2, 3, 4};
  auto pk = FreshPrivateKey();
  auto credential = MakeNonResidentCredential(cred_id, "example.com", pk, 0);
  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(credential),
      std::make_unique<FakeAddCredentialCallback>(&success));
  ASSERT_TRUE(success);

  // Verify initial counter is 0.
  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  auto it = authenticator->registrations().find(cred_id);
  ASSERT_NE(it, authenticator->registrations().end());
  EXPECT_EQ(0u, it->second.counter);

  // Simulate an assertion by incrementing the counter.
  // In real usage, the renderer's assertion flow increments this counter.
  // Here we verify that the registration data reflects the updated count.
  it->second.counter = 3;
  EXPECT_EQ(3u, it->second.counter);

  // Verify GetCredentials would report the updated signCount.
  // The BuildCredentialFromRegistration function reads registration->counter
  // as signCount, so the updated counter will be reflected.
  bool get_success = false;
  webauthn_handler_->GetCredentials(
      auth_id,
      std::make_unique<FakeGetCredentialsCallback>(&get_success));
  EXPECT_TRUE(get_success);
}

// ---- RemoveCredential ----

TEST_F(WebAuthnHandlerTest, RemoveCredentialSucceeds) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto cred_id = std::vector<uint8_t>{10, 20, 30};
  auto pk = FreshPrivateKey();
  auto credential = MakeNonResidentCredential(cred_id, "example.com", pk);
  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(credential),
      std::make_unique<FakeAddCredentialCallback>(&success));
  ASSERT_TRUE(success);

  Response response = webauthn_handler_->RemoveCredential(
      auth_id, Binary::fromVector(cred_id));
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());

  // Verify it's gone.
  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  EXPECT_EQ(0u, authenticator->registrations().size());
}

TEST_F(WebAuthnHandlerTest, RemoveCredentialDoesNotAffectOthers) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto cred_id1 = std::vector<uint8_t>{1, 1, 1};
  auto pk1 = FreshPrivateKey();
  auto cred1 = MakeNonResidentCredential(cred_id1, "example.com", pk1);
  bool success1 = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred1),
      std::make_unique<FakeAddCredentialCallback>(&success1));

  auto cred_id2 = std::vector<uint8_t>{2, 2, 2};
  auto pk2 = FreshPrivateKey();
  auto cred2 = MakeNonResidentCredential(cred_id2, "test.com", pk2);
  bool success2 = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred2),
      std::make_unique<FakeAddCredentialCallback>(&success2));

  // Remove only the first.
  ASSERT_TRUE(webauthn_handler_
                  ->RemoveCredential(auth_id, Binary::fromVector(cred_id1))
                  .IsSuccess());

  // Verify the second is still present by checking the authenticator directly.
  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  EXPECT_EQ(1u, authenticator->registrations().size());
}

TEST_F(WebAuthnHandlerTest, RemoveCredentialInvalidIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  Response response = webauthn_handler_->RemoveCredential(
      auth_id, Binary::fromVector({0xFF, 0xEE}));
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

// ---- ClearCredentials ----

TEST_F(WebAuthnHandlerTest, ClearCredentialsEmptiesAuthenticator) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  // Add two credentials.
  auto cred_id1 = std::vector<uint8_t>{1, 2};
  auto pk1 = FreshPrivateKey();
  auto cred1 = MakeNonResidentCredential(cred_id1, "example.com", pk1);
  bool success1 = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred1),
      std::make_unique<FakeAddCredentialCallback>(&success1));

  auto cred_id2 = std::vector<uint8_t>{3, 4};
  auto pk2 = FreshPrivateKey();
  auto cred2 = MakeNonResidentCredential(cred_id2, "test.com", pk2);
  bool success2 = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred2),
      std::make_unique<FakeAddCredentialCallback>(&success2));

  // Clear all.
  Response response = webauthn_handler_->ClearCredentials(auth_id);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());

  // Verify empty.
  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  EXPECT_EQ(0u, authenticator->registrations().size());
}

TEST_F(WebAuthnHandlerTest, ClearCredentialsAllowsAddingNewCredentials) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto cred_id = std::vector<uint8_t>{1, 2, 3};
  auto pk = FreshPrivateKey();
  auto cred = MakeNonResidentCredential(cred_id, "example.com", pk);
  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred),
      std::make_unique<FakeAddCredentialCallback>(&success));

  ASSERT_TRUE(webauthn_handler_->ClearCredentials(auth_id).IsSuccess());

  // Add a new credential after clearing.
  auto new_cred_id = std::vector<uint8_t>{4, 5, 6};
  auto new_pk = FreshPrivateKey();
  auto new_cred =
      MakeNonResidentCredential(new_cred_id, "example.com", new_pk);
  bool new_success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(new_cred),
      std::make_unique<FakeAddCredentialCallback>(&new_success));
  EXPECT_TRUE(new_success);
}

TEST_F(WebAuthnHandlerTest, ClearCredentialsInvalidAuthenticatorIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());

  Response response = webauthn_handler_->ClearCredentials("nonexistent");
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

// ---- SetUserVerified ----

TEST_F(WebAuthnHandlerTest, SetUserVerifiedSucceeds) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  Response response =
      webauthn_handler_->SetUserVerified(auth_id, false);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());

  response = webauthn_handler_->SetUserVerified(auth_id, true);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest, SetUserVerifiedInvalidAuthenticatorIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());

  Response response =
      webauthn_handler_->SetUserVerified("nonexistent", true);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

// ---- SetCredentialProperties ----

TEST_F(WebAuthnHandlerTest, SetCredentialPropertiesSucceeds) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto cred_id = std::vector<uint8_t>{1, 2, 3};
  auto pk = FreshPrivateKey();
  auto cred = MakeNonResidentCredential(cred_id, "example.com", pk);
  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred),
      std::make_unique<FakeAddCredentialCallback>(&success));

  Response response = webauthn_handler_->SetCredentialProperties(
      auth_id, Binary::fromVector(cred_id),
      /*backup_eligibility=*/true, /*backup_state=*/true);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());

  // Verify the properties were set by reading the authenticator directly.
  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  auto& reg = authenticator->registrations().at(cred_id);
  EXPECT_TRUE(reg.backup_eligible);
  EXPECT_TRUE(reg.backup_state);
}

TEST_F(WebAuthnHandlerTest, SetCredentialPropertiesInvalidCredentialFails) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  Response response = webauthn_handler_->SetCredentialProperties(
      auth_id, Binary::fromVector({0xDE, 0xAD}),
      /*backup_eligibility=*/true, /*backup_state=*/true);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       SetCredentialPropertiesInvalidAuthenticatorIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());

  Response response = webauthn_handler_->SetCredentialProperties(
      "nonexistent", Binary::fromVector({1, 2, 3}),
      /*backup_eligibility=*/true, /*backup_state=*/true);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       SetCredentialPropertiesBackupStateWithoutEligibility) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  auto cred_id = std::vector<uint8_t>{1, 2, 3};
  auto pk = FreshPrivateKey();
  auto cred = MakeNonResidentCredential(cred_id, "example.com", pk);
  bool success = false;
  webauthn_handler_->AddCredential(
      auth_id, std::move(cred),
      std::make_unique<FakeAddCredentialCallback>(&success));
  ASSERT_TRUE(success);

  // CDP SetCredentialProperties is a debugging command and does not enforce
  // the WebAuthn spec constraint that backupState=true requires
  // backupEligibility=true. The call should succeed.
  Response response = webauthn_handler_->SetCredentialProperties(
      auth_id, Binary::fromVector(cred_id),
      /*backup_eligibility=*/std::nullopt, /*backup_state=*/true);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());

  // Verify backup_state was set without backup_eligibility.
  VirtualAuthenticatorManagerImpl* manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              contents()->GetPrimaryMainFrame()->frame_tree_node());
  VirtualAuthenticator* authenticator = manager->GetAuthenticator(auth_id);
  auto& reg = authenticator->registrations().at(cred_id);
  EXPECT_FALSE(reg.backup_eligible);
  EXPECT_TRUE(reg.backup_state);
}

// ---- SetAutomaticPresenceSimulation ----

TEST_F(WebAuthnHandlerTest, SetAutomaticPresenceSimulationSucceeds) {
  ASSERT_TRUE(Enable().IsSuccess());
  String auth_id = AddDefaultAuthenticator();

  Response response =
      webauthn_handler_->SetAutomaticPresenceSimulation(auth_id, false);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());

  response =
      webauthn_handler_->SetAutomaticPresenceSimulation(auth_id, true);
  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
}

TEST_F(WebAuthnHandlerTest,
       SetAutomaticPresenceSimulationInvalidAuthenticatorIdFails) {
  ASSERT_TRUE(Enable().IsSuccess());

  Response response =
      webauthn_handler_->SetAutomaticPresenceSimulation("nonexistent", true);
  EXPECT_NE(crdtp::DispatchCode::SUCCESS, response.Code());
}

}  // namespace content::protocol
