#!/system/bin/sh
# ============================================================================
# PixelClean 手机本机编译环境 —— 一键部署脚本
#
# 部署目标：本脚本所在目录（可用 --prefix <dir> 覆盖）
# 产物布局（<DEPLOY> = 部署根目录）：
#   <DEPLOY>/probe/root/data/data/com.termux/files/usr/   # Termux 原生工具链前缀
#   <DEPLOY>/android-sdk/android-36/android.jar           # 平台存根 jar（d8/aapt2 link 用）
#   <DEPLOY>/stubs/                                        # （可选，留空说明见下）
#   <DEPLOY>/android-toolchain.cmake                       # CMake 交叉工具链文件
#   <DEPLOY>/env.sh                                        # 环境变量（source 后用）
#   <DEPLOY>/setup.log                                     # 部署日志
#
# 已验证设备：荣耀 PPG-AN00（arm64-v8a, Android 17）。任何 arm64 + Android 8+ 通用。
# 前置要求：
#   - 能跑本脚本的 sh（系统自带 /system/bin/sh 即可；DeepSeek Harness 沙箱内直接可用）
#   - python3 与 curl：优先用 payload 自带的
#     /data/user/0/com.deepseek.harness/files/payload/bin/python3
#     /data/user/0/com.deepseek.harness/files/payload/runtime/bin/curl
#     （没有 Termux App 也行；脚本会自检）
#   - 磁盘 ~2GB，网络可达清华 Termux 镜像与 dl.google.com
#
# 用法：
#   sh setup-pixelclean-env.sh                 # 部署到脚本所在目录
#   sh setup-pixelclean-env.sh --prefix /path  # 部署到指定目录
#   sh setup-pixelclean-env.sh --only-lite     # 只装打包链（aapt2/d8/apksigner/kotlin/jdk，跳过 clang 全家桶）
#   sh setup-pixelclean-env.sh --check         # 只校验已部署环境，不改动
#
# 完成后：
#   . <DEPLOY>/env.sh
#   然后按 PixelClean-手机编译完整经验.md 第 3~6 节执行（编译 .so / 着色器 / 打 APK）。
# ============================================================================
set -e

# ---------- 参数 ----------
PREFIX=""
ONLY_LITE=0
CHECK_ONLY=0
for a in "$@"; do
  case "$a" in
    --prefix) NEXT=1 ;;
    --only-lite) ONLY_LITE=1 ;;
    --check) CHECK_ONLY=1 ;;
    *) if [ "${NEXT:-0}" = 1 ]; then PREFIX="$a"; NEXT=0; fi ;;
  esac
done

# 默认部署到脚本所在目录
if [ -z "$PREFIX" ]; then
  SELF=$(readlink -f "$0" 2>/dev/null || echo "$0")
  PREFIX=$(dirname "$SELF")
fi
PREFIX=$(cd "$PREFIX" 2>/dev/null && pwd) || { echo "部署目录不存在: $PREFIX"; exit 1; }

TCR="$PREFIX/probe/root/data/data/com.termux/files/usr"
JVM="$TCR/lib/jvm/java-21-openjdk"
SDK="$PREFIX/android-sdk"
LOG="$PREFIX/setup.log"

# ---------- 工具自检 ----------
PY=""
for c in python3 /data/user/0/com.deepseek.harness/files/payload/bin/python3 \
         /data/data/com.termux/files/usr/bin/python3; do
  if command -v "$c" >/dev/null 2>&1; then PY="$c"; break; fi
done
[ -n "$PY" ] || { echo "FATAL: 找不到 python3（payload 或 Termux 任一即可）"; exit 1; }

CURL=""
for c in curl /data/user/0/com.deepseek.harness/files/payload/runtime/bin/curl \
         /data/data/com.termux/files/usr/bin/curl; do
  if command -v "$c" >/dev/null 2>&1; then CURL="$c"; break; fi
done
[ -n "$CURL" ] || { echo "FATAL: 找不到 curl"; exit 1; }

MIRROR="https://mirrors.tuna.tsinghua.edu.cn/termux/apt/termux-main"
IDX_URL="$MIRROR/dists/stable/main/binary-aarch64/Packages"
# Android 36 平台包（含 android.jar，~65MB）——腾讯云镜像实测可用，dl.google.com 兜底
PLATFORM_URLS="https://mirrors.cloud.tencent.com/AndroidSDK/platform-36_r02.zip https://dl.google.com/android/repository/platform-36_r02.zip"
# vk_mem_alloc.h 单头文件（工程 .gitignore 漏掉的 vendored 依赖），按顺序尝试
VMA_URLS="https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h https://ghproxy.net/https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h https://chromium.googlesource.com/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/+/refs/heads/main/include/vk_mem_alloc.h?format=TEXT"

say() { echo "[$(date +%H:%M:%S)] $*" | tee -a "$LOG"; }

# ---------- 预检：部署目录必须可执行 ELF ----------
# /storage/emulated/0（fuse/sdcardfs）是 noexec：解包出来的 clang/aapt2/java 全部无法运行。
# 工具链必须落在 app 私有 ext4 目录（DeepSeek Harness 沙箱 = /data/user/0/com.deepseek.harness/files/…；
# 真 Termux App = /data/data/com.termux/files/…）。脚本目录 noexec 时自动回退。
# 探针：toybox 按 argv[0] 的 basename 分发命令，所以复制为名为 true 的文件再执行。
mkdir -p "$PREFIX" 2>/dev/null || true
exec_ok() {
  rm -rf "$1/.exeprobe"; mkdir -p "$1/.exeprobe" 2>/dev/null || { rm -rf "$1/.exeprobe"; return 1; }
  cp /system/bin/toybox "$1/.exeprobe/true" 2>/dev/null && chmod 700 "$1/.exeprobe/true" 2>/dev/null
  "$1/.exeprobe/true" 2>/dev/null; local rc=$?
  rm -rf "$1/.exeprobe"; return $rc
}
if ! exec_ok "$PREFIX"; then
  FALLBACK="/data/user/0/com.deepseek.harness/files/$(basename "$PREFIX")-env"
  if exec_ok "$FALLBACK" 2>/dev/null || { mkdir -p "$FALLBACK" 2>/dev/null && exec_ok "$FALLBACK"; }; then
    say "WARN: 目标目录 noexec（共享存储 fuse），工具链无法从那里执行:"
    say "        $PREFIX"
    say "WARN: 自动回退部署到 app 私有 ext4: $FALLBACK"
    PREFIX="$FALLBACK"
    TCR="$PREFIX/probe/root/data/data/com.termux/files/usr"
    JVM="$TCR/lib/jvm/java-21-openjdk"
    SDK="$PREFIX/android-sdk"
    LOG="$PREFIX/setup.log"
  else
    echo "FATAL: 目标目录与回退目录都不可执行二进制。请手工指定 ext4 目录:"
    echo "       sh $0 --prefix /data/user/0/<宿主app>/files/pcenv"
    exit 1
  fi
fi

# ---------- 校验模式 ----------
check_env() {
  ok=1
  if [ "$ONLY_LITE" = 1 ]; then
    CORE_BINS="aapt2 bash patchelf"
  else
    CORE_BINS="clang clang++ cmake make aapt2 glslangValidator patchelf bash"
  fi
  for t in $CORE_BINS; do
    [ -x "$TCR/bin/$t" ] || { say "MISSING bin/$t"; ok=0; }
  done
  [ -f "$TCR/share/java/d8.jar" ] || { say "MISSING d8.jar"; ok=0; }
  [ -f "$TCR/share/java/apksigner.jar" ] || { say "MISSING apksigner.jar"; ok=0; }
  [ -x "$JVM/bin/java" ] || { say "MISSING java"; ok=0; }
  [ -x "$TCR/bin/kotlinc2" ] || { say "MISSING kotlinc2 wrapper"; ok=0; }
  if [ "$ONLY_LITE" != 1 ]; then
    [ -f "$TCR/include/jni.h" ] || { say "MISSING ndk-sysroot headers"; ok=0; }
    [ -f "$TCR/include/vulkan/vulkan.h" ] || { say "MISSING vulkan headers"; ok=0; }
    [ -f "$TCR/include/android/api-level.h" ] || { say "MISSING api-level.h (PREFIX 推导依赖)"; ok=0; }
    for l in liblog libvulkan libandroid libOpenSLES libc++; do
      ls "$TCR/lib/$l"*.so* >/dev/null 2>&1 || { say "MISSING stub $l.so"; ok=0; }
    done
    # shell 路径补丁检查（cmake/make 内烧的 SHELL 路径已替换且等长）
    "$PY" - "$TCR" <<'PYEOF' || { say "shell-path patch missing"; ok=0; }
import sys
R=sys.argv[1]
for f in [R+'/bin/cmake', R+'/bin/make']:
    d=open(f,'rb').read()
    assert b'/system/bin/sh' in d, f
PYEOF
  fi
  [ -f "$SDK/android-36/android.jar" ] || { say "MISSING android.jar"; ok=0; }
  [ $ok = 1 ] && say "CHECK: 环境完整 ✔  用 . $PREFIX/env.sh 激活" || say "CHECK: 有缺项（见上），重跑本脚本补齐"
  return $((1-ok))
}

if [ "$CHECK_ONLY" = 1 ]; then check_env; exit $?; fi

say "=== PixelClean 编译环境部署开始 ==="
say "部署根目录: $PREFIX"
mkdir -p "$PREFIX/probe/debs" "$SDK" "$PREFIX/gen"

# ============================================================================
# 阶段 1：下载 Termux deb 依赖闭包并解包（Python 单文件逻辑）
# ============================================================================
TOPLEVEL_FULL="clang cmake make ndk-sysroot libandroid-stub vulkan-headers glslang spirv-tools shaderc glm aapt2 d8 apksigner openjdk-21 kotlin bash patchelf curl coreutils tar xz-utils sed grep diffutils findutils procps which"
TOPLEVEL_LITE="aapt2 d8 apksigner openjdk-21 kotlin bash patchelf curl coreutils tar xz-utils"
if [ "$ONLY_LITE" = 1 ]; then TOPLEVEL="$TOPLEVEL_LITE"; else TOPLEVEL="$TOPLEVEL_FULL"; fi

TOPLEVEL="$TOPLEVEL" TCR="$TCR" PREFIX="$PREFIX" MIRROR="$MIRROR" IDX_URL="$IDX_URL" CURLBIN="$CURL" VMA_URLS="$VMA_URLS" "$PY" - <<'PYEOF'
import os, re, sys, subprocess, tarfile, io

PREFIX  = os.environ['PREFIX']
TCR     = os.environ['TCR']
MIRROR  = os.environ['MIRROR']
CURL    = os.environ['CURLBIN']
DEBS    = os.path.join(PREFIX, 'probe', 'debs')
ROOT    = os.path.join(PREFIX, 'probe', 'root')
IDX     = os.path.join(PREFIX, 'probe', 'Packages')
toplevel = os.environ['TOPLEVEL'].split()

# 1a) 拉仓库索引（9MB，断点续传 + Size 校验，防并行截断坑 #11）
def fetch(url, dst, want=None):
    for _ in range(5):
        if os.path.exists(dst) and (want is None or os.path.getsize(dst) == want):
            return True
        subprocess.run([CURL, '-sL', '-C', '-', '--max-time', '600', '-o', dst, url])
        if want and os.path.exists(dst) and os.path.getsize(dst) != want:
            print('retry size mismatch', dst, flush=True); continue
        return os.path.exists(dst)
    return False

if not os.path.exists(IDX) or os.path.getsize(IDX) < 1_000_000:
    print('DL Packages index ...', flush=True)
    assert fetch(os.environ['IDX_URL'], IDX), 'index download failed'

blocks = {}; cur = None
for line in open(IDX, encoding='utf-8', errors='replace'):
    if line.startswith('Package: '):
        cur = line[9:].strip(); blocks[cur] = {'deps': [], 'file': None, 'size': None, 'ver': None}
    elif cur and line.startswith('Depends:'):
        for d in line[8:].split(','):
            d = d.strip().split()[0].split('(')[0]
            if d: blocks[cur]['deps'].append(d)
    elif cur and line.startswith('Filename:'): blocks[cur]['file'] = line[9:].strip()
    elif cur and line.startswith('Size:'):     blocks[cur]['size'] = int(line[5:].strip())
    elif cur and line.startswith('Version:'):  blocks[cur]['ver'] = line[8:].strip()

# 系统自带库不装（Android linker 提供）
SKIP = {'libc', 'libm', 'libdl', 'libpthread', 'liblog', 'libandroid', 'libz',
        'termux-tools', 'termux-exec', 'termux-am', 'dialog', 'ca-certificates-java'}

# 已装标记（支持重跑/断点续装：sdcardfs 上部分 deb 文件名含 ':' 无法落盘，见 sanitize）
MARK = os.path.join(PREFIX, 'probe', 'installed.txt')
done = set()
if os.path.exists(MARK):
    done = set(x.strip() for x in open(MARK) if x.strip())

def sanitize(name):
    # /storage/emulated/0 (fuse) 禁止文件名含 ':'（epoch 版本号如 openssl_1:3.6.5.deb）
    return re.sub(r'[^A-Za-z0-9._+-]', '_', name)

def ar_data(debpath):
    """deb = ar 归档，取 data.tar.* 成员"""
    b = open(debpath, 'rb').read(); off = 8; parts = {}
    while off + 60 <= len(b):
        h = b[off:off+60]
        name = h[0:16].decode().strip().rstrip('/').rstrip('/')
        try: size = int(h[48:58].decode().strip())
        except ValueError: break
        parts[name] = b[off+60:off+60+size]
        off += 60 + size + (size % 2)
    for k, v in parts.items():
        if k.startswith('data.tar'): return v
    raise RuntimeError('no data.tar in ' + debpath)

installed = set(); failed = set()
def ensure(name, depth=0):
    if name in installed or name in SKIP: return
    p = blocks.get(name)
    if not p or not p['file']:
        print('  ' * depth + 'NOPE', name, flush=True); failed.add(name); return
    if name in done:
        installed.add(name)
        for d in p['deps']: ensure(d, depth + 1)
        return
    dst = os.path.join(DEBS, sanitize(os.path.basename(p['file'])))
    if not fetch(MIRROR + '/' + p['file'], dst, p['size']):
        print('  ' * depth + 'DLFAIL', name, flush=True); failed.add(name); return
    print('  ' * depth + 'EXT', name, p['ver'], flush=True)
    data = ar_data(dst)
    # 解到 Termux 布局前缀；放行符号链接（否则 AbsoluteLinkError）
    t = tarfile.open(fileobj=io.BytesIO(data))
    t.extractall(ROOT, filter=lambda ti, ar: ti)
    t.close()
    installed.add(name); done.add(name)
    with open(MARK, 'a') as f: f.write(name + '\n')
    for d in p['deps']: ensure(d, depth + 1)

for n in toplevel: ensure(n)
print(f'deb packages extracted: {len(installed)}' + (f'  FAILED: {sorted(failed)}' if failed else ''))
assert not failed, 'dependency download failed'

# 1b) vma 单头文件（工程 .gitignore 漏掉的 vendored 依赖）
vma_dir = os.path.join(TCR, 'include', 'vma')
vma = os.path.join(vma_dir, 'vk_mem_alloc.h')
if not os.path.exists(vma):
    os.makedirs(vma_dir, exist_ok=True)
    got = False
    for u in os.environ.get('VMA_URLS', '').split():
        tmp = vma + '.part'
        r = subprocess.run([CURL, '-sL', '--max-time', '300', '-o', tmp, u])
        if r.returncode == 0 and os.path.exists(tmp) and os.path.getsize(tmp) > 500_000:
            head = open(tmp, 'rb').read(4096)
            if b'?format=TEXT' not in u.encode() and b'<?xml' in head[:200]:
                continue
            if b'?format=TEXT' in u.encode():   # googlesource 返回 base64
                import base64
                open(tmp, 'wb').write(base64.b64decode(open(tmp, 'rb').read()))
            os.replace(tmp, vma); got = True
            print('vma header OK from', u.split('/')[2], flush=True); break
        if os.path.exists(tmp): os.remove(tmp)
    if not got: print('WARN: vk_mem_alloc.h 下载失败（编译前需手工放置）', flush=True)
PYEOF

# ============================================================================
# 阶段 1.5：Android 系统 stub 库（liblog/libvulkan/…）
# 工程 CMakeLists 用 find_library(vulkan/log/android) 在 sysroot 里找这些库；
# Termux 前缀只有 libandroid-stub deb 带的 4 个，其余从 /system/lib64 复制真实
# 库进来（链接期只读符号表，运行期由设备 linker 按名解析，天然正确）。
# ============================================================================
say "安装 Android 系统 stub 库..."
for l in liblog libvulkan libandroid libOpenSLES libEGL libGLESv2 libaaudio libmediandk \
         libjnigraphics libnativewindow libsync libcamera2ndk; do
  if [ ! -f "$TCR/lib/$l.so" ]; then
    cp "/system/lib64/$l.so" "$TCR/lib/$l.so" 2>/dev/null || say "WARN: /system/lib64/$l.so 不存在"
  fi
done

# ============================================================================
# 阶段 2：RUNPATH 烧录（坑 #2/#13：不能全局 LD_LIBRARY_PATH，patchelf 根治）
# 自举：patchelf 自己也依赖 $TCR/lib 的 libc++，所以仅这一步给 patchelf 子进程
# 带 LD_LIBRARY_PATH（不导出给全局，不污染 toybox），烧完后所有工具脱离 env 可跑。
# ============================================================================
say "patchelf RUNPATH 烧录中（约 1-3 分钟）..."
TCR="$TCR" "$PY" - <<'PYEOF'
import os, subprocess, sys
R = os.environ['TCR']
PATCHELF = os.path.join(R, 'bin', 'patchelf')
assert os.path.exists(PATCHELF), 'patchelf 未解包？'
BOOTENV = dict(os.environ, LD_LIBRARY_PATH=os.path.join(R, 'lib'))  # 仅 patchelf 自举用

def is_elf(p):
    try:
        with open(p, 'rb') as f: return f.read(4) == b'\x7fELF'
    except OSError: return False

J = os.path.join(R, 'lib', 'jvm', 'java-21-openjdk')
RPATH_J = f"{R}/lib:{J}/lib:{J}/lib/server"   # 坑 #2：JVM 库互相引用，RUNPATH 要写全
n = 0; errs = 0
for base, dirs, files in os.walk(R):
    for fn in files:
        p = os.path.join(base, fn)
        if not is_elf(p): continue
        rpath = RPATH_J if (p.startswith(J) or os.path.basename(p) in
                            ('java', 'keytool', 'jlink', 'jarsigner')) else f"{R}/lib"
        r = subprocess.run([PATCHELF, '--set-rpath', rpath, p],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=BOOTENV)
        n += 1; errs += (r.returncode != 0)
print(f'RUNPATH burned into {n} ELF files ({errs} errors)')
assert errs < n // 4, 'patchelf 大面积失败，检查 bootstrap 环境'
PYEOF

# ============================================================================
# 阶段 3：make/cmake 硬编码 shell 路径等长替换（坑 #3/#13）
# 二进制里有两个字符串：/data/data/com.termux/files/usr/bin/sh(38B) 与
# .../bin/bash(40B)，都要等长替换为 /system/bin/sh + \x00 填充。
# ============================================================================
TCR="$TCR" "$PY" - <<'PYEOF'
import os
R = os.environ['TCR']
NEW = b'/system/bin/sh'
OLDS = [b'/data/data/com.termux/files/usr/bin/sh',
        b'/data/data/com.termux/files/usr/bin/bash']
for f in ['bin/cmake', 'bin/make']:
    p = os.path.join(R, f)
    d = open(p, 'rb').read()
    total = 0
    for OLD in OLDS:
        PAD = NEW + b'\x00' * (len(OLD) - len(NEW))   # 等长，\x00 填充（坑 #13）
        c = d.count(OLD)
        if c:
            d = d.replace(OLD, PAD); total += c
    os.remove(p) if os.path.exists(p) else None
    open(p, 'wb').write(d)
    os.chmod(p, 0o755)                                 # 坑 #13：写回丢执行位，补 chmod +x
    print(f'{f}: patched {total} occurrence(s)')
PYEOF

# ============================================================================
# 阶段 4：包装脚本 shebang + kotlinc2/d8/apksigner wrapper（坑 #6/#7/#17）
# ============================================================================
BASH="$TCR/bin/bash"   # 解包出来的真 bash（payload 的 bash 是 toybox sh 的伪装，不支持 =~）
[ -x "$BASH" ] || { say "FATAL: $TCR/bin/bash 不存在（bash deb 未装？）"; exit 1; }

# 4a) 所有 Termux bin 下的 shell 脚本 shebang 改指解包 bash
TCR="$TCR" BASH="$BASH" "$PY" - <<'PYEOF'
import os
R = os.environ['TCR']; BASH = os.environ['BASH']
n = 0
for fn in os.listdir(os.path.join(R, 'bin')):
    p = os.path.join(R, 'bin', fn)
    if os.path.islink(p) or not os.path.isfile(p): continue
    try: head = open(p, 'rb').read(80)
    except OSError: continue
    if not head.startswith(b'#!'): continue
    first = head.split(b'\n', 1)[0]
    if b'/data/data/com.termux' in first or b'/usr/bin/env' in first:
        rest = open(p, 'rb').read()[len(first):]
        os.remove(p)
        with open(p, 'wb') as f: f.write(f'#!{BASH}'.encode() + rest)
        os.chmod(p, 0o755); n += 1
print(f'shebang rewritten: {n} scripts')
PYEOF

# 4b) kotlinc2 wrapper（官方 kotlinc 依赖 KOTLIN_HOME 推导，直接极简版；
#     JVM 的 java.io.tmpdir 默认指向不存在的 Termux 路径，jansi 解 .so 会失败，显式给 TMPDIR）
mkdir -p "$TCR/tmp"
cat > "$TCR/bin/kotlinc2" <<EOF
#!$BASH
R=$TCR
export TMPDIR="\$R/tmp"
exec \$R/lib/jvm/java-21-openjdk/bin/java -Djava.io.tmpdir="\$R/tmp" -cp \\
  "\$R/opt/kotlin/lib/kotlin-compiler.jar:\$R/opt/kotlin/lib/kotlin-stdlib.jar:\\
\$R/opt/kotlin/lib/kotlin-reflect.jar:\$R/opt/kotlin/lib/kotlin-script-runtime.jar" \\
  org.jetbrains.kotlin.cli.jvm.K2JVMCompiler "\$@"
EOF
chmod 755 "$TCR/bin/kotlinc2"

# 4c) d8 / apksigner wrapper：绝对 java 路径，不依赖 PATH 里的 java（坑 #17）
cat > "$TCR/bin/d8" <<EOF
#!$BASH
R=$TCR
export TMPDIR="\$R/tmp"
exec \$R/lib/jvm/java-21-openjdk/bin/java -Djava.io.tmpdir="\$R/tmp" -cp "\$R/share/java/d8.jar" com.android.tools.r8.D8 "\$@"
EOF
cat > "$TCR/bin/apksigner" <<EOF
#!$BASH
R=$TCR
export TMPDIR="\$R/tmp"
exec \$R/lib/jvm/java-21-openjdk/bin/java -Djava.io.tmpdir="\$R/tmp" -jar "\$R/share/java/apksigner.jar" "\$@"
EOF
chmod 755 "$TCR/bin/d8" "$TCR/bin/apksigner" "$TCR/bin/kotlinc2" 2>/dev/null || true

# ============================================================================
# 阶段 5：android.jar（平台存根，aapt2 link -I 与 kotlinc/d8 -cp 用）
# ============================================================================
if [ ! -f "$SDK/android-36/android.jar" ]; then
  say "下载 android-36 平台包（~65MB）..."
  OK=0
  for u in $PLATFORM_URLS; do
    if "$CURL" -sL -C - --max-time 1200 -o "$PREFIX/platform-36.zip" "$u" && \
       [ "$(stat -c%s "$PREFIX/platform-36.zip" 2>/dev/null || echo 0)" -gt 30000000 ]; then
      OK=1; say "平台包来源: $u"; break
    fi
  done
  [ "$OK" = 1 ] || { say "FATAL: android.jar 下载失败"; exit 1; }
  say "解包 android.jar ..."
  SDK="$SDK" PREFIX="$PREFIX" "$PY" - <<'PYEOF'
import os, zipfile
z = zipfile.ZipFile(os.environ['PREFIX'] + '/platform-36.zip')
dst = os.environ['SDK'] + '/android-36'
os.makedirs(dst, exist_ok=True)
for name in ['android.jar', 'build.prop']:
    for cand in z.namelist():
        if cand.endswith('/' + name):
            open(os.path.join(dst, name), 'wb').write(z.read(cand)); break
print('android.jar:', os.path.getsize(dst + '/android.jar'), 'bytes')
PYEOF
  rm -f "$PREFIX/platform-36.zip"
fi

# ============================================================================
# 阶段 6：生成 env.sh 与 android-toolchain.cmake（路径全部指向本次部署）
# ============================================================================
cat > "$PREFIX/env.sh" <<EOF
# PixelClean 编译环境（setup-pixelclean-env.sh 生成）
# 用法: . $PREFIX/env.sh
R="$TCR"
export TCROOT=\$R
export JAVA_HOME="\$R/lib/jvm/java-21-openjdk"
export ANDROID_JAR="$SDK/android-36/android.jar"
export GLSLANG="\$R/bin/glslangValidator"
# 关键：中和沙箱预置的 LD_LIBRARY_PATH（payload 的旧 libc++ 会劫持 Termux 工具，
# 报 __hash_memory 找不到）。RUNPATH 已烧进每个 ELF，不需要 LD_LIBRARY_PATH。
unset LD_LIBRARY_PATH
export PATH="\$JAVA_HOME/bin:\$R/bin:\$PATH"
alias kotlinc=kotlinc2 2>/dev/null
EOF

cat > "$PREFIX/android-toolchain.cmake" <<EOF
# 由 setup-pixelclean-env.sh 生成 —— 指向本部署的 Termux 原生交叉前缀
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 26)
set(TCR $TCR)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_C_COMPILER   \${TCR}/bin/clang)
set(CMAKE_CXX_COMPILER \${TCR}/bin/clang++)
set(CMAKE_AR           \${TCR}/bin/llvm-ar)
set(CMAKE_RANLIB       \${TCR}/bin/llvm-ranlib)
set(CMAKE_STRIP        \${TCR}/bin/llvm-strip)
set(CMAKE_MAKE_PROGRAM \${TCR}/bin/make CACHE FILEPATH "")
set(TRIPLE aarch64-linux-android26)
set(CMAKE_C_FLAGS_INIT   "--target=\${TRIPLE} -fPIC")
set(CMAKE_CXX_FLAGS_INIT "--target=\${TRIPLE} -fPIC")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-Wl,--sysroot=\${TCR}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--sysroot=\${TCR}")
set(CMAKE_FIND_ROOT_PATH \${TCR})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_SYSTEM_INCLUDE_PATH \${TCR}/include)
set(CMAKE_SYSTEM_LIBRARY_PATH \${TCR}/lib)
EOF

# ============================================================================
# 阶段 7：收尾校验
# ============================================================================
say "最终校验..."
check_env
say "=== 部署完成 ==="
say "激活环境:  . $PREFIX/env.sh"
say "交叉编译:  cmake -G \"Unix Makefiles\" -B build_android -S <工程>/MyApplication/app/src/main/cpp -DCMAKE_TOOLCHAIN_FILE=$PREFIX/android-toolchain.cmake -DCMAKE_BUILD_TYPE=Release && make -C build_android -j8"
say "后续步骤（着色器/打 APK 含 3 处必要修正）见 PixelClean-手机编译完整经验.md 第 5、6、12、13 节"
