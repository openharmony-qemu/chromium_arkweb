#!/usr/bin/env python3

import argparse
import json
import sys
import os
import subprocess
import shutil

sys.path.append(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
                 "build", "android", "gyp"))
from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.


class HapProject:
  def __init__(self, root, sdk_root, node_root, abi, entry_output,
               feature_output='', rk=False):
    self.rk = rk
    self.root = os.path.abspath(root)
    self.entry_output = entry_output
    self.feature_output = feature_output if not rk else ''

    self.node = os.path.abspath(node_root)
    self.abi = abi
    self.hars = []

    if rk:
      self.sdk = os.path.abspath(sdk_root)
      self.ohpm_exe = os.path.abspath(
          os.path.join(self.sdk, '..', 'ohos_build', 'ohpm', 'bin', 'ohpm'))
      self.hvigor_wrapper = os.path.join(self.root, 'hvigorw')
      path_entries = [os.path.join(self.node, "bin")]
    else:
      self.sdk = os.path.abspath(os.path.join(sdk_root, 'sdk'))
      self.ohpm_exe = os.path.abspath(
          os.path.join(self.sdk, '../..', 'ohos_build', 'ohpm', 'bin', 'ohpm'))
      self.hvigorw = os.path.abspath(
          os.path.join('../../../', 'prebuilts', 'hvigorw', 'hvigorw', 'bin'))
      self.hvigor_wrapper = os.path.join('hvigorw')
      path_entries = [os.path.join(self.node, "bin"),
                      os.path.join(self.hvigorw)]

    self.env = {
        "HOME": os.environ['HOME'],
        "OHOS_BASE_SDK_HOME": self.sdk,
        "NODE_HOME": self.node,
        "PATH":
        ":".join(path_entries + [os.environ.get("PATH")])
    }

  def _cleanup_hap_dir(self, dir_name, need_create):
    if os.path.isdir(dir_name):
      shutil.rmtree(dir_name)
    if need_create and not os.path.isdir(dir_name):
      os.makedirs(dir_name)

  def WriteDepfile(self, depfile):
    action_helpers.write_depfile(depfile,
                                 os.path.relpath(self.entry_output,
                                                 self.root), ['build.ninja'])
    pass

  def OverrideProjectFiles(self, src_dir, libs, resources, feature_libs):
    legacy_lib_dir = os.path.join(self.root, 'entry/libs')
    abiDir = ""
    if self.abi == 'x64':
      abiDir = "x86_64"
    elif self.abi == 'arm':
      abiDir = "armeabi-v7a"
    else:
      abiDir = "arm64-v8a"
    lib_dst_dir = os.path.join(self.root, 'entry/libs/%s' % abiDir)
    resources_dst_dir = os.path.join(self.root,
                                     'entry/src/main/resources/rawfile')
    locales_dst_dir = os.path.join(resources_dst_dir, 'locales')

    if not os.path.isdir(src_dir):
      raise FileNotFoundError('Could not find override directory: %s' % src_dir)

    self._cleanup_hap_dir(legacy_lib_dir, False)
    self._cleanup_hap_dir(lib_dst_dir, True)
    if self.feature_output:
      feature_legacy_lib_dir = os.path.join(self.root, 'feature/libs')
      feature_lib_dst_dir = os.path.join(self.root, 'feature/libs/%s' % abiDir)
      self._cleanup_hap_dir(feature_legacy_lib_dir, False)
      self._cleanup_hap_dir(feature_lib_dst_dir, True)
    self._cleanup_hap_dir(resources_dst_dir, True)
    self._cleanup_hap_dir(locales_dst_dir, True)

    for lib in libs:
      src = os.path.join(src_dir, lib)
      dst = os.path.join(lib_dst_dir, lib)
      shutil.copyfile(src, dst)
    if self.feature_output:
      for lib in feature_libs:
        src = os.path.join(src_dir, lib)
        dst = os.path.join(feature_lib_dst_dir, lib)
        shutil.copyfile(src, dst)
    for res in resources:
      src = os.path.join(src_dir, res)
      dst = os.path.join(resources_dst_dir, res)
      shutil.copyfile(src, dst)

  def _ensure_npmrc(self):
    _npmrc = os.path.expanduser('~/.npmrc')
    if os.path.exists(_npmrc):
      return
    with open(_npmrc, 'w') as rc:
      rc.write('''no-proxy=.huawei.com
strict-ssl=false
registry=https://repo.huaweicloud.com/repository/npm/
disturl=https://repo.huaweicloud.com/nodejs/
''')
      rc.flush()

  def UpdateHars(self, hars):
    for _har_kv in hars:
      _src, _dst = _har_kv.split(':')
      _dst_dir = os.path.dirname(_dst)
      if not os.path.exists(_dst_dir):
        os.makedirs(_dst_dir)
      shutil.copyfile(_src, _dst)
      if _dst not in self.hars:
        self.hars.append(_dst)

  def Ohpm(self, cmd_array):
    self._ensure_npmrc()
    cmd = [self.ohpm_exe, '--strict_ssl', 'false'] + cmd_array
    process = subprocess.Popen(cmd,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               env=self.env)
    stdout, stderr = process.communicate()
    if process.returncode != 0:
      err = stderr if len(stderr) > 0 else stdout
      raise RuntimeError('Command \'%s\' failed\n%s' % (' '.join(cmd), err))
    return stdout

  def Hvigor(self, cmd_array):
    if not self.rk:
      path_value = os.environ.get('PATH','')
      os.environ['PATH'] = self.hvigorw + os.pathsep + path_value
    cmd = [self.hvigor_wrapper] + cmd_array
    if self.rk:
      # rk mode: Popen call is vestigial; tolerate failure and continue
      stdout = ''
      stderr = ''
      try:
        process = subprocess.Popen(cmd,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   env=self.env)
        stdout, stderr = process.communicate()
      except Exception as e:
        print(f"An error occurred: {e}")
    else:
      # non-rk mode: Popen call must succeed
      process = subprocess.Popen(cmd,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE,
                                 env=self.env)
      stdout, stderr = process.communicate()

    print(self.root)
    os.chdir(self.root)
    module_param = 'entry,feature' if self.feature_output else 'entry'
    if self.rk:
      hvigor_cmd_name = './hvigorw'
    else:
      hvigor_cmd_name = 'hvigorw'
    command = [hvigor_cmd_name, '--mode=module', '-p', 'module=' + module_param,
               'clean', '-p', 'debuggable=false', 'assembleHap', '--no-daemon']
    try:
      result = subprocess.run(command, capture_output=True, text=True)
      print("Standard output:", result.stdout)
      print("Standard error:", result.stderr)
    except Exception as e:
      print(f"An error occurred: {e}")
    if not self.rk and process.returncode != 0:
      err = stderr if len(stderr) > 0 else stdout
      raise RuntimeError('Command \'%s\' failed\n%s' % (' '.join(cmd), err))
    return stdout

  def _build(self, node_cmd):
    os.chdir(self.root)

    if not self.rk:
      source_dir = os.path.join(self.root, '../../ohos_sdk/24/')
      dest_dir = os.path.join(self.root, '../../ohos_sdk/sdk/')
      if os.path.exists(dest_dir):
        shutil.rmtree(dest_dir)
      shutil.copytree(source_dir, f'{dest_dir}24')
      subprocess.run(['mkdir', '-p', f'{dest_dir}24/native'], check=True)
      subprocess.run(['mkdir', '-p', f'{source_dir}native'], check=True)
      shutil.copy(f'{source_dir}../ohos-sdk/linux/native/oh-uni-package.json', f'{dest_dir}24/native/')
      shutil.copy(f'{source_dir}../ohos-sdk/linux/native/oh-uni-package.json', f'{source_dir}native/')

    local_properties = os.path.join(os.path.abspath(self.root),
                                    'local.properties')
    local_properties_backup = ''
    if os.path.exists(local_properties):
      local_properties_backup = '%s.backup' % local_properties
      shutil.move(local_properties, local_properties_backup)

    # Clean up *_modules;
    for modules in ('node_modules', 'oh_modules'):
      _modules_dir = os.path.join(self.root, modules)
      if os.path.isdir(_modules_dir):
        shutil.rmtree(_modules_dir)

    try:
      self._adjust_build_profile()
      for _har in self.hars:
        self.Ohpm(['install', _har])
      self.Ohpm(['install'])
      self.Hvigor(node_cmd)
    finally:
      self._restore_build_profile()
      if len(local_properties_backup) > 0:
        shutil.move(local_properties_backup, local_properties)

  def Build(self, auto_signed):
    self._build([
        '--mode=module', 'clean', '-p', 'debuggable=false', 'assembleHap',
        '--no-daemon'
    ])

    _hap = os.path.join(
        self.root, 'entry/build/default/outputs/default/entry-default-%s.hap' %
        ('signed' if auto_signed else 'unsigned'))
    if not os.path.exists(_hap):
      raise FileNotFoundError("Could not find final build hap file: %s" % _hap)

    shutil.copyfile(_hap, self.entry_output)

    if self.feature_output:
      _feature_hap = os.path.join(
          self.root, 'feature/build/default/outputs/default/feature-default-%s.hap' %
          ('signed' if auto_signed else 'unsigned'))
      if not os.path.exists(_feature_hap):
        raise FileNotFoundError("Could not find final build feature hap file: %s" % _feature_hap)
      shutil.copyfile(_feature_hap, self.feature_output)

    return 0

  def BuildHar(self, module):
    self._build([
        '--mode', 'module', '-p', 'product', '-p', 'module=' + module,
        'assembleHar', '--no-daemon'
    ])

    _har = ''
    with open(os.path.join(self.root, module, 'package.json'), 'r') as fd:
      data = json.load(fd)
      _har = '%s-%s.tgz' % (data['name'].replace('@', '').replace(
          '/', '-'), data['version'])
      _har = os.path.join(self.root, module, 'build/default/outputs/default',
                          _har)
    if not os.path.exists(_har):
      raise FileNotFoundError("Could not find final build har file: %s" % _har)

    shutil.copyfile(_har, self.entry_output)
    return 0

  def CollectSoFiles(self, dir):
    return [f for f in os.listdir(dir)
        if f.endswith('.so') and os.path.isfile(os.path.join(dir, f))]

  def _adjust_build_profile(self):
    """Dynamically adjust build-profile.json5 based on feature_output.

    When feature_output is empty, remove the feature module from
    build-profile.json5 so that hvigorw only processes the entry module.
    The original file is backed up and restored after build.
    """
    profile_path = os.path.join(self.root, 'build-profile.json5')
    if not os.path.exists(profile_path):
      return
    self._build_profile_backup = '%s.backup' % profile_path
    shutil.copyfile(profile_path, self._build_profile_backup)
    with open(profile_path, 'r') as f:
      profile = json.load(f)
    if not self.feature_output:
      profile['modules'] = [m for m in profile.get('modules', [])
                            if m.get('name') != 'feature']
    with open(profile_path, 'w') as f:
      json.dump(profile, f, indent=2)

  def _restore_build_profile(self):
    """Restore the original build-profile.json5 after build."""
    profile_path = os.path.join(self.root, 'build-profile.json5')
    backup_path = '%s.backup' % profile_path
    if os.path.exists(backup_path):
      shutil.move(backup_path, profile_path)


def main():
  project_root = os.path.dirname(os.path.realpath(__file__))

  parser = argparse.ArgumentParser()
  parser.add_argument('--root',
                      default=project_root,
                      help='Path to the root director of HAP project',
                      required=False)
  parser.add_argument('--sdk-root',
                      default=os.path.join(project_root, '../ohos_sdk/'),
                      help='Path to the root director of OHOS SDK',
                      required=False)
  parser.add_argument('--node-root',
                      default=os.path.join(
                          project_root,
                          '../third_party/node/linux/node-linux-x64/'),
                      help='Path to the root director of node.js',
                      required=False)
  parser.add_argument('--override',
                      default='',
                      help='Override project files from this directory',
                      required=False)
  parser.add_argument('--entry-output',
                      default='',
                      help='Copy entry hap file to',
                      required=False)
  parser.add_argument('--feature-output',
                      default='',
                      help='Copy feature hap file to',
                      required=False)
  parser.add_argument('--entry-libs',
                      default=[],
                      action='append',
                      help='Including libs from "override" directory.',
                      required=False)
  parser.add_argument('--feature-libs',
                      default=[],
                      action='append',
                      help='Including feature libs from "override" directory.',
                      required=False)
  parser.add_argument('--entry-rawfiles',
                      default=[],
                      action='append',
                      help='Including rawfile from "override" directory.',
                      required=False)
  parser.add_argument('--depfile',
                      default='',
                      help='Generate depfile.',
                      required=False)
  parser.add_argument('--abi',
                      default='arm',
                      choices=['arm', 'arm64', 'x64'],
                      help='Native libraries architecture.',
                      required=False)
  parser.add_argument('--har',
                      default='',
                      help='Build har module.',
                      required=False)
  parser.add_argument('--update-har',
                      default=[],
                      action='append',
                      help='Update har files before build hap',
                      required=False)
  parser.add_argument('--auto-signed',
                      default=False,
                      action='store_true',
                      help='The hap package is auto-signed',
                      required=False)
  parser.add_argument('--component',
                      default=False,
                      action='store_true',
                      help='The hap package component so.',
                      required=False)
  parser.add_argument('--rk',
                      default=False,
                      action='store_true',
                      help='Build in rk mode (non-arkweb_ext)',
                      required=False)

  options = parser.parse_args(sys.argv[1:])
  if options.override is not None and len(options.override) > 0:
    options.override = os.path.abspath(options.override)
  else:
    options.override = ''
  if options.entry_output is not None and len(options.entry_output) > 0:
    options.entry_output = os.path.abspath(options.entry_output)
  else:
    options.entry_output = ''
  if options.feature_output is not None and len(options.feature_output) > 0:
    options.feature_output = os.path.abspath(options.feature_output)
  else:
    options.feature_output = ''
  if len(options.depfile) > 0:
    options.depfile = os.path.abspath(options.depfile)
  for _har_kv in options.update_har:
    _kv = _har_kv.split(':')
    if len(_kv) != 2:
      raise RuntimeError('Incorrect --update-har argument: %s' % _har_kv)
    if not os.path.exists(_kv[0]):
      raise FileNotFoundError('Could not find har file to update: %s' % _har)
  hap_project = HapProject(options.root, options.sdk_root, options.node_root,
                           options.abi, options.entry_output,
                           options.feature_output, options.rk)
  if len(options.depfile) > 0:
    hap_project.WriteDepfile(options.depfile)
  if len(options.har) > 0:
    return hap_project.BuildHar(options.har)

  if len(options.override) > 0:
    if options.component:
      libs = hap_project.CollectSoFiles(options.override)
    else:
      libs = options.entry_libs
    hap_project.OverrideProjectFiles(options.override, libs,
                                     options.entry_rawfiles,
                                     options.feature_libs)
  if len(options.update_har) > 0:
    hap_project.UpdateHars(options.update_har)
  return hap_project.Build(options.auto_signed)


if __name__ == '__main__':
  sys.exit(main())
