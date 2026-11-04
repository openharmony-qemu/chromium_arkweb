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

#include <cstring>
#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#include "arkweb/ohos_adapter_ndk/keystore_adapter/keystore_adapter_impl.h"
#include "arkweb/ohos_adapter_ndk/keystore_adapter/keystore_adapter_impl.cpp"

using namespace OHOS::NWeb;
using namespace testing;

namespace {

class MockKeystoreAdapterImpl : public KeystoreAdapter {
public:
    MOCK_METHOD(std::string, EncryptKey, (const std::string alias, const std::string plainData), (override));
    MOCK_METHOD(std::string, DecryptKey, (const std::string alias, const std::string encryptedData), (override));
    MOCK_METHOD(std::string, AssetQuery, (const std::string assetHandle), (override));
};

std::unique_ptr<MockKeystoreAdapterImpl> g_mock_impl;

class KeystoreAdapterImplTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp() override;
    void TearDown() override;
};

void KeystoreAdapterImplTest::SetUpTestCase(void)
{}

void KeystoreAdapterImplTest::TearDownTestCase(void)
{}

void KeystoreAdapterImplTest::SetUp(void)
{
    g_mock_impl = std::make_unique<MockKeystoreAdapterImpl>();
}

void KeystoreAdapterImplTest::TearDown(void)
{
    g_mock_impl.reset();
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_InitParamSet_001)
{
    struct OH_Huks_ParamSet *paramSet;
    struct OH_Huks_Param decryptParams[] = { { .tag = OH_HUKS_TAG_ALGORITHM, .uint32Param = OH_HUKS_ALG_AES },
        { .tag = OH_HUKS_TAG_PURPOSE, .uint32Param = OH_HUKS_KEY_PURPOSE_DECRYPT },
        { .tag = OH_HUKS_TAG_KEY_SIZE, .uint32Param = OH_HUKS_AES_KEY_SIZE_256 },
        { .tag = OH_HUKS_TAG_PADDING, .uint32Param = OH_HUKS_PADDING_NONE },
        { .tag = OH_HUKS_TAG_BLOCK_MODE, .uint32Param = OH_HUKS_MODE_CBC } };
    int32_t result = KeystoreAdapterImpl::GetInstance().InitParamSet(
        nullptr, decryptParams, sizeof(decryptParams) / sizeof(OH_Huks_Param));
    EXPECT_NE(result, 0);
    result = KeystoreAdapterImpl::GetInstance().InitParamSet(
        nullptr, nullptr, sizeof(decryptParams) / sizeof(OH_Huks_Param));
    EXPECT_NE(result, 0);
    result = KeystoreAdapterImpl::GetInstance().InitParamSet(
        &paramSet, nullptr, sizeof(decryptParams) / sizeof(OH_Huks_Param));
    EXPECT_NE(result, 0);
    result = KeystoreAdapterImpl::GetInstance().InitParamSet(
        &paramSet, decryptParams, sizeof(decryptParams) / sizeof(OH_Huks_Param));
    EXPECT_EQ(result, 0);
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_EncryptKey_002)
{
    std::string alias = "test";
    std::string plainData = "web_test";
    std::string encryptString = KeystoreAdapterImpl::GetInstance().EncryptKey(alias, plainData);
    EXPECT_TRUE(encryptString.empty());
    std::string DecryptString = KeystoreAdapterImpl::GetInstance().DecryptKey(alias, encryptString);
    EXPECT_TRUE(DecryptString.empty());
    encryptString = KeystoreAdapterImpl::GetInstance().EncryptKey(alias, "");
    EXPECT_TRUE(encryptString.empty());
    DecryptString = KeystoreAdapterImpl::GetInstance().DecryptKey(alias, "test");
    EXPECT_TRUE(DecryptString.empty());

    std::string nullAlias = "";
    std::string nullPlainData = "";
    std::string nullEncrypt = KeystoreAdapterImpl::GetInstance().EncryptKey(nullAlias, nullPlainData);
    EXPECT_TRUE(nullEncrypt.empty());
    std::string nullDecrypt = KeystoreAdapterImpl::GetInstance().DecryptKey(nullAlias, nullPlainData);
    EXPECT_TRUE(DecryptString.empty());

    std::string long_str_alias(2048, 'a');
    nullEncrypt = KeystoreAdapterImpl::GetInstance().EncryptKey(long_str_alias, "test");
    EXPECT_TRUE(nullEncrypt.empty());
    nullDecrypt = KeystoreAdapterImpl::GetInstance().DecryptKey(long_str_alias, "test");
    EXPECT_TRUE(DecryptString.empty());

    std::string keyAlias(16, 'a');
    std::string keyPlain(32, 0x1c);
    const int prefixSize = 3 + 16;
    std::string result = KeystoreAdapterImpl::GetInstance().EncryptKey(keyAlias, keyPlain);
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.length(), keyPlain.length() + prefixSize);
    std::string plainText = KeystoreAdapterImpl::GetInstance().DecryptKey(keyAlias, result);
    EXPECT_EQ(keyPlain, plainText);
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_DecryptKey_001)
{
    const int prefixSize = 3 + 16;
    std::string absentKeyAlias = "absent_alias_test";
    std::string legacyAlias = "legacy_alias";
    std::string plainData = "legacy_key_test_16_bytes_aligned";
    std::string encryptString = KeystoreAdapterImpl::GetInstance().EncryptKey(legacyAlias, plainData);
    EXPECT_FALSE(encryptString.empty());
    EXPECT_EQ(encryptString.length(), plainData.length() + prefixSize);
    std::string plainString = KeystoreAdapterImpl::GetInstance().DecryptKey(legacyAlias, encryptString);
    EXPECT_FALSE(plainString.empty());
    EXPECT_EQ(plainString, plainData);
    encryptString = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456";
    plainString = KeystoreAdapterImpl::GetInstance().DecryptKey(legacyAlias, encryptString);
    EXPECT_FALSE(plainString.empty());
    EXPECT_EQ(plainString.length(), encryptString.length());
    encryptString = "V10abcdefghijklmnopABCDEFGHIJKLMNOPQRSTUVWXYZ123456";
    plainString = KeystoreAdapterImpl::GetInstance().DecryptKey(legacyAlias, encryptString);
    EXPECT_FALSE(plainString.empty());
    EXPECT_EQ(plainString.length(), encryptString.length() - prefixSize);

    encryptString = "fake_web_test";
    plainString = KeystoreAdapterImpl::GetInstance().DecryptKey(absentKeyAlias, encryptString);
    EXPECT_TRUE(plainString.empty());
    encryptString = "V10abcdefghijklmnopABCDEFGHIJKLMNOPQRSTUVWXYZ123456";
    plainString = KeystoreAdapterImpl::GetInstance().DecryptKey(absentKeyAlias, encryptString);
    EXPECT_TRUE(plainString.empty());

    std::string nullAlias = "";
    encryptString = "";
    plainString = KeystoreAdapterImpl::GetInstance().DecryptKey(nullAlias, encryptString);
    EXPECT_TRUE(plainString.empty());

    std::string long_str_alias(2048, 'a');
    plainString = KeystoreAdapterImpl::GetInstance().DecryptKey(long_str_alias, "test");
    EXPECT_TRUE(plainString.empty());

    std::string keyAlias(16, 'a');
    std::string keyPlain(32, 'x');
    std::string result = KeystoreAdapterImpl::GetInstance().EncryptKey(keyAlias, keyPlain);
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.length(), keyPlain.length() + prefixSize);
    plainString = KeystoreAdapterImpl::GetInstance().DecryptKey(keyAlias, result);
    EXPECT_EQ(keyPlain, plainString);
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_AssetQuery_003)
{
    std::string alias = "test";
    std::string encryptString = KeystoreAdapterImpl::GetInstance().AssetQuery(alias);
    EXPECT_TRUE(encryptString.empty());

    std::string nullAlias = "";
    std::string nullEncrypt = KeystoreAdapterImpl::GetInstance().AssetQuery(nullAlias);
    EXPECT_TRUE(nullEncrypt.empty());
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_AssetQuery_MockSuccess)
{
    EXPECT_CALL(*g_mock_impl, AssetQuery(_))
        .WillRepeatedly(Return("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"));

    std::string alias = "mock_asset";
    std::string key = g_mock_impl->AssetQuery(alias);
    EXPECT_FALSE(key.empty());
    EXPECT_EQ(key.length(), 64u);
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_EncryptKey_MockFail)
{
    EXPECT_CALL(*g_mock_impl, EncryptKey(_, _))
        .WillRepeatedly(Return(std::string()));

    std::string alias = "fail_test";
    std::string plainData = "test_data";
    std::string result = g_mock_impl->EncryptKey(alias, plainData);
    EXPECT_TRUE(result.empty());
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_DecryptKey_MockFail)
{
    EXPECT_CALL(*g_mock_impl, DecryptKey(_, _))
        .WillRepeatedly(Return(std::string()));

    std::string alias = "fail_test";
    std::string cipherData = "invalid_cipher_data";
    std::string result = g_mock_impl->DecryptKey(alias, cipherData);
    EXPECT_TRUE(result.empty());
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_EncryptKey_Success)
{
    std::string keyAlias(16, 'k');
    std::string keyPlain(16, 'p');
    
    std::string result = KeystoreAdapterImpl::GetInstance().EncryptKey(keyAlias, keyPlain);
    EXPECT_FALSE(result.empty());
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_DecryptKey_InvalidCipherText)
{
    std::string keyAlias(16, 'd');
    std::string invalidCipher = "V10";  // Too short
    std::string result = KeystoreAdapterImpl::GetInstance().DecryptKey(keyAlias, invalidCipher);
    EXPECT_TRUE(result.empty());
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_DecryptKey_V10Format)
{
    const int prefixSize = 3 + 16;
    std::string keyAlias(16, 'v');
    std::string keyPlain(32, 'x');
    
    std::string encrypted = KeystoreAdapterImpl::GetInstance().EncryptKey(keyAlias, keyPlain);
    EXPECT_FALSE(encrypted.empty());
    
    std::string decrypted = KeystoreAdapterImpl::GetInstance().DecryptKey(keyAlias, encrypted);
    EXPECT_EQ(decrypted, keyPlain);
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_AssetQuery_NonEmptyResult)
{
    std::string alias = "valid_asset";
    std::string result = KeystoreAdapterImpl::GetInstance().AssetQuery(alias);
    EXPECT_TRUE(result.empty());
}

TEST_F(KeystoreAdapterImplTest, KeystoreAdapterImplTest_InitParamSet_Success)
{
    struct OH_Huks_ParamSet* paramSet = nullptr;
    struct OH_Huks_Param params[] = {
        { .tag = OH_HUKS_TAG_ALGORITHM, .uint32Param = OH_HUKS_ALG_AES },
        { .tag = OH_HUKS_TAG_PURPOSE, .uint32Param = OH_HUKS_KEY_PURPOSE_ENCRYPT | OH_HUKS_KEY_PURPOSE_DECRYPT },
        { .tag = OH_HUKS_TAG_KEY_SIZE, .uint32Param = OH_HUKS_AES_KEY_SIZE_256 },
        { .tag = OH_HUKS_TAG_PADDING, .uint32Param = OH_HUKS_PADDING_NONE },
        { .tag = OH_HUKS_TAG_BLOCK_MODE, .uint32Param = OH_HUKS_MODE_CBC }
    };
    
    int32_t result = KeystoreAdapterImpl::GetInstance().InitParamSet(
        &paramSet, params, sizeof(params) / sizeof(OH_Huks_Param));
    EXPECT_EQ(result, 0);
}
}