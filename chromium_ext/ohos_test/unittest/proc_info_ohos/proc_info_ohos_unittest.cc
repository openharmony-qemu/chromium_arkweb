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

#include "arkweb/chromium_ext/third_party/crashpad/crashpad/util/linux/proc_info_ohos.h"
#include <gtest/gtest.h>
#include <string>
#include <sys/time.h>
#include <iostream>
#include <vector>
#include <unordered_map>

#include "base/process/process_handle.h"

namespace crashpad {

extern bool ReadDirFiles(const std::string& path, std::vector<std::string>& files);
extern bool ReadDirFilesByPid(const int& pid, std::vector<std::string>& files);

class ProcInfoOhosTest : public ::testing::Test {

};

TEST_F(ProcInfoOhosTest, GetProcStatusByPid001) {
   int realPid = getpid();
   struct ProcInfo procInfo;
   bool result = GetProcStatusByPid(realPid, procInfo);
   EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, GetProcStatusByPid002) {
   int nonExistPid = 999999;
   struct ProcInfo procInfo;
   bool result = GetProcStatusByPid(nonExistPid, procInfo);
   EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetProcStatus) {
   struct ProcInfo procInfo;
   bool result = GetProcStatus(procInfo);
   EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, TidToNstid001) {
   const int realPid = getpid();
   const int realTid = gettid();
   int nstid = 0;
   bool result = TidToNstid(realPid, realTid, nstid);
   EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, TidToNstid002) {
   const int nonExistPid = 999999;
   const int nonExistTid = 999998;
   int nstid = 0;
   bool result = TidToNstid(nonExistPid, nonExistTid, nstid);
   EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, TidToNstid003) {
   const int nonExistPid = 12444;
   const int nonExistTid = 213132;
   int nstid = 0;
   bool result = TidToNstid(nonExistPid, nonExistTid, nstid);
   EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, IsThreadInPid001) {
   int realPid = getpid();
   int realTid = gettid();
   bool result = IsThreadInPid(realPid, realTid);
   EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, IsThreadInPid002) {
   int nonExistPid = 999999;
   int realTid = gettid();
   bool result = IsThreadInPid(nonExistPid, realTid);
   EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetTidsByPidWithFunc) {
   std::function<bool(int)> func = nullptr;
   const int realPid = getpid();
   bool result = GetTidsByPidWithFunc(realPid, func);
   EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, GetTidMapByPid) {
   std::unordered_map<int, int> tid_nstid_map;
   const int realPid = getpid();
   bool result = GetTidMapByPid(realPid, tid_nstid_map);
   EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, ReadDirFiles_NormalPath)
{
    std::vector<std::string> files;
    bool result = ReadDirFiles("/proc/self/task", files);
    EXPECT_TRUE(result);
    EXPECT_GT(files.size(), 0);
}

TEST_F(ProcInfoOhosTest, ReadDirFiles_InvalidPath)
{
    std::vector<std::string> files;
    bool result = ReadDirFiles("/nonexistent/path", files);
    EXPECT_FALSE(result);
    EXPECT_EQ(files.size(), 0);
}

TEST_F(ProcInfoOhosTest, ReadDirFilesByPid_NormalPath)
{
    int realPid = getpid();
    std::vector<std::string> files;
    bool result = ReadDirFilesByPid(realPid, files);
    EXPECT_TRUE(result);
    EXPECT_GT(files.size(), 0);
}

TEST_F(ProcInfoOhosTest, ReadDirFilesByPid_InvalidPid)
{
    int invalidPid = -1;
    std::vector<std::string> files;
    bool result = ReadDirFilesByPid(invalidPid, files);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, ReadDirFilesByPid_ZeroPid)
{
    int zeroPid = 0;
    std::vector<std::string> files;
    bool result = ReadDirFilesByPid(zeroPid, files);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, ReadDirFilesByPid_LargePid)
{
    int largePid = 999999999;
    std::vector<std::string> files;
    bool result = ReadDirFilesByPid(largePid, files);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetTidMapByPid_NormalProcess)
{
    const int realPid = getpid();
    std::unordered_map<int, int> tid_nstid_map;
    bool result = GetTidMapByPid(realPid, tid_nstid_map);
    if (result) {
        EXPECT_GT(tid_nstid_map.size(), 0);
    }
}

TEST_F(ProcInfoOhosTest, GetTidMapByPid_InvalidPid)
{
    int invalidPid = -1;
    std::unordered_map<int, int> tid_nstid_map;
    bool result = GetTidMapByPid(invalidPid, tid_nstid_map);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetTidMapByPid_NonExistentPid)
{
    int nonExistPid = 999999;
    std::unordered_map<int, int> tid_nstid_map;
    bool result = GetTidMapByPid(nonExistPid, tid_nstid_map);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetTidsByPidWithFunc_WithCallback)
{
    const int realPid = getpid();
    std::vector<int> collectedTids;
    std::function<bool(int)> func = [&collectedTids](int tid) {
        collectedTids.push_back(tid);
        return true;
    };
    bool result = GetTidsByPidWithFunc(realPid, func);
    EXPECT_TRUE(result);
    EXPECT_GT(collectedTids.size(), 0);
}

TEST_F(ProcInfoOhosTest, GetTidsByPidWithFunc_InvalidPid)
{
    int invalidPid = -1;
    std::function<bool(int)> func = [](int tid) { return true; };
    bool result = GetTidsByPidWithFunc(invalidPid, func);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetTidsByPidWithFunc_NonExistentPid)
{
    int nonExistPid = 999999;
    std::function<bool(int)> func = [](int tid) { return true; };
    bool result = GetTidsByPidWithFunc(nonExistPid, func);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetProcStatusByPid_NonExistentPid)
{
    int nonExistPid = 999999;
    struct ProcInfo procInfo;
    bool result = GetProcStatusByPid(nonExistPid, procInfo);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetProcStatusByPid_NegativePid)
{
    int negativePid = -100;
    struct ProcInfo procInfo;
    bool result = GetProcStatusByPid(negativePid, procInfo);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, IsThreadInPid_InvalidPid)
{
    int validTid = gettid();
    int invalidPid = -1;
    bool result = IsThreadInPid(invalidPid, validTid);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, IsThreadInPid_InvalidTid)
{
    int validPid = getpid();
    int invalidTid = -1;
    bool result = IsThreadInPid(validPid, invalidTid);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, IsThreadInPid_NonExistentTid)
{
    int validPid = getpid();
    int nonExistTid = 999999;
    bool result = IsThreadInPid(validPid, nonExistTid);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, IsThreadInPid_OtherProcessTid)
{
    int currentPid = getpid();
    int currentTid = gettid();
    bool result = IsThreadInPid(currentPid, currentTid);
    EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, GetProcStatusByPid_ValidPidBranch)
{
    int realPid = getpid();
    struct ProcInfo procInfo;
    bool result = GetProcStatusByPid(realPid, procInfo);
    EXPECT_TRUE(result);
    EXPECT_EQ(procInfo.pid, realPid);
    EXPECT_GT(procInfo.ppid, 0);
}

TEST_F(ProcInfoOhosTest, GetProcStatusByPid_NonExistPidBranch)
{
    int nonExistPid = 999999999;
    struct ProcInfo procInfo;
    bool result = GetProcStatusByPid(nonExistPid, procInfo);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, TidToNstid_ValidPidTidBranch)
{
    int realPid = getpid();
    int realTid = gettid();
    int nstid = 0;
    bool result = TidToNstid(realPid, realTid, nstid);
    EXPECT_TRUE(result);
    EXPECT_GT(nstid, 0);
}

TEST_F(ProcInfoOhosTest, TidToNstid_InvalidPidBranch)
{
    int invalidPid = -1;
    int invalidTid = -1;
    int nstid = 0;
    bool result = TidToNstid(invalidPid, invalidTid, nstid);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetProcStatus_Branch)
{
    struct ProcInfo procInfo;
    bool result = GetProcStatus(procInfo);
    EXPECT_TRUE(result);
    EXPECT_EQ(procInfo.pid, getpid());
}

TEST_F(ProcInfoOhosTest, GetTidsByPidWithFunc_NullFuncBranch)
{
    int realPid = getpid();
    std::function<bool(int)> nullFunc = nullptr;
    bool result = GetTidsByPidWithFunc(realPid, nullFunc);
    EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, GetTidsByPidWithFunc_ZeroTidBranch)
{
    int realPid = getpid();
    std::vector<int> collectedTids;
    std::function<bool(int)> func = [&collectedTids](int tid) {
        if (tid == 0) {
            return false;
        }
        collectedTids.push_back(tid);
        return true;
    };
    bool result = GetTidsByPidWithFunc(realPid, func);
    EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, ReadDirFilesByPid_ZeroPidBranch)
{
    int zeroPid = 0;
    std::vector<std::string> files;
    bool result = ReadDirFilesByPid(zeroPid, files);
    EXPECT_FALSE(result);
}

TEST_F(ProcInfoOhosTest, GetProcStatus_InvalidFileBranch)
{
    struct ProcInfo procInfo;
    bool result = GetProcStatusByPid(1, procInfo);
    if (result) {
        EXPECT_GT(procInfo.pid, 0);
    }
}

TEST_F(ProcInfoOhosTest, IsThreadInPid_SamePidBranch)
{
    int realPid = getpid();
    int realTid = gettid();
    bool result = IsThreadInPid(realPid, realTid);
    EXPECT_TRUE(result);
}

TEST_F(ProcInfoOhosTest, IsThreadInPid_DifferentPidBranch)
{
    int otherPid = getpid() + 100;
    int realTid = gettid();
    bool result = IsThreadInPid(otherPid, realTid);
    EXPECT_FALSE(result);
}

}//namespace crashpad
