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

TEST_P(PasswordStoreBuiltInBackendTest, UpdateLoginDisplayNameAsync) {
  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());
  form.display_name = u"Original display name";
  backend->AddLoginAsync(form, base::DoNothing());
  RunUntilIdle();

  form.display_name = u"Updated display name";
  backend->UpdateLoginDisplayNameAsync(form);
  RunUntilIdle();

  ClosePasswordStore();
  sql::Database verification_db(sql::test::kTestTag);
  ASSERT_TRUE(verification_db.Open(test_login_db_file_path()));
  sql::Statement statement(
      verification_db.GetUniqueStatement("SELECT display_name FROM logins"));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(form.display_name, statement.ColumnString16(0));
  EXPECT_FALSE(statement.Step());
  EXPECT_TRUE(statement.Succeeded());
}

TEST_P(PasswordStoreBuiltInBackendTest,
       UpdateLoginDisplayNameAsyncForMissingLoginDoesNotCreateOne) {
  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());
  form.display_name = u"Missing login";

  backend->UpdateLoginDisplayNameAsync(form);
  RunUntilIdle();

  bool callback_called = false;
  backend->GetAllLoginsAsync(base::BindOnce(
      [](bool* callback_called, LoginsResultOrError result) {
        *callback_called = true;
        ASSERT_TRUE(std::holds_alternative<LoginsResult>(result));
        EXPECT_TRUE(std::get<LoginsResult>(result).empty());
      },
      &callback_called));
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       UpdateLoginDisplayNameAsyncHandlesUnavailableDatabase) {
  PasswordStoreBackend* backend =
      CreateBackend(std::make_unique<BadLoginDatabase>(GetParam()));
  InitializeBackend(backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());

  backend->UpdateLoginDisplayNameAsync(form);
  RunUntilIdle();

  bool callback_called = false;
  backend->GetAllLoginsAsync(base::BindOnce(
      [](bool* callback_called, LoginsResultOrError result) {
        *callback_called = true;
        EXPECT_TRUE(std::holds_alternative<PasswordStoreBackendError>(result));
      },
      &callback_called));
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}
