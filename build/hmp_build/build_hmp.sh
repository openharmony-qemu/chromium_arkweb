#!/bin/bash
set -e
set -x
 
echo "building hmp..."

cd $HW_HARMONY_ENGINE_ROOT/signature_nweb
cp $HW_HARMONY_ENGINE_OUT_RELEASE/ohos_nweb.hap $HW_HARMONY_ENGINE_ROOT/signature_nweb/

ls -lh $HW_HARMONY_ENGINE_OUT_RELEASE/ohos_nweb.hap

# Process HAP before signing
python3 $HW_HARMONY_ENGINE_ROOT/src/arkweb/build/hmp_build/process_hap.py \
    --input ohos_nweb.hap \
    --output ohos_nweb_processed.hap \
    --sdk $HW_HARMONY_ENGINE_ROOT/src/ohos_sdk/sdk/24
 
# Overwrite the original hap file
cp ohos_nweb_processed.hap ohos_nweb.hap
if [ "${OH_LEGACY}" -eq 0 ]; then
  sh ./sign.sh
else
  sh ./sign_legacy.sh
  cp NWeb-$ohos_mr_build_type.hap $HW_HARMONY_ENGINE_OUT_RELEASE/Nweb-Legacy.hap
fi
 

if [ ! -f "$HW_HARMONY_ENGINE_ROOT/signature_nweb/NWeb-$ohos_mr_build_type.hap" ]; then
  echo -e "sign fail"
  exit 1
fi
echo -e "sign success "

cp NWeb-$ohos_mr_build_type.hap $HW_HARMONY_ENGINE_OUT_RELEASE/
