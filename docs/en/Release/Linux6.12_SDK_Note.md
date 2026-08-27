# Linux6.12 SDK Note

---

**Contents**

[TOC]

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

### Major Issues Fixed

- **Display / VOP2**: Fixed RK3572 Cluster1-before-Cluster0 enable order, cluster1 AFBC disable, msmart grid mode minimum act\_width, color abnormality when DCI enabled, and background color error with ACM/MCU display interface (RK3572)
- **HDMI / DP**: Fixed dw-hdmi-qp AVI InfoFrame, HDCP2 resume CEC IRQ crash, dw-dp link-retraining FIFO overflow and HPD deadlock, and multiple cdn-dp error-handling issues
- **ISP / VICAP**: Fixed isp33 sharp/gic noise curve after resume, isp\_vir1 index error, lsc tasklet count; vicap fixed frame-loss statistics and toisp reset on CSI size error
- **MPP**: Fixed low H.264 decode FPS at 4096x2160, av1d parser reading one extra bit, AFBC border extension, vdpu384b scale-down
- **Storage / eMMC**: Fixed cqhci HALT status clear and timeout, sdhci HS200/HS400 lower-speed return path; re-enabled RK3572 CQE support
- **USB / TypeC / PCIe**: Fixed husb311 PM suspend/resume IRQ management, inno-usb2 host-mode charger detection, and kernel panic from PCIe LTSSM trace; added PCIe EP signal compliance mode and slot reset
- **Power / DMC / PMIC**: Hold pclk\_mailbox during DDR frequency scaling; removed RK3572 vdd\_gpu/vdd\_npu always-on in favor of early-regulator-enable; fixed rk817-battery charge status and pmdomain vd\_offset (RK3572)
- **Audio**: Fixed multicodecs HEADPHONE report for 4-pole headsets, spdifrx runtime kcontrol errors
- **WiFi/BT**: rkwifibt drops prebuilt brcm\_tools binaries; kernel rfkill-bt avoids uart\_rts-gpios pinctrl mux conflict

### SDK Component Update Status

| Module       | Content                                                      |
| ------------ | ------------------------------------------------------------ |
| Kernel       | 1. **Platform**: Added RK3572S (dtsi/eval board/tablet) with Mali/rknpu3/cpufreq OPP; added RK3572 Android9 and UVC device trees<br/>2. **Display/VOP2**: Cluster1 enable order, AFBC disable mask, VP0→RGB route, RK3576 HDR10+ overlay, DCI/ACM/MCU display fixes<br/>3. **HDMI/DP**: HDCP2.3→1.4 fallback, HDCP1.4 ioctl; dw-dp FIFO overflow / MST→SST / HPD deadlock fixes, MST audio plug/unplug notification<br/>4. **RGA3**: AFBC32x8 color fill, auto\_reset to avoid FIFO read errors, full\_csc fixes<br/>5. **ISP/VICAP**: isp33 software wrap, 8K (isp3.5/3.51), new OS08G10/OX03C10 (LOFIC HDR)/LT6911UXE (HDR) sensors, rk628 pattern mode; vicap frame-loss/toisp reset<br/>6. **NPU**: rknpu3 iotlb flush, shootdown-entire handling, and iommu-addresses support (drops redundant iommu\_map/unmap, reserves npu r/w iommu-address)<br/>7. **Storage**: cqhci HALT/timeout fixes, re-enabled RK3572 CQE<br/>8. **USB/TypeC/PCIe**: husb311 PM, inno-usb2, i3c RK3572 support, PCIe EP enhancements and LTSSM tracepoint<br/>9. **Power/DMC**: hold pclk\_mailbox during DDR scaling, early-regulator-enable, dmc opp voltage update<br/>10. **Audio**: rk730-es7202 sound card, pdm one-line-one-channel, multicodecs 4-pole HEADPHONE fix<br/>11. **Crypto**: rkce GCM tag loss fix<br/>12. **GPIO/IRQ**: GPIO V2 IRQ thread CPU-affinity binding; SIP GPIO interrupt-division calls |
| U-Boot       | 1. **Security**: AVB RSA4096, avbtool dump enhancements, multiple upstream CVE fixes; NPU IP verification in board\_init (optee IP\_str)<br/>2. **Display**: vop2 esmart scale, ACM/MCU display, dsi2 OOB, analogix\_dp mode validation, vop2 automatic multi-VP synchronization<br/>3. **RK3572**: SDMMC iomux hardware check, AMP configs, rk3572-rt.config, removed vehicle dice configs, cntfrq\_el0 from DDR binary<br/>4. **Storage**: mmc erase performance, sdhci enhance store<br/>5. **Peripheral**: i2c SDA bus recovery, pinctrl set\_gpio\_mux ops |
| rkbin        | RK3572: SPL → **v1.03**, BL31 → **v1.10**, BL32 → **v1.06**, DDR → **v1.05**<br/>RK3576: BL31 → **v1.25**, BL32 → **v1.11**, DDR → **v1.13**<br/>RK3588: BL32 → **v1.23**<br/>ddrbin\_tool → **v1.35**; added release-doc.sh tooling |
| Debian       | 1. rkaiq → **v6.0x33.0**, mpp → **v1.0.12**, libmali → g29p1<br/>2. mutter supports AFBC YUV420 8/10bit and NV12/P010/NV15 GPU compositing<br/>3. GStreamer enables ARM AFBC on RK3572<br/>4. Added Mali GPU display backend auto-config script and rk730-es7202 UCM2 |
| Buildroot    | 1. weston → **15.0**, gstreamer1 → **1.28.2**, flac → 1.5.0<br/>2. Added RKNN3 runtime and rknn3-test packages for RK3572 LLM; rk3572 defconfig enables rkai/rknn3-test<br/>3. Added rk3572 uvc-app defconfig; runtime sound-card detection for rk3572<br/>4. legal-info generates **SBOM**; weston no-EGL build fix<br/>5. **Migrated Buildroot base 2025.02 → 2026.02.2**, with all upstream packages bumped to the 2026.02 baseline; re-synced Rockchip downstream customizations (weston/gstreamer1/libdrm/pulseaudio/xserver) onto the new base |
| Yocto        | 1. Poky upgraded to **6.0 / 6.0.1** (codename **wrynose**), DISTRO\_VERSION=6.0.1<br/>2. linux-yocto / yocto-bsps kernel → **6.18.x**<br/>3. meta-rockchip adds wrynose support, drops kernel 5.10/4.19; gstreamer → 1.28.2, chromium → 149.0.7827<br/>4. Extensive upstream package bumps in openembedded-core/meta-openembedded (mesa 26.0, gtk4 4.22) and CVE updates |
| MPP          | Upgraded to **v1.0.12**: VLA config, ref\_cfg JSON, force-IDR by meta, RKFBC/FBC tile data\_layout; fixed 4096x2160 H.264 low FPS, av1d extra bit, AFBC border, vdpu384b scale-down |
| linux-rga    | Full CSC matrix support (BT.2020, 8-bit, AFBC32x8), CFA handling and limit checks, CSC logic refactor |
| ISP/RKAIQ    | camera\_engine\_rkaiq → **v6.0x33.0**, GCC 15.2 build fix; virtual sensor preview-capture frame id setting |
| NPU          | rknpu3 iotlb flush / shootdown-entire handling; rknn3-runtime → **v1.0.4**; rknn3\_model\_zoo → v1.0.4 (TokenPruner renamed SpeedUp) |
| RKNNStream   | Added TEXT/LLM, VAD, ASR (whisper), RockXPacket API, inference device API, splitter/merger; multi-device and YAML config support |
| libmali      | 1. Migrated to **next branch** (meson build, Debian packaging, hook injection, gpu-chips.txt)<br/>2. **Vulkan support added**: Vulkan headers v1.4.303, x11/wayland winsys integration, G610 CSF firmware mali\_csffw.bin<br/>3. Userspace driver updated to **g29p1-11eac-16** (G310/G610) and **g29p1-4** (G31/G52)<br/>4. Kernel-side valhall: added rk3572s OPP data and rk3588 nvmem cell customer-demand parsing |
| rockit       | prebuilt lib **v1.7.32** (framework **v2.51.0**); updated all test cases; common\_algorithm adds DIS (GPU version), libRkEis 0.0.25.392 |
| Applications | 1. app/rkadk: multi audio-player mixing, rkmuxer refactor adaptation, memory leak fixes<br/>2. external/uvc\_app: RK3572 support, BGR888 output, face detection V2.2<br/>3. external/samples: rknnstream\_launch (YOLO/LLM/ASR pipeline), RK3572 IDC→GDC example |

## rockchip_linux6.12_release_v0.1.0_20260420.xml Note

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
- **rkbin** updates: RK3572 SPL → v1.02, BL31 → v1.08, BL32 → v1.04
- Debian **libcamera** upgraded to 0.5.2, enabling Cheese to enumerate ISP cameras
- Buildroot added **robot defconfig**, Xenomai 3.3.2 support, LVGL9 → v9.5.0
- MPP adds **RKFBC output format** support and H.264 encoder tuning procedure
- SPI NAND: added support for **12 new devices** from Winbond, GigaDevice, Etron, hyf, etc.
- RT-Thread RGA driver fully overhauled: IOMMU support, cacheline alignment
- **Rockit** IDC→GDC distortion correction example added for RK3572; adapted to framework v2.50.0 (prebuilt lib v1.7.29)
- **ROCKIVA** updated with RK3588 model and libraries
- **RKAUDIO** algorithm library updated: VQE/SED/EQDRC algorithms added for arm-r123llvm1504-linux-musl toolchain; IDC library v1.1.0.86 added
- **Security**: RK3572 DICE UDS OTP and Secure OTP support added (U-Boot + rkbin); kernel crypto module fixed

### Major Issues Fixed

- **RGA**: Fixed RGA3 request leak on multi-task submit failure; fixed RGA2/RGA3 slave\_mode execution failure after master\_mode switch
- **ISP / VICAP**: Fixed thunderboot buffer rotation error under high CPU load; fixed online mode frame loss with multi-stream active
- **Display / VOP2**: Fixed VOP2 sharp initialization and FBC timeout on RK3572; fixed dw-hdmi-qp YUV420 color error entering kernel
- **DMC**: Fixed DFI count\_rate for different DRAM types on RK3572
- **MPP**: Fixed H.265 splitter out-of-bounds read and carry-over data loss; fixed mpp\_rc drop-gap bypass during re-encode
- **Crypto**: Fixed calculation error beyond 256 pages; fixed bit-shift and array-index errors
- **PMIC / Power**: Removed forced reset mode on rk8xx suspend/resume; added power-key release wait before reboot
- **WiFi**: Fixed bcmdhd `cfg80211_port_authorized` for kernel 6.1+

### SDK Component Update Status

| Module       | Content                                                      |
| ------------ | ------------------------------------------------------------ |
| Kernel       | 1. **RGA3**: Added hardware batching; added full\_csc 10-bit pixel; fixed rotate 90/270 output\_params loss; fixed multi-task request leak<br/>2. **ISP/VICAP**: ISP driver v3.3.0; VICAP adds quad bayer dual-pipe mode and yuv-in path; multiple frame-sync and buffer-rotation fixes<br/>3. **Display / VOP2**: Fixed FBC timeout (RK3572); fixed post-buf-empty scan-line accuracy; fixed VOP2 sharp init; added VP0→RGB DT config<br/>4. **HDMI PHY**: Refactored samsung-hdptx as unified HDMI+eDP driver; HDMI 2.1 TxFFE support; fixed TMDS mode TxFFE config error<br/>5. **NPU/GPU**: rknpu3 v1.0.2; Mali DDK g29p1-12eac0; fixed irq-clear task timeout<br/>6. **DMC/DSMC**: Fixed RK3572 DFI count\_rate; added delayed frequency scaling; DSMC adds DMA link-list macros and runtime PM<br/>7. **Audio**: Fixed multicodecs spurious key events on headset unplug; fixed rk817 PDM/I2S sample rate; added rk730 suspend/resume<br/>8. **USB/TypeC**: Fixed husb311 suspended flag; optimized rockchip\_pmic\_tcpc vsafe0v detection; fixed inno-usb2 linestate filter<br/>9. **PCIe**: PTM support initialized<br/>10. **CAN**: Added RK3572 CAN driver<br/>11. **SPI NAND**: Added support for 12 new devices (Winbond W25N04, GigaDevice GD5F4GM7/GD5F8GM8, Etron EM73D044, hyf HYFQ512, and more)<br/>12. **Crypto**: Fixed calculation error beyond 256 pages; fixed bit-shift and array-index errors<br/>13. **PMIC**: rk8xx register-only reset support; rk809 power-key release delay; rk806 removed forced reset mode on suspend |
| U-Boot       | 1. **HDMI PHY**: Merged samsung-hdptx HDMI/eDP into single driver; all display PHY helpers migrated to generic-phy ops<br/>2. **DICE Security**: RK3572 DICE UDS OTP support; fixed OTP revoke key address; rockusb DICE UDS write support<br/>3. **Boot Flow**: Kernel DTB from embedded resource file; distro bootcmd updated to `bootflow scan -bl`; simplified env Kconfig<br/>4. **Fixes**: VOP2 background color error fix (ACM mode); dw-hdmi-qp YUV420 fix; sysreset/rk8xx waits for power-key release; AVB Kconfig unified to `LIBAVB_` prefix |
| Debian       | 1. **libcamera** upgraded to 0.5.2-3\~bpo13+1+rk1; ISP camera enumeration now works in Cheese<br/>2. MPP prebuilt packages updated with VPU support<br/>3. Mali valhall G310 prebuilt updated to **g29p1-9**<br/>4. RK3572 rkaiq updated to **v6.0x32.3**<br/>5. RGA2 prebuilt packages updated |
| Buildroot    | 1. Added **rockchip\_rk3572\_robot\_defconfig** for robotics scenarios<br/>2. ROS2: added python-netifaces, python-psutil and PYTHON\_ARGCOMPLETE dependencies<br/>3. Fixed **weston waylandsink window freeze**; waylandsink honors video alpha channel<br/>4. **Xenomai** upgraded to 3.3.2<br/>5. **LVGL9** upgraded to v9.5.0; LVGL8 patch set updated<br/>6. Fixed pixman AArch64 NEON support<br/>7. Added RK3572 alsa config and udev rule files |
| MPP          | 1. Added **RKFBC output format** (legacy vpu\_api and new mpp\_frame path)<br/>2. vepu511a: Added H.264 tuning procedure; fixed RDO lambda\_idx\_p error<br/>3. Refactored hal debug framework with unified dump interfaces<br/>4. Fixed H.265 splitter OOB read and carry-over data loss<br/>5. Fixed mpp\_rc drop-gap bypass during re-encode<br/>6. Fixed static build missing hal objects<br/>7. Developer Guide updated to **v0.8** |
| rkbin        | RK3572: SPL → **v1.02**, BL31 → **v1.08**, BL32 → **v1.04**<br/>boot\_merger tool → **v1.38**<br/>DDR: Added PCB template descriptions for WDQS |
| rockit       | 1. common\_algorithm: Added **IDC library v1.1.0.86**; added VQE/SED/EQDRC/rkmuxer libraries for arm-r123llvm1504-linux-musl toolchain<br />2. Added **IDC→GDC distortion correction** example for RK3572 (external/samples, external/rockit)<br/>3. Adapted to rockit framework **v2.50.0** (prebuilt lib **v1.7.29**) across samples and rkipc |
| libmali      | Mali G310 userspace driver updated to **g29p1-9**; removed outdated Mali libraries |
| rkwifibt     | Fixed bcmdhd `cfg80211_port_authorized` for kernel 6.1+; adapted bcmdhd.101.10.591.x to RT-Linux |
| Applications | 1. external/samples: Added RK3572 IDC→GDC example; fixed rk3588 display; adapted to rockit framework v2.50.0<br/>2. app/rkadk: Player supports multi-audio-player mixing via rockit amix |

## rockchip_linux6.12_release_v0.0.1_20260320.xml Note

```
- The first Alpha version
```
