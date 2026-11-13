#!/bin/bash
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

# use ./build.sh -nosym to build content_shell without symbol.
# 禁用set -e 以确保编译出错时脚本不会停止
# set -e
basedir=$(dirname "$0")
CUR_DIR=$PWD
ROOT_DIR="${CUR_DIR%/src*}""/src"
BUILD_HMP_TOOL_DIR="${ROOT_DIR}/ohos_build/hmp_build"
# Global variables.
BUILD_TARGET_WEBVIEW="ohos_nweb_hap"
BUILD_TARGET_V8="v8:v8_shared"
BUILD_TARGET_BROWSERSHELL="ohos_browser_shell"
BUILD_TARGET_COMPONENT="ohos_nweb_hap"
BUILD_TARGET_NATIVE="adapter_ndk_stub libarkweb_engine libarkweb_render arkweb_crashpad_handler libffmpeg"
BUILD_TARGET_BROWSER_SERVICE="ohos_nweb_ex/browser_service"
BUILD_TARGET_BROWSER_SERVICE_HAR="browser_service_har"
BUILD_TARGET_BROWSER_ENGINE_CORE_UNITTESTS="arkweb/build/unittests:oh_core_unittests"
BUILD_TARGET_BROWSER_ENGINE_BASE_UNITTESTS="arkweb/build/unittests:oh_base_unittests"
BUILD_TARGET_BROWSER_ENGINE_SMOKE_UNITTESTS="arkweb/build/unittests:oh_smoke_unittests"
BUILD_TARGET_ARKWEB_TEST_TOOLS="arkweb/build/tools:arkweb_test_tools"
TEXT_BOLD="\033[1m"
TEXT_NORMAL="\033[0m"

#Add build args begin
buildargs="
  target_os=\"ohos\"
  is_component_build=false
  is_chrome_branded=false
  use_official_google_api_keys=false
  use_ozone=true
  use_aura=true
  ozone_auto_platforms=false
  ozone_platform=\"headless\"
  ozone_platform_headless=true
  enable_extensions=true
  ffmpeg_branding=\"Chrome\"
  use_kerberos=false
  use_bundled_fontconfig=true
  enable_resource_allowlist_generation=false
  clang_use_chrome_plugins=false
  enable_message_center=true
  use_sysroot=true
  gpu_switch=\"on\"
  proprietary_codecs=true
  enable_remoting=true
  enable_nacl=false
  enable_plugins=true
  enable_offline_pages=false
  media_use_ffmpeg=true
  v8_enable_builtins_optimization=false
  enable_widevine=true
  enable_rust=true
  enable_rust_cxx=true
  enable_chromium_prelude=true
  enable_av1_decoder=false
  clang_base_path=\"//ohos_sdk/openharmony/native/llvm\"
  rustc_version=\"bca5fdebe0e539d123f33df5f2149d5976392e76-1-llvmorg-20-init-9764-gb81d8e90\"
  rust_sysroot_absolute=\"//ohos_sdk/rust-toolchain\"
  llvm_ohos_mainline=true"
#Add build args end

buildgn=1
buildcount=0
buildccache=0
buildsymbol=0
buildproduct=""
buildarg_cpu="target_cpu=\"arm\""
buildarg_musl="use_musl=true"
build_v8=0
build_dir="out/rk3568/"
build_product_name="product_name=\"rk3568\""
build_target="${BUILD_TARGET_NATIVE}"
build_modules=""
build_output=""
artifact_mode=0
with_nweb_ex=0
build_sysroot="use_ohos_sdk_sysroot=false"
build_asan=0
build_hwasan=0
build_release=0
build_fuzz=0
use_thin_lto=0
additional_gn_args=""
build_hmp=0
build_hap=0
build_type="$1"
build_component=0
precise_build_ut=0
is_official_build=1
build_debug_version=0
build_legacy=0
debug_args="is_debug=false"
# Explicit public SDK preparation also owns its locked host tool configuration.
if [ "${ARKWEB_PREPARED_SDK:-0}" != 1 ]; then
npm config set registry http://mirrors.tools.huawei.com/npm/
npm config set @ohpm-test:registry https://cmc.centralrepo.rnd.huawei.com/artifactory/api/npm/product_npm/
npm config set @ohos:registry https://cmc.centralrepo.rnd.huawei.com/artifactory/api/npm/product_npm/
npm config set @baize:registry https://cmc.centralrepo.rnd.huawei.com/artifactory/api/npm/product_npm/
npm config set strict-ssl false
cat ${HOME}/.npmrc | grep 'lockfile=false' || echo 'lockfile=false' >> ${HOME}/.npmrc
fi
cp -rf ./src/upper_level/include_vector_patch.py ./
cp -rf ./src/upper_level/.cloudbuild ./
cp -rf ./src/upper_level/.codecheck ./
cp -rf ./src/upper_level/.gclient ./
cp -rf ./src/upper_level/.gclient_entries ./
cp -rf ./src/upper_level/.gclient_previous_sync_commits ./
cp -rf ./src/upper_level/.gcs_entries ./
if [ -d "${ROOT_DIR}/arkweb/ohos_nweb_ex" ]; then
  with_nweb_ex=1
fi

usage() {
  echo -ne "USAGE: $0 [OPTIONS] [PRODUCT]

${TEXT_BOLD}OPTIONS${TEXT_NORMAL}:
  -j N              force number of build jobs
  -ccache           Enable CCache.
  -debug            Enable debug build.
  -t <target>       Build target, supports:
                      n Build native files.
                      b Build BrowserShell.
                      w Build NWeb WebView.
                      m Build NWebEx napi module.
                      M Build NWebEx napi npm package.
                      p Build HMP and NWeb WebView.
                      component Build .so in split.
  -o <output_dir>   Output directory, for example: Default.
  -r                release build.
  -A, -artifact     Artifact mode, using pre-built NDK rather than building
                    them locally.
  -asan             Enable AddressSanitizer (ASan).
  -fuzzer           Enable Fuzzer test(with Asan).
  -use-pac          Enable cfi protection.
  -use-ptr-check    Enable raw_ptr_plugin check.
  -use-blink-gc     Enable blink_gc_plugin check.
  -coverage         Enable use_clang_coverage.
  -G <gn_args>      Additional gn args.

${TEXT_BOLD}PRODUCT${TEXT_NORMAL}:
  rk3568 rk3568_64 musl_64 x86_64
  Default is: rk3568
"
}

while [ "$1" != "" ]; do
  case $1 in
    "musl_64")
      buildarg_cpu="target_cpu=\"arm64\""
      buildarg_musl="use_musl=true"
      build_dir="out/musl_64/"
      build_product_name="product_name=\"all\""
      build_type="musl_64"
    ;;
    "rk3568")
      buildarg_cpu="target_cpu=\"arm\""
      buildarg_musl="use_musl=true"
      build_dir="out/rk3568/"
      build_product_name="product_name=\"rk3568\""
      build_type="rk3568"
    ;;
    "rk3568_64")
      buildarg_cpu="target_cpu=\"arm64\""
      buildarg_musl="use_musl=true"
      build_dir="out/rk3568_64/"
      build_product_name="product_name=\"rk3568\""
      build_type="rk3568_64"
    ;;
    "x86_64")
      buildarg_cpu="target_cpu=\"x64\""
      buildarg_musl="use_musl=true"
      build_dir="out/x86_64/"
      build_product_name="product_name=\"all\""
      build_type="x86_64"
    ;;
    "-j")
      shift
      buildcount=$1
    ;;
    "-ccache")
      buildccache=1
    ;;
    "-sym")
      buildsymbol=1
    ;;
    "-t")
      shift
      build_target=$1
      ;;
    "-o")
      shift
      build_output=$1
      ;;
    "-artifact"|"-A")
      artifact_mode=1
      ;;
    "-asan")
      build_asan=1
      ;;
    "-hwasan")
      build_hwasan=1
      ;;
    "-r")
      build_release=1
      ;;
    "-fuzzer")
      build_fuzz=1
      build_asan=1
      ;;
    "-nolto")
      use_thin_lto=1
      ;;
    "-ex")
      with_nweb_ex=1
      ;;
    "-without-nweb-ex")
      with_nweb_ex=0
      ;;
    "-use-pac")
      buildargs="${buildargs}
      build_pac_cfi=true"
      ;;
    "-coverage")
      buildargs="${buildargs}
      use_clang_coverage=true"
      ;;
    "-use-ptr-check")
      buildargs="${buildargs}
      clang_use_raw_ptr_plugin=true
      enable_check_raw_ref_fields=true
      enable_check_raw_ptr_fields=true
      force_enable_raw_ptr_exclusion=true"
      ;;
    "-use-blink-gc")
      buildargs="${buildargs}
      enable_blink_gc_check=true"
      ;;
    "-debug")
      build_debug_version=1
      ;;
    "-legacy")
      build_legacy=1
      ;;
    "-G")
      shift
      additional_gn_args="$1"
      ;;
    "-m")
      shift
      build_modules="$1"
      ;;
    "-h")
      usage
      exit 0
      ;;
    *)
      echo " -> $1 <- is not a valid option, please follow the usage below: "
      usage
      exit 1
    ;;
  esac
  shift
done

if [ "-${build_output}" != "-" ]; then
  build_dir="out/${build_output}/"
fi

if ! [ -d "${ROOT_DIR}"/"${build_dir}" ]; then
  mkdir -p "${ROOT_DIR}"/"${build_dir}"
fi
log_file="${ROOT_DIR}/${build_dir}build.log"
if [ -f "$log_file" ]; then
  mv "$log_file" "${log_file%.*}_$(date -r "$log_file" +%Y%m%d%H%M%S).log"
fi
exec > >(tee "$log_file") 2>&1

case "${build_target}" in
  "w"|"${BUILD_TARGET_WEBVIEW}")
    build_target="${BUILD_TARGET_WEBVIEW}"
    build_hap=1
    ;;
  "p")
    build_target="${BUILD_TARGET_WEBVIEW}"
    build_hmp=1
    ;;
  "v"|"${BUILD_TARGET_V8}")
    build_target="${BUILD_TARGET_V8}"
    build_v8=1
    ;;
  "b"|"${BUILD_TARGET_BROWSERSHELL}")
    build_target="${BUILD_TARGET_BROWSERSHELL}"
    [[ "-$buildarg_musl" == "-use_musl=true" ]] && build_sysroot="use_ohos_sdk_sysroot=true"
    ;;
  "n"|"${BUILD_TARGET_NATIVE}")
    build_target="${BUILD_TARGET_NATIVE}"
    ;;
  "m"|"${BUILD_TARGET_BROWSER_SERVICE}")
    build_target="${BUILD_TARGET_BROWSER_SERVICE}"
    build_sysroot="use_ohos_sdk_sysroot=true"
    [[ "-${build_product_name}" != "-product_name=\"rk3568\"" ]] && echo \
      "Use -t M instead of -t m to build ${build_target} for current platform."\
      && exit 1
    ;;
  "M"|"${BUILD_TARGET_BROWSER_SERVICE_HAR}")
    build_target="${BUILD_TARGET_BROWSER_SERVICE_HAR}"
    [[ "-$buildarg_musl" == "-use_musl=true" ]] && build_sysroot="use_ohos_sdk_sysroot=true"
    ;;
  "coreut"|"${BUILD_TARGET_BROWSER_ENGINE_CORE_UNITTESTS}")
    buildargs="${buildargs}
    arkweb_test=true"
    build_target="${BUILD_TARGET_BROWSER_ENGINE_CORE_UNITTESTS}"
    ;;
  "allut"|"${BUILD_TARGET_BROWSER_ENGINE_BASE_UNITTESTS}")
    buildargs="${buildargs}
    arkweb_test=true"
    build_target="${BUILD_TARGET_BROWSER_ENGINE_BASE_UNITTESTS}"
    ;;
  "smokeut"|"${BUILD_TARGET_BROWSER_ENGINE_SMOKE_UNITTESTS}")
    buildargs="${buildargs}
    arkweb_test=true"
    build_target="${BUILD_TARGET_BROWSER_ENGINE_SMOKE_UNITTESTS}"
    ;;
  "preciseut")
    precise_build_ut=1
    buildargs="${buildargs}
    arkweb_test=true
    arkweb_precise_build_ut=true
    precise_build_config=\"arkweb/ut_tools/precise_build_config.json\"
    coverage_instrumentation_input_file=\"//out/musl_64/coverage_instrumentation_input.txt\""
    ;;
  "component")
    build_component=1
    build_target="${BUILD_TARGET_COMPONENT}"
    if [ "-${build_modules}" != "-" ]; then
      build_target="${build_modules}"
    fi
    ;;
  "testtools")
    build_target="${BUILD_TARGET_ARKWEB_TEST_TOOLS}"
    ;;
  *)
    echo "build target: ${build_target}"
    ;;
esac

SIGN_INFO="$CUR_DIR/signature_nweb/signature_info.json"
if [[ -z "$ONLINE_USERNAME" ]] || [[ -z "$ONLINE_PASSWD" ]] ; then
  if [[ $build_hmp =~ 1 ]] && [[ -f $SIGN_INFO ]] ; then
    usernameForSignHmp=$(jq -r '.usernameForSignHmp' "$SIGN_INFO")
    passwordForSignHmp=$(jq -r '.passwordForSignHmp' "$SIGN_INFO")
    if [[ -z $usernameForSignHmp ]] || [[ -z $passwordForSignHmp ]]; then
      echo -e "\033[1;31mYou should fill the info in signature_nweb/signature_info.json before build HMP package.\033[0m"
      exit 2
    fi
  fi
fi

if [ $build_debug_version = 1 ]; then
  buildsymbol=1
  is_official_build=0
  debug_args="is_debug=true use_debug_fission=true"
fi

SYMBOL_LEVEL=1
if [ $buildsymbol = 1 ]; then
  SYMBOL_LEVEL=2
fi

if [ $buildcount = 0 ]; then
  buildcount=$(grep processor /proc/cpuinfo | wc -l)
  #buildcount=40
fi

if [ $buildccache = 1 ]; then
  if [ $buildcount = 0 ]; then
    buildcount=64
  fi
  GN_ARGS="cc_wrapper=\"ccache\" clang_use_chrome_plugins=false linux_use_bundled_binutils=false"
  export CCACHE_CPP2=yes
fi

if [ ${artifact_mode} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} build_chromium_with_ohos_src=false"
else
  GN_ARGS="${GN_ARGS} build_chromium_with_ohos_src=true"
fi

if [ ${build_asan} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} is_asan=true"
else
  GN_ARGS="${GN_ARGS} is_asan=false"
fi

if [ ${build_legacy} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} is_nolegacy=false"
  export OH_LEGACY=1
else
  GN_ARGS="${GN_ARGS} is_nolegacy=true"
  export OH_LEGACY=0
fi

if [ ${build_hwasan} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} is_hwasan=true use_thin_lto=false v8_enable_pointer_compression=false v8_use_external_startup_data=false"
else
  GN_ARGS="${GN_ARGS} is_hwasan=false"
fi

if [[ "${build_target}" = "${BUILD_TARGET_BROWSERSHELL}" ]] || [[ "${build_target}" = "${BUILD_TARGET_BROWSER_SERVICE_HAR}" ]]; then
  GN_ARGS="${GN_ARGS} is_browser_service=true"
else
  GN_ARGS="${GN_ARGS} is_browser_service=false"
fi

if [ ${build_release} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} is_release_build=true"
else
  GN_ARGS="${GN_ARGS} is_release_build=false"
fi

if [ ${build_fuzz} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} dcheck_always_on = false enable_mojom_fuzzer = false use_libfuzzer = true use_remoteexec = false use_thin_lto=false use_clang_coverage=true"
fi

if [ ${build_v8} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} v8_component_build = true use_custom_libcxx = false use_custom_libcxx_for_host = true v8_enable_pointer_compression = true v8_enable_pointer_compression_shared_cage = true v8_enable_sandbox = false v8_use_external_startup_data = false v8_deprecation_warnings = false v8_use_libm_trig_functions = false v8_enable_i18n_support = false cppgc_enable_slim_write_barrier = false v8_enable_pointer_compression_8gb = false use_allocator_shim = false enable_backup_ref_ptr_support = false v8_enable_jitfort = true arkweb_test = true"
fi

if [ ${use_thin_lto} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} use_thin_lto=false"
fi
if [ ${is_official_build} -eq 0 ]; then
  GN_ARGS="${GN_ARGS} is_official_build=false"
else
  GN_ARGS="${GN_ARGS} is_official_build=true"
fi

if [ ${build_component} -eq 1 ]; then
  GN_ARGS="${GN_ARGS} is_component_build=true is_official_build=false is_debug=false"
fi

if [[ "${build_type}" != "rk3568" ]] && [ ${build_hwasan} -ne 1 ]; then
  GN_ARGS="${GN_ARGS} v8_enable_drumbrake=true"
fi

# Extract ohos-sdk.
if [ -f "src/ohos_sdk/.install" ] && [ "${ARKWEB_PREPARED_SDK:-0}" != 1 ]; then
  bash "src/ohos_sdk/.install"
  if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install ohos-sdk, abort!"
    exit 1
  fi
fi

if [ -f "${ROOT_DIR}/third_party/ohos_nweb_hap/BUILD.gn" ]; then
  buildargs="${buildargs}
  enable_ohos_nweb_hap=true"
fi

if [[ "${build_type}" == "rk3568" ]] || [[ "${build_type}" == "rk3568_64" ]]; then
  rm -rf ./src/arkweb/ohos_nweb_ex
  with_nweb_ex=0
fi

# if [ ${with_nweb_ex} -eq 1 -a ${artifact_mode} -eq 1 ]; then
if [ ${with_nweb_ex} -eq 1 ]; then
  if ! [ -d "${ROOT_DIR}"/"${build_dir}" ]; then
    mkdir -p "${ROOT_DIR}"/"${build_dir}"
  fi

  BUILD_CONFIG_NAME="ohos_nweb_ex_config.gn"
  CONFIG_AUTO_GEN_DIR="build/config_gn_java"
  config_override=build/config/default.json
  config_to_gn_args=""

  if [ -f "${ROOT_DIR}"/"${build_dir}""${BUILD_CONFIG_NAME}" ]; then
    rm "${ROOT_DIR}"/"${build_dir}""${BUILD_CONFIG_NAME}"
  fi
  # Generate build_config.gn
  if ! [ -d "${CONFIG_AUTO_GEN_DIR}" ];then
      mkdir "${CONFIG_AUTO_GEN_DIR}"
  fi

  result=python build/config_to_gn.py \
     -o "${ROOT_DIR}"/"${build_dir}""${BUILD_CONFIG_NAME}" \
     -d ${CONFIG_AUTO_GEN_DIR} \
     -i ${config_override} ${config_to_gn_args}
  if [ $? -ne 0 ]; then
    echo -e "Failed to execute build/config_to_gn.py, see errors above."
    exit 1
  fi
  rm -rf ./src/third_party/ohos_nweb_hap/hvigor ./src/third_party/ohos_nweb_hap/signature
  cp -r ./src/third_party/ohos_nweb_hap/dependencies/musl/* ./src/third_party/ohos_nweb_hap/
  buildargs="${buildargs}
    ohos_nweb_ex_config_name=\"//${build_dir}${BUILD_CONFIG_NAME}\"
    arkweb_ext_dir=\"//arkweb/ohos_nweb_ex\"
    "
else
  echo "ROOT_DIR ${ROOT_DIR}"
  echo "build_dir ${build_dir}"
  if ! [ -d "${ROOT_DIR}"/"${build_dir}" ]; then
    mkdir -p "${ROOT_DIR}"/"${build_dir}"
  fi

  BUILD_CONFIG_NAME="ohos_nweb_ex_config.gn"
  CONFIG_AUTO_GEN_DIR="build/config_gn_java"
  # for define macro
  config_override=build/config/ohos.json
  config_to_gn_args=""

  if [ -f "${ROOT_DIR}"/"${build_dir}""${BUILD_CONFIG_NAME}" ]; then
    rm "${ROOT_DIR}"/"${build_dir}""${BUILD_CONFIG_NAME}"
  fi
  # Generate build_config.gn
  if ! [ -d "${CONFIG_AUTO_GEN_DIR}" ];then
      mkdir "${CONFIG_AUTO_GEN_DIR}"
  fi

  result=python build/config_to_gn.py \
     -o "${ROOT_DIR}"/"${build_dir}""${BUILD_CONFIG_NAME}" \
     -d ${CONFIG_AUTO_GEN_DIR} \
     -i ${config_override} ${config_to_gn_args}
  if [ $? -ne 0 ]; then
    echo -e "Failed to execute build/config_to_gn.py, see errors above."
    exit 1
  fi
  rm -rf ./src/third_party/ohos_nweb_hap/hvigor ./src/third_party/ohos_nweb_hap/signature
  cp -r ./src/third_party/ohos_nweb_hap/dependencies/rk/* ./src/third_party/ohos_nweb_hap/
  buildargs="${buildargs}
    ohos_nweb_ex_config_name=\"\"
    arkweb_ext_dir=\"\"
    "
fi

echo "Copying NDK stub files..."
python3 "${ROOT_DIR}/arkweb/build/copy_ndk_files.py" "${ROOT_DIR}"

if ! [ -d "${CUR_DIR}/deps_code" ]; then
  mkdir -p ${CUR_DIR}/deps_code
  echo "create new deps_code dir"
  cd deps_code
# use ssh download deps_code
  git clone https://gitee.com/openharmony/web_webview.git -b master
  mv web_webview webview
  cd ..
fi
cd src
source arkweb/build/prepare.sh $build_dir

cd cef/tools
bash ./translator.sh
cd -

# Keep additional build-edge patches after generated Chromium/CEF inputs.
if [ -n "${ARKWEB_EXTRA_BUILD_PATCH_SCRIPT:-}" ]; then
  bash "${ARKWEB_EXTRA_BUILD_PATCH_SCRIPT}" --source-root "${ROOT_DIR%/src}"
fi

time_start_for_build=$(date +%s)
time_start_for_gn=$time_start_for_build

if [ $buildgn = 1 ]; then
  echo "generating args list:"
  echo "$buildargs $buildarg_cpu $buildarg_musl $build_sysroot $build_product_name $GN_ARGS $debug_args symbol_level=$SYMBOL_LEVEL $additional_gn_args"

  third_party/depot_tools/gn gen $build_dir --export-compile-commands --args="$buildargs $buildarg_cpu $buildarg_musl $build_sysroot $build_product_name $GN_ARGS symbol_level=$SYMBOL_LEVEL $additional_gn_args"
  if [ "${build_target}" = "ohos_nweb_hap" ]; then
    echo "extract build_metadata file and generate arkweb_prebuild.gni"
    python3 ${ROOT_DIR}/arkweb/build/collect_prebuild_targets.py --search-root ${ROOT_DIR}/${build_dir} --output ${ROOT_DIR}/${build_dir}arkweb_prebuild.gni --threads 10
    buildargs="${buildargs}
      arkweb_enable_prebuild=true"
    echo "generating args list:"
    echo "$buildargs $buildarg_cpu $buildarg_musl $build_sysroot $build_product_name $GN_ARGS symbol_level=$SYMBOL_LEVEL $additional_gn_args"
    third_party/depot_tools/gn gen $build_dir --export-compile-commands --args="$buildargs $buildarg_cpu $buildarg_musl $build_sysroot $build_product_name $GN_ARGS $debug_args symbol_level=$SYMBOL_LEVEL $additional_gn_args"
    # 删除build.ninja中的generator规则，避免ninja无限循环重新生成
    # 原因：GN每次gen都会重写args.gn，ninja检测到变化就会触发generator规则重新运行GN
    # 解决方案：删除build.ninja中的generator标记和gn规则，彻底禁用自动重新生成
    sed -i '/^rule gn$/,/^build build.ninja: phony build.ninja.stamp$/d' ${ROOT_DIR}/${build_dir}build.ninja
    sed -i '/generator = 1/d' ${ROOT_DIR}/${build_dir}build.ninja
  fi
fi
time_end_for_gn=$(date +%s)

if [ ${build_fuzz} -eq 1 ]; then
  echo "################################################"
  echo "use following command to find your fuzzer target"
  echo "cd src && third_party/depot_tools/gn ls out/\${target} | grep fuzzer"
  echo "################################################"
  echo "build fuzzer with:"
  echo "cd src && ninja -C out/\${platfrom} \${target}"
  echo "like:ninja -C out/rk3568 string_pattern_fuzzer"
  echo "################################################"
  exit 0
fi
export OHOS_BASE_SDK_HOME="${ROOT_DIR}/ohos_sdk"

if [ ${precise_build_ut} -eq 1 ]; then
  # precise_ut_result files
  file="$build_dir/ut_precise/precise_ut_result.txt"
  precise_build_result=""
  while IFS= read -r line || [[ -n "$line" ]]; do
    precise_build_result+="$line "
  done < "$file"

  if [ -z "${precise_build_result// }" ]; then
    buildargs="${buildargs}
    arkweb_test=true
    use_clang_coverage=false
    arkweb_precise_build_ut=false"
    third_party/depot_tools/gn gen $build_dir --export-compile-commands --args="$buildargs $buildarg_cpu $buildarg_musl $build_sysroot $build_product_name $GN_ARGS $debug_args symbol_level=$SYMBOL_LEVEL $additional_gn_args"
    build_target="${BUILD_TARGET_BROWSER_ENGINE_CORE_UNITTESTS}"
  else
    build_target="${precise_build_result}"
  fi
elif [ "${build_target}" = "ohos_nweb_hap" ]; then
  echo "third_party/depot_tools/ninja -C $build_dir -j$buildcount arkweb_prebuild"
  ninja_prebuild_start=$(date +%s)
  third_party/depot_tools/ninja -C $build_dir -j$buildcount arkweb_prebuild_buildflags
  third_party/depot_tools/ninja -C $build_dir -j$buildcount arkweb_prebuild_mojom
  ninja_prebuild_end=$(date +%s)
  mojo_pre_time_cost=$(($ninja_mojo_pre_end - $ninja_mojo_pre_start))
  echo "##############################################"
  echo "ninja mojo_pre exit code: $ninja_mojo_pre_exit_code"
  echo "ninja arkweb_prebuild time cost: $(($ninja_prebuild_end - $ninja_prebuild_start))"
  echo "##############################################"

  if [ $ninja_mojo_pre_exit_code -ne 0 ]; then
    echo "WARNING: ninja mojo_pre failed with exit code $ninja_mojo_pre_exit_code, but continuing script execution..."
  fi
fi

echo "third_party/depot_tools/ninja -C $build_dir -j$buildcount ${build_target}"
third_party/depot_tools/ninja -C $build_dir -j$buildcount ${build_target}
ninja_exit_code=$?

echo "ninja exit code: $ninja_exit_code"

# 如果ninja有错误，输出提示信息
if [ $ninja_exit_code -ne 0 ]; then
  echo "WARNING: ninja build failed with exit code $ninja_exit_code, but continuing script execution..."
fi

if [[ $build_hmp =~ 1 ]] ; then
  export HW_HARMONY_ENGINE_ROOT="${CUR_DIR}"
  export HW_HARMONY_ENGINE_OUT_RELEASE="${ROOT_DIR}/${build_dir}/"
  if [[ -n "$ONLINE_USERNAME" ]] && [[ -n "$ONLINE_PASSWD" ]] ; then
    export SIGN_NEW_USERNAME="${ONLINE_USERNAME}"
    export SIGN_NEW_PASSWORD="${ONLINE_PASSWD}"
  else
    export SIGN_NEW_USERNAME="${usernameForSignHmp}"
    export SIGN_NEW_PASSWORD="${passwordForSignHmp}"
  fi
  export ohos_mr_build_type="$build_type"

  bash -c "${BUILD_HMP_TOOL_DIR}/build_hmp.sh"
elif [[ $build_hap =~ 1 ]] && [[ ${with_nweb_ex} -eq 0 ]] ; then
    export HW_HARMONY_ENGINE_ROOT="${CUR_DIR}"
    bash -c "bash ${ROOT_DIR}/arkweb/build/sign.sh ${build_type}"
elif [[ $build_hap =~ 1 ]] && [[ ${build_hwasan} -eq 1 ]] ; then
    export HW_HARMONY_ENGINE_ROOT="${CUR_DIR}"
    export HW_HARMONY_ENGINE_OUT_RELEASE="${ROOT_DIR}/${build_dir}/"
    if [[ -n "$ONLINE_USERNAME" ]] && [[ -n "$ONLINE_PASSWD" ]] ; then
      export SIGN_NEW_USERNAME="${ONLINE_USERNAME}"
      export SIGN_NEW_PASSWORD="${ONLINE_PASSWD}"
    else
      export SIGN_NEW_USERNAME="${usernameForSignHmp}"
      export SIGN_NEW_PASSWORD="${passwordForSignHmp}"
    fi
    export ohos_mr_build_type="$build_type"
    bash -c "bash ${ROOT_DIR}/ohos_nweb_ex/signature_nweb/sign_asan.sh"
fi

time_end_for_build=$(date +%s)
time_format() {
  hours=$((($1 - $2) / 3600))
  minutes=$((($1 - $2) % 3600 / 60))
  seconds=$((($1 - $2) % 60))
}

time_format $time_end_for_gn $time_start_for_gn
printf "\n\e[32mTime for gn  : %dH:%dM:%dS \e[0m\n" $hours $minutes $seconds
time_format $time_end_for_build $time_end_for_gn
printf "\e[32mTime for build : %dH:%dM:%dS \e[0m\n" $hours $minutes $seconds
time_format $time_end_for_build $time_start_for_build
printf "\e[32mTime for Total : %dH:%dM:%dS \e[0m\n\n" $hours $minutes $seconds

json_file="$CUR_DIR/src/arkweb/build/assume_unchanged.json"
while IFS= read -r dir; do
    directory=$(echo "$dir" | jq -r '.key')
    if [ -d "$CUR_DIR/$directory" ]; then
        cd "$CUR_DIR/$directory"
        files=$(echo "$dir" | jq -r '.value[]')
        for file in $files; do
            git update-index --assume-unchanged "$file"
        done
        cd -
    fi
done < <(jq -c '. | to_entries[]' "$json_file")

python3 $ROOT_DIR/arkweb/build/ninja2trace.py --ninja-log $ROOT_DIR/$build_dir/.ninja_log --trace-file $ROOT_DIR/$build_dir/build.trace --ninja-start-time "0" --duration-file $ROOT_DIR/$build_dir/sorted_action_duration.txt
echo "Build completed. For more build information, see: $log_file"
