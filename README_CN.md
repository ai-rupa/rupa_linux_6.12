# Rupa Linux 6.12 SDK — 开源精简版（Source-only Edition）

> **English version: [README.md](README.md).**

这是 **Rupa Linux 6.12 SDK** 的源码发布版，面向 Rockchip **RK3572 / RK3576** 平台。

内核、U-Boot、驱动、上层应用与构建系统的源码全部完整保留，同时保留了编译所需的 GNU
交叉工具链。第三方预编译二进制、厂商闭源运行库、官方 PDF 文档与 Windows 工具包
**不在本仓库中再分发**。完整清单见
[`opensource-dropped-files.tsv`](opensource-dropped-files.tsv)，恢复方式见
[`download_prebuilts.sh`](download_prebuilts.sh)。

> 原厂 SDK 说明文档保留在 [`device/rockchip/common/README.md`](device/rockchip/common/README.md)；
> 仓库根目录的 `README.md` 原本是指向它的符号链接。

---

## 1. 这个仓库是什么

仓库保留内容：

| 类别 | 说明 |
|---|---|
| Linux 内核源码 | `kernel-6.12/`（含 Rockchip BSP 补丁） |
| U-Boot 引导 | `u-boot/` |
| 设备配置与构建脚本 | `device/rockchip/`、`build.sh`、`Makefile`、`rkflash.sh` |
| 上层应用与库源码 | `app/`、`external/` 中的源码部分 |
| Rootfs 构建 | `buildroot/`、`debian/`（源码与配方） |
| 交叉编译工具链 | `prebuilts/gcc/linux-x86/`（ARM GNU Toolchain 13.3 / 14.3，aarch64 与 arm 两套，完整可用） |
| 打包与签名工具 | `rkbin/tools/`、`tools/linux/rk_sign_tool/`、`tools/linux/Linux_SecurityAVB/` |
| 构建辅助 | `device/rockchip/common/linux-kbuild/`、`device/rockchip/common/tools/x86_64/`（qemu-static） |

提交历史完整保留（`hugsun_linux_rkr1` 分支 28 个提交、`master` 分支 2 个提交）。

## 2. 这个仓库不是什么

**不是**厂商完整 SDK 的 1:1 副本。为了让仓库可以公开托管、并避免再分发第三方闭源产物，
下列内容已被剔除，合计 **925 个文件、约 7.68 GiB / 单个 checkout**：

| 类别 | 文件数 | 体积 | 典型内容 |
|---|---|---|---|
| 第三方预编译库（`external/`） | 634 | 5.91 GiB | libmali GPU 闭源库、OP-TEE/TA 预编译库、OpenCV 静态库、RKNN 工具链 wheel、NPU 示例模型 |
| 厂商工具包（`tools/`） | 27 | 558 MiB | Windows 端烧录/调试工具、PinDebug、DDR 工具 |
| 浏览器离线包（`debian/`） | 55 | 443 MiB | chromium `.deb` / `.tar.xz` |
| 官方文档（`docs/`） | 121 | 426 MiB | Rockchip 官方 PDF（TRM、Datasheet、开发指南） |
| 设备端预编译件（`device/`） | 50 | 216 MiB | 中文字体包、交叉 gdb / perf |
| Buildroot 下载缓存 | 15 | 75 MiB | font / libcamera 源码归档 |
| 应用资源（`app/`） | 14 | 36 MiB | 字体、模型、预编译 so |
| mbedTLS 测试向量（`u-boot/`） | 8 | 17 MiB | `test_suite_*.data` |
| 其它固件（`rkbin/`） | 1 | 2 MiB | 非本平台 trust 镜像 |

判定规则很简单，也便于复现：**体积 > 1 MiB 且不属于「源码/文本」或「编译必需」的文件**
一律剔除。完整清单见 `opensource-dropped-files.tsv`（含 sha256 与体积）。

> 保留了工具链（约 1.1 GiB）是一个有意的取舍：这样 clone 下来即可编译，不必先去官网下工具链。
> 如果你的使用场景更在意仓库体积，可以把 `prebuilts/gcc/` 也移除，改用
> [ARM GNU Toolchain 官方发布](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) 自行安装。

## 3. 快速开始

```bash
# 依赖（Ubuntu 22.04 / 24.04 主机）
sudo apt-get install -y build-essential git python3 python3-pip \
     bison flex bc libssl-dev libncurses-dev device-tree-compiler \
     u-boot-tools rsync cpio kmod

# 选择开发板配置
./build.sh lunch            # 交互式选择，或直接指定，例如：
./build.sh rk3576_defconfig

# 编译整包
./build.sh                  # 内核 + U-Boot + rootfs
./build.sh kernel           # 只编内核
./build.sh uboot            # 只编 U-Boot
```

产物位于 `rockdev/`（`boot.img`、`rootfs.img`、`update.img` 等）。

如果编译报错提示缺少文件，先运行依赖检查：

```bash
./download_prebuilts.sh --check
```

## 4. 补齐被剔除的第三方产物

```bash
./download_prebuilts.sh --list                          # 列出全部缺失文件
./download_prebuilts.sh --check                         # 检查当前工作区还缺哪些
./download_prebuilts.sh --restore --verify              # 从默认源恢复并校验 sha256
RK_PREBUILTS_URL=https://your-mirror/sdk-prebuilds \
  ./download_prebuilts.sh --restore --verify            # 指定镜像源
```

脚本按 `opensource-dropped-files.tsv` 的清单工作，恢复后会校验 sha256。

**这些产物的官方来源：**

| 内容 | 官方来源 |
|---|---|
| ARM GNU Toolchain（如不用仓库内置版本） | developer.arm.com/downloads/-/arm-gnu-toolchain-downloads |
| libmali GPU 用户态库 | Rockchip Linux SDK 官方发布包 |
| OP-TEE / TA 预编译库 | Rockchip Linux SDK 官方发布包 |
| RKNN Toolkit2 / RKNPU2 / RKNN Model Zoo | github.com/airockchip |
| Chromium arm64/armhf 包 | Rockchip Linux SDK 官方发布包 |
| Buildroot 源码归档 | buildroot.org（或 `make source` 自动拉取） |
| OpenCV 预编译库 | opencv.org |
| Rockchip 官方 PDF 文档 | Rockchip 官方 SDK 发布包 |

**许可证提示**：本仓库不包含、也不再分发上述第三方闭源产物。使用它们前请自行确认与
Rockchip / 各上游项目的授权条款。

## 5. 许可证与合规

- 本 SDK 基于 **Linux 内核（GPLv2）** 与 **U-Boot（GPLv2+）**，对外分发二进制固件时，
  **必须同时提供对应的完整源码**。这是许可证义务，不是可选项。
- 仓库内保留的交叉工具链来自 ARM GNU Toolchain（GCC 系列，GPL/LGPL），其许可文本随工具链
  目录一同保留（`prebuilts/gcc/**/share/licenses`、`COPYING*`）。
- 本仓库**不包含**任何 Rockchip 闭源二进制或用例文档，因此不构成对 Rockchip 授权条款的再分发。
- 若你在产品中使用了 Rockchip 的闭源库（libmali、NPU、OP-TEE 等），请与 Rockchip 确认授权范围。

## 6. 上游与致谢

| 组件 | 上游 |
|---|---|
| Linux Kernel | kernel.org |
| U-Boot | denx.de/u-boot |
| Buildroot | buildroot.org |
| RKNPU2 / RKNN Toolkit2 | github.com/airockchip |
| ARM GNU Toolchain | developer.arm.com |

## 7. 反馈

问题、补丁与建议请通过 Issue 或 Pull Request 提交。

---

*本文件由人工维护。若你发现清单与实际仓库不一致，欢迎提 Issue 说明。*
