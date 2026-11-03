#!/bin/bash
export NB_FILE_SERVER_IP=7.190.160.98:7.190.160.230:7.190.160.207:7.190.163.243:7.190.161.35
CUR_DIR=$PWD
ROOT_DIR="${CUR_DIR%/src*}""/src"

for arg in "$@"; do
        case $arg in
                -cfi)
                        echo "pc"
                        export product_type="pc" && bash build_arkweb.sh -t p musl_64 -use-pac
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -watch)
                        echo "watch"
                        export product_type="watch" && bash nextbuild.sh -t p musl_64 -G build_watch=true
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -x86)
                        echo "x86"
                        bash nextbuild.sh -t p x86_64
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -ut)
                        echo "ut"
                        bash nextbuild.sh  -A -t allut musl_64
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        cd $ROOT_DIR
                        zip -r ut.zip out/musl_64/ut
                        ;;
                -coreut)
                        echo "coreut"
                        bash nextbuild.sh -A -t coreut musl_64
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        cd $ROOT_DIR
                        zip -r report.zip out/musl_64/ut
                        ;;
                -allut)
                        echo "coverage"
                        bash nextbuild.sh -t p musl_64
                        bash $ROOT_DIR/arkweb/ut_tools/push_data/build_ut.sh
                        cd $ROOT_DIR;python ./arkweb/build/ut_upload.py "coverage"
                        zip -r report.zip out/musl_64/ut
                        ;;
                -asan)
                        echo "asan"
                        bash nextbuild.sh -t w musl_64 -hwasan
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -pac)
                        echo "use-pac"
                        bash nextbuild.sh -t p musl_64 -use-pac
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -rk)
                        echo "rk3568"
                        bash nextbuild.sh -t w -A rk3568
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -bs)
                        echo "browserservice"
                        bash $ROOT_DIR/arkweb/ohos_nweb_ex/build/package.sh
                        ;;
                -component)
                        echo "component"
                        bash nextbuild.sh -t component musl_64
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -legacy)
                        echo "legacy"
                        bash nextbuild.sh -t p musl_64 -A -legacy -j 32
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -fuzz)
                        echo "fuzz"
                        sed -i '/-fuzzer/ {n;n;s/build_asan=1/build_asan=0/}' nextbuild.sh;timeout 180s ./nextbuild.sh -t n musl_64;./nextbuild.sh musl_64 -A -fuzzer;ninja -C src/out/musl_64 nweb_impl_createweb
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -debug)
                        echo "debug"
                        cd src&&git stash &&git fetch https://szv-open.codehub.huawei.com/OpenSourceCenter_CR/openharmony-tpc/oh-chromium.git refs/change-requests/9960/2 && git cherry-pick FETCH_HEAD&&git stash pop&&cd -
                        cd src/cef&&git stash &&git fetch https://szv-open.codehub.huawei.com/OpenSourceCenter_CR/openharmony-tpc/chromium_cef.git refs/change-requests/3279/1 && git cherry-pick FETCH_HEAD&&git stash pop&&cd -
                        bash nextbuild.sh -t p musl_64 -debug
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -debug_ci)
                        echo "debug_ci"
                        cd src&&git fetch https://szv-open.codehub.huawei.com/OpenSourceCenter_CR/openharmony-tpc/oh-chromium.git refs/change-requests/9960/2 && git cherry-pick FETCH_HEAD&&cd -
                        cd src/cef&&git fetch https://szv-open.codehub.huawei.com/OpenSourceCenter_CR/openharmony-tpc/chromium_cef.git refs/change-requests/3279/1 && git cherry-pick FETCH_HEAD&&cd -
                        bash nextbuild.sh -t p musl_64 -debug
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -tools)
                        echo "tools"
                        bash nextbuild.sh -t testtools musl_64
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                -plugin)
                        echo "plugin"
                        bash nextbuild.sh -t p musl_64 -use-ptr-check -use-blink-gc  -j 24
                        bash $ROOT_DIR/arkweb/build/upload.sh
                        ;;
                *)
                        echo"error:$arg"
                        exit 1
                        ;;
        esac
done

if [ $# -eq 0 ];then
        export product_type="phone" && bash nextbuild.sh -t p musl_64 -use-pac -A -j 32
        bash $ROOT_DIR/arkweb/build/upload.sh
fi
