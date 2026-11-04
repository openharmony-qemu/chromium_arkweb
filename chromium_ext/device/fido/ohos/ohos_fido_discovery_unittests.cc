// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #include "device/fido/ohos/ohos_fido_discovery.h"

#include <gtest/gtest.h>
#include "device/fido/fido_request_handler_base.h"

#define private public
#include "device/fido/ohos/ohos_fido_discovery.h"
#undef private

namespace device {

namespace {

class OhosFidoDiscoveryTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void OhosFidoDiscoveryTest::SetUpTestCase() {}

void OhosFidoDiscoveryTest::TearDownTestCase() {}

void OhosFidoDiscoveryTest::SetUp() {}

void OhosFidoDiscoveryTest::TearDown() {}

TEST_F(OhosFidoDiscoveryTest, StartWithObserverSet)
{
    OhosFidoDiscovery discovery;
    discovery.Start();
    EXPECT_FALSE(discovery.authenticator_);
}

TEST_F(OhosFidoDiscoveryTest, GetWeakPtr)
{
    OhosFidoDiscovery discovery;
    auto weak_ptr = discovery.weak_factory_.GetWeakPtr();
    EXPECT_TRUE(weak_ptr.get() != nullptr);
}

} // namespace

} // namespace device
