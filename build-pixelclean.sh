#!/system/bin/sh
# ============================================================================
# PixelClean 手机本机一键编译脚本（配合 setup-pixelclean-env.sh 使用）
#
# 把《PixelClean-手机编译完整经验.md》第 3~6、12、13 节固化为一条命令：
#   源码补丁(幂等) → CMake 交叉编译 libpixelclean.so → compile.bat 移植编译
#   36 个 SPIR-V → kotlinc+d8 出 dex → aapt2 打包 → cp437 文件名回转 → 签名
#
# 用法：
#   sh build-pixelclean.sh                     # 全自动（环境自动探测）
#   sh build-pixelclean.sh --env <dir>         # 指定 setup 脚本的部署目录
#   sh build-pixelclean.sh --src <PixelClean>  # 指定源码目录（默认脚本旁的 PixelClean/）
#   sh build-pixelclean.sh --skip-native       # 跳过 C++ 编译（复用上次 .so，只重打包）
#   sh build-pixelclean.sh --clean             # 清掉构建目录后重来
#   sh build-pixelclean.sh --install           # 编完顺手装到手机（需特权 shell 通道时生效）
#
# 产物：<ENV>/build/out/PixelClean-signed.apk，并复制到脚本目录一份。
# ============================================================================
set -e

SELF=$(readlink -f "$0" 2>/dev/null || echo "$0")
SCRIPT_DIR=$(dirname "$SELF")

SRC=""; ENVDIR=""; OUT=""; SKIP_NATIVE=0; DO_INSTALL=0; DO_CLEAN=0
NEXT=""
for a in "$@"; do
  case "$a" in
    --env) NEXT=ENV ;; --src) NEXT=SRC ;; --out) NEXT=OUT ;;
    --skip-native) SKIP_NATIVE=1 ;;
    --install) DO_INSTALL=1 ;;
    --clean) DO_CLEAN=1 ;;
    *) [ -n "$NEXT" ] || continue
       eval "$NEXT=\"\$a\""; NEXT="" ;;
  esac
done

PY=""
for c in python3 /data/user/0/com.deepseek.harness/files/payload/bin/python3 \
         /data/data/com.termux/files/usr/bin/python3; do
  command -v "$c" >/dev/null 2>&1 && { PY="$c"; break; }
done
[ -n "$PY" ] || { echo "FATAL: 找不到 python3"; exit 1; }

# ---------- 环境自动探测 ----------
# 依次尝试：--env > $PCENV_DIR > 脚本目录/env.sh > 沙箱私有目录下 *-env/env.sh
find_env() {
  for d in "$ENVDIR" "$PCENV_DIR" "$SCRIPT_DIR"; do
    [ -n "$d" ] && [ -f "$d/env.sh" ] && { echo "$d"; return 0; }
  done
  for f in /data/user/0/com.deepseek.harness/files/*-env/env.sh \
           /data/user/0/com.deepseek.harness/files/*pcenv*/env.sh; do
    [ -f "$f" ] && { dirname "$f"; return 0; }
  done
  return 1
}
ENVDIR=$(find_env) || { echo "FATAL: 找不到已部署环境（先跑 setup-pixelclean-env.sh，或 --env 指定）"; exit 1; }

[ -n "$SRC" ] || SRC="$SCRIPT_DIR/PixelClean"
[ -d "$SRC/MyApplication" ] || { echo "FATAL: 源码目录不对: $SRC"; exit 1; }
[ -n "$OUT" ] || OUT="$ENVDIR/build"

# 关键：中和宿主预置 LD_LIBRARY_PATH（坑 #22，payload 旧 libc++ 会劫持 Termux 工具）
unset LD_LIBRARY_PATH
# shellcheck disable=SC1091
. "$ENVDIR/env.sh"          # 提供 R/TCROOT/JAVA_HOME/ANDROID_JAR/GLSLANG/PATH
TCR="$TCROOT"
JAR="$ANDROID_JAR"
KLIB="$TCR/opt/kotlin/lib"
[ -f "$JAR" ] || { echo "FATAL: android.jar 缺失（重跑 setup 脚本）"; exit 1; }
for t in clang cmake make aapt2 d8 apksigner kotlinc2 glslangValidator patchelf; do
  command -v "$t" >/dev/null 2>&1 || { echo "FATAL: 工具缺失: $t"; exit 1; }
done

SRC="$SRC" OUT="$OUT" DO_CLEAN="$DO_CLEAN" "$PY" - <<'PYEOF'
import os, shutil, sys
SRC=os.environ['SRC']; OUT=os.environ['OUT']
if os.environ.get('DO_CLEAN')=='1' and os.path.isdir(OUT):
    shutil.rmtree(OUT); print('cleaned', OUT)
# 源码树拷进 ext4 构建目录再打补丁（fuse 上不能建符号链接目标执行，且避免污染原树）
WS=os.path.join(OUT,'src')
os.makedirs(OUT, exist_ok=True)
if not os.path.exists(os.path.join(WS,'CMakeLists.txt')):
    print('copying source tree (105MB) ...', flush=True)
    shutil.copytree(SRC, WS, symlinks=True, dirs_exist_ok=True)
    print('source copied to', WS)
PYEOF

WS="$OUT/src"
BUILD="$OUT/build_android"
SPV="$OUT/android_shaders"
APKW="$OUT/apkwork"
LOG="$OUT/build.log"
say() { echo "[$(date +%H:%M:%S)] $*" | tee -a "$LOG"; }

say "=== PixelClean 编译开始 ==="
say "env=$ENVDIR  src=$WS"

# ============================================================================
# 阶段 1：源码补丁（幂等，见经验文档第 3 节）
# ============================================================================
say "[1/6] 源码补丁（大小写符号链接 / MemoryPool / vma 头）..."
WS="$WS" TCR="$TCR" "$PY" - <<'PYEOF'
import os
WS=os.environ['WS']; TCR=os.environ['TCR']

# 3.1 大小写 include：建符号链接（Windows 掩盖的问题）
links = [
    ('Vulkan/window.h', 'Vulkan/Window.h'),           # include "window.h"（同目录，Windows 大小写不敏感）
    ('VulKan', 'Vulkan'),                             # include "../VulKan/..."
    ('VulKanTool', 'VulkanTool'),                     # include "../VulKanTool/..."
    ('ImGui', 'Environment/imgui'),                   # include "../ImGui/..."
]
for ln, target in links:
    p = os.path.join(WS, ln)
    tgt_abs = os.path.join(WS, target)
    if os.path.islink(p):
        if os.path.exists(p): continue               # 已有且可解析，跳过
        os.remove(p)                                  # 坏链（指向错误）→ 重建
    elif os.path.exists(p):
        continue                                      # 真实文件/目录，不动
    os.makedirs(os.path.dirname(p), exist_ok=True)
    rel = os.path.relpath(tgt_abs, os.path.dirname(p))
    os.symlink(rel, p); print('  symlink', ln, '->', rel)

# 3.2 MemoryPool.h: memoryPool.freeSlots -> freeSlots_（clang 严格，MSVC permissive）
mp = os.path.join(WS, 'Tool/MemoryPool.h')
d = open(mp, encoding='utf-8', errors='replace').read()
import re
fixed = re.sub(r'memoryPool\.freeSlots(?!_)', 'memoryPool.freeSlots_', d)
if fixed != d:
    open(mp, 'w', encoding='utf-8').write(fixed); print('  MemoryPool.h patched')

# 3.3 vma 头（.gitignore 漏掉的 vendored 依赖）——从部署环境借
vma_src = os.path.join(TCR, 'include/vma/vk_mem_alloc.h')
vma_dst = os.path.join(WS, 'Environment/vma/vk_mem_alloc.h')
if not os.path.exists(vma_dst):
    if os.path.exists(vma_src):
        os.makedirs(os.path.dirname(vma_dst), exist_ok=True)
        import shutil; shutil.copy2(vma_src, vma_dst); print('  vma header copied from env')
    else:
        raise SystemExit('FATAL: vk_mem_alloc.h 既不在源码也不在环境（重跑 setup 脚本）')
PYEOF

# ============================================================================
# 阶段 2：CMake 交叉编译 libpixelclean.so（第 4 节）
# ============================================================================
if [ "$SKIP_NATIVE" = 1 ] && [ -f "$BUILD/libpixelclean.so" ]; then
  say "[2/6] 跳过 native（复用 $BUILD/libpixelclean.so）"
else
  say "[2/6] CMake configure + make -j8（约 3-4 分钟）..."
  mkdir -p "$BUILD"
  # PREFIX=$TCR：坑 #5（cmake Android 分支读 $ENV{PREFIX}/include/android/api-level.h）
  ( cd "$WS" && env -i PATH="$TCR/bin:/system/bin" PREFIX="$TCR" HOME="$OUT" \
      cmake -G "Unix Makefiles" \
        -B "$BUILD" -S "$WS/MyApplication/app/src/main/cpp" \
        -DCMAKE_TOOLCHAIN_FILE="$ENVDIR/android-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release ) >>"$LOG" 2>&1
  ( cd "$WS" && env -i PATH="$TCR/bin:/system/bin" PREFIX="$TCR" HOME="$OUT" \
      make -C "$BUILD" -j"$(getconf _NPROCESSORS_ONLN)" ) >>"$LOG" 2>&1 \
    || { say "FATAL: native 编译失败，见 $LOG 尾部"; tail -30 "$LOG"; exit 1; }
  ls -la "$BUILD/libpixelclean.so" | awk '{print "  libpixelclean.so "$5" bytes"}'
fi

# ============================================================================
# 阶段 3：着色器 GLSL→SPIR-V（第 5 节：compile.bat 动态移植，新增条目自动跟随）
# ============================================================================
say "[3/6] 编译着色器（解析 shaders/compile.bat）..."
mkdir -p "$SPV"
SRC="$WS" SPV="$SPV" GLSLANG="$GLSLANG" "$PY" - <<'PYEOF'
import os, re, subprocess
SRC=os.environ['SRC']; SPV=os.environ['SPV']; G=os.environ['GLSLANG']
bat=open(os.path.join(SRC,'shaders/compile.bat'), encoding='utf-8', errors='replace').read()
pat=re.compile(r'"%GLSLANG%"\s+-V\s+"%SCRIPT_DIR%([^"]+)"\s+-o\s+"%OUTPUT_DIR%/([^"]+)"')
jobs=pat.findall(bat)
assert jobs, 'compile.bat 解析结果为空'
ok=fail=0
for src,out in jobs:
    r=subprocess.run([G,'-V',os.path.join(SRC,'shaders',src),'-o',os.path.join(SPV,out)],
                     capture_output=True)
    if r.returncode==0: ok+=1
    else: fail+=1; print('  SHADER FAIL', src, r.stderr.decode()[:200])
print(f'  shaders: {ok} ok, {fail} fail, total {len(jobs)}')
assert fail==0
PYEOF

# ============================================================================
# 阶段 4：Kotlin → dex（第 6.2 步骤 1-2）
# ============================================================================
say "[4/6] kotlinc2 + d8 ..."
mkdir -p "$APKW/classes" "$OUT/dexout"
cat > "$OUT/BuildConfig.kt" <<'EOF'
package com.pixelclean
object BuildConfig {
    const val BUILD_TYPE: String = "Release"
    const val FLAVOR: String = ""
    const val LIBRARY_PACKAGE_NAME: String = "com.pixelclean"
    const val APPLICATION_ID: String = "com.pixelclean"
    const val VERSION_CODE: Int = 1
    const val VERSION_NAME: String = "1.0"
    const val DEBUG: Boolean = false
}
EOF
kotlinc2 -nowarn -cp "$JAR" -jvm-target 17 \
  "$OUT/BuildConfig.kt" "$WS/MyApplication/app/src/main/java/com/pixelclean/MainActivity.kt" \
  -d "$APKW/classes" 2>&1 | grep -v -i jansi | tail -3
CLASSES=$(find "$APKW/classes" -name '*.class' | tr '\n' ' ')
d8 --release --min-api 26 --lib "$JAR" --classpath "$KLIB/kotlin-stdlib.jar" \
   --output "$OUT/dexout" $CLASSES "$KLIB/kotlin-stdlib.jar" 2>&1 | tail -2
[ -f "$OUT/dexout/classes.dex" ] || { say "FATAL: classes.dex 未产出"; exit 1; }
ls -la "$OUT/dexout/classes.dex" | awk '{print "  classes.dex "$5" bytes"}'

# ============================================================================
# 阶段 5：打包（第 6.1 布局 + 第 12 节 Manifest 修正 + 第 13 节 cp437 回转）
# ============================================================================
say "[5/6] aapt2 打包 + 资源/图标/Manifest 修正 ..."
rm -rf "$APKW/res" "$APKW/assets" "$OUT/compiled"
mkdir -p "$APKW/res" "$APKW/assets" "$OUT/compiled"

# 5a) 真图标（坑 #16：不要用占位纯色图）
cp -r "$WS/MyApplication/app/src/main/res/mipmap-hdpi" \
      "$WS/MyApplication/app/src/main/res/mipmap-mdpi" \
      "$WS/MyApplication/app/src/main/res/mipmap-xhdpi" \
      "$WS/MyApplication/app/src/main/res/mipmap-xxhdpi" \
      "$WS/MyApplication/app/src/main/res/mipmap-xxxhdpi" "$APKW/res/"

# 5b) Manifest（坑 #15：uses-sdk/versionCode/roundIcon/resizeableActivity 一个不能少）
cat > "$APKW/AndroidManifest.xml" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.pixelclean"
    android:versionCode="1" android:versionName="1.0">
    <uses-sdk android:minSdkVersion="26" android:targetSdkVersion="36"/>
    <uses-feature android:name="android.hardware.vulkan.level" android:version="1" android:required="false"/>
    <uses-feature android:name="android.hardware.vulkan.compute" android:required="false"/>
    <uses-permission android:name="android.permission.INTERNET"/>
    <application android:allowBackup="true" android:icon="@mipmap/ic_launcher"
        android:roundIcon="@mipmap/ic_launcher_round" android:label="PixelClean"
        android:theme="@android:style/Theme.Material.NoActionBar" android:hasCode="true"
        android:extractNativeLibs="true" android:resizeableActivity="true">
        <activity android:name="com.pixelclean.MainActivity" android:launchMode="singleTask"
            android:configChanges="orientation|keyboardHidden|screenSize|screenLayout"
            android:exported="true" android:screenOrientation="landscape"
            android:resizeableActivity="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
    </application>
</manifest>
EOF

# 5c) assets（对齐 gradle prepareAssets：本仓库无 SoundEffect/Resources 与 InterfaceUI/*.ttf，跳过）
cp "$WS/Info.ini" "$WS/PhysicsBlock.ini" "$APKW/assets/"
cp -r "$WS/Resource" "$APKW/assets/Resource"
cp -r "$SPV" "$APKW/assets/shaders"
mkdir -p "$APKW/assets/BlockS"
cp -r "$WS/BlockS/Pixel图片" "$APKW/assets/BlockS/Pixel图片"

# 5d) aapt2 compile + link
aapt2 compile --dir "$APKW/res" -o "$OUT/compiled/res.zip"
aapt2 link -o "$OUT/compiled/base.apk" -I "$JAR" \
  --manifest "$APKW/AndroidManifest.xml" "$OUT/compiled/res.zip" -A "$APKW/assets"

# 5e) 追加 dex/lib + cp437 文件名回转（坑 #18，link 后、签名前）
OUT="$OUT" TCR="$TCR" "$PY" - <<'PYEOF'
import os, zipfile
OUT=os.environ['OUT']
APKW=os.path.join(OUT,'apkwork')
zin=zipfile.ZipFile(os.path.join(OUT,'compiled/base.apk'))
tmp=os.path.join(OUT,'PixelClean-stage.apk')
zout=zipfile.ZipFile(tmp,'w',zipfile.ZIP_DEFLATED)
fixed=0
for item in zin.infolist():
    name=item.filename
    if any(ord(c)>127 for c in name):
        try:
            r=name.encode('cp437').decode('utf-8')   # 还原 aapt2 cp437 转写的真名
            if r!=name: name=r; fixed+=1
        except Exception: pass
    zi=zipfile.ZipInfo(name, date_time=item.date_time)
    zi.compress_type=item.compress_type; zi.external_attr=item.external_attr
    zout.writestr(zi, zin.read(item.filename))
zin.close()
zout.write(os.path.join(OUT,'dexout/classes.dex'), 'classes.dex')
libdir=os.path.join(APKW,'lib/arm64-v8a'); os.makedirs(libdir, exist_ok=True)
# 引擎主库 + libc++_shared（Termux 版，随包分发）
import shutil
shutil.copy(os.path.join(OUT,'build_android/libpixelclean.so'), os.path.join(libdir,'libpixelclean.so'))
cxx=os.environ['TCR']+'/lib/libc++_shared.so'
if os.path.exists(cxx): shutil.copy(cxx, os.path.join(libdir,'libc++_shared.so'))
for so in os.listdir(libdir):
    zout.write(os.path.join(libdir,so), f'lib/arm64-v8a/{so}')
zout.close()
print(f'  name-fixed entries: {fixed}; staged apk: {os.path.getsize(tmp)} bytes')
PYEOF

# ============================================================================
# 阶段 6：签名 + 校验
# ============================================================================
say "[6/6] apksigner 签名 + 校验..."
KS="$ENVDIR/debug.keystore"
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass android -alias androiddebugkey \
          -keyalg RSA -keysize 2048 -dname "CN=PixelClean" -validity 10000 2>/dev/null
fi
mkdir -p "$OUT/out"
apksigner sign --ks "$KS" --ks-pass pass:android --key-pass pass:android \
  --out "$OUT/out/PixelClean-signed.apk" "$OUT/PixelClean-stage.apk" 2>&1 | grep -v -i "tempdir\|WARNING" || true
apksigner verify --verbose "$OUT/out/PixelClean-signed.apk" 2>&1 | grep -v -i "tempdir\|WARNING" | head -4

# 终检：非 ASCII 名无乱码 + badging 图标 + 体积
OUT="$OUT" "$PY" - <<'PYEOF'
import os, zipfile
apk=os.path.join(os.environ['OUT'],'out/PixelClean-signed.apk')
z=zipfile.ZipFile(apk)
bad=[i.filename for i in z.infolist() if any(ord(c)>127 for c in i.filename)
     and ('\xcf\x83' in i.filename or 'σ' in i.filename)]
assert not bad, f'mojibake remains: {bad}'
names=[i.filename for i in z.infolist()]
assert 'assets/Resource/Music/夜に駆ける.mid' in names, 'MIDI name wrong!'
assert 'classes.dex' in names and any(n.startswith('lib/arm64-v8a/') for n in names)
print(f'  FINAL OK: {os.path.getsize(apk)/1e6:.1f} MB, {len(names)} entries, names clean')
PYEOF
aapt2 dump badging "$OUT/out/PixelClean-signed.apk" 2>/dev/null | grep -E "^application-icon|^targetSdk" | head -7

# 复制一份到脚本目录方便取用
cp "$OUT/out/PixelClean-signed.apk" "$SCRIPT_DIR/PixelClean-signed.apk"
say "=== 编译完成 ==="
say "APK: $OUT/out/PixelClean-signed.apk（另存一份: $SCRIPT_DIR/PixelClean-signed.apk）"

if [ "$DO_INSTALL" = 1 ]; then
  cp "$OUT/out/PixelClean-signed.apk" /storage/emulated/0/DSH/PixelClean-latest.apk 2>/dev/null || true
  say "安装（需 shell 特权通道，如 Shizuku/root）："
  say "  cp /storage/emulated/0/DSH/PixelClean-latest.apk /data/local/tmp/ && pm install -r -t /data/local/tmp/PixelClean-latest.apk"
  command -v pm >/dev/null 2>&1 && pm install -r -t "$OUT/out/PixelClean-signed.apk" 2>/dev/null \
    && say "pm install 成功" || true
fi
