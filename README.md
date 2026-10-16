# Rupa Linux 6.12 SDK — Source-only Edition

> **Simplified Chinese version: [README_CN.md](README_CN.md).**

A **source-only** edition of the Rupa Linux 6.12 SDK for Rockchip **RK3572 / RK3576** platforms.

All kernel, U-Boot, driver, application and build-system sources are included in full, together
with the prebuilt GNU cross toolchains required to compile them. Third-party prebuilt binaries,
vendor-proprietary runtime libraries, official PDF documents and Windows tool packages are
**not** redistributed here. See [`opensource-dropped-files.tsv`](opensource-dropped-files.tsv)
for the exact list, and [`download_prebuilts.sh`](download_prebuilts.sh) for how to restore them.

> The original vendor SDK readme is preserved at
> [`device/rockchip/common/README.md`](device/rockchip/common/README.md) — the root `README.md`
> used to be a symlink pointing to it.

---

## 1. What This Repository Is

This is the source-only release of the **Rupa Linux 6.12 SDK**, targeting **RK3572 / RK3576**
(Rockchip 6.12 kernel) platforms.

What is kept:

| Category | Contents |
|---|---|
| Linux kernel sources | `kernel-6.12/` (including Rockchip BSP patches) |
| U-Boot bootloader | `u-boot/` |
| Board configuration and build scripts | `device/rockchip/`, `build.sh`, `Makefile`, `rkflash.sh` |
| Application and library sources | `app/`, the source parts of `external/` |
| Root filesystem build | `buildroot/`, `debian/` (sources and recipes) |
| Cross-compilation toolchains | `prebuilts/gcc/linux-x86/` (ARM GNU Toolchain 13.3 / 14.3, both aarch64 and arm — complete and usable) |
| Packaging and signing tools | `rkbin/tools/`, `tools/linux/rk_sign_tool/`, `tools/linux/Linux_SecurityAVB/` |
| Build helpers | `device/rockchip/common/linux-kbuild/`, `device/rockchip/common/tools/x86_64/` (qemu-static) |

Commit history is preserved in full. `master` mirrors the latest released source-only snapshot,
and ongoing development takes place on the `rupa_opensource_rkr*` branches.

## 2. What This Repository Is Not

It is **not** a 1:1 copy of the vendor's complete SDK. So that the repository can be hosted
publicly, and to avoid redistributing third-party proprietary artifacts, the following content
has been removed — **925 files, about 7.68 GiB per checkout**:

| Category | Files | Size | Typical contents |
|---|---|---|---|
| Third-party prebuilt libraries (`external/`) | 634 | 5.91 GiB | libmali GPU blobs, OP-TEE/TA prebuilt libraries, OpenCV static libraries, RKNN toolkit wheels, NPU sample models |
| Vendor tool packages (`tools/`) | 27 | 558 MiB | Windows-side flashing/debug tools, PinDebug, DDR tools |
| Offline browser packages (`debian/`) | 55 | 443 MiB | chromium `.deb` / `.tar.xz` |
| Official documents (`docs/`) | 121 | 426 MiB | Rockchip official PDFs (TRM, datasheet, development guides) |
| Prebuilt device-side files (`device/`) | 50 | 216 MiB | CJK font packages, cross gdb / perf |
| Buildroot download cache | 15 | 75 MiB | font / libcamera source archives |
| Application resources (`app/`) | 14 | 36 MiB | fonts, models, prebuilt `.so` files |
| mbedTLS test vectors (`u-boot/`) | 8 | 17 MiB | `test_suite_*.data` |
| Other firmware (`rkbin/`) | 1 | 2 MiB | trust images for non-target platforms |

The rule is simple and reproducible: **any file larger than 1 MiB that is neither source/text nor
required for compilation is removed.** The full list is in
[`opensource-dropped-files.tsv`](opensource-dropped-files.tsv) (with sha256 and size).

> Keeping the toolchains (about 1.1 GiB) is a deliberate trade-off: the tree compiles right after
> `git clone`, without fetching a toolchain first. If repository size matters more to you, you can
> delete `prebuilts/gcc/` and install the
> [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) yourself.

## 3. Quick Start

```bash
# Host dependencies (Ubuntu 22.04 / 24.04)
sudo apt-get install -y build-essential git python3 python3-pip \
     bison flex bc libssl-dev libncurses-dev device-tree-compiler \
     u-boot-tools rsync cpio kmod

# Select a board configuration
./build.sh lunch            # interactive, or specify directly, e.g.:
./build.sh rk3576_defconfig

# Build
./build.sh                  # kernel + U-Boot + rootfs
./build.sh kernel           # kernel only
./build.sh uboot            # U-Boot only
```

Outputs land in `rockdev/` (`boot.img`, `rootfs.img`, `update.img`, and so on).

If a build fails with missing files, run the dependency check first:

```bash
./download_prebuilts.sh --check
```

## 4. Restoring the Removed Third-party Artifacts

```bash
./download_prebuilts.sh --list                          # list every missing file
./download_prebuilts.sh --check                         # check what the working tree still lacks
./download_prebuilts.sh --restore --verify              # restore from the default source, verify sha256
RK_PREBUILTS_URL=https://your-mirror/sdk-prebuilds \
  ./download_prebuilts.sh --restore --verify            # use a custom mirror
```

The script works from the `opensource-dropped-files.tsv` manifest and verifies sha256 after
restoring.

**Where these artifacts come from officially:**

| Content | Official source |
|---|---|
| ARM GNU Toolchain (if you do not use the in-tree copy) | developer.arm.com/downloads/-/arm-gnu-toolchain-downloads |
| libmali GPU user-space libraries | Rockchip Linux SDK official release |
| OP-TEE / TA prebuilt libraries | Rockchip Linux SDK official release |
| RKNN Toolkit2 / RKNPU2 / RKNN Model Zoo | github.com/airockchip |
| Chromium arm64/armhf packages | Rockchip Linux SDK official release |
| Buildroot source archives | buildroot.org (or fetched automatically by `make source`) |
| OpenCV prebuilt libraries | opencv.org |
| Rockchip official PDF documents | Rockchip official SDK release |

**License notice**: this repository does not contain, and does not redistribute, any of the
third-party proprietary artifacts listed above. Confirm the licensing terms with Rockchip and the
respective upstream projects before using them.

## 5. License and Compliance

- This SDK is based on the **Linux kernel (GPLv2)** and **U-Boot (GPLv2+)**. When you distribute
  firmware binaries built from it, you **must** also make the corresponding complete source
  available. This is a license obligation, not optional.
- The cross toolchains kept in this repository come from ARM GNU Toolchain (GCC family, GPL/LGPL).
  Their license texts ship alongside the toolchain directories
  (`prebuilts/gcc/**/share/licenses`, `COPYING*`).
- This repository contains **no** Rockchip proprietary binaries or documentation, and therefore
  does not constitute redistribution under Rockchip's licensing terms.
- If your product uses Rockchip proprietary libraries (libmali, NPU, OP-TEE, and so on), confirm
  the scope of your license with Rockchip.

## 6. Upstream and Credits

| Component | Upstream |
|---|---|
| Linux Kernel | kernel.org |
| U-Boot | denx.de/u-boot |
| Buildroot | buildroot.org |
| RKNPU2 / RKNN Toolkit2 | github.com/airockchip |
| ARM GNU Toolchain | developer.arm.com |

## 7. Feedback

Please report problems, patches and suggestions through an Issue or a Pull Request.

---

*This document is maintained by hand. If you find it does not match the actual repository, please
open an Issue.*
