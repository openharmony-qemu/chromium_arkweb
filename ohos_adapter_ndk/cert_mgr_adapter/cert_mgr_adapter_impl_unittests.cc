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

#include "cert_mgr_adapter_impl.h"
#include "third_party/bounds_checking_function/include/securec.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network/netstack/net_ssl/net_ssl_c.h>

#include <cstdint>
#include <cstring>
#include <thread>
#include <huks/native_huks_api.h>
#include <huks/native_huks_external_crypto_api.h>
#include <huks/native_huks_param.h>
#include <device_certificate/certmanager/cm_native_api.h>
#include "third_party/boringssl/src/include/openssl/ssl.h"

using namespace testing;
using namespace std;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace OHOS::NWeb {
// ---------- Mock for NetStack APIs (GetTrustAnchorsForHostName, GetPinSetForHostName) ----------
class MockNetStack {
 public:
  MOCK_METHOD(int32_t,
              Mock_OH_NetStack_GetCertificatesForHostName,
              (const char*, NetStack_Certificates*));
  MOCK_METHOD(void,
              Mock_OH_Netstack_DestroyCertificatesContent,
              (NetStack_Certificates*));
  MOCK_METHOD(int32_t,
              Mock_OH_NetStack_GetPinSetForHostName,
              (const char*, NetStack_CertificatePinning*));
};

MockNetStack* g_mockNetStack = nullptr;

extern "C" {
int32_t OH_NetStack_GetCertificatesForHostName(
    const char* hostname,
    NetStack_Certificates* certificates) {
  if (hostname == nullptr || certificates == nullptr) {
    return -1;
  }
  return g_mockNetStack->Mock_OH_NetStack_GetCertificatesForHostName(
      hostname, certificates);
}

void OH_Netstack_DestroyCertificatesContent(
    NetStack_Certificates* certificates) {
  if (certificates == nullptr) {
    return;
  }
  g_mockNetStack->Mock_OH_Netstack_DestroyCertificatesContent(certificates);
}
int32_t OH_NetStack_GetPinSetForHostName(
    const char* hostname,
    NetStack_CertificatePinning* certificates) {
  if (hostname == nullptr || certificates == nullptr) {
    return -1;
  }
  return g_mockNetStack->Mock_OH_NetStack_GetPinSetForHostName(hostname,
                                                               certificates);
}
} // extern "C"

// ---------- Mock for Huks APIs ----------
class MockHuks {
 public:
  MOCK_METHOD(OH_Huks_Result, OH_Huks_InitParamSet, (OH_Huks_ParamSet **paramSet));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_AddParams, (OH_Huks_ParamSet *paramSet,
                                                    const OH_Huks_Param *params,
                                                    uint32_t paramCount));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_BuildParamSet, (OH_Huks_ParamSet **paramSet));
  MOCK_METHOD(void, OH_Huks_FreeParamSet, (OH_Huks_ParamSet **paramSet));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_InitSession, (const OH_Huks_Blob* keyAlias,
                                                      const OH_Huks_ParamSet* paramSet,
                                                      OH_Huks_Blob* handle,
                                                      OH_Huks_Blob* token));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_FinishSession, (const OH_Huks_Blob* handle,
                                                        const OH_Huks_ParamSet* paramSet,
                                                        const OH_Huks_Blob* inData,
                                                        OH_Huks_Blob* outData));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_InitExternalCryptoParamSet, (OH_Huks_ExternalCryptoParamSet **paramSet));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_AddExternalCryptoParams, (OH_Huks_ExternalCryptoParamSet *paramSet,
                                                                  const OH_Huks_ExternalCryptoParam *params,
                                                                  uint32_t paramCnt));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_BuildExternalCryptoParamSet, (OH_Huks_ExternalCryptoParamSet **paramSet));
  MOCK_METHOD(void, OH_Huks_FreeExternalCryptoParamSet, (OH_Huks_ExternalCryptoParamSet **paramSet));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_OpenResource, (const struct OH_Huks_Blob *resourceId,
                                                        const OH_Huks_ExternalCryptoParamSet *paramSet));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_CloseResource, (const struct OH_Huks_Blob *resourceId,
                                                         const OH_Huks_ExternalCryptoParamSet *paramSet));
  MOCK_METHOD(OH_Huks_Result, OH_Huks_GetUkeyPinAuthState, (const struct OH_Huks_Blob *resourceId,
                                                               const OH_Huks_ExternalCryptoParamSet *paramSet,
                                                               OH_Huks_ExternalPinAuthState *authState));
};

MockHuks* g_mockHuks = nullptr;

extern "C" {
OH_Huks_Result OH_Huks_InitParamSet(OH_Huks_ParamSet **paramSet) {
  return g_mockHuks->OH_Huks_InitParamSet(paramSet);
}

OH_Huks_Result OH_Huks_AddParams(OH_Huks_ParamSet* paramSet, const OH_Huks_Param* params, uint32_t paramCount) {
  return g_mockHuks->OH_Huks_AddParams(paramSet, params, paramCount);
}

OH_Huks_Result OH_Huks_BuildParamSet(OH_Huks_ParamSet **paramSet) {
  return g_mockHuks->OH_Huks_BuildParamSet(paramSet);
}

void OH_Huks_FreeParamSet(OH_Huks_ParamSet **paramSet) {
  g_mockHuks->OH_Huks_FreeParamSet(paramSet);
}

OH_Huks_Result OH_Huks_InitSession(const OH_Huks_Blob* keyAlias,
                                   const OH_Huks_ParamSet* paramSet,
                                   OH_Huks_Blob* handle,
                                   OH_Huks_Blob* token) {
  return g_mockHuks->OH_Huks_InitSession(keyAlias, paramSet, handle, token);
}

OH_Huks_Result OH_Huks_FinishSession(const OH_Huks_Blob* handle,
                                     const OH_Huks_ParamSet* paramSet,
                                     const OH_Huks_Blob* inData,
                                     OH_Huks_Blob* outData) {
  return g_mockHuks->OH_Huks_FinishSession(handle, paramSet, inData, outData);
}

OH_Huks_Result OH_Huks_InitExternalCryptoParamSet(OH_Huks_ExternalCryptoParamSet **paramSet) {
  return g_mockHuks->OH_Huks_InitExternalCryptoParamSet(paramSet);
}

OH_Huks_Result OH_Huks_AddExternalCryptoParams(OH_Huks_ExternalCryptoParamSet *paramSet,
                                                const OH_Huks_ExternalCryptoParam *params,
                                                uint32_t paramCnt) {
  return g_mockHuks->OH_Huks_AddExternalCryptoParams(paramSet, params, paramCnt);
}

OH_Huks_Result OH_Huks_BuildExternalCryptoParamSet(OH_Huks_ExternalCryptoParamSet **paramSet) {
  return g_mockHuks->OH_Huks_BuildExternalCryptoParamSet(paramSet);
}

void OH_Huks_FreeExternalCryptoParamSet(OH_Huks_ExternalCryptoParamSet **paramSet) {
  g_mockHuks->OH_Huks_FreeExternalCryptoParamSet(paramSet);
}

OH_Huks_Result OH_Huks_OpenResource(const struct OH_Huks_Blob *resourceId,
                                     const OH_Huks_ExternalCryptoParamSet *paramSet) {
  return g_mockHuks->OH_Huks_OpenResource(resourceId, paramSet);
}

OH_Huks_Result OH_Huks_CloseResource(const struct OH_Huks_Blob *resourceId,
                                      const OH_Huks_ExternalCryptoParamSet *paramSet) {
  return g_mockHuks->OH_Huks_CloseResource(resourceId, paramSet);
}

OH_Huks_Result OH_Huks_GetUkeyPinAuthState(const struct OH_Huks_Blob *resourceId,
                                            const OH_Huks_ExternalCryptoParamSet *paramSet,
                                            OH_Huks_ExternalPinAuthState *authState) {
  return g_mockHuks->OH_Huks_GetUkeyPinAuthState(resourceId, paramSet, authState);
}
} // extern "C"

// ---------- Mock for CertManager APIs ----------
class MockCertManager {
 public:
  MOCK_METHOD(int32_t, OH_CertManager_GetUkeyCertificate, (const OH_CM_Blob *ukeyCertIndex,
                                                             const OH_CM_UkeyInfo *ukeyInfo,
                                                             OH_CM_CredentialDetailList *certificateList));
  MOCK_METHOD(void, OH_CertManager_FreeUkeyCertificate, (OH_CM_CredentialDetailList *certificateList));
};

MockCertManager* g_mockCertManager = nullptr;

extern "C" {
int32_t OH_CertManager_GetUkeyCertificate(const OH_CM_Blob *ukeyCertIndex,
                                           const OH_CM_UkeyInfo *ukeyInfo,
                                           OH_CM_CredentialDetailList *certificateList) {
  return g_mockCertManager->OH_CertManager_GetUkeyCertificate(ukeyCertIndex, ukeyInfo, certificateList);
}

void OH_CertManager_FreeUkeyCertificate(OH_CM_CredentialDetailList *certificateList) {
  g_mockCertManager->OH_CertManager_FreeUkeyCertificate(certificateList);
}
} // extern "C"

namespace cert_mgr_adapter_impl_test {
    struct HuksSignatureSpec {
        uint32_t algorithm;
        uint32_t padding;
        uint32_t digest;
    };

    OH_Huks_Result InitParamSet(struct OH_Huks_ParamSet** paramSet,
                                const struct OH_Huks_Param* params,
                                uint32_t paramCount);
    OH_Huks_Result InitExtParamSet(OH_Huks_ExternalCryptoParamSet** paramSet,
                                const OH_Huks_ExternalCryptoParam* params,
                                uint32_t paramCount);
    bool GetHuksSignatureSpec(uint16_t algorithm, HuksSignatureSpec& result);
} // namespace cert_mgr_adapter_impl_test

// ---------- Unified Test fixture ----------
class CertManagerAdapterImplTest : public testing::Test {
 public:
  void SetUp() override {
    g_mockNetStack = new MockNetStack();
    g_mockHuks = new MockHuks();
    g_mockCertManager = new MockCertManager();
  }
  void TearDown() override {
    delete g_mockNetStack;
    g_mockNetStack = nullptr;
    delete g_mockHuks;
    g_mockHuks = nullptr;
    delete g_mockCertManager;
    g_mockCertManager = nullptr;
  }
  CertManagerAdapterImpl adapter;
};

// ==================== GetTrustAnchorsForHostName tests ====================

// Test case 1: Success case with certificates returned
TEST_F(CertManagerAdapterImplTest, GetTrustAnchorsForHostName001) {
  const string hostname = "example.com";
  vector<string> expected_certs = {"cert1", "cert2"};

  NetStack_Certificates mock_certs;
  mock_certs.length = expected_certs.size();
  mock_certs.content = new char *[mock_certs.length];
  for (size_t i = 0; i < mock_certs.length; i++) {
    mock_certs.content[i] = new char[expected_certs[i].size() + 1];
    errno_t res = strcpy_s(mock_certs.content[i], sizeof(mock_certs.content[i]), expected_certs[i].c_str());
    if (res != EOK) {
      return;
    }
  }

  EXPECT_CALL(*g_mockNetStack, Mock_OH_NetStack_GetCertificatesForHostName(
                                   StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_certs), Return(0)));

  EXPECT_CALL(*g_mockNetStack, Mock_OH_Netstack_DestroyCertificatesContent(_))
      .Times(1);

  vector<string> actual_certs;
  bool result = adapter.GetTrustAnchorsForHostName(hostname, actual_certs);
  EXPECT_TRUE(result);
  EXPECT_THAT(actual_certs, ContainerEq(expected_certs));

  for (size_t i = 0; i < mock_certs.length; i++) {
    delete[] mock_certs.content[i];
  }
  delete[] mock_certs.content;
}

// Test case 2: API return failure
TEST_F(CertManagerAdapterImplTest, GetTrustAnchorsForHostName002) {
  const string hostname = "invalid.com";

  EXPECT_CALL(*g_mockNetStack, Mock_OH_NetStack_GetCertificatesForHostName(
                                   StrEq(hostname.c_str()), _))
      .WillOnce(Return(-1));

  EXPECT_CALL(*g_mockNetStack, Mock_OH_Netstack_DestroyCertificatesContent(_))
      .Times(1);

  vector<string> actual_certs;
  bool result = adapter.GetTrustAnchorsForHostName(hostname, actual_certs);
  EXPECT_FALSE(result);
  EXPECT_TRUE(actual_certs.empty());
}

// Test case 3: Empty certificates returned
TEST_F(CertManagerAdapterImplTest, GetTrustAnchorsForHostName003) {
  const string hostname = "empty.example.com";

  NetStack_Certificates mock_certs;
  mock_certs.length = 0;
  mock_certs.content = nullptr;

  EXPECT_CALL(*g_mockNetStack, Mock_OH_NetStack_GetCertificatesForHostName(
                                   StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_certs), Return(0)));

  EXPECT_CALL(*g_mockNetStack, Mock_OH_Netstack_DestroyCertificatesContent(_))
      .Times(1);

  vector<string> actual_certs;
  bool result = adapter.GetTrustAnchorsForHostName(hostname, actual_certs);

  EXPECT_TRUE(result);
  EXPECT_TRUE(actual_certs.empty());
}

// Test case 4: Memory management test - ensure destruction is called even on failure
TEST_F(CertManagerAdapterImplTest, GetTrustAnchorsForHostName004) {
  const string hostname = "test.com";

  NetStack_Certificates mock_certs;
  mock_certs.length = 2;
  mock_certs.content = new char *[mock_certs.length];
  mock_certs.content[0] = new char[10];
  errno_t res = strcpy_s(mock_certs.content[0], sizeof(mock_certs.content[0]), "cert1");
  if (res != EOK) {
    return;
  }

  mock_certs.content[1] = new char[10];
  res = strcpy_s(mock_certs.content[1], sizeof(mock_certs.content[1]), "cert2");
  if (res != EOK) {
    return;
  }

  EXPECT_CALL(*g_mockNetStack, Mock_OH_NetStack_GetCertificatesForHostName(
                                   StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_certs), Return(1)));

  EXPECT_CALL(*g_mockNetStack, Mock_OH_Netstack_DestroyCertificatesContent(_))
      .Times(1);

  vector<string> actual_certs;
  bool result = adapter.GetTrustAnchorsForHostName(hostname, actual_certs);
  EXPECT_FALSE(result);

  delete[] mock_certs.content[0];
  delete[] mock_certs.content[1];
  delete[] mock_certs.content;
}

// ==================== GetPinSetForHostName tests ====================

// Test case 1: Success case with multiple pins
TEST_F(CertManagerAdapterImplTest, GetPinSetForHostName001) {
  const string hostname = "example.com";
  const string pinsString = "pin1;pin2;pin3";
  vector<string> expected_pins = {"pin1", "pin2", "pin3"};

  NetStack_CertificatePinning mock_pining;
  mock_pining.publicKeyHash = strdup(pinsString.c_str());
  if (mock_pining.publicKeyHash == nullptr) {
    return;
  }

  EXPECT_CALL(*g_mockNetStack,
              Mock_OH_NetStack_GetPinSetForHostName(StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_pining), Return(0)));

  vector<string> actual_pins;
  bool result = adapter.GetPinSetForHostName(hostname, actual_pins);

  EXPECT_TRUE(result);
  EXPECT_THAT(actual_pins, ContainerEq(expected_pins));
}

// Test case 2: Success case with single pins
TEST_F(CertManagerAdapterImplTest, GetPinSetForHostName002) {
  const string hostname = "single.example.com";
  const string pinsString = "single_pin";
  vector<string> expected_pins = {"single_pin"};

  NetStack_CertificatePinning mock_pining;
  mock_pining.publicKeyHash = strdup(pinsString.c_str());
  if (mock_pining.publicKeyHash == nullptr) {
    return;
  }

  EXPECT_CALL(*g_mockNetStack,
              Mock_OH_NetStack_GetPinSetForHostName(StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_pining), Return(0)));

  vector<string> actual_pins;
  bool result = adapter.GetPinSetForHostName(hostname, actual_pins);
  EXPECT_TRUE(result);
  EXPECT_THAT(actual_pins, ContainerEq(expected_pins));
}

// Test case 3: Empty publicKeyHash (nullptr)
TEST_F(CertManagerAdapterImplTest, GetPinSetForHostName003) {
  const string hostname = "empty.example.com";
  NetStack_CertificatePinning mock_pining;
  mock_pining.publicKeyHash = nullptr;

  EXPECT_CALL(*g_mockNetStack,
              Mock_OH_NetStack_GetPinSetForHostName(StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_pining), Return(0)));

  vector<string> actual_pins;
  bool result = adapter.GetPinSetForHostName(hostname, actual_pins);
  EXPECT_TRUE(result);
  EXPECT_TRUE(actual_pins.empty());
}

// Test case 4: Empty publicKeyHash (empty string)
TEST_F(CertManagerAdapterImplTest, GetPinSetForHostName004) {
  const string hostname = "empty-string.example.com";
  vector<string> expected_pins = {""};

  NetStack_CertificatePinning mock_pining;
  mock_pining.publicKeyHash = strdup("");
  if (mock_pining.publicKeyHash == nullptr) {
    return;
  }

  EXPECT_CALL(*g_mockNetStack,
              Mock_OH_NetStack_GetPinSetForHostName(StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_pining), Return(0)));

  vector<string> actual_pins;
  bool result = adapter.GetPinSetForHostName(hostname, actual_pins);

  EXPECT_TRUE(result);
  EXPECT_THAT(actual_pins, ContainerEq(expected_pins));
}

// Test case 5: API return failure
TEST_F(CertManagerAdapterImplTest, GetPinSetForHostName005) {
  const string hostname = "failure.example.com";

  EXPECT_CALL(*g_mockNetStack,
              Mock_OH_NetStack_GetPinSetForHostName(StrEq(hostname.c_str()), _))
      .WillOnce(Return(-1));

  vector<string> actual_pins;
  bool result = adapter.GetPinSetForHostName(hostname, actual_pins);

  EXPECT_FALSE(result);
  EXPECT_TRUE(actual_pins.empty());
}

// Test case 6: Pins with various separators
TEST_F(CertManagerAdapterImplTest, GetPinSetForHostName006) {
  const string hostname = "complex.example.com";
  const string pinsString = "pin1;pin2;pin3;pin4";
  vector<string> expected_pins = {"pin1", "pin2", "pin3", "pin4"};

  NetStack_CertificatePinning mock_pining;
  mock_pining.publicKeyHash = strdup(pinsString.c_str());
  if (mock_pining.publicKeyHash == nullptr) {
    return;
  }

  EXPECT_CALL(*g_mockNetStack,
              Mock_OH_NetStack_GetPinSetForHostName(StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_pining), Return(0)));

  vector<string> actual_pins;
  bool result = adapter.GetPinSetForHostName(hostname, actual_pins);

  EXPECT_TRUE(result);
  EXPECT_THAT(actual_pins, ContainerEq(expected_pins));
}

// Test case 7: No semicolon in pins string
TEST_F(CertManagerAdapterImplTest, GetPinSetForHostName007) {
  const string hostname = "nosemicolon.example.com";
  const string pinsString = "justonepin";
  vector<string> expected_pins = {"justonepin"};

  NetStack_CertificatePinning mock_pining;
  mock_pining.publicKeyHash = strdup(pinsString.c_str());
  if (mock_pining.publicKeyHash == nullptr) {
    return;
  }

  EXPECT_CALL(*g_mockNetStack,
              Mock_OH_NetStack_GetPinSetForHostName(StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_pining), Return(0)));

  vector<string> actual_pins;
  bool result = adapter.GetPinSetForHostName(hostname, actual_pins);

  EXPECT_TRUE(result);
  EXPECT_THAT(actual_pins, ContainerEq(expected_pins));
}

// Test case 8: Memory leak test - ensure free called (indirectly)
TEST_F(CertManagerAdapterImplTest, GetPinSetForHostName008) {
  const string hostname = "memory.example.com";
  const string pinsString = "pin1;pin2";

  NetStack_CertificatePinning mock_pining;
  mock_pining.publicKeyHash = strdup(pinsString.c_str());
  if (mock_pining.publicKeyHash == nullptr) {
    return;
  }

  EXPECT_CALL(*g_mockNetStack,
              Mock_OH_NetStack_GetPinSetForHostName(StrEq(hostname.c_str()), _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_pining), Return(0)));

  vector<string> actual_pins;
  bool result = adapter.GetPinSetForHostName(hostname, actual_pins);

  EXPECT_TRUE(result);
  EXPECT_THAT(actual_pins, ElementsAre("pin1", "pin2"));
}

// ==================== InitParamSet tests ====================

TEST_F(CertManagerAdapterImplTest, InitParamSet_001) {
  OH_Huks_ParamSet* paramSet = nullptr;
  OH_Huks_Param params[2] = {
      {.tag = OH_HUKS_TAG_ALGORITHM, .uint32Param = OH_HUKS_ALG_RSA},
      {.tag = OH_HUKS_TAG_KEY_SIZE, .uint32Param = 2048}
  };
  uint32_t paramCount = 2;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddParams(::testing::_, ::testing::_, paramCount))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeParamSet(::testing::_)).Times(0);

  OH_Huks_Result result = cert_mgr_adapter_impl_test::InitParamSet(&paramSet, params, paramCount);
  EXPECT_EQ(result.errorCode, OH_HUKS_SUCCESS);
}

TEST_F(CertManagerAdapterImplTest, InitParamSet_002) {
  OH_Huks_ParamSet* paramSet = nullptr;
  OH_Huks_Param params[1] = {{.tag = OH_HUKS_TAG_ALGORITHM, .uint32Param = OH_HUKS_ALG_RSA}};
  uint32_t paramCount = 1;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddParams(::testing::_, ::testing::_, ::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeParamSet(::testing::_)).Times(0);

  OH_Huks_Result result = cert_mgr_adapter_impl_test::InitParamSet(&paramSet, params, paramCount);
  EXPECT_EQ(result.errorCode, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

TEST_F(CertManagerAdapterImplTest, InitParamSet_003) {
  OH_Huks_ParamSet* paramSet = nullptr;
  OH_Huks_Param params[1] = {{.tag = OH_HUKS_TAG_ALGORITHM, .uint32Param = OH_HUKS_ALG_RSA}};
  uint32_t paramCount = 1;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddParams(::testing::_, ::testing::_, paramCount))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeParamSet(::testing::_))
      .WillOnce(::testing::Return());
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildParamSet(::testing::_)).Times(0);

  OH_Huks_Result result = cert_mgr_adapter_impl_test::InitParamSet(&paramSet, params, paramCount);
  EXPECT_EQ(result.errorCode, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

TEST_F(CertManagerAdapterImplTest, InitParamSet_004) {
  OH_Huks_ParamSet* paramSet = nullptr;
  OH_Huks_Param params[1] = {{.tag = OH_HUKS_TAG_ALGORITHM, .uint32Param = OH_HUKS_ALG_RSA}};
  uint32_t paramCount = 1;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddParams(::testing::_, ::testing::_, paramCount))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeParamSet(::testing::_))
      .WillOnce(::testing::Return());

  OH_Huks_Result result = cert_mgr_adapter_impl_test::InitParamSet(&paramSet, params, paramCount);
  EXPECT_EQ(result.errorCode, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// ==================== InitExtParamSet tests ====================

TEST_F(CertManagerAdapterImplTest, InitExtParamSet_001) {
  OH_Huks_ExternalCryptoParamSet* paramSet = nullptr;
  OH_Huks_ExternalCryptoParam params[1];
  uint32_t paramCount = 1;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(0);

  OH_Huks_Result result = cert_mgr_adapter_impl_test::InitExtParamSet(&paramSet, params, paramCount);
  EXPECT_EQ(result.errorCode, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

TEST_F(CertManagerAdapterImplTest, InitExtParamSet_002) {
  OH_Huks_ExternalCryptoParamSet* paramSet = nullptr;
  OH_Huks_ExternalCryptoParam params[1];
  uint32_t paramCount = 1;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_)).Times(0);

  OH_Huks_Result result = cert_mgr_adapter_impl_test::InitExtParamSet(&paramSet, params, paramCount);
  EXPECT_EQ(result.errorCode, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

TEST_F(CertManagerAdapterImplTest, InitExtParamSet_003) {
  OH_Huks_ExternalCryptoParamSet* paramSet = nullptr;
  OH_Huks_ExternalCryptoParam params[1];
  uint32_t paramCount = 1;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  OH_Huks_Result result = cert_mgr_adapter_impl_test::InitExtParamSet(&paramSet, params, paramCount);
  EXPECT_EQ(result.errorCode, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

TEST_F(CertManagerAdapterImplTest, InitExtParamSet_004) {
  OH_Huks_ExternalCryptoParamSet* paramSet = nullptr;
  OH_Huks_ExternalCryptoParam params[1];
  uint32_t paramCount = 1;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(0);

  OH_Huks_Result result = cert_mgr_adapter_impl_test::InitExtParamSet(&paramSet, params, paramCount);
  EXPECT_EQ(result.errorCode, OH_HUKS_SUCCESS);
}

// ==================== GetHuksSignatureSpec tests ====================

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_001) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_RSA_PKCS1_SHA256, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_RSA);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_PKCS1_V1_5);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA256);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_002) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_RSA_PKCS1_SHA384, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_RSA);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_PKCS1_V1_5);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA384);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_003) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_RSA_PKCS1_SHA512, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_RSA);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_PKCS1_V1_5);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA512);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_004) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_RSA_PSS_RSAE_SHA256, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_RSA);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_PSS);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA256);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_005) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_RSA_PSS_RSAE_SHA384, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_RSA);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_PSS);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA384);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_006) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_RSA_PSS_RSAE_SHA512, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_RSA);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_PSS);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA512);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_007) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_ECDSA_SECP256R1_SHA256, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_ECC);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_NONE);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA256);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_008) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_ECDSA_SECP384R1_SHA384, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_ECC);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_NONE);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA384);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_009) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(SSL_SIGN_ECDSA_SECP521R1_SHA512, result);

  EXPECT_TRUE(success);
  EXPECT_EQ(result.algorithm, OH_HUKS_ALG_ECC);
  EXPECT_EQ(result.padding, OH_HUKS_PADDING_NONE);
  EXPECT_EQ(result.digest, OH_HUKS_DIGEST_SHA512);
}

TEST_F(CertManagerAdapterImplTest, GetHuksSignatureSpec_010) {
  cert_mgr_adapter_impl_test::HuksSignatureSpec result;
  bool success = cert_mgr_adapter_impl_test::GetHuksSignatureSpec(0xFFFF, result);
  EXPECT_FALSE(success);
}

// ==================== GetUkeyCert tests ====================

// Test case 1: GetUkeyCertificate returns failure
TEST_F(CertManagerAdapterImplTest, GetUkeyCert_001) {
  const string identity = "test_identity";
  uint8_t certData[OH_CM_MAX_LEN_CERTIFICATE_CHAIN];
  uint32_t certDataLen = 0;

  EXPECT_CALL(*g_mockCertManager, OH_CertManager_GetUkeyCertificate(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(-1));
  EXPECT_CALL(*g_mockCertManager, OH_CertManager_FreeUkeyCertificate(::testing::_)).Times(1);

  int32_t result = adapter.GetUkeyCert(identity, certData, &certDataLen);
  EXPECT_EQ(result, -1);
}

// Test case 2: GetUkeyCertificate returns success with empty credential list
TEST_F(CertManagerAdapterImplTest, GetUkeyCert_002) {
  const string identity = "test_identity";
  uint8_t certData[OH_CM_MAX_LEN_CERTIFICATE_CHAIN];
  uint32_t certDataLen = 0;

  OH_CM_CredentialDetailList emptyCredList = {0, nullptr};

  EXPECT_CALL(*g_mockCertManager, OH_CertManager_GetUkeyCertificate(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(DoAll(SetArgPointee<2>(emptyCredList), ::testing::Return(OH_CM_SUCCESS)));
  EXPECT_CALL(*g_mockCertManager, OH_CertManager_FreeUkeyCertificate(::testing::_)).Times(1);

  int32_t result = adapter.GetUkeyCert(identity, certData, &certDataLen);
  EXPECT_EQ(result, OH_CM_SUCCESS);
}

// Test case 3: GetUkeyCertificate returns success with null credential pointer
TEST_F(CertManagerAdapterImplTest, GetUkeyCert_003) {
  const string identity = "test_identity";
  uint8_t certData[OH_CM_MAX_LEN_CERTIFICATE_CHAIN];
  uint32_t certDataLen = 0;

  static OH_CM_Credential nullDataCredArray[1] = {
      {1, "test_type", "test_alias", "test_key_uri", 1, 1, {100, nullptr}, OH_CM_CERT_PURPOSE_SIGN}
  };
  OH_CM_CredentialDetailList nullDataCredList = {1, nullDataCredArray};

  EXPECT_CALL(*g_mockCertManager, OH_CertManager_GetUkeyCertificate(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(DoAll(SetArgPointee<2>(nullDataCredList), ::testing::Return(OH_CM_SUCCESS)));
  EXPECT_CALL(*g_mockCertManager, OH_CertManager_FreeUkeyCertificate(::testing::_)).Times(1);

  int32_t result = adapter.GetUkeyCert(identity, certData, &certDataLen);
  EXPECT_EQ(result, OH_CM_SUCCESS);
}

// Test case 4: GetUkeyCertificate returns success with zero-size credential data
TEST_F(CertManagerAdapterImplTest, GetUkeyCert_004) {
  const string identity = "test_identity";
  uint8_t certData[OH_CM_MAX_LEN_CERTIFICATE_CHAIN];
  uint32_t certDataLen = 0;

  static uint8_t dummyData[100];
  static OH_CM_Credential zeroSizeCredArray[1] = {
      {1, "test_type", "test_alias", "test_key_uri", 1, 1, {0, dummyData}, OH_CM_CERT_PURPOSE_SIGN}
  };
  OH_CM_CredentialDetailList zeroSizeCredList = {1, zeroSizeCredArray};

  EXPECT_CALL(*g_mockCertManager, OH_CertManager_GetUkeyCertificate(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(DoAll(SetArgPointee<2>(zeroSizeCredList), ::testing::Return(OH_CM_SUCCESS)));
  EXPECT_CALL(*g_mockCertManager, OH_CertManager_FreeUkeyCertificate(::testing::_)).Times(1);

  int32_t result = adapter.GetUkeyCert(identity, certData, &certDataLen);
  EXPECT_EQ(result, OH_CM_SUCCESS);
}

// Test case 5: GetUkeyCertificate success with valid credential data
TEST_F(CertManagerAdapterImplTest, GetUkeyCert_005) {
  const string identity = "test_identity";
  uint8_t certData[OH_CM_MAX_LEN_CERTIFICATE_CHAIN];
  uint32_t certDataLen = 0;

  static uint8_t dummyCertData[100];
  memset_s(dummyCertData, sizeof(dummyCertData), 'A', sizeof(dummyCertData));
  static OH_CM_Credential validCredArray[1] = {
      {1, "test_type", "test_alias", "test_key_uri", 1, 1, {100, dummyCertData}, OH_CM_CERT_PURPOSE_SIGN}
  };
  OH_CM_CredentialDetailList validCredList = {1, validCredArray};

  EXPECT_CALL(*g_mockCertManager, OH_CertManager_GetUkeyCertificate(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(DoAll(SetArgPointee<2>(validCredList), ::testing::Return(OH_CM_SUCCESS)));
  EXPECT_CALL(*g_mockCertManager, OH_CertManager_FreeUkeyCertificate(::testing::_)).Times(1);

  int32_t result = adapter.GetUkeyCert(identity, certData, &certDataLen);
  EXPECT_EQ(result, OH_CM_SUCCESS);
  EXPECT_EQ(certDataLen, 100u);
}

// ==================== OpenUKeyRemoteHandle tests ====================

// Test case 1: InitExtParamSet fails
TEST_F(CertManagerAdapterImplTest, OpenUKeyRemoteHandle_001) {
  const string identity = "test_identity";

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_OpenResource(::testing::_, ::testing::_)).Times(0);

  int32_t result = adapter.OpenUKeyRemoteHandle(identity);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 2: OpenResource returns failure (not KEY_ALREADY_EXIST)
TEST_F(CertManagerAdapterImplTest, OpenUKeyRemoteHandle_002) {
  const string identity = "test_identity";

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_OpenResource(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  int32_t result = adapter.OpenUKeyRemoteHandle(identity);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 3: OpenResource returns KEY_ALREADY_EXIST (treated as success)
TEST_F(CertManagerAdapterImplTest, OpenUKeyRemoteHandle_003) {
  const string identity = "test_identity";

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_OpenResource(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_KEY_ALREADY_EXIST, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  int32_t result = adapter.OpenUKeyRemoteHandle(identity);
  EXPECT_EQ(result, OH_HUKS_SUCCESS);
}

// Test case 4: Full success path
TEST_F(CertManagerAdapterImplTest, OpenUKeyRemoteHandle_004) {
  const string identity = "test_identity";

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_OpenResource(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  int32_t result = adapter.OpenUKeyRemoteHandle(identity);
  EXPECT_EQ(result, OH_HUKS_SUCCESS);
}

// ==================== CloseUKeyRemoteHandle tests ====================

// Test case 1: InitExtParamSet fails
TEST_F(CertManagerAdapterImplTest, CloseUKeyRemoteHandle_001) {
  const string identity = "test_identity";

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_CloseResource(::testing::_, ::testing::_)).Times(0);

  int32_t result = adapter.CloseUKeyRemoteHandle(identity);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 2: CloseResource returns failure
TEST_F(CertManagerAdapterImplTest, CloseUKeyRemoteHandle_002) {
  const string identity = "test_identity";

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_CloseResource(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  int32_t result = adapter.CloseUKeyRemoteHandle(identity);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 3: Full success path
TEST_F(CertManagerAdapterImplTest, CloseUKeyRemoteHandle_003) {
  const string identity = "test_identity";

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_CloseResource(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  int32_t result = adapter.CloseUKeyRemoteHandle(identity);
  EXPECT_EQ(result, OH_HUKS_SUCCESS);
}

// ==================== GetUkeyPinAuthState tests ====================

// Test case 1: InitExtParamSet fails
TEST_F(CertManagerAdapterImplTest, GetUkeyPinAuthState_001) {
  const string identity = "test_identity";
  bool state = true;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_GetUkeyPinAuthState(::testing::_, ::testing::_, ::testing::_)).Times(0);

  int32_t result = adapter.GetUkeyPinAuthState(identity, &state);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 2: GetUkeyPinAuthState returns failure
TEST_F(CertManagerAdapterImplTest, GetUkeyPinAuthState_002) {
  const string identity = "test_identity";
  bool state = true;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_GetUkeyPinAuthState(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  int32_t result = adapter.GetUkeyPinAuthState(identity, &state);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 3: GetUkeyPinAuthState success with PIN_AUTH_SUCCEEDED
TEST_F(CertManagerAdapterImplTest, GetUkeyPinAuthState_003) {
  const string identity = "test_identity";
  bool state = false;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_GetUkeyPinAuthState(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(DoAll(SetArgPointee<2>(OH_HUKS_EXT_CRYPTO_PIN_AUTH_SUCCEEDED),
                      ::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0})));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  int32_t result = adapter.GetUkeyPinAuthState(identity, &state);
  EXPECT_EQ(result, OH_HUKS_SUCCESS);
  EXPECT_TRUE(state);
}

// Test case 4: GetUkeyPinAuthState success with PIN_NO_AUTH
TEST_F(CertManagerAdapterImplTest, GetUkeyPinAuthState_004) {
  const string identity = "test_identity";
  bool state = true;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddExternalCryptoParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildExternalCryptoParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_GetUkeyPinAuthState(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(DoAll(SetArgPointee<2>(OH_HUKS_EXT_CRYPTO_PIN_NO_AUTH),
                      ::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0})));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeExternalCryptoParamSet(::testing::_)).Times(1);

  int32_t result = adapter.GetUkeyPinAuthState(identity, &state);
  EXPECT_EQ(result, OH_HUKS_SUCCESS);
  EXPECT_FALSE(state);
}

// ==================== SignUsingHuks tests ====================

// Test case 1: Unsupported algorithm
TEST_F(CertManagerAdapterImplTest, SignUsingHuks_001) {
  const string identity = "test_identity";
  uint8_t certData[100] = {0};
  uint32_t certDataLen = 100;
  uint8_t signData[128] = {0};
  uint32_t signDataLen = 128;
  uint16_t algorithm = 0xFFFF;
  uint32_t keySize = 2048;

  int32_t result =
      adapter.SignUsingHuks(identity, certData, certDataLen, signData,
                            &signDataLen, algorithm, keySize);
  EXPECT_EQ(result, -1);
}

// Test case 2: InitParamSet fails
TEST_F(CertManagerAdapterImplTest, SignUsingHuks_002) {
  const string identity = "test_identity";
  uint8_t certData[100] = {0};
  uint32_t certDataLen = 100;
  uint8_t signData[128] = {0};
  uint32_t signDataLen = 128;
  uint16_t algorithm = SSL_SIGN_RSA_PKCS1_SHA256;
  uint32_t keySize = 2048;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));

  EXPECT_CALL(*g_mockHuks, OH_Huks_AddParams(::testing::_, ::testing::_, ::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildParamSet(::testing::_)).Times(0);
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeParamSet(::testing::_)).Times(0);
  int32_t result =
      adapter.SignUsingHuks(identity, certData, certDataLen, signData,
                            &signDataLen, algorithm, keySize);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 3: InitSession fails
TEST_F(CertManagerAdapterImplTest, SignUsingHuks_003) {
  const string identity = "test_identity";
  uint8_t certData[100] = {0};
  uint32_t certDataLen = 100;
  uint8_t signData[128] = {0};
  uint32_t signDataLen = 128;
  uint16_t algorithm = SSL_SIGN_RSA_PKCS1_SHA256;
  uint32_t keySize = 2048;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeParamSet(::testing::_)).Times(1);
  EXPECT_CALL(*g_mockHuks, OH_Huks_InitSession(::testing::_, ::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  int32_t result =
      adapter.SignUsingHuks(identity, certData, certDataLen, signData,
                            &signDataLen, algorithm, keySize);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 4: FinishSession fails
TEST_F(CertManagerAdapterImplTest, SignUsingHuks_004) {
  const string identity = "test_identity";
  uint8_t certData[100] = {0};
  uint32_t certDataLen = 100;
  uint8_t signData[128] = {0};
  uint32_t signDataLen = 128;
  uint16_t algorithm = SSL_SIGN_RSA_PKCS1_SHA256;
  uint32_t keySize = 2048;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeParamSet(::testing::_)).Times(1);
  EXPECT_CALL(*g_mockHuks, OH_Huks_InitSession(::testing::_, ::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FinishSession(::testing::_, ::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT, 0}));
  int32_t result =
      adapter.SignUsingHuks(identity, certData, certDataLen, signData,
                            &signDataLen, algorithm, keySize);
  EXPECT_EQ(result, OH_HUKS_ERR_CODE_ILLEGAL_ARGUMENT);
}

// Test case 5: Full success path
TEST_F(CertManagerAdapterImplTest, SignUsingHuks_005) {
  const string identity = "test_identity";
  uint8_t certData[100] = {0};
  uint32_t certDataLen = 100;
  uint8_t signData[128] = {0};
  uint32_t signDataLen = 128;
  uint16_t algorithm = SSL_SIGN_RSA_PKCS1_SHA256;
  uint32_t keySize = 2048;

  EXPECT_CALL(*g_mockHuks, OH_Huks_InitParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_AddParams(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_BuildParamSet(::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FreeParamSet(::testing::_)).Times(1);
  EXPECT_CALL(*g_mockHuks, OH_Huks_InitSession(::testing::_, ::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  EXPECT_CALL(*g_mockHuks, OH_Huks_FinishSession(::testing::_, ::testing::_, ::testing::_, ::testing::_))
      .WillOnce(::testing::Return(OH_Huks_Result{OH_HUKS_SUCCESS, 0}));
  int32_t result =
      adapter.SignUsingHuks(identity, certData, certDataLen, signData,
                            &signDataLen, algorithm, keySize);
  EXPECT_EQ(result, OH_HUKS_SUCCESS);
}

} // namespace OHOS::NWeb
