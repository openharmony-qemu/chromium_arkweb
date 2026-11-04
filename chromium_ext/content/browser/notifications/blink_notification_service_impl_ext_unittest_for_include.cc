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

TEST_F(BlinkNotificationServiceImplTest,
       GetPermissionStatusAsyncWithoutPermissionDelegate) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableNwebEx);
  browser_context_.SetPermissionControllerDelegate(nullptr);

  base::RunLoop run_loop;
  notification_service_remote_->GetPermissionStatus(
      base::BindOnce(&BlinkNotificationServiceImplTest::DidGetPermissionStatus,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED,
            GetPermissionCallbackResult());
}

TEST_F(BlinkNotificationServiceImplTest, GetPermissionStatusAsyncForDocument) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableNwebEx);
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  base::RunLoop run_loop;
  notification_service_remote_->GetPermissionStatus(
      base::BindOnce(&BlinkNotificationServiceImplTest::DidGetPermissionStatus,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED,
            GetPermissionCallbackResult());
}

TEST_F(BlinkNotificationServiceImplTest,
       GetPermissionStatusAsyncRejectsInvalidDocument) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableNwebEx);
  ResetDocumentForArkWebTest();

  base::RunLoop run_loop;
  notification_service_remote_->GetPermissionStatus(
      base::BindOnce(&BlinkNotificationServiceImplTest::DidGetPermissionStatus,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED,
            GetPermissionCallbackResult());
}

TEST_F(BlinkNotificationServiceImplTest,
       GetPermissionStatusAsyncForServiceWorker) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableNwebEx);
  RecreateArkWebServiceForServiceWorker(&render_process_host_);
  SetPermissionStatus(blink::mojom::PermissionStatus::ASK);

  base::RunLoop run_loop;
  notification_service_remote_->GetPermissionStatus(
      base::BindOnce(&BlinkNotificationServiceImplTest::DidGetPermissionStatus,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(blink::mojom::PermissionStatus::ASK, GetPermissionCallbackResult());
}

TEST_F(BlinkNotificationServiceImplTest,
       GetPermissionStatusAsyncRejectsMissingServiceWorkerProcess) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableNwebEx);
  {
    auto dead_process =
        std::make_unique<MockRenderProcessHost>(&browser_context_);
    RecreateArkWebServiceForServiceWorker(dead_process.get());
  }

  base::RunLoop run_loop;
  notification_service_remote_->GetPermissionStatus(
      base::BindOnce(&BlinkNotificationServiceImplTest::DidGetPermissionStatus,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED,
            GetPermissionCallbackResult());
}
