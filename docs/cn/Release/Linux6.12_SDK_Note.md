# Linux6.12 SDK Note

---

**Contents**

[TOC]

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

### Major Issues Fixed

- **RGA**: Fixed RGA3 request leak on multi-task submit failure; fixed RGA2/RGA3 slave\_mode execution failure after master\_mode switch
- **ISP / VICAP**: Fixed thunderboot buffer rotation error under high CPU load; fixed online mode frame loss with multi-stream active
- **Display / VOP2**: Fixed VOP2 sharp initialization and FBC timeout on RK3572/RK3538; fixed dw-hdmi-qp YUV420 color error entering kernel
- **DMC**: Fixed DFI count\_rate for different DRAM types on RK3572
- **MPP**: Fixed H.265 splitter out-of-bounds read and carry-over data loss; fixed mpp\_rc drop-gap bypass during re-encode
- **Crypto**: Fixed calculation error beyond 256 pages; fixed bit-shift and array-index errors
- **PMIC / Power**: Removed forced reset mode on rk8xx suspend/resume; added power-key release wait before reboot
- **WiFi**: Fixed bcmdhd `cfg80211_port_authorized` for kernel 6.1+

### SDK Component Update Status

| Module       | Content                                                      |
| ------------ | ------------------------------------------------------------ |
| Kernel       | 1. **RGA3**: Added hardware batching; added full\_csc 10-bit pixel; fixed rotate 90/270 output\_params loss; fixed multi-task request leak<br/>2. **ISP/VICAP**: ISP driver v3.3.0; VICAP adds quad bayer dual-pipe mode and yuv-in path; multiple frame-sync and buffer-rotation fixes<br/>3. **Display / VOP2**: Fixed FBC timeout (RK3572/RK3538); fixed post-buf-empty scan-line accuracy; fixed VOP2 sharp init; added VP0→RGB DT config<br/>4. **HDMI PHY**: Refactored samsung-hdptx as unified HDMI+eDP driver; HDMI 2.1 TxFFE support; fixed TMDS mode TxFFE config error<br/>5. **NPU/GPU**: rknpu3 v1.0.2; Mali DDK g29p1-12eac0; fixed irq-clear task timeout<br/>6. **DMC/DSMC**: Fixed RK3572 DFI count\_rate; added delayed frequency scaling; DSMC adds DMA link-list macros and runtime PM<br/>7. **Audio**: Fixed multicodecs spurious key events on headset unplug; fixed rk817 PDM/I2S sample rate; added rk730 suspend/resume<br/>8. **USB/TypeC**: Fixed husb311 suspended flag; optimized rockchip\_pmic\_tcpc vsafe0v detection; fixed inno-usb2 linestate filter<br/>9. **PCIe**: PTM support initialized<br/>10. **CAN**: Added RK3572 CAN driver<br/>11. **SPI NAND**: Added support for 12 new devices (Winbond W25N04, GigaDevice GD5F4GM7/GD5F8GM8, Etron EM73D044, hyf HYFQ512, and more)<br/>12. **Crypto**: Fixed calculation error beyond 256 pages; fixed bit-shift and array-index errors<br/>13. **PMIC**: rk8xx register-only reset support; rk809 power-key release delay; rk806 removed forced reset mode on suspend |
| U-Boot       | 1. **HDMI PHY**: Merged samsung-hdptx HDMI/eDP into single driver; all display PHY helpers migrated to generic-phy ops<br/>2. **DICE Security**: RK3572 DICE UDS OTP support; fixed OTP revoke key address; rockusb DICE UDS write support<br/>3. **Boot Flow**: Kernel DTB from embedded resource file; distro bootcmd updated to `bootflow scan -bl`; simplified env Kconfig<br/>4. **Fixes**: VOP2 background color error fix (ACM mode); dw-hdmi-qp YUV420 fix; sysreset/rk8xx waits for power-key release; AVB Kconfig unified to `LIBAVB_` prefix |
| Debian       | 1. **libcamera** upgraded to 0.5.2-3\~bpo13+1+rk1; ISP camera enumeration now works in Cheese<br/>2. MPP prebuilt packages updated with VPU support<br/>3. Mali valhall G310 prebuilt updated to **g29p1-9**<br/>4. RK3572 rkaiq updated to **v6.0x32.3**<br/>5. RGA2 prebuilt packages updated |
| Buildroot    | 1. Added **rockchip\_rk3572\_robot\_defconfig** for robotics scenarios<br/>2. ROS2: added python-netifaces, python-psutil and PYTHON\_ARGCOMPLETE dependencies<br/>3. Fixed **weston waylandsink window freeze**; waylandsink honors video alpha channel<br/>4. **Xenomai** upgraded to 3.3.2<br/>5. **LVGL9** upgraded to v9.5.0; LVGL8 patch set updated<br/>6. Fixed pixman AArch64 NEON support<br/>7. Added RK3572 alsa config and udev rule files |
| MPP          | 1. Added **RKFBC output format** (legacy vpu\_api and new mpp\_frame path)<br/>2. vepu511a: Added H.264 tuning procedure; fixed RDO lambda\_idx\_p error<br/>3. Refactored hal debug framework with unified dump interfaces<br/>4. Fixed H.265 splitter OOB read and carry-over data loss<br/>5. Fixed mpp\_rc drop-gap bypass during re-encode<br/>6. Fixed static build missing hal objects<br/>7. Developer Guide updated to **v0.8** |
| rkbin        | RK3572: SPL → **v1.02**, BL31 → **v1.08**, BL32 → **v1.04**<br/>RK3538: BL31 → **v1.03**, BL32 → **v1.04**<br/>boot\_merger tool → **v1.38**<br/>DDR: Added PCB template descriptions for WDQS |
| rockit       | 1. common\_algorithm: Added **IDC library v1.1.0.86**; added VQE/SED/EQDRC/rkmuxer libraries for arm-r123llvm1504-linux-musl toolchain<br />2. Added **IDC→GDC distortion correction** example for RK3572 (external/samples, external/rockit)<br/>3. Adapted to rockit **v2.50.0** across samples and rkipc |
| libmali      | Mali G310 userspace driver updated to **g29p1-9**; removed outdated Mali libraries |
| rkwifibt     | Fixed bcmdhd `cfg80211_port_authorized` for kernel 6.1+; adapted bcmdhd.101.10.591.x to RT-Linux |
| Applications | 1. external/samples: Added RK3572 IDC→GDC example; fixed rk3588 display; adapted to rockit v2.50.0<br/>2. app/rkadk: Player supports multi-audio-player mixing via rockit amix |

## linux6.12_release_v0.0.1_20260320.xml Note

```
- The first Alpha version
```

