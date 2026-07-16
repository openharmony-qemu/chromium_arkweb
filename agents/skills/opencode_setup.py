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
"""Installs and manages agent skills for OpenCode.

OpenCode discovers skills from a set of well-known locations (see
https://opencode.ai/docs/zh-cn/skills/ for the full reference):

  * Project: ``.opencode/skills/<name>/SKILL.md`` (also ``.claude/skills`` and
    ``.agents/skills`` for compatibility).
  * Global: ``~/.config/opencode/skills/<name>/SKILL.md`` (also
    ``~/.claude/skills`` and ``~/.agents/skills``).

A skill's enabled/disabled/ask state is controlled through the ``permission``
block of an ``opencode.json`` (or ``opencode.jsonc``) config file::

    {
      "permission": {
        "skill": {
          "my-skill": "allow"   // allow | deny | ask
        }
      }
    }

Project scoped operations target the Chromium src root (``<src>``) when
arkweb is mounted inside it, editing ``<src>/.opencode/opencode.json`` and
installing under ``<src>/.opencode/skills``; global scoped operations edit
``~/.config/opencode/opencode.json``. The ``--global`` flag selects the global
scope.

Installable skills are discovered from both this (ArkWeb) repository's own
``agents/skills`` and the outer Chromium src tree's ``agents/skills`` when
arkweb is mounted inside it (``src/arkweb``), so the Chromium-shipped skills
(e.g. ``histograms``, ``fuzzing``) are available to install alongside any
ArkWeb-specific ones.
"""

import argparse
import json
import logging
import os
import sys
from dataclasses import dataclass
from pathlib import Path

# The ArkWeb repository root (where this script lives).
_ARKWEB_ROOT = Path(__file__).resolve().parents[2]

# Subdirectories (relative to a repo root) that may ship skills. Mirrors the
# layout used by the upstream Chromium ``agents/skills`` setup.
_SKILL_SOURCE_SUBDIRS = (
    ('agents', 'internal', 'skills'),
    ('agents', 'shared', 'skills'),
    ('agents', 'skills'),
    ('internal', 'agents', 'skills'),
    ('third_party', 'depot_tools', 'agents', 'skills'),
)


def _find_chromium_src_root(arkweb_root: Path) -> Path | None:
    """Returns the Chromium src root that contains ``arkweb/``, if any.

    When arkweb is mounted inside the Chromium src tree (``src/arkweb/``) the
    parent directory is the Chromium src root, recognized by the Chromium
    markers ``DEPS`` / ``.gn``. Returns ``None`` when arkweb is checked out
    standalone, in which case only arkweb's own skills are discovered.
    """
    candidate = arkweb_root.parent
    if candidate == arkweb_root:
        return None
    if (candidate / 'DEPS').exists() or (candidate / '.gn').exists():
        return candidate
    return None


# The outer Chromium src root (chromium_src), when arkweb is nested inside it.
_CHROMIUM_SRC_ROOT = _find_chromium_src_root(_ARKWEB_ROOT)


def _opencode_project_root(arkweb_root: Path) -> Path:
    """Returns the OpenCode project root for the given arkweb root.

    This is the outer Chromium src root when arkweb is mounted inside it
    (so skills install under ``<src>/.opencode/skills`` rather than
    ``<src>/arkweb/...``), otherwise the arkweb root itself.
    """
    return _find_chromium_src_root(arkweb_root) or arkweb_root


# The OpenCode project root: where skills are installed (``.opencode/skills``)
# and where the project ``opencode.json`` is read/written.
_PROJECT_ROOT = _opencode_project_root(_ARKWEB_ROOT)


def _skill_source_dirs(arkweb_root: Path,
                       chromium_src_root: Path | None) -> list[Path]:
    """Builds the list of source directories scanned for installable skills.

    Both ArkWeb's own skills (under ``arkweb_root``) and the Chromium-shipped
    skills (under ``chromium_src_root``, e.g. ``agents/skills``) are included.
    Chromium entries are listed first and ArkWeb entries last so that a
    same-named ArkWeb skill takes precedence over the Chromium one.
    """
    roots = []
    if chromium_src_root is not None and chromium_src_root != arkweb_root:
        roots.append(chromium_src_root)
    roots.append(arkweb_root)
    dirs = []
    for root in roots:
        for sub in _SKILL_SOURCE_SUBDIRS:
            dirs.append(root.joinpath(*sub))
    return dirs


SKILL_DIRS = _skill_source_dirs(_ARKWEB_ROOT, _CHROMIUM_SRC_ROOT)

# Config file names searched in order of preference. OpenCode accepts both
# ``opencode.json`` and the JSONC variant ``opencode.jsonc``.
_CONFIG_NAMES = ('opencode.jsonc', 'opencode.json')

# permission.skill values understood by OpenCode.
PERM_ALLOW = 'allow'
PERM_DENY = 'deny'
PERM_ASK = 'ask'
_VALID_PERMS = (PERM_ALLOW, PERM_DENY, PERM_ASK)

# Maps a CLI action to the permission value written to the config file.
_ACTION_TO_PERM = {
    'enable': PERM_ALLOW,
    'disable': PERM_DENY,
    'ask': PERM_ASK,
}


@dataclass
class SkillInfo:
    """Holds information about a skill."""
    name: str
    available: bool = False
    path: Path | None = None
    project_installed: bool = False
    global_installed: bool = False
    project_location: Path | None = None
    global_location: Path | None = None
    project_permission: str | None = None
    global_permission: str | None = None

    @property
    def installed(self) -> bool:
        """Whether the skill is installed in any scope."""
        return self.project_installed or self.global_installed


def _project_skill_dirs() -> list[Path]:
    """Discovery directories for project scoped skills."""
    root = _PROJECT_ROOT
    return [
        root / '.opencode' / 'skills',
        root / '.claude' / 'skills',
        root / '.agents' / 'skills',
    ]


def _global_skill_dirs() -> list[Path]:
    """Discovery directories for globally scoped skills."""
    home = Path.home()
    return [
        home / '.config' / 'opencode' / 'skills',
        home / '.claude' / 'skills',
        home / '.agents' / 'skills',
    ]


def _skills_install_dir(global_scope: bool) -> Path:
    """Returns the directory where skills are installed by this tool."""
    if global_scope:
        return Path.home() / '.config' / 'opencode' / 'skills'
    return _PROJECT_ROOT / '.opencode' / 'skills'


def _config_path(global_scope: bool) -> Path:
    """Returns the config file path to read/write for the given scope.

    Prefers an existing ``opencode.jsonc`` over ``opencode.json`` so the user's
    chosen format is preserved. Defaults to ``opencode.json`` when none exists.

    For the project scope the config lives inside the ``.opencode`` directory
    (``<src>/.opencode/opencode.json``), which OpenCode reads and which takes
    precedence over a root ``<src>/opencode.json``; this keeps the managed
    config next to ``.opencode/skills``.
    """
    base = (Path.home() / '.config' / 'opencode') if global_scope else \
        _PROJECT_ROOT / '.opencode'
    for name in _CONFIG_NAMES:
        candidate = base / name
        if candidate.exists():
            return candidate
    return base / 'opencode.json'


def _jsonc_to_json(text: str) -> str:
    """Normalizes JSONC text into strict JSON.

    Strips ``//`` line comments, ``/* */`` block comments and trailing commas
    while leaving string contents untouched, so the result can be fed to
    ``json.loads``.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            out.append(c)
            i += 1
            while i < n:
                c = text[i]
                if c == '\\' and i + 1 < n:
                    out.append(c)
                    out.append(text[i + 1])
                    i += 2
                    continue
                out.append(c)
                i += 1
                if c == '"':
                    break
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            i += 2
            while i < n and text[i] not in '\r\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            i += 2
            while i < n and not (text[i] == '*' and i + 1 < n
                                 and text[i + 1] == '/'):
                i += 1
            i += 2
            continue
        if c == ',':
            j = i + 1
            while j < n and text[j] in ' \t\r\n':
                j += 1
            if j < n and text[j] in '}]':
                i += 1
                continue
        out.append(c)
        i += 1
    return ''.join(out)


def _read_config(path: Path) -> dict:
    """Reads and parses an opencode config file, returning a dict.

    Returns an empty dict if the file does not exist or cannot be parsed.
    """
    if not path.exists():
        return {}
    try:
        text = path.read_text(encoding='utf-8')
        data = json.loads(_jsonc_to_json(text))
        return data if isinstance(data, dict) else {}
    except (json.JSONDecodeError, OSError) as e:
        logging.warning('Could not parse %s: %s', path, e)
        return {}


def _write_config(path: Path, config: dict) -> None:
    """Writes the config dict back to disk as pretty-printed JSON."""
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(config, indent=2, ensure_ascii=False) + '\n'
    path.write_text(text, encoding='utf-8')


def _get_skill_permission(config: dict, name: str) -> str | None:
    """Returns the permission value for a skill, or None if unset."""
    perm = config.get('permission')
    if not isinstance(perm, dict):
        return None
    skill_perm = perm.get('skill')
    if not isinstance(skill_perm, dict):
        return None
    value = skill_perm.get(name)
    return value if value in _VALID_PERMS else None


def _set_skill_permission(path: Path, name: str, value: str) -> None:
    """Sets ``permission.skill.<name>`` to ``value`` in the config file."""
    config = _read_config(path)
    if not isinstance(config, dict):
        config = {}
    perm = config.get('permission')
    if not isinstance(perm, dict):
        perm = {}
        config['permission'] = perm
    skill_perm = perm.get('skill')
    if not isinstance(skill_perm, dict):
        skill_perm = {}
        perm['skill'] = skill_perm
    skill_perm[name] = value
    _write_config(path, config)


def get_available_skills() -> dict[str, SkillInfo]:
    """Returns a dictionary of skills available to install in the project.

    These are the skills shipped under the ``SKILL_DIRS`` source directories
    (e.g. ``agents/skills``).

    Returns:
      A dictionary mapping skill names to SkillInfo objects.
    """
    available_skills = {}
    for d in SKILL_DIRS:
        if d.exists():
            for skill in sorted(d.iterdir()):
                if skill.is_dir() and (skill / 'SKILL.md').exists():
                    available_skills[skill.name] = SkillInfo(name=skill.name,
                                                             available=True,
                                                             path=skill)
    return available_skills


def get_installed_skills() -> dict[str, SkillInfo]:
    """Returns a dictionary of skills installed in any discovery location.

    Both project and global scopes are scanned, and the explicit permission
    entry (if any) from the matching config file is attached to each skill.

    Returns:
      A dictionary mapping skill names to SkillInfo objects.
    """
    skills: dict[str, SkillInfo] = {}
    project_cfg = _read_config(_config_path(False))
    global_cfg = _read_config(_config_path(True))

    for d in _project_skill_dirs():
        if d.exists():
            for sub in sorted(d.iterdir()):
                if sub.is_dir() and (sub / 'SKILL.md').exists():
                    info = skills.setdefault(sub.name,
                                             SkillInfo(name=sub.name))
                    info.project_installed = True
                    info.project_location = sub

    for d in _global_skill_dirs():
        if d.exists():
            for sub in sorted(d.iterdir()):
                if sub.is_dir() and (sub / 'SKILL.md').exists():
                    info = skills.setdefault(sub.name,
                                             SkillInfo(name=sub.name))
                    info.global_installed = True
                    info.global_location = sub

    for name, info in skills.items():
        info.project_permission = _get_skill_permission(project_cfg, name)
        info.global_permission = _get_skill_permission(global_cfg, name)
    return skills


def _shorten_path(path_str: str | Path | None) -> str:
    """Shortens a path string for display.

    Shortens the project root to ``//`` and the user home to ``~``.
    """
    if path_str is None:
        return '-'

    path = Path(path_str)
    try:
        if path.is_relative_to(_PROJECT_ROOT):
            relative_path = path.relative_to(_PROJECT_ROOT)
            return f'//{relative_path.as_posix()}'
    except ValueError:
        pass

    try:
        home = Path.home()
        if path.is_relative_to(home):
            relative_path = path.relative_to(home)
            return f'~/{relative_path.as_posix()}'
    except (ValueError, RuntimeError):
        pass

    return path.as_posix()


def _state_code(installed: bool, permission: str | None) -> str:
    """Returns a short state code for a scope column."""
    if not installed:
        return 'no'
    if permission == PERM_DENY:
        return 'off'
    if permission == PERM_ASK:
        return 'ask'
    return 'on'


def _destination(skill: SkillInfo) -> str:
    """Returns the install directory for a skill, for display.

    Prefers the actual install location (project over global) when the skill
    is installed; otherwise the default project install target (where
    ``install <name>`` would place it).
    """
    if skill.project_location is not None:
        return _shorten_path(skill.project_location)
    if skill.global_location is not None:
        return _shorten_path(skill.global_location)
    return _shorten_path(_skills_install_dir(False) / skill.name)


def _print_skills_table(data: dict[str, SkillInfo]) -> None:
    """Prints a formatted table of skills."""
    headers = [
        'SKILL', 'AVAILABLE', 'PROJECT', 'GLOBAL', 'SOURCE', 'DESTINATION'
    ]
    col_widths = {h: len(h) for h in headers}
    rows = []

    for name in sorted(data.keys()):
        skill = data[name]
        available = 'yes' if skill.available else 'no'
        project = _state_code(skill.project_installed,
                              skill.project_permission)
        glob = _state_code(skill.global_installed, skill.global_permission)
        source = _shorten_path(skill.path) if skill.path else '-'
        destination = _destination(skill)
        rows.append((name, available, project, glob, source, destination))
        col_widths['SKILL'] = max(col_widths['SKILL'], len(name))
        col_widths['AVAILABLE'] = max(col_widths['AVAILABLE'], len(available))
        col_widths['PROJECT'] = max(col_widths['PROJECT'], len(project))
        col_widths['GLOBAL'] = max(col_widths['GLOBAL'], len(glob))
        col_widths['SOURCE'] = max(col_widths['SOURCE'], len(source))
        col_widths['DESTINATION'] = max(col_widths['DESTINATION'],
                                        len(destination))

    sep = '  '
    print(sep.join(h.ljust(col_widths[h]) for h in headers))
    print(sep.join('-' * col_widths[h] for h in headers))
    for row in rows:
        print(
            sep.join(row[i].ljust(col_widths[h])
                     for i, h in enumerate(headers)))
    print()
    print('State: on=enabled  off=disabled  ask=confirm-before-load  '
          'no=not installed')


def _handle_list(args: argparse.Namespace) -> bool:
    del args
    return list_skills()


def list_skills() -> bool:
    """Handles the 'list' command."""
    installed = get_installed_skills()
    available = get_available_skills()
    all_skills = {name: SkillInfo(name=name) for name in installed}
    for name, info in available.items():
        if name in all_skills:
            all_skills[name].available = info.available
            all_skills[name].path = info.path
        else:
            all_skills[name] = info
    for name, info in installed.items():
        all_skills.setdefault(name, SkillInfo(name=name))
        all_skills[name].project_installed = info.project_installed
        all_skills[name].global_installed = info.global_installed
        all_skills[name].project_location = info.project_location
        all_skills[name].global_location = info.global_location
        all_skills[name].project_permission = info.project_permission
        all_skills[name].global_permission = info.global_permission

    _print_skills_table(all_skills)
    return True


def _handle_install(args: argparse.Namespace) -> bool:
    return install_skills(args.names, args.global_scope)


def install_skills(names: list[str], global_scope: bool = False) -> bool:
    """Handles the 'install' command.

    Symlinks each named skill from its source directory into the target scope's
    ``skills`` directory so it stays in sync with the checkout.
    """
    available = get_available_skills()
    target_dir = _skills_install_dir(global_scope)
    target_dir.mkdir(parents=True, exist_ok=True)
    scope = 'global' if global_scope else 'project'
    success = True
    for name in names:
        skill = available.get(name)
        if not skill or not skill.path:
            logging.error('Error: Skill "%s" not found in agents/skills/.',
                          name)
            success = False
            continue
        link = target_dir / name
        if link.is_symlink() or link.exists():
            logging.info('Skill "%s" already installed (%s).', name, scope)
            continue
        try:
            os.symlink(skill.path, link, True)
            logging.info('Installed "%s" -> %s (%s)', name,
                         _shorten_path(skill.path), scope)
        except OSError as e:
            logging.error('Error: Failed to install "%s": %s', name, e)
            success = False
    return success


def _handle_uninstall(args: argparse.Namespace) -> bool:
    return uninstall_skills(args.names, args.global_scope)


def uninstall_skills(names: list[str], global_scope: bool = False) -> bool:
    """Handles the 'uninstall' command.

    Removes the symlinked skill. Real (non-symlink) directories are never
    deleted to avoid permanently losing files.
    """
    target_dir = _skills_install_dir(global_scope)
    scope = 'global' if global_scope else 'project'
    success = True
    for name in names:
        loc = target_dir / name
        if not loc.exists() and not loc.is_symlink():
            logging.info('Skill "%s" not installed (%s). Skipping.', name,
                         scope)
            continue
        if loc.is_symlink():
            os.unlink(loc)
            logging.info('Uninstalled "%s" (%s).', name, scope)
        else:
            logging.warning(
                '"%s" at %s is not a symlink; refusing to delete a real '
                'directory. Remove it manually.', name, _shorten_path(loc))
            success = False
    return success


def _handle_set_permission(args: argparse.Namespace) -> bool:
    return set_permission(args.action, args.names, args.global_scope)


def set_permission(action: str,
                   names: list[str],
                   global_scope: bool = False) -> bool:
    """Handles 'enable', 'disable' and 'ask' commands.

    Writes the corresponding permission value for each skill name into the
    scoped config file. Skills need not be installed first, since OpenCode
    matches permissions by name (including wildcard patterns).
    """
    value = _ACTION_TO_PERM[action]
    path = _config_path(global_scope)
    scope = 'global' if global_scope else 'project'
    available = get_available_skills()
    installed = get_installed_skills()
    success = True
    for name in names:
        if name not in available and name not in installed:
            logging.warning(
                'Warning: "%s" is not a known skill; setting permission '
                'anyway.', name)
        try:
            _set_skill_permission(path, name, value)
            logging.info('%s: "%s" -> "%s" (%s)', action.capitalize(), name,
                         value, scope)
        except OSError as e:
            logging.error('Error: Failed to %s "%s": %s', action, name, e)
            success = False
    return success


def main() -> None:
    """CLI for managing OpenCode agent skills."""
    parser = argparse.ArgumentParser(
        description='Manage OpenCode agent skills.')
    subparsers = parser.add_subparsers(dest='command', help='Commands')

    list_parser = subparsers.add_parser('list', help='List skills')
    list_parser.set_defaults(func=_handle_list)

    # Common parser for commands that take skill names and a scope flag.
    names_parser = argparse.ArgumentParser(add_help=False)
    names_parser.add_argument('names', nargs='+', help='Names of the skills')
    names_parser.add_argument(
        '--global',
        dest='global_scope',
        action='store_true',
        help='Operate on the global (~/.config/opencode) '
        'scope instead of the project scope.')

    handlers = {
        'install': _handle_install,
        'uninstall': _handle_uninstall,
        'enable': _handle_set_permission,
        'disable': _handle_set_permission,
        'ask': _handle_set_permission,
    }
    help_texts = {
        'install': 'Install skills (symlink into the scope)',
        'uninstall': 'Uninstall skills from the scope',
        'enable': 'Enable skills (permission: allow)',
        'disable': 'Disable skills (permission: deny)',
        'ask': 'Require confirmation before loading (permission: ask)',
    }
    for cmd in ('install', 'uninstall', 'enable', 'disable', 'ask'):
        p = subparsers.add_parser(cmd,
                                  help=help_texts[cmd],
                                  parents=[names_parser])
        p.set_defaults(func=handlers[cmd], action=cmd)

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    logging.basicConfig(level=logging.INFO, format='%(message)s')

    try:
        success = args.func(args)
        if args.command != 'list':
            logging.info('Done')
        if not success:
            sys.exit(1)
    except Exception as e:
        logging.error('Error: %s', e)
        sys.exit(1)


if __name__ == '__main__':
    main()
