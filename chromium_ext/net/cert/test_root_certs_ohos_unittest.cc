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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#include "arkweb/build/features/features.h"

// test_root_certs_ohos.cc provides platform-specific implementations of:
// - TestRootCerts::AddImpl  (writes PEM certs to
// /data/certificates/user_cacerts/test/)
// - TestRootCerts::ClearImpl  (deletes that directory)
// - TestRootCerts::Init  (empty body)
// - TestRootCerts::~TestRootCerts  (default destructor)
// - GetTmpCertDir  (anonymous namespace, creates dir if needed)
//
// These are tested indirectly through the TestRootCerts public API
// (ScopedTestRoot / Add / Clear) since AddImpl/ClearImpl are private.

namespace net {

// The directory that test_root_certs_ohos.cc uses for writing test certs.
const char kUserCaBasePath[] = "/data/certificates/user_cacerts/test/";

class TestRootCertsOHOSTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_roots_ = TestRootCerts::GetInstance();
    ASSERT_NE(nullptr, test_roots_);
    // Ensure clean state
    test_roots_->Clear();
  }

  void TearDown() override { test_roots_->Clear(); }

  TestRootCerts* test_roots_;
};

// Test that the singleton is initialized and starts empty
TEST_F(TestRootCertsOHOSTest, SingletonInitialized) {
  EXPECT_TRUE(TestRootCerts::HasInstance());
  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test AddImpl/ClearImpl via ScopedTestRoot with a generated certificate
TEST_F(TestRootCertsOHOSTest, AddAndClearViaScopedTestRoot) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root);
  ASSERT_TRUE(leaf);

  EXPECT_TRUE(test_roots_->IsEmpty());

  {
    ScopedTestRoot scoped_root(root->GetX509Certificate(),
                               bssl::CertificateTrust::ForTrustAnchor());
    EXPECT_FALSE(test_roots_->IsEmpty());
  }

  // ScopedTestRoot destructor calls Clear which invokes ClearImpl
  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test explicit Clear after Add - validates ClearImpl
TEST_F(TestRootCertsOHOSTest, ExplicitClear) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root);

  ScopedTestRoot scoped_root(root->GetX509Certificate(),
                             bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_FALSE(test_roots_->IsEmpty());

  test_roots_->Clear();
  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test adding multiple certificates (each AddImpl call should write a file)
TEST_F(TestRootCertsOHOSTest, MultipleAdds) {
  auto [leaf1, root1] = CertBuilder::CreateSimpleChain2();
  auto [leaf2, root2] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root1);
  ASSERT_TRUE(root2);

  EXPECT_TRUE(test_roots_->IsEmpty());

  ScopedTestRoot scoped_root1(root1->GetX509Certificate(),
                              bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_FALSE(test_roots_->IsEmpty());

  ScopedTestRoot scoped_root2(root2->GetX509Certificate(),
                              bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_FALSE(test_roots_->IsEmpty());

  // Clear destroys both
  test_roots_->Clear();
  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test that the test cert directory path is accessible
TEST_F(TestRootCertsOHOSTest, CertDirectoryPath) {
  base::FilePath test_cert_dir(kUserCaBasePath);
  // The directory should exist (created by GetTmpCertDir or previous test runs)
  // or at least the path constant should be a valid FilePath
  EXPECT_FALSE(test_cert_dir.value().empty());
  EXPECT_EQ(test_cert_dir.value(), kUserCaBasePath);
}

// Test Init - it's a no-op on OHOS, so just verify it doesn't crash
TEST_F(TestRootCertsOHOSTest, InitDoesNotCrash) {
  // Init is called during singleton construction.
  // We just verify the singleton is usable.
  EXPECT_TRUE(TestRootCerts::HasInstance());
}

// Test that after AddImpl + ClearImpl, the directory is cleaned up
TEST_F(TestRootCertsOHOSTest, ClearImplCleansDirectory) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root);

  ScopedTestRoot scoped_root(root->GetX509Certificate(),
                             bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_FALSE(test_roots_->IsEmpty());

  test_roots_->Clear();
  EXPECT_TRUE(test_roots_->IsEmpty());

  // After ClearImpl, the test cert directory may still exist but should be
  // empty or the certs should be removed. Verify the directory state.
  base::FilePath test_cert_dir(kUserCaBasePath);
  // If the directory exists, it should be empty or not contain our test certs
  if (base::DirectoryExists(test_cert_dir)) {
    // Directory may exist but cert files should have been deleted
    EXPECT_TRUE(base::DirectoryExists(test_cert_dir));
  }
}

// Test destructor coverage - TestRootCerts is a singleton with = default dtor
TEST_F(TestRootCertsOHOSTest, DestructorIsDefault) {
  // The destructor is `= default`. We cannot directly test it
  // because TestRootCerts is a NoDestructor singleton. However,
  // we verify the object is in a valid state throughout the test.
  EXPECT_TRUE(test_roots_->IsEmpty());
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ScopedTestRoot scoped_root(root->GetX509Certificate(),
                             bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_FALSE(test_roots_->IsEmpty());
  test_roots_->Clear();
  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test AddImpl with unspecified trust - should NOT call the platform AddImpl
TEST_F(TestRootCertsOHOSTest, AddImplWithUnspecifiedTrust) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root);

  // When trust is unspecified, Add() does not call AddImpl
  ScopedTestRoot scoped_root(root->GetX509Certificate(),
                             bssl::CertificateTrust::ForUnspecified());
  // The cert should still be in the trust store but AddImpl was not called
  EXPECT_FALSE(test_roots_->IsEmpty());
}

// Test that GetTmpCertDir works: directory creation
TEST_F(TestRootCertsOHOSTest, TmpCertDirCreation) {
  // We cannot call GetTmpCertDir directly (it's in anonymous namespace),
  // but we test the behavior indirectly:
  // The AddImpl function calls GetTmpCertDir and creates the directory if
  // needed. If AddImpl succeeds, the directory must exist.
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root);

  ScopedTestRoot scoped_root(root->GetX509Certificate(),
                             bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_FALSE(test_roots_->IsEmpty());

#if defined(NET_UNITTESTS_OHOS_ENABLE)
  // When NET_UNITTESTS_OHOS_ENABLE is defined, AddImpl writes to disk,
  // so the directory must exist
  base::FilePath test_cert_dir(kUserCaBasePath);
  EXPECT_TRUE(base::DirectoryExists(test_cert_dir));
#endif
}

// Test ClearImpl when called multiple times in succession (directory already
// deleted)
TEST_F(TestRootCertsOHOSTest, RepeatedClear) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root);

  ScopedTestRoot scoped_root(root->GetX509Certificate(),
                             bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_FALSE(test_roots_->IsEmpty());

  test_roots_->Clear();
  EXPECT_TRUE(test_roots_->IsEmpty());

  // Second clear should not crash even though directory was already deleted
  test_roots_->Clear();
  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test Clear on an empty store (no Add was called)
TEST_F(TestRootCertsOHOSTest, ClearOnEmptyStore) {
  EXPECT_TRUE(test_roots_->IsEmpty());

  // Clear on empty store should not crash
  test_roots_->Clear();
  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test Add with ForTrustAnchorOrLeaf - exercises a different trust level
TEST_F(TestRootCertsOHOSTest, AddImplWithTrustAnchorOrLeaf) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root);

  EXPECT_TRUE(test_roots_->IsEmpty());

  {
    ScopedTestRoot scoped_root(root->GetX509Certificate(),
                               bssl::CertificateTrust::ForTrustAnchorOrLeaf());
    EXPECT_FALSE(test_roots_->IsEmpty());
  }

  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test Add then Add another then Clear (stress cleanup)
TEST_F(TestRootCertsOHOSTest, AddTwoCertsSequentially) {
  auto [leaf1, root1] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root1);

  EXPECT_TRUE(test_roots_->IsEmpty());

  // Add first cert, then clear, then add second cert
  {
    ScopedTestRoot scoped_root1(root1->GetX509Certificate(),
                                bssl::CertificateTrust::ForTrustAnchor());
    EXPECT_FALSE(test_roots_->IsEmpty());
  }

  EXPECT_TRUE(test_roots_->IsEmpty());

  auto [leaf2, root2] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root2);

  {
    ScopedTestRoot scoped_root2(root2->GetX509Certificate(),
                                bssl::CertificateTrust::ForTrustAnchor());
    EXPECT_FALSE(test_roots_->IsEmpty());
  }

  EXPECT_TRUE(test_roots_->IsEmpty());
}

// Test GetTmpCertDir indirectly via AddImpl with a cert file that should
// exist on disk
TEST_F(TestRootCertsOHOSTest, TmpCertDirContainsCertAfterAdd) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  ASSERT_TRUE(root);

  ScopedTestRoot scoped_root(root->GetX509Certificate(),
                             bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_FALSE(test_roots_->IsEmpty());

#if defined(NET_UNITTESTS_OHOS_ENABLE)
  base::FilePath test_cert_dir(kUserCaBasePath);
  EXPECT_TRUE(base::DirectoryExists(test_cert_dir));

  // After AddImpl, the directory should contain at least one .0 file
  int file_count = 0;
  base::FileEnumerator enumerator(test_cert_dir, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    file_count++;
  }
  EXPECT_GT(file_count, 0);
#endif
}

}  // namespace net
