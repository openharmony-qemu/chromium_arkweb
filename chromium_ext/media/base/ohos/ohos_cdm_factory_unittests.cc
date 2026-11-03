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

#include "media/base/ohos/ohos_cdm_factory.h"

#include "arkweb/chromium_ext/media/cdm/wiseplay_cdm_common.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/base/cdm_config.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_drm_storage.h"
#include "media/base/media_switches.h"
#include "media/base/ohos/ohos_media_drm_bridge.h"
#include "media/base/provision_fetcher.h"
#include "media/cdm/aes_decryptor.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// FakeMediaDrmStorage that provides a configurable Initialize result.
class FakeMediaDrmStorage final : public MediaDrmStorage {
 public:
  explicit FakeMediaDrmStorage(bool init_success)
      : init_success_(init_success) {}

  void Initialize(InitCB init_cb) override {
    if (init_success_) {
      std::move(init_cb).Run(true, base::UnguessableToken::Create());
    } else {
      std::move(init_cb).Run(false, std::nullopt);
    }
  }

  void OnProvisioned(ResultCB result_cb) override {
    std::move(result_cb).Run(true);
  }

  void SavePersistentSession(const std::string& session_id,
                             const SessionData& session_data,
                             ResultCB result_cb) override {
    std::move(result_cb).Run(true);
  }

  void LoadPersistentSession(
      const std::string& session_id,
      LoadPersistentSessionCB load_persistent_session_cb) override {
    std::move(load_persistent_session_cb).Run(nullptr);
  }

  void RemovePersistentSession(const std::string& session_id,
                               ResultCB result_cb) override {
    std::move(result_cb).Run(true);
  }

  base::WeakPtr<MediaDrmStorage> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool init_success_;
  base::WeakPtrFactory<FakeMediaDrmStorage> weak_factory_{this};
};

// FakeContentDecryptionModule provides no-op implementations for testing.
class FakeContentDecryptionModule final : public ContentDecryptionModule {
 public:
  FakeContentDecryptionModule() = default;

  void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      std::unique_ptr<SimpleCdmPromise> promise) override {
    promise->resolve();
  }

  void GetStatusForPolicy(
      HdcpVersion min_hdcp_version,
      std::unique_ptr<KeyStatusCdmPromise> promise) override {
    promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                    "Not supported");
  }

  void CreateSessionAndGenerateRequest(
      CdmSessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<NewSessionCdmPromise> promise) override {
    promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                    "Not supported");
  }

  void LoadSession(CdmSessionType session_type,
                   const std::string& session_id,
                   std::unique_ptr<NewSessionCdmPromise> promise) override {
    promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                    "Not supported");
  }

  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<SimpleCdmPromise> promise) override {
    promise->resolve();
  }

  void CloseSession(const std::string& session_id,
                    std::unique_ptr<SimpleCdmPromise> promise) override {
    promise->resolve();
  }

  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<SimpleCdmPromise> promise) override {
    promise->resolve();
  }

  CdmContext* GetCdmContext() override { return nullptr; }

 private:
  ~FakeContentDecryptionModule() override = default;
};

std::unique_ptr<ProvisionFetcher> CreateFakeProvisionFetcher() {
  return nullptr;
}

std::unique_ptr<MediaDrmStorage> CreateFakeMediaDrmStorage() {
  return std::make_unique<FakeMediaDrmStorage>(true);
}

// No-op callbacks for CDM creation. AesDecryptor's constructor CHECKs that
// SessionMessageCB and SessionClosedCB are non-null, so these must be valid.
SessionMessageCB CreateSessionMessageCb() {
  return base::BindRepeating(
      [](const std::string&, CdmMessageType, const std::vector<uint8_t>&) {});
}

SessionClosedCB CreateSessionClosedCb() {
  return base::BindRepeating([](const std::string&, CdmSessionClosedReason) {});
}

SessionKeysChangeCB CreateSessionKeysChangeCb() {
  return base::BindRepeating([](const std::string&, bool, CdmKeysInfo) {});
}

SessionExpirationUpdateCB CreateSessionExpirationUpdateCb() {
  return base::BindRepeating([](const std::string&, base::Time) {});
}

}  // namespace

class OHOSCdmFactoryTest : public ::testing::Test {
 protected:
  OHOSCdmFactoryTest() = default;
  ~OHOSCdmFactoryTest() override = default;

  std::unique_ptr<media::OHOSCdmFactory> cdm_factory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  void SetUp() override {
    cdm_factory_ = std::make_unique<media::OHOSCdmFactory>(
        base::BindRepeating(&CreateFakeProvisionFetcher),
        base::BindRepeating(&CreateFakeMediaDrmStorage));
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    cdm_factory_.reset();
  }

  // Helper methods that access private members through friend access.
  // These must be defined in the fixture class (not in TEST_F bodies)
  // because friend access is not inherited by gtest's derived test classes.
  void EmplacePendingCreation(
      uint32_t creation_id,
      std::unique_ptr<OHOSMediaDrmBridgeFactory> factory,
      CdmCreatedCB bound_cb) {
    cdm_factory_->pending_creations_.emplace(
        creation_id, std::make_pair(std::move(factory), std::move(bound_cb)));
  }

  void CallOnCdmCreated(uint32_t creation_id,
                        const scoped_refptr<ContentDecryptionModule>& cdm,
                        CreateCdmStatus status) {
    cdm_factory_->OnCdmCreated(creation_id, cdm, status);
  }

  bool IsPendingCreationsEmpty() {
    return cdm_factory_->pending_creations_.empty();
  }
};

TEST_F(OHOSCdmFactoryTest, Create_UnsupportedKeySystem) {
  CdmConfig cdm_config;
  cdm_config.key_system = "com.unsupported.drm";

  CreateCdmStatus result_status = CreateCdmStatus::kSuccess;
  scoped_refptr<ContentDecryptionModule> result_cdm;

  cdm_factory_->Create(cdm_config, CreateSessionMessageCb(),
                       CreateSessionClosedCb(), CreateSessionKeysChangeCb(),
                       CreateSessionExpirationUpdateCb(),
                       base::BindOnce(
                           [](scoped_refptr<ContentDecryptionModule>* out_cdm,
                              CreateCdmStatus* out_status,
                              const scoped_refptr<ContentDecryptionModule>& cdm,
                              CreateCdmStatus status) {
                             *out_cdm = cdm;
                             *out_status = status;
                           },
                           &result_cdm, &result_status));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_status, CreateCdmStatus::kUnsupportedKeySystem);
  EXPECT_EQ(result_cdm, nullptr);
}

TEST_F(OHOSCdmFactoryTest, Create_ClearKeyWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({media::kExternalClearKeyForTesting}, {});

  CdmConfig cdm_config;
  cdm_config.key_system = media::kExternalClearKeyKeySystem;

  CreateCdmStatus result_status = CreateCdmStatus::kCdmCreationAborted;
  scoped_refptr<ContentDecryptionModule> result_cdm;

  cdm_factory_->Create(cdm_config, CreateSessionMessageCb(),
                       CreateSessionClosedCb(), CreateSessionKeysChangeCb(),
                       CreateSessionExpirationUpdateCb(),
                       base::BindOnce(
                           [](scoped_refptr<ContentDecryptionModule>* out_cdm,
                              CreateCdmStatus* out_status,
                              const scoped_refptr<ContentDecryptionModule>& cdm,
                              CreateCdmStatus status) {
                             *out_cdm = cdm;
                             *out_status = status;
                           },
                           &result_cdm, &result_status));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_status, CreateCdmStatus::kSuccess);
  EXPECT_NE(result_cdm, nullptr);
}

TEST_F(OHOSCdmFactoryTest, Create_ExternalClearKeyTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({media::kExternalClearKeyForTesting}, {});

  CdmConfig cdm_config;
  cdm_config.key_system = "org.chromium.externalclearkey.test";

  CreateCdmStatus result_status = CreateCdmStatus::kCdmCreationAborted;
  scoped_refptr<ContentDecryptionModule> result_cdm;

  cdm_factory_->Create(cdm_config, CreateSessionMessageCb(),
                       CreateSessionClosedCb(), CreateSessionKeysChangeCb(),
                       CreateSessionExpirationUpdateCb(),
                       base::BindOnce(
                           [](scoped_refptr<ContentDecryptionModule>* out_cdm,
                              CreateCdmStatus* out_status,
                              const scoped_refptr<ContentDecryptionModule>& cdm,
                              CreateCdmStatus status) {
                             *out_cdm = cdm;
                             *out_status = status;
                           },
                           &result_cdm, &result_status));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_status, CreateCdmStatus::kSuccess);
  EXPECT_NE(result_cdm, nullptr);
}

TEST_F(OHOSCdmFactoryTest, Create_WiseplayKeySystem) {
  CdmConfig cdm_config;
  cdm_config.key_system = media::kWiseplayKeySystem;

  CreateCdmStatus result_status = CreateCdmStatus::kCdmCreationAborted;
  scoped_refptr<ContentDecryptionModule> result_cdm;

  cdm_factory_->Create(cdm_config, CreateSessionMessageCb(),
                       CreateSessionClosedCb(), CreateSessionKeysChangeCb(),
                       CreateSessionExpirationUpdateCb(),
                       base::BindOnce(
                           [](scoped_refptr<ContentDecryptionModule>* out_cdm,
                              CreateCdmStatus* out_status,
                              const scoped_refptr<ContentDecryptionModule>& cdm,
                              CreateCdmStatus status) {
                             *out_cdm = cdm;
                             *out_status = status;
                           },
                           &result_cdm, &result_status));

  task_environment_.RunUntilIdle();

  if (OHOSMediaDrmBridge::IsKeySystemSupported(media::kWiseplayKeySystem)) {
    // Device supports Wiseplay - creation was attempted through
    // OHOSMediaDrmBridgeFactory. The callback should have been invoked.
    EXPECT_NE(result_status, CreateCdmStatus::kCdmCreationAborted);
  } else {
    EXPECT_EQ(result_status, CreateCdmStatus::kUnsupportedKeySystem);
    EXPECT_EQ(result_cdm, nullptr);
  }
}

TEST_F(OHOSCdmFactoryTest, OnCdmCreated) {
  const uint32_t creation_id = 1;
  scoped_refptr<FakeContentDecryptionModule> cdm(
      new FakeContentDecryptionModule());

  CreateCdmStatus result_status = CreateCdmStatus::kCdmCreationAborted;
  scoped_refptr<ContentDecryptionModule> result_cdm;

  CdmCreatedCB bound_cb = base::BindPostTaskToCurrentDefault(base::BindOnce(
      [](scoped_refptr<ContentDecryptionModule>* out_cdm,
         CreateCdmStatus* out_status,
         const scoped_refptr<ContentDecryptionModule>& cdm,
         CreateCdmStatus status) {
        *out_cdm = cdm;
        *out_status = status;
      },
      &result_cdm, &result_status));

  EmplacePendingCreation(creation_id, nullptr, std::move(bound_cb));
  CallOnCdmCreated(creation_id, cdm, CreateCdmStatus::kSuccess);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_status, CreateCdmStatus::kSuccess);
  EXPECT_EQ(result_cdm, cdm);
  EXPECT_TRUE(IsPendingCreationsEmpty());
}

TEST_F(OHOSCdmFactoryTest, Destructor_AbortsPendingCreations) {
  CdmConfig cdm_config;
  cdm_config.key_system = media::kWiseplayKeySystem;

  CreateCdmStatus result_status = CreateCdmStatus::kSuccess;
  scoped_refptr<ContentDecryptionModule> result_cdm;

  cdm_factory_->Create(cdm_config, CreateSessionMessageCb(),
                       CreateSessionClosedCb(), CreateSessionKeysChangeCb(),
                       CreateSessionExpirationUpdateCb(),
                       base::BindOnce(
                           [](scoped_refptr<ContentDecryptionModule>* out_cdm,
                              CreateCdmStatus* out_status,
                              const scoped_refptr<ContentDecryptionModule>& cdm,
                              CreateCdmStatus status) {
                             *out_cdm = cdm;
                             *out_status = status;
                           },
                           &result_cdm, &result_status));

  // Destroy the factory while there may be pending creations
  cdm_factory_.reset();
  task_environment_.RunUntilIdle();

  // If there were pending creations, the callback should have been called
  // with kCdmCreationAborted. If the device doesn't support Wiseplay,
  // the callback was already called synchronously.
  if (result_status == CreateCdmStatus::kCdmCreationAborted) {
    EXPECT_EQ(result_cdm, nullptr);
  }
}

}  // namespace media
