#!/bin/bash
set -e
set -x

SRC_DIR=$HW_HARMONY_ENGINE_ROOT/src
RELEASE_DIR=$HW_HARMONY_ENGINE_OUT_RELEASE
SIGNATURE_DIR=$HW_HARMONY_ENGINE_ROOT/signature_nweb

echo "building hmp..."

function build_hap() {
  if [ ! -f "$RELEASE_DIR/$1.hap" ]; then
    return 0
  fi

  cd $SIGNATURE_DIR
  cp $RELEASE_DIR/$1.hap ./ && ls -lh $RELEASE_DIR/$1.hap

  # Process HAP before signing
  python3 $SRC_DIR/arkweb/build/hmp_build/process_hap.py \
      --input $1.hap --output $1_processed.hap --sdk $SRC_DIR/ohos_sdk/sdk/24

  # Overwrite the original hap file
  cp $1_processed.hap $1.hap

  sh ./sign.sh ${OH_LEGACY} $1 $2
  if [ ! -f "$SIGNATURE_DIR/$2-$ohos_mr_build_type.hap" ]; then
    echo -e "$1.hap sign fail"
    exit 1
  fi

  echo -e "$1.hap sign success "

  if [ "${OH_LEGACY:-0}" -eq 0 ]; then
    cp $2-$ohos_mr_build_type.hap $RELEASE_DIR/
  else
    cp $2-$ohos_mr_build_type.hap $RELEASE_DIR/$2-Legacy.hap
  fi
}

build_hap ohos_nweb NWeb
build_hap ohos_nweb_ex NWebEx
