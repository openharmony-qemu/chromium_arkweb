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

#include "arkweb/chromium_ext/net/dns/secure_dns_fallback_utils.h"

#include "arkweb/ohos_nweb_ex/build/features/features.h"

#if BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class SecureDnsFallbackUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    StoreSuspectIPListAndSourceHostList({}, {});
  }
};

TEST_F(SecureDnsFallbackUtilsTest, EmptyLegacyAddresses_ReturnsFalse) {
  std::vector<IPEndPoint> legacy;
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_FALSE(MaybeNeedToProcessAddressList("host", legacy, true, out,
                                             need_modify, truncation));
}

TEST_F(SecureDnsFallbackUtilsTest, HostNotInList_ReturnsFalse) {
  IPAddress ip(1, 2, 3, 4);
  IPEndPoint ep(ip, 443);
  std::vector<IPEndPoint> legacy = {ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_FALSE(MaybeNeedToProcessAddressList("unknown.com", legacy, true, out,
                                             need_modify, truncation));
}

TEST_F(SecureDnsFallbackUtilsTest, HostInList_NoSuspectIP_AllGoToOut) {
  IPAddress ip(1, 2, 3, 4);
  IPEndPoint ep(ip, 443);
  StoreSuspectIPListAndSourceHostList({"10.0.0.1"}, {"test.com"});
  std::vector<IPEndPoint> legacy = {ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_FALSE(MaybeNeedToProcessAddressList("test.com", legacy, true, out,
                                             need_modify, truncation));
  EXPECT_EQ(out.size(), 1u);
  EXPECT_TRUE(truncation.empty());
}

TEST_F(SecureDnsFallbackUtilsTest, SuspectIPMatch_SeparatesAddresses) {
  IPAddress suspect_ip(10, 0, 0, 1);
  IPAddress safe_ip(1, 2, 3, 4);
  IPEndPoint suspect_ep(suspect_ip, 443);
  IPEndPoint safe_ep(safe_ip, 443);
  StoreSuspectIPListAndSourceHostList({"10.0.0.1"}, {"test.com"});
  std::vector<IPEndPoint> legacy = {safe_ep, suspect_ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_TRUE(MaybeNeedToProcessAddressList("test.com", legacy, true, out,
                                            need_modify, truncation));
  EXPECT_EQ(out.size(), 1u);
  EXPECT_EQ(truncation.size(), 1u);
  EXPECT_FALSE(need_modify);
}

TEST_F(SecureDnsFallbackUtilsTest, AllSuspectIP_DohAvailable_NeedModify) {
  IPAddress suspect_ip(10, 0, 0, 1);
  IPEndPoint ep(suspect_ip, 443);
  StoreSuspectIPListAndSourceHostList({"10.0.0.1"}, {"test.com"});
  std::vector<IPEndPoint> legacy = {ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_TRUE(MaybeNeedToProcessAddressList("test.com", legacy, true, out,
                                            need_modify, truncation));
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(truncation.size(), 1u);
  EXPECT_TRUE(need_modify);
}

TEST_F(SecureDnsFallbackUtilsTest,
       AllSuspectIP_NoDohAvailable_OutResetToLegacy) {
  IPAddress suspect_ip(10, 0, 0, 1);
  IPEndPoint ep(suspect_ip, 443);
  StoreSuspectIPListAndSourceHostList({"10.0.0.1"}, {"test.com"});
  std::vector<IPEndPoint> legacy = {ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_TRUE(MaybeNeedToProcessAddressList("test.com", legacy, false, out,
                                            need_modify, truncation));
  EXPECT_EQ(out.size(), 1u);
  EXPECT_EQ(truncation.size(), 1u);
  EXPECT_FALSE(need_modify);
}

TEST_F(SecureDnsFallbackUtilsTest, InvalidIPInSuspectList_Ignored) {
  StoreSuspectIPListAndSourceHostList({"not_an_ip"}, {"test.com"});
  IPAddress ip(1, 2, 3, 4);
  IPEndPoint ep(ip, 443);
  std::vector<IPEndPoint> legacy = {ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_FALSE(MaybeNeedToProcessAddressList("test.com", legacy, true, out,
                                             need_modify, truncation));
}

TEST_F(SecureDnsFallbackUtilsTest, StoreAndMatchMultipleHosts) {
  StoreSuspectIPListAndSourceHostList({"10.0.0.1"}, {"a.com", "b.com"});
  IPAddress suspect_ip(10, 0, 0, 1);
  IPEndPoint ep(suspect_ip, 443);
  std::vector<IPEndPoint> legacy = {ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;

  EXPECT_TRUE(MaybeNeedToProcessAddressList("a.com", legacy, true, out,
                                            need_modify, truncation));

  out.clear();
  truncation.clear();
  need_modify = false;
  EXPECT_TRUE(MaybeNeedToProcessAddressList("b.com", legacy, true, out,
                                            need_modify, truncation));
}

TEST_F(SecureDnsFallbackUtilsTest, ClearPreviousDataOnStore) {
  IPAddress suspect_ip(10, 0, 0, 1);
  IPEndPoint suspect_ep(suspect_ip, 443);
  StoreSuspectIPListAndSourceHostList({"10.0.0.1"}, {"test.com"});

  StoreSuspectIPListAndSourceHostList({}, {});

  std::vector<IPEndPoint> legacy = {suspect_ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_FALSE(MaybeNeedToProcessAddressList("test.com", legacy, true, out,
                                             need_modify, truncation));
}

TEST_F(SecureDnsFallbackUtilsTest,
       MultipleSuspectIPs_MixedAddresses_PartialMatch) {
  IPAddress suspect_ip1(10, 0, 0, 1);
  IPAddress suspect_ip2(10, 0, 0, 2);
  IPAddress safe_ip(1, 2, 3, 4);
  IPEndPoint suspect_ep1(suspect_ip1, 443);
  IPEndPoint suspect_ep2(suspect_ip2, 443);
  IPEndPoint safe_ep(safe_ip, 443);

  StoreSuspectIPListAndSourceHostList({"10.0.0.1", "10.0.0.2"}, {"test.com"});

  std::vector<IPEndPoint> legacy = {safe_ep, suspect_ep1, suspect_ep2};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_TRUE(MaybeNeedToProcessAddressList("test.com", legacy, true, out,
                                            need_modify, truncation));
  EXPECT_EQ(out.size(), 1u);
  EXPECT_EQ(truncation.size(), 2u);
  EXPECT_FALSE(need_modify);
}

TEST_F(SecureDnsFallbackUtilsTest, MultipleIPsInSuspectList_SomeInvalid) {
  StoreSuspectIPListAndSourceHostList({"10.0.0.1", "invalid_ip", "1.2.3.4"},
                                      {"test.com"});
  IPAddress safe_ip(192, 168, 1, 1);
  IPEndPoint safe_ep(safe_ip, 443);
  std::vector<IPEndPoint> legacy = {safe_ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_FALSE(MaybeNeedToProcessAddressList("test.com", legacy, true, out,
                                             need_modify, truncation));
  EXPECT_EQ(out.size(), 1u);
}

TEST_F(SecureDnsFallbackUtilsTest, IPv6SuspectIP) {
  StoreSuspectIPListAndSourceHostList({"::1"}, {"test.com"});
  uint8_t bytes[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  IPAddress ip_addr(bytes);
  IPEndPoint ep(ip_addr, 443);
  std::vector<IPEndPoint> legacy = {ep};
  std::vector<IPEndPoint> out;
  std::vector<IPEndPoint> truncation;
  bool need_modify = false;
  EXPECT_TRUE(MaybeNeedToProcessAddressList("test.com", legacy, false, out,
                                            need_modify, truncation));
  EXPECT_EQ(out.size(), 1u);
  EXPECT_EQ(truncation.size(), 1u);
  EXPECT_FALSE(need_modify);
}

}  // namespace net

#endif  // BUILDFLAG(ARKWEB_EXT_HTTP_DNS_FALLBACK)
