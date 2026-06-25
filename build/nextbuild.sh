
#!/bin/bash
source /etc/profile
export SHELL=/bin/bash
export DEPOT_TOOLS_UPDATE=0

#set -x
SCRIPT_FOLDER=$(cd "$(dirname "$0")";pwd)
WORKSPACE=$(realpath "$SCRIPT_FOLDER/")
NEXTBUILD_VERSION="$(ls /opt/buildtools | grep nextbuild | head -n 1)"

if [ ! -f "/opt/buildtools/${NEXTBUILD_VERSION}/NextBuild" ];then
  NEXTBUILD_VERSION="nextbuild"
fi

echo "NextBuild WORKSPACE=${WORKSPACE}"

export NEXT_BUILD_SHELL="/opt/buildtools/${NEXTBUILD_VERSION}/NextBuild"
export NEXT_BUILD_DIR="/opt/buildtools/${NEXTBUILD_VERSION}"
export NB_CONFIG_FILE="/opt/buildtools/${NEXTBUILD_VERSION}/nextbuild.conf"
export CCACHE_DIR="${WORKSPACE}/.ccache"
export CCACHE_MAXSIZE=50G
export CCACHE_CPP2=true
export CCACHE_SLOPPINESS=locale,time_macros
export CCACHE_BASEDIR=${WORKSPACE}
export CCACHE_NOHASHDIR=true
export USE_CCACHE=0


if [ ! -f $NEXT_BUILD_SHELL ];then
  echo "===Sorry!=== $NEXT_BUILD_SHELL not exist!"
  cd $WORKSPACE
  build_arkweb.sh "$@"
fi

if [ ! -d $CCACHE_DIR ];then
  echo "mkdir -p $CCACHE_DIR"
  mkdir -p $CCACHE_DIR
fi

echo -e "======== NextBuild build start. ========"
time=$(date "+%Y-%m-%d %H:%M:%S")
echo -e "START TIME:$time"

cd $WORKSPACE
for ((attempt = 1; attempt <= 3; attempt++)); do
  echo "$NEXT_BUILD_SHELL --cache build_arkweb.sh $@"
  $NEXT_BUILD_SHELL --cache bash build_arkweb.sh "$@"
  exit_status=$?
  if [ $exit_status -eq 0 ]; then
    echo "retry $attempt times"
    time=$(date "+%Y-%m-%d %H:%M:%S")
    echo -e "END TIME:$time"
    echo -e "======== NextBuild build end.   ========"
    exit 0
  fi
done
 
echo "retry 3 times failed"
exit 1
