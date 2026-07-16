#!/usr/bin/env vpython3

# Copyright (c) 2021 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests for OpenCode skills setup."""

import argparse
import io
import json
import os
import unittest
import unittest.mock
from pathlib import Path

from pyfakefs import fake_filesystem_unittest

import opencode_setup


class TestOpenCodeSkillsSetup(fake_filesystem_unittest.TestCase):
    """Unit tests for the OpenCode skills setup script."""

    # pylint: disable=protected-access

    def setUp(self):
        self.setUpPyfakefs()
        self.project_root = Path('/root/src')
        self.home = Path('/home/user')

        self._patch_root = unittest.mock.patch('opencode_setup._PROJECT_ROOT',
                                               self.project_root)
        self._patch_home = unittest.mock.patch('pathlib.Path.home',
                                               return_value=self.home)
        self.skill_src = self.project_root / 'agents' / 'skills'
        self._patch_dirs = unittest.mock.patch('opencode_setup.SKILL_DIRS',
                                               [self.skill_src])
        self._patch_root.start()
        self._patch_home.start()
        self._patch_dirs.start()
        self.addCleanup(unittest.mock.patch.stopall)

    # -- _jsonc_to_json ---------------------------------------------------

    def test_jsonc_strips_line_comments(self):
        text = '{\n"a": 1, // comment\n"b": 2\n}'
        self.assertEqual(json.loads(opencode_setup._jsonc_to_json(text)), {
            'a': 1,
            'b': 2
        })

    def test_jsonc_strips_block_comments(self):
        text = '{\n/* block\ncomment */"a": 1\n}'
        self.assertEqual(json.loads(opencode_setup._jsonc_to_json(text)),
                         {'a': 1})

    def test_jsonc_strips_trailing_commas(self):
        text = '{\n"a": 1,\n"b": [1, 2,],\n}'
        self.assertEqual(json.loads(opencode_setup._jsonc_to_json(text)), {
            'a': 1,
            'b': [1, 2]
        })

    def test_jsonc_preserves_strings_with_special_chars(self):
        # Commas, braces and comment markers inside strings must survive.
        text = '{"a": "x,} // not a comment", "b": 2,}'
        self.assertEqual(json.loads(opencode_setup._jsonc_to_json(text)), {
            'a': 'x,} // not a comment',
            'b': 2
        })

    def test_jsonc_preserves_escaped_quotes(self):
        text = '{"a": "say \\"hi\\""}'
        self.assertEqual(json.loads(opencode_setup._jsonc_to_json(text)),
                         {'a': 'say "hi"'})

    # -- config helpers ----------------------------------------------------

    def test_config_path_prefers_jsonc(self):
        base = self.project_root / '.opencode'
        self.fs.create_file(base / 'opencode.jsonc')
        self.assertEqual(opencode_setup._config_path(False),
                         base / 'opencode.jsonc')

    def test_config_path_defaults_to_json(self):
        # Project config lives inside the .opencode directory.
        self.assertEqual(opencode_setup._config_path(False),
                         self.project_root / '.opencode' / 'opencode.json')

    def test_config_path_global_scope(self):
        base = self.home / '.config' / 'opencode'
        self.fs.create_file(base / 'opencode.json')
        self.assertEqual(opencode_setup._config_path(True),
                         base / 'opencode.json')

    def test_read_config_missing_file(self):
        self.assertEqual(
            opencode_setup._read_config(self.project_root / 'nope.json'), {})

    def test_read_config_jsonc(self):
        self.fs.create_dir(self.project_root)
        path = self.project_root / 'opencode.jsonc'
        path.write_text(
            '{\n"$schema": "x", // schema\n"permission": {"skill": {"a": "deny",}},\n}',
            encoding='utf-8')
        cfg = opencode_setup._read_config(path)
        self.assertEqual(cfg['permission']['skill']['a'], 'deny')
        self.assertEqual(cfg['$schema'], 'x')

    def test_get_and_set_skill_permission(self):
        path = self.project_root / 'opencode.json'
        opencode_setup._set_skill_permission(path, 'my-skill', 'ask')
        cfg = opencode_setup._read_config(path)
        self.assertEqual(opencode_setup._get_skill_permission(cfg, 'my-skill'),
                         'ask')

        opencode_setup._set_skill_permission(path, 'my-skill', 'allow')
        cfg = opencode_setup._read_config(path)
        self.assertEqual(opencode_setup._get_skill_permission(cfg, 'my-skill'),
                         'allow')

    def test_set_skill_permission_preserves_other_keys(self):
        self.fs.create_dir(self.project_root)
        path = self.project_root / 'opencode.jsonc'
        path.write_text(
            '{"provider": {"x": 1}, "permission": {"skill": {"old": "deny"}}}',
            encoding='utf-8')
        opencode_setup._set_skill_permission(path, 'new', 'ask')
        cfg = opencode_setup._read_config(path)
        self.assertEqual(cfg['provider'], {'x': 1})
        self.assertEqual(cfg['permission']['skill']['old'], 'deny')
        self.assertEqual(cfg['permission']['skill']['new'], 'ask')

    # -- discovery ---------------------------------------------------------

    def test_get_available_skills(self):
        skill1 = self.skill_src / 'skill1'
        self.fs.create_file(skill1 / 'SKILL.md')
        # Not a skill (no SKILL.md).
        self.fs.create_dir(self.skill_src / 'not-a-skill')

        available = opencode_setup.get_available_skills()
        self.assertEqual(set(available), {'skill1'})
        self.assertTrue(available['skill1'].available)
        self.assertEqual(available['skill1'].path, skill1)

    def test_skill_source_dirs_includes_both_roots(self):
        chromium = Path('/chromium')
        arkweb = Path('/arkweb')
        dirs = opencode_setup._skill_source_dirs(arkweb, chromium)
        # 5 subdirs per root, 10 in total.
        self.assertEqual(len(dirs), 10)
        chromium_skills = chromium / 'agents' / 'skills'
        arkweb_skills = arkweb / 'agents' / 'skills'
        self.assertIn(chromium_skills, dirs)
        self.assertIn(arkweb_skills, dirs)
        # Chromium is listed before ArkWeb so ArkWeb takes precedence.
        self.assertLess(dirs.index(chromium_skills), dirs.index(arkweb_skills))

    def test_skill_source_dirs_skips_none_and_duplicate(self):
        arkweb = Path('/arkweb')
        self.assertEqual(len(opencode_setup._skill_source_dirs(arkweb, None)),
                         5)
        # A duplicate root must not be scanned twice.
        self.assertEqual(
            len(opencode_setup._skill_source_dirs(arkweb, arkweb)), 5)

    def test_find_chromium_src_root_with_marker(self):
        self.fs.create_file('/chromium/DEPS')
        self.fs.create_dir('/chromium/arkweb')
        self.assertEqual(
            opencode_setup._find_chromium_src_root(Path('/chromium/arkweb')),
            Path('/chromium'))

    def test_find_chromium_src_root_without_marker(self):
        # Parent carries no Chromium markers -> standalone arkweb.
        self.fs.create_dir('/somewhere/arkweb')
        self.assertIsNone(
            opencode_setup._find_chromium_src_root(Path('/somewhere/arkweb')))

    def test_opencode_project_root_nested_targets_src(self):
        # When arkweb is nested in a Chromium src tree, the project root is
        # the outer src root (install/config go to <src>, not <src>/arkweb).
        self.fs.create_file('/chromium/DEPS')
        self.fs.create_dir('/chromium/arkweb')
        self.assertEqual(
            opencode_setup._opencode_project_root(Path('/chromium/arkweb')),
            Path('/chromium'))

    def test_opencode_project_root_standalone_falls_back(self):
        self.fs.create_dir('/somewhere/arkweb')
        self.assertEqual(
            opencode_setup._opencode_project_root(Path('/somewhere/arkweb')),
            Path('/somewhere/arkweb'))

    def test_project_scope_uses_project_root(self):
        # _PROJECT_ROOT is patched to /root/src in setUp.
        self.assertEqual(opencode_setup._skills_install_dir(False),
                         self.project_root / '.opencode' / 'skills')
        self.assertEqual(opencode_setup._config_path(False),
                         self.project_root / '.opencode' / 'opencode.json')

    def test_get_available_skills_includes_chromium_and_arkweb(self):
        chromium = Path('/chromium')
        arkweb = Path('/arkweb')
        dirs = opencode_setup._skill_source_dirs(arkweb, chromium)
        with unittest.mock.patch('opencode_setup.SKILL_DIRS', dirs):
            self.fs.create_file(chromium / 'agents' / 'skills' / 'histograms' /
                                'SKILL.md')
            self.fs.create_file(arkweb / 'agents' / 'skills' / 'arkweb-only' /
                                'SKILL.md')
            available = opencode_setup.get_available_skills()
        self.assertEqual(set(available), {'histograms', 'arkweb-only'})

    def test_get_available_skills_arkweb_overrides_chromium(self):
        chromium = Path('/chromium')
        arkweb = Path('/arkweb')
        dirs = opencode_setup._skill_source_dirs(arkweb, chromium)
        with unittest.mock.patch('opencode_setup.SKILL_DIRS', dirs):
            self.fs.create_file(chromium / 'agents' / 'skills' / 'shared' /
                                'SKILL.md')
            self.fs.create_file(arkweb / 'agents' / 'skills' / 'shared' /
                                'SKILL.md')
            available = opencode_setup.get_available_skills()
        self.assertEqual(set(available), {'shared'})
        # ArkWeb version wins because it is listed last.
        self.assertEqual(available['shared'].path,
                         arkweb / 'agents' / 'skills' / 'shared')

    def test_get_installed_skills_project_and_global(self):
        proj_dir = self.project_root / '.opencode' / 'skills'
        glob_dir = self.home / '.config' / 'opencode' / 'skills'
        self.fs.create_file(proj_dir / 'shared' / 'SKILL.md')
        self.fs.create_file(proj_dir / 'proj-only' / 'SKILL.md')
        self.fs.create_file(glob_dir / 'shared' / 'SKILL.md')
        self.fs.create_file(glob_dir / 'glob-only' / 'SKILL.md')

        # Project config denies proj-only; global config asks shared. The
        # project config lives under .opencode/ (created above by proj_dir).
        (self.project_root / '.opencode' / 'opencode.json').write_text(
            '{"permission": {"skill": {"proj-only": "deny"}}}',
            encoding='utf-8')
        (self.home / '.config' / 'opencode' / 'opencode.json').write_text(
            '{"permission": {"skill": {"shared": "ask"}}}', encoding='utf-8')

        installed = opencode_setup.get_installed_skills()
        self.assertTrue(installed['shared'].project_installed)
        self.assertTrue(installed['shared'].global_installed)
        self.assertEqual(installed['shared'].global_permission, 'ask')
        self.assertTrue(installed['proj-only'].project_installed)
        self.assertFalse(installed['proj-only'].global_installed)
        self.assertEqual(installed['proj-only'].project_permission, 'deny')
        self.assertFalse(installed['glob-only'].project_installed)
        self.assertTrue(installed['glob-only'].global_installed)

    # -- path / state helpers ---------------------------------------------

    def test_shorten_path(self):
        self.assertEqual(opencode_setup._shorten_path('/root/src/foo/bar'),
                         '//foo/bar')
        self.assertEqual(opencode_setup._shorten_path('/home/user/docs'),
                         '~/docs')
        self.assertEqual(opencode_setup._shorten_path('/other/path'),
                         '/other/path')
        self.assertEqual(opencode_setup._shorten_path(None), '-')

    def test_state_code(self):
        self.assertEqual(opencode_setup._state_code(False, None), 'no')
        self.assertEqual(opencode_setup._state_code(True, None), 'on')
        self.assertEqual(opencode_setup._state_code(True, 'allow'), 'on')
        self.assertEqual(opencode_setup._state_code(True, 'deny'), 'off')
        self.assertEqual(opencode_setup._state_code(True, 'ask'), 'ask')

    def test_destination_prefers_project_location(self):
        skill = opencode_setup.SkillInfo(name='foo',
                                         project_location=self.project_root /
                                         '.opencode' / 'skills' / 'foo',
                                         global_location=self.home /
                                         '.config' / 'opencode' / 'skills' /
                                         'foo')
        self.assertEqual(opencode_setup._destination(skill),
                         '//.opencode/skills/foo')

    def test_destination_global_when_only_global(self):
        skill = opencode_setup.SkillInfo(name='foo',
                                         global_location=self.home /
                                         '.config' / 'opencode' / 'skills' /
                                         'foo')
        self.assertEqual(opencode_setup._destination(skill),
                         '~/.config/opencode/skills/foo')

    def test_destination_default_target_when_not_installed(self):
        skill = opencode_setup.SkillInfo(name='foo')
        self.assertEqual(opencode_setup._destination(skill),
                         '//.opencode/skills/foo')

    # -- table printing ----------------------------------------------------

    def test_print_skills_table(self):
        skills = {
            'skill1':
            opencode_setup.SkillInfo(name='skill1',
                                     available=True,
                                     path=self.skill_src / 'skill1',
                                     project_installed=True,
                                     project_permission=None,
                                     project_location=self.project_root /
                                     '.opencode' / 'skills' / 'skill1',
                                     global_installed=False),
            'skill2':
            opencode_setup.SkillInfo(name='skill2',
                                     available=True,
                                     path=self.skill_src / 'skill2',
                                     project_installed=True,
                                     project_permission='deny',
                                     project_location=self.project_root /
                                     '.opencode' / 'skills' / 'skill2',
                                     global_installed=True,
                                     global_permission='ask',
                                     global_location=self.home / '.config' /
                                     'opencode' / 'skills' / 'skill2'),
            'skill3':
            opencode_setup.SkillInfo(name='skill3', available=False),
        }

        with unittest.mock.patch('sys.stdout',
                                 new_callable=io.StringIO) as mock_stdout:
            opencode_setup._print_skills_table(skills)

        out = mock_stdout.getvalue()
        lines = out.splitlines()
        # Header, separator, three skill rows, blank, legend.
        self.assertEqual(len(lines), 7)
        self.assertEqual(lines[0].split(), [
            'SKILL', 'AVAILABLE', 'PROJECT', 'GLOBAL', 'SOURCE', 'DESTINATION'
        ])

        skill1_line = next(ln for ln in lines if ln.startswith('skill1'))
        self.assertIn('yes', skill1_line)
        self.assertIn(' on ', skill1_line)
        self.assertIn(' no ', skill1_line)
        self.assertIn('//agents/skills/skill1', skill1_line)  # SOURCE
        # DESTINATION shows the actual project install location.
        self.assertIn('//.opencode/skills/skill1', skill1_line)

        skill2_line = next(ln for ln in lines if ln.startswith('skill2'))
        self.assertIn(' off ', skill2_line)
        self.assertIn(' ask ', skill2_line)
        # Project location is preferred over the global one.
        self.assertIn('//.opencode/skills/skill2', skill2_line)

        skill3_line = next(ln for ln in lines if ln.startswith('skill3'))
        # Not installed -> default project install target.
        self.assertIn('//.opencode/skills/skill3', skill3_line)

        self.assertIn('State:', lines[-1])

    # -- install / uninstall ----------------------------------------------

    def test_install_creates_symlink(self):
        src = self.skill_src / 'skill1'
        self.fs.create_file(src / 'SKILL.md')

        success = opencode_setup.install_skills(['skill1'])
        self.assertTrue(success)
        link = self.project_root / '.opencode' / 'skills' / 'skill1'
        self.assertTrue(link.is_symlink())
        self.assertEqual(os.readlink(link), str(src))

    def test_install_global_scope(self):
        src = self.skill_src / 'skill1'
        self.fs.create_file(src / 'SKILL.md')

        success = opencode_setup.install_skills(['skill1'], global_scope=True)
        self.assertTrue(success)
        link = self.home / '.config' / 'opencode' / 'skills' / 'skill1'
        self.assertTrue(link.is_symlink())

    def test_install_already_installed(self):
        src = self.skill_src / 'skill1'
        self.fs.create_file(src / 'SKILL.md')
        target_dir = self.project_root / '.opencode' / 'skills'
        self.fs.create_dir(target_dir)
        os.symlink(src, target_dir / 'skill1')

        success = opencode_setup.install_skills(['skill1'])
        self.assertTrue(success)

    def test_install_not_found(self):
        success = opencode_setup.install_skills(['missing'])
        self.assertFalse(success)

    def test_uninstall_removes_symlink(self):
        src = self.skill_src / 'skill1'
        self.fs.create_file(src / 'SKILL.md')
        link = self.project_root / '.opencode' / 'skills' / 'skill1'
        link.parent.mkdir(parents=True, exist_ok=True)
        os.symlink(src, link)

        success = opencode_setup.uninstall_skills(['skill1'])
        self.assertTrue(success)
        self.assertFalse(link.exists())

    def test_uninstall_refuses_real_directory(self):
        link = self.project_root / '.opencode' / 'skills' / 'skill1'
        self.fs.create_dir(link)

        success = opencode_setup.uninstall_skills(['skill1'])
        self.assertFalse(success)
        self.assertTrue(link.exists())

    def test_uninstall_not_installed(self):
        success = opencode_setup.uninstall_skills(['missing'])
        self.assertTrue(success)

    # -- permissions -------------------------------------------------------

    def test_set_permission_enable_writes_allow(self):
        success = opencode_setup.set_permission('enable', ['foo'])
        self.assertTrue(success)
        cfg = opencode_setup._read_config(self.project_root / '.opencode' /
                                          'opencode.json')
        self.assertEqual(cfg['permission']['skill']['foo'], 'allow')

    def test_set_permission_disable_writes_deny(self):
        opencode_setup.set_permission('disable', ['bar'])
        cfg = opencode_setup._read_config(self.project_root / '.opencode' /
                                          'opencode.json')
        self.assertEqual(cfg['permission']['skill']['bar'], 'deny')

    def test_set_permission_ask_writes_ask(self):
        opencode_setup.set_permission('ask', ['baz'])
        cfg = opencode_setup._read_config(self.project_root / '.opencode' /
                                          'opencode.json')
        self.assertEqual(cfg['permission']['skill']['baz'], 'ask')

    def test_set_permission_global_scope(self):
        opencode_setup.set_permission('disable', ['foo'], global_scope=True)
        cfg = opencode_setup._read_config(self.home / '.config' / 'opencode' /
                                          'opencode.json')
        self.assertEqual(cfg['permission']['skill']['foo'], 'deny')

    def test_set_permission_unknown_skill_warns_but_writes(self):
        success = opencode_setup.set_permission('enable', ['totally-unknown'])
        self.assertTrue(success)
        cfg = opencode_setup._read_config(self.project_root / '.opencode' /
                                          'opencode.json')
        self.assertEqual(cfg['permission']['skill']['totally-unknown'],
                         'allow')

    # -- list / main -------------------------------------------------------

    @unittest.mock.patch('opencode_setup._print_skills_table')
    @unittest.mock.patch('opencode_setup.get_installed_skills')
    @unittest.mock.patch('opencode_setup.get_available_skills')
    def test_list_merges_available_and_installed(self, mock_avail, mock_inst,
                                                 mock_print):
        mock_avail.return_value = {
            'skill1':
            opencode_setup.SkillInfo(name='skill1',
                                     available=True,
                                     path=Path('agents/skills/skill1'))
        }
        mock_inst.return_value = {
            'skill1':
            opencode_setup.SkillInfo(name='skill1',
                                     project_installed=True,
                                     project_permission='deny'),
            'skill2':
            opencode_setup.SkillInfo(name='skill2', global_installed=True),
        }

        args = argparse.Namespace()
        self.assertTrue(opencode_setup._handle_list(args))

        mock_print.assert_called_once()
        merged = mock_print.call_args[0][0]
        self.assertEqual(set(merged), {'skill1', 'skill2'})
        self.assertTrue(merged['skill1'].available)
        self.assertTrue(merged['skill1'].project_installed)
        self.assertEqual(merged['skill1'].project_permission, 'deny')
        self.assertTrue(merged['skill2'].global_installed)

    def test_main_list_dispatch(self):
        argv = ['list']
        with (unittest.mock.patch('sys.argv', [__file__] + argv),
              unittest.mock.patch('opencode_setup.list_skills',
                                  return_value=True) as
              mock_list, unittest.mock.patch('sys.stdout', io.StringIO())):
            opencode_setup.main()
        mock_list.assert_called_once()

    def test_main_install_dispatch(self):
        argv = ['install', 'skill1', '--global']
        with (unittest.mock.patch('sys.argv', [__file__] + argv),
              unittest.mock.patch('opencode_setup.install_skills',
                                  return_value=True) as
              mock_install, unittest.mock.patch('sys.stdout', io.StringIO())):
            opencode_setup.main()
        mock_install.assert_called_once_with(['skill1'], True)

    def test_main_no_command_prints_help(self):
        with (unittest.mock.patch('sys.argv', [__file__]),
              unittest.mock.patch('sys.stdout', io.StringIO()),
              self.assertRaises(SystemExit)):
            opencode_setup.main()


if __name__ == '__main__':
    unittest.main()
