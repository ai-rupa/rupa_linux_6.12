# libcamera 自定义 Pipeline 构建指南（Debian 13 / trixie）

本指南说明如何基于 Debian 13 (trixie) 维护并构建 libcamera 0.5.2 定制包。
通过叠加两个自定义 pipeline 补丁，使 `libcamerasrc`、cheese、Chromium
等上层应用能够访问 Debian 主线 pipeline handler 不识别的 V4L2 设备
（例如 ISP 或 CSI 接入的 sensor 节点）。

本文档不绑定具体 SoC 或板型，适用于任何基于 Debian 13 的 arm64 rootfs。

## 1. 源码来源与版本

| 项目 | 值 |
| --- | --- |
| 源码 | Debian trixie-backports（`apt-get source`） |
| 包名与版本 | `libcamera 0.5.2-3~bpo13+1` |
| 上游 tag | `v0.5.2`，<https://git.libcamera.org/libcamera/libcamera.git> |
| 目标架构 | arm64 |
| 构建环境 | Debian 13 (trixie) 或 bookworm chroot 叠加 trixie-backports 源 |

## 2. 补丁集

目录 `packages-patches/libcamera/` 维护以下补丁：

| 文件 | 作用 |
| --- | --- |
| `0003-HACK-v4l2-Bypass-API-version-check.patch` | 跳过 V4L2 驱动上报的 API 版本检查，以适配自定义版本号 |
| `0004-HACK-pipeline-Support-custom-pipeline.patch` | 新增 `src/libcamera/pipeline/custom/` 模块，通过 `LIBCAMERA_CUSTOM_DRIVERS`、`LIBCAMERA_CUSTOM_DEFAULT`、`LIBCAMERA_CUSTOM_FORMAT`、`LIBCAMERA_CUSTOM_BUF_CNT` 环境变量在运行期绑定任意 V4L2 设备 |

补丁文件头保留原始 `From`、`Date`、`Subject` 字段，便于使用 `git am`
回上游仓库溯源。

## 3. 构建流程

本节提供两条构建路径：

- 3.1 Host 机器上利用已有 chroot 构建，适用于具备镜像构建环境的主开发机。
- 3.2 目标板端原生构建后导出，适用于仅具备目标设备、缺少交叉构建环境的场景。

两条路径的核心步骤一致：

1. 使用 `apt-get source libcamera=0.5.2-3~bpo13+1` 获取源码。
2. 将 `packages-patches/libcamera/*.patch` 复制到 `debian/patches/`
   并追加至 `series`。
3. 使用 `dch --local` 追加本地版本号，避免被 Debian 官方同版本升级覆盖。
4. 使用 `mk-build-deps` 安装构建依赖。
5. 使用 `dpkg-buildpackage` 生成二进制包。
6. 从产物中挑选运行时包归入发行仓库目录。

本指南示例使用 `+rk1` 作为本地版本后缀，实际可替换为 `+<vendor>N`
任意前缀，遵循 `dch --local` 规则即可。

### 3.1 Host chroot 构建

#### 3.1.1 选择构建沙盒

镜像构建流程通常会生成 arm64 rootfs，可按下表优先级复用：

| 候选 | 来源 | 建议 |
| --- | --- | --- |
| rootfs tarball 解压目录 | `debootstrap` 或 `lb build` 产物 | 推荐 |
| live-build 中间 `chroot/` 目录 | `lb build` 未执行 `lb clean` 时保留 | 可用 |
| 镜像脚本第二阶段 `binary/` 目录 | 二次定制后的 rootfs | 不建议，每次 rerun 会被清除 |

推荐流程：解压 tarball 到独立沙盒。

```bash
cd <debian-rootfs-build-dir>
SANDBOX=/tmp/libcamera-chroot
sudo rm -rf $SANDBOX && sudo mkdir -p $SANDBOX
sudo tar -xpf <rootfs-tarball>.tar.gz -C $SANDBOX
```

若 tarball 不可用，可克隆已有 chroot：

```bash
sudo cp -a <existing-chroot-path> $SANDBOX
```

#### 3.1.2 挂载运行时并进入 chroot

```bash
sudo mount --bind /dev     $SANDBOX/dev
sudo mount --bind /dev/pts $SANDBOX/dev/pts
sudo mount --bind /proc    $SANDBOX/proc
sudo mount --bind /sys     $SANDBOX/sys

sudo mkdir -p $SANDBOX/rk-patches
sudo cp <repo-path>/packages-patches/libcamera/*.patch $SANDBOX/rk-patches/

sudo chroot $SANDBOX /bin/bash -l
```

进入 chroot 后应显式写入
公共 DNS：

```bash
cat >> /etc/resolv.conf <<'EOF'
nameserver 8.8.8.8
nameserver 114.114.114.114
EOF
```

#### 3.1.3 chroot 内构建

```bash
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y devscripts build-essential equivs dpkg-dev quilt fakeroot

grep -qr deb-src /etc/apt/sources.list* || \
  echo 'deb-src http://deb.debian.org/debian trixie-backports main' \
    > /etc/apt/sources.list.d/bpo-src.list
apt-get update

cd /tmp
apt-get source libcamera=0.5.2-3~bpo13+1
cd libcamera-0.5.2

cp /rk-patches/0003-HACK-v4l2-Bypass-API-version-check.patch \
   /rk-patches/0004-HACK-pipeline-Support-custom-pipeline.patch \
   debian/patches/
echo 0003-HACK-v4l2-Bypass-API-version-check.patch    >> debian/patches/series
echo 0004-HACK-pipeline-Support-custom-pipeline.patch >> debian/patches/series

dch --local "+rk" "Apply custom-pipeline HACK for downstream V4L2 drivers."

mk-build-deps -i -t 'apt-get -y --no-install-recommends' debian/control
DEB_BUILD_OPTIONS=nodoc dpkg-buildpackage -us -uc -b -j$(nproc)

ls /tmp/*.deb
exit
```

典型耗时：qemu 模拟下构建依赖安装约 5–10 分钟，`dpkg-buildpackage`
约 15–30 分钟；原生 arm64 主机两步合计约 5–8 分钟。

#### 3.1.4 归档运行时包

```bash
cp $SANDBOX/tmp/libcamera0.5_*_arm64.deb \
   $SANDBOX/tmp/libcamera-ipa_*_arm64.deb \
   $SANDBOX/tmp/gstreamer1.0-libcamera_*_arm64.deb \
   $SANDBOX/tmp/libcamera-v4l2_*_arm64.deb \
   $SANDBOX/tmp/libcamera-tools_*_arm64.deb \
   <debian-rootfs-build-dir>/packages/arm64/libcamera/
```

#### 3.1.5 卸载沙盒

```bash
sudo umount $SANDBOX/{sys,proc,dev/pts,dev}
sudo rm -rf $SANDBOX
```

### 3.2 板端构建

#### 3.2.1 前置条件

- 目标板已运行基于 Debian 13 的镜像，已安装 libcamera0.5 运行时。
- 已建立 SSH 或 adb 连接。
- 系统根分区剩余空间不少于 2 GB。

#### 3.2.2 安装构建环境

```bash
apt-get update
apt-get install -y devscripts build-essential dpkg-dev quilt fakeroot \
                   equivs ca-certificates

grep -qr deb-src /etc/apt/sources.list* || \
  echo 'deb-src http://deb.debian.org/debian trixie-backports main' \
    > /etc/apt/sources.list.d/bpo-src.list
apt-get update
```

#### 3.2.3 上传补丁

在 host 上执行：

```bash
scp <repo-path>/packages-patches/libcamera/*.patch root@<board-ip>:/root/
```

#### 3.2.4 板上构建

```bash
cd /root
apt-get source libcamera=0.5.2-3~bpo13+1
cd libcamera-0.5.2

cp /root/0003-HACK-v4l2-Bypass-API-version-check.patch \
   /root/0004-HACK-pipeline-Support-custom-pipeline.patch \
   debian/patches/
echo 0003-HACK-v4l2-Bypass-API-version-check.patch    >> debian/patches/series
echo 0004-HACK-pipeline-Support-custom-pipeline.patch >> debian/patches/series

dch --local "+rk" "Apply custom-pipeline HACK for downstream V4L2 drivers."

mk-build-deps -i -t 'apt-get -y --no-install-recommends' debian/control
DEB_BUILD_OPTIONS=nodoc dpkg-buildpackage -us -uc -b -j$(nproc)
```

典型耗时：4 核 arm64 设备原生构建约 8–15 分钟。

#### 3.2.5 导出产物

板上确认产物：

```bash
ls /root/*.deb
```

Host 端拉取：

```bash
mkdir -p /tmp/libcamera-out
scp root@<board-ip>:/root/libcamera0.5_*_arm64.deb \
    root@<board-ip>:/root/libcamera-ipa_*_arm64.deb \
    root@<board-ip>:/root/gstreamer1.0-libcamera_*_arm64.deb \
    root@<board-ip>:/root/libcamera-v4l2_*_arm64.deb \
    root@<board-ip>:/root/libcamera-tools_*_arm64.deb \
    /tmp/libcamera-out/
```

#### 3.2.6 归档与清理

归档至发行仓库：

```bash
cp /tmp/libcamera-out/*.deb <debian-rootfs-build-dir>/packages/arm64/libcamera/
```

板上清理（可选）：

```bash
rm -rf /root/libcamera-0.5.2 /root/*.deb /root/*.changes /root/*.buildinfo
apt-get autoremove --purge -y
```

### 3.3 产物校验

两种构建路径均应通过以下检查：

```bash
dpkg-deb -f libcamera0.5_*_arm64.deb Version       | grep '+rk1'
dpkg-deb -c libcamera0.5_*_arm64.deb               | grep custom
dpkg-deb -c libcamera-v4l2_*_arm64.deb             | grep v4l2-compat
dpkg-deb -c gstreamer1.0-libcamera_*_arm64.deb     | grep libgstlibcamera
dpkg-deb -f libcamera0.5_*_arm64.deb Depends
```

校验内容依次为：本地版本后缀、自定义 pipeline 已编入、V4L2 兼容层、
GStreamer 元素、依赖关系。任一检查失败时，应回到 quilt 与
`dpkg-buildpackage` 步骤，重点检查 `debian/patches/series` 是否正确
追加，以及是否执行过 `quilt refresh`。

## 4. 运行时最小包集

libcamera 源码包构建产出 14 个 binary deb。镜像通常仅保留下列 5 个
运行时包：

| 包 | 用途 |
| --- | --- |
| `libcamera0.5` | 核心运行时库 |
| `libcamera-ipa` | IPA 模块，带 ISP 的设备必需 |
| `gstreamer1.0-libcamera` | `libcamerasrc` GStreamer 元素，cheese 与 PipeWire 依赖 |
| `libcamera-v4l2` | V4L2 兼容层（`v4l2-compat.so`），供 Chromium、OpenCV、ffmpeg 等原生 V4L2 应用访问 libcamera pipeline |
| `libcamera-tools` | `cam`、`qcam` 命令行工具，用于现场诊断 |

其余包不建议合入 release 镜像：

- `libcamera-dev`：仅构建使用。
- `libcamera-doc`：文档，约 18 MB。
- `*-dbgsym`：调试符号。
- `python3-libcamera`：Python 绑定，一般无使用场景。

归档命令：

```bash
cp libcamera0.5_0.5.2-3~bpo13+1+rk1_arm64.deb \
   libcamera-ipa_0.5.2-3~bpo13+1+rk1_arm64.deb \
   gstreamer1.0-libcamera_0.5.2-3~bpo13+1+rk1_arm64.deb \
   libcamera-v4l2_0.5.2-3~bpo13+1+rk1_arm64.deb \
   libcamera-tools_0.5.2-3~bpo13+1+rk1_arm64.deb \
   <dist-repo>/packages/arm64/libcamera/
```

## 5. rootfs 集成

典型的 Debian 13 rootfs 构建脚本应完成以下操作：

- 在 chroot 内安装运行时最小包集：`apt-get install /packages/libcamera/*.deb`。
- 显式清理 base tarball 可能携带的旧版本：`apt-get purge -y libcamera0.4`。
  libcamera 0.5 的 `Breaks` 字段仅覆盖更早的 `libcamera0`，不自动替换 0.4。
- 使用 `apt-mark hold` 锁定上述 5 个包，避免 `dist-upgrade` 回退至官方版本。
- 将自定义 pipeline 环境变量写入 `/etc/profile.d/*.sh` 或
  `/etc/environment.d/*.conf`，以便桌面会话加载。

## 6. 版本升级流程

升级至新版本时，沿用 3.1 或 3.2 流程，区别仅在源码版本：

1. 使用 `apt-get source libcamera=<新版本>` 获取目标版本源码。
2. 将本指南所述补丁复制到 `debian/patches/`，追加至 `series`，
   使用 `quilt push -a` 验证。若上游改动 `v4l2_videodevice.cpp`、
   `device_enumerator.cpp` 或 `pipeline/` 目录导致 `push` 失败，
   手动修复后执行 `quilt refresh`，并将更新后的补丁回写至
   `packages-patches/libcamera/`。
3. 使用 `dch --local "+rk"` 递增本地版本号。
4. 执行 `dpkg-buildpackage -b` 构建，将 5 个运行时包替换至
   `packages/arm64/libcamera/`。
5. 重新构建 rootfs，刷机后验证：

   ```bash
   cam --list
   gst-launch-1.0 libcamerasrc ! videoconvert ! autovideosink
   cheese
   ```

## 7. 下游定制

- 修改 sensor 选择或 bypass 参数：在 rootfs overlay 中调整
  `LIBCAMERA_CUSTOM_*` 变量。
- 追加下游补丁：放入 `packages-patches/libcamera/`，同步更新
  `debian/patches/series`，递增本地版本号。
- 调试镜像：临时合入 `*-dbgsym` 包以携带调试符号，不应合入主线 release。
