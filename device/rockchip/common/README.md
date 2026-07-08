# Rk Linux 6.12 
- 此分支为rockchip原始sdks master 分支上修改，Linux 6.12 rkb0版本

## 本地无此项目仓库时

### 克隆远程仓到本地目录
- git clone http://192.168.0.188/industry/rk3572_linux_6.12.git
- 将需要添加的文件拷贝到rk3572_linux_6.12目录下，并添加至仓库
- git add .
- git commit -a -m '描述添加的内容'
- git push origin master


## 本地已经有项目仓库时

### 先同步仓库至最新版本
- git pull origin master
- 将需要添加的文件拷贝到rk3572_linux_6.12目录下，并添加至仓库
- git add .
- git commit -a -m '描述添加的内容'
- git push origin master


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
