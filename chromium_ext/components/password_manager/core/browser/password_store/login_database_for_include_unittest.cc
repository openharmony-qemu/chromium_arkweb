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

#if BUILDFLAG(ARKWEB_EXT_PASSWORD)
TEST_F(LoginDatabaseTest, UpdateLoginDisplayName) {
  PasswordForm form;

  password_manager::PasswordStoreChangeList changes = db().AddLogin(form);
  EXPECT_FALSE(db().UpdateLoginDisplayName(form, nullptr));

  changes = db().AddLogin(form);
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";
  EXPECT_FALSE(db().UpdateLoginDisplayName(form, &changes));
}

TEST_F(LoginDatabaseTest, UpdateLoginDisplayNameSuccess) {
  PasswordForm form = GenerateExamplePasswordForm();
  form.display_name = u"Original display name";
  ASSERT_EQ(db().AddLogin(form), AddChangeForForm(form));
  form.display_name = u"Updated display name";
  PasswordStoreChangeList changes;

  ASSERT_TRUE(db().UpdateLoginDisplayName(form, &changes));

  ASSERT_EQ(changes.size(), 1u);
  EXPECT_EQ(changes[0].type(), PasswordStoreChange::UPDATE);
  EXPECT_EQ(changes[0].form().display_name, form.display_name);
  EXPECT_TRUE(changes[0].password_changed());

  db_.reset();
  sql::Database verification_db(sql::test::kTestTag);
  ASSERT_TRUE(verification_db.Open(file_));
  sql::Statement statement(
      verification_db.GetUniqueStatement("SELECT display_name FROM logins"));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(form.display_name, statement.ColumnString16(0));
  EXPECT_FALSE(statement.Step());
  EXPECT_TRUE(statement.Succeeded());
}

TEST_F(LoginDatabaseTest, UpdateLoginDisplayNameSuccessWithoutChangeList) {
  PasswordForm form = GenerateExamplePasswordForm();
  ASSERT_EQ(db().AddLogin(form), AddChangeForForm(form));
  form.display_name = u"Updated without a change list";

  ASSERT_TRUE(db().UpdateLoginDisplayName(form, nullptr));

  db_.reset();
  sql::Database verification_db(sql::test::kTestTag);
  ASSERT_TRUE(verification_db.Open(file_));
  sql::Statement statement(
      verification_db.GetUniqueStatement("SELECT display_name FROM logins"));
  ASSERT_TRUE(statement.Step());
  EXPECT_EQ(form.display_name, statement.ColumnString16(0));
  EXPECT_FALSE(statement.Step());
  EXPECT_TRUE(statement.Succeeded());
}
#endif
