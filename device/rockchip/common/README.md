# Rk Linux 6.12 
- 此分支为rockchip原始sdks master 分支上修改，Linux 6.12 rkr1版本

## 本地无此项目仓库时

### 克隆远程仓到本地目录
- git clone https://github.com/ai-rupa/rupa_linux_6.12.git
- 将需要添加的文件拷贝到rk_linux_6.12目录下，并添加至仓库
- git add .
- git commit -a -m '描述添加的内容'
- git push origin master


## 本地已经有项目仓库时

### 先同步仓库至最新版本
- git pull origin master
- 将需要添加的文件拷贝到rk_linux_6.12目录下，并添加至仓库
- git add .
- git commit -a -m '描述添加的内容'
- git push origin master


## rockchip_linux6.12_release_v1.0.0_20260620.xml Note

```
- The first Release version
```

### Summary of Major Updates

- **Mali GPU migrated to next branch with Vulkan support**: Vulkan headers **v1.4.303** imported; x11/wayland winsys integration; G610 CSF firmware added; userspace driver upgraded to **g29p1-11eac-16** (G310/G610) and **g29p1-4** (G31/G52)
- **HDMI**: dw-hdmi-qp added **HDCP 2.3→1.4 fallback** and HDCP 1.4 mode ioctl
- **DisplayPort**: fixed link-retraining FIFO overflow; added MST→SST sink change; fixed HPD IRQ deadlock; added audio plug/unplug notification for MST streams
- **VOP2**: added **HDR10+** overlay support for RK3576
- **RGA**: linux-rga added **full CSC matrix** support (BT.2020, 8-bit pixel depth, AFBC32x8) and CFA handling
- **MPP updated to v1.0.12**: VLA config support, ref\_cfg JSON apply/extract, force-IDR by meta, RKFBC/FBC tile data\_layout
- **ISP**: isp33 software wrap support added; 8K supported for isp3.5/3.51; OS08G10, OX03C10 (LOFIC HDR), LT6911UXE (HDR) sensors added
- **RKAIQ updated to v6.0x33.0**; GCC 15.2 build errors fixed
- **NPU**: rknpu3 added iotlb flush and shootdown-entire handling; rknn3-runtime updated to **v1.0.4**; RKNN3 runtime and rknn3-test integrated into Buildroot for RK3572 LLM
- **RKNNStream**: TEXT/LLM module, VAD, ASR (whisper), RockXPacket API, inference device API, and splitter/merger added
- **rkbin**: RK3572 SPL → **v1.03**, BL31 → **v1.10**, BL32 → **v1.06**, DDR → **v1.05**; RK3576 BL31 → v1.25, BL32 → v1.11, DDR → v1.13; ddrbin\_tool → v1.35
- **RK730-ES7202 sound card** support added (ALSA UCM2, device tree, runtime detection)
- **Yocto upgraded to 6.0**: Poky → **6.0/6.0.1** (codename **wrynose**); linux-yocto kernel → **6.18.x**; meta-rockchip added wrynose support, dropped kernel 5.10/4.19
- **Buildroot base migrated 2025.02 → 2026.02.2**: all upstream packages bumped to the 2026.02 baseline; Rockchip downstream customizations (weston/gstreamer1/libdrm/pulseaudio/xserver) re-synced onto the new base
- **Debian / Buildroot**: Debian mutter supports AFBC YUV420/NV12/P010/NV15 GPU compositing; Buildroot weston → **15.0**, gstreamer → **1.28.2**; legal-info generates SBOM
- **Security**: U-Boot added AVB **RSA4096** support; multiple upstream CVE fixes merged; rkce GCM tag loss fixed


## linux6.12_release_v0.1.0_20260420.xml Note

```
- The first Beta version
```

### Summary of Major Updates

- **HDMI PHY driver refactored**: samsung-hdptx unified into a single driver supporting HDMI and eDP; HDMI 2.1 **TxFFE** support added
- RGA3 added **hardware batching** and **full\_csc 10-bit pixel** support
- ISP driver updated to **v3.3.0**; VICAP supports **quad bayer dual-pipe** and independent scale-node memory mode
- Mali GPU DDK upgraded to **g29p1-12eac0** (from g25p0-00eac0)
- rknpu3 driver updated to **v1.0.2**; priority queue capacity increased and handling improved
- **DICE UDS OTP** support added for RK3572 (U-Boot + kernel)
- **rkbin** updates: RK3572 SPL → v1.02, BL31 → v1.08, BL32 → v1.04; RK3538 BL31 → v1.03, BL32 → v1.04
- Debian **libcamera** upgraded to 0.5.2, enabling Cheese to enumerate ISP cameras
- Buildroot added **robot defconfig**, Xenomai 3.3.2 support, LVGL9 → v9.5.0
- MPP adds **RKFBC output format** support and H.264 encoder tuning procedure
- SPI NAND: added support for **12 new devices** from Winbond, GigaDevice, Etron, hyf, etc.
- RT-Thread RGA driver fully overhauled: IOMMU support, cacheline alignment, RK1820 chip support
- **Rockit** IDC→GDC distortion correction example added for RK3572; adapted to v2.50.0
- **ROCKIVA** updated with RK3588 model and libraries
- **RKAUDIO** algorithm library updated: VQE/SED/EQDRC algorithms added for arm-r123llvm1504-linux-musl toolchain; IDC library v1.1.0.86 added
- **Security**: RK3572 DICE UDS OTP and Secure OTP support added (U-Boot + rkbin); kernel crypto module fixed
