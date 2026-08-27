[TOC]

# SBOM 与安全漏洞扫描参考文档

## 概述

软件物料清单（SBOM, Software Bill of Materials）是描述软件中所包含组件的结构化清单。它详细列出了软件中包含的**开源组件、第三方库、版本信息、许可证、依赖关系、漏洞关联信息**等内容。SBOM 对于软件供应链安全管理和合规审计具有重要意义。

Rockchip Linux SDK 的 `sbom` 目录提供了针对不同操作系统（Buildroot、Debian、Yocto）及 SDK 的 SBOM 和安全漏洞扫描报告，帮助开发者了解 SDK 中所包含的软件组件及其安全状态。**每个操作系统和 SDK 均提供 4 种格式的 SBOM 和安全分析报告**：SPDX JSON、CycloneDX JSON、CSV 和 HTML，以满足不同工具和场景的需求。

## 目录结构

```
sbom/
├── buildroot/
│   ├── sbom/                               # Buildroot 系统 SBOM 文件
│   │   ├── sbom.spdx.json                 # SPDX 格式
│   │   ├── sbom.cyclonedx.json            # CycloneDX 格式
│   │   ├── sbom.csv                       # CSV 格式
│   │   └── sbom.html                      # HTML 格式
│   └── security/                           # Buildroot 安全扫描报告
│       ├── cve-report.spdx.json           # SPDX 格式
│       ├── cve-report.cyclonedx.json      # CycloneDX 格式
│       ├── cve-report.csv                 # CSV 格式
│       └── cve-report.html                # HTML 格式
├── debian/
│   ├── sbom/                               # Debian 系统 SBOM 文件
│   │   ├── sbom.spdx.json                 # SPDX 格式
│   │   ├── sbom.cyclonedx.json            # CycloneDX 格式
│   │   ├── sbom.csv                       # CSV 格式
│   │   └── sbom.html                      # HTML 格式
│   └── security/                           # Debian 安全扫描报告
│       ├── cve-report.spdx.json           # SPDX 格式
│       ├── cve-report.cyclonedx.json      # CycloneDX 格式
│       ├── cve-report.csv                 # CSV 格式
│       └── cve-report.html                # HTML 格式
├── kernel/
│   ├── Latest-Release-Rockchip-Kernel-CVEs-Link.txt  # 内核 CVE 最新链接
│   └── cve/                                # 内核月度 CVE 报告
│       ├── 2025-06.md
│       ├── 2025-07.md
│       ├── ...
│       └── 2026-05.md
├── sdk/
│   ├── sbom/                               # SDK SBOM 文件
│   │   ├── sdk-sbom.spdx.json             # SPDX 格式
│   │   ├── sdk-sbom.cyclonedx.json        # CycloneDX 格式
│   │   ├── sdk-sbom.csv                   # CSV 格式
│   │   └── sdk-sbom.html                  # HTML 格式
│   └── security/                           # SDK 安全扫描报告
│       ├── sdk-security-report.spdx.json  # SPDX 格式
│       ├── sdk-security-report.cyclonedx.json # CycloneDX 格式
│       ├── sdk-security-report.csv        # CSV 格式
│       └── sdk-security-report.html       # HTML 格式
├── u-boot/
│   ├── Latest-Release-Rockchip-U-Boot-CVEs-Link.txt  # U-Boot CVE 最新链接
│   └── cve/                                # U-Boot CVE 报告
│       ├── U-Boot-CVE-2026-06_CN.md
│       └── U-Boot-CVE-2026-06_EN.md
└── yocto/
    ├── sbom/                               # Yocto 系统 SBOM 文件
    │   ├── sbom.spdx.json                 # SPDX 格式
    │   ├── sbom.cyclonedx.json            # CycloneDX 格式
    │   ├── sbom.csv                       # CSV 格式
    │   └── sbom.html                      # HTML 格式
    └── security/                           # Yocto 安全扫描报告
        ├── cve-report.spdx.json           # SPDX 格式
        ├── cve-report.cyclonedx.json      # CycloneDX 格式
        ├── cve-report.csv                 # CSV 格式
        └── cve-report.html                # HTML 格式
```

## SBOM 文件格式说明

### SPDX 格式

SPDX（Software Package Data Exchange）是一种由 Linux 基金会维护的开放标准，用于描述软件物料清单。Rockchip SDK 中的 SPDX 文件采用 JSON 格式，包含以下关键信息：

- **SPDXVersion**: SPDX 规范版本
- **packages**: 软件包列表，包含包名、版本、许可证等
- **relationships**: 软件包之间的依赖关系
- **files**: 文件级别的组件信息

**使用场景**: 适用于合规审计、许可证管理和供应链安全分析。

**示例文件**:
- `buildroot/sbom/sbom.spdx.json` — Buildroot 系统的完整 SBOM
- `debian/sbom/sbom.spdx.json` — Debian 系统的完整 SBOM
- `yocto/sbom/sbom.spdx.json` — Yocto 系统的完整 SBOM
- `sdk/sbom/sdk-sbom.spdx.json` — SDK 的完整 SBOM

### CycloneDX 格式

CycloneDX 是一种轻量级的 SBOM 标准，专注于安全和漏洞管理。Rockchip SDK 中的 CycloneDX 文件采用 JSON 格式，包含以下关键信息：

- **bomFormat**: BOM 格式标识（CycloneDX）
- **components**: 软件组件列表，包含名称、版本、类型等
- **vulnerabilities**: 已知漏洞信息
- **dependencies**: 组件间的依赖关系

**使用场景**: 适用于漏洞管理、安全分析和 DevSecOps 流程。

**示例文件**:
- `buildroot/sbom/sbom.cyclonedx.json` — Buildroot 系统的 SBOM
- `debian/sbom/sbom.cyclonedx.json` — Debian 系统的 SBOM
- `yocto/sbom/sbom.cyclonedx.json` — Yocto 系统的 SBOM
- `sdk/sbom/sdk-sbom.cyclonedx.json` — SDK 的 SBOM

### CSV 格式

CSV（Comma-Separated Values）格式的 SBOM 文件以表格形式展示组件信息，便于在电子表格软件中查看和分析。

**典型字段**:

| 字段 | 说明 |
|:---|:---|
| Package | 软件包名称 |
| Version | 版本号 |
| License | 许可证类型 |
| Supplier | 供应商 |
| CVE | 关联的 CVE 编号 |

**示例文件**:
- `buildroot/sbom/sbom.csv` — Buildroot 系统组件列表
- `debian/sbom/sbom.csv` — Debian 系统组件列表
- `yocto/sbom/sbom.csv` — Yocto 系统组件列表
- `sdk/sbom/sdk-sbom.csv` — SDK 组件列表

### HTML 格式

HTML 格式的报告提供可视化的 SBOM 和安全扫描结果，可直接在浏览器中查看，便于快速浏览和分析。

**使用场景**: 适合快速查看、演示和非技术人员理解。

**示例文件**:
- `buildroot/sbom/sbom.html` — Buildroot 系统 SBOM 可视化报告
- `debian/sbom/sbom.html` — Debian 系统 SBOM 可视化报告
- `yocto/sbom/sbom.html` — Yocto 系统 SBOM 可视化报告
- `sdk/sbom/sdk-sbom.html` — SDK SBOM 可视化报告

## 安全漏洞扫描说明

### Buildroot 系统

Buildroot 系统通过 `security/` 目录提供安全漏洞扫描报告，包含 4 种格式：

| 文件 | 格式 | 说明 |
|:---|:---|:---|
| `cve-report.spdx.json` | SPDX JSON | 安全扫描结果（SPDX 格式） |
| `cve-report.cyclonedx.json` | CycloneDX JSON | 安全扫描结果（CycloneDX 格式） |
| `cve-report.csv` | CSV | 安全扫描汇总表 |
| `cve-report.html` | HTML | 安全扫描可视化报告 |

**使用方法**: 使用浏览器打开 `sbom/buildroot/security/cve-report.html` 即可查看完整报告。

### Debian 系统

Debian 系统通过 `security/` 目录提供安全漏洞扫描报告，包含 4 种格式：

| 文件 | 格式 | 说明 |
|:---|:---|:---|
| `cve-report.spdx.json` | SPDX JSON | 安全扫描结果（SPDX 格式） |
| `cve-report.cyclonedx.json` | CycloneDX JSON | 安全扫描结果（CycloneDX 格式） |
| `cve-report.csv` | CSV | 安全扫描汇总表 |
| `cve-report.html` | HTML | 安全扫描可视化报告 |

**使用方法**: 使用浏览器打开 `sbom/debian/security/cve-report.html` 即可查看完整报告。

### Yocto 系统

Yocto 系统通过 `security/` 目录提供安全漏洞扫描报告，包含 4 种格式：

| 文件 | 格式 | 说明 |
|:---|:---|:---|
| `cve-report.spdx.json` | SPDX JSON | 安全扫描结果（SPDX 格式） |
| `cve-report.cyclonedx.json` | CycloneDX JSON | 安全扫描结果（CycloneDX 格式） |
| `cve-report.csv` | CSV | 安全扫描汇总表 |
| `cve-report.html` | HTML | 安全扫描可视化报告 |

**使用方法**: 使用浏览器打开 `sbom/yocto/security/cve-report.html` 即可查看完整报告。

### SDK 系统

SDK 通过 `security/` 目录提供安全漏洞扫描报告，包含 4 种格式：

| 文件 | 格式 | 说明 |
|:---|:---|:---|
| `sdk-security-report.spdx.json` | SPDX JSON | SDK 安全扫描结果（SPDX 格式） |
| `sdk-security-report.cyclonedx.json` | CycloneDX JSON | SDK 安全扫描结果（CycloneDX 格式） |
| `sdk-security-report.csv` | CSV | SDK 安全扫描汇总表 |
| `sdk-security-report.html` | HTML | SDK 安全扫描可视化报告 |

**使用方法**: 使用浏览器打开 `sbom/sdk/security/sdk-security-report.html` 即可查看完整报告。

### Kernel 系统

内核 CVE 报告按月度组织，存放在 `sbom/kernel/cve/` 目录下。每份报告包含：

- 当月内核相关的 CVE 漏洞列表
- 漏洞影响的内核版本
- 漏洞严重程度和修复状态

**最新 CVE 链接**: 参见 `sbom/kernel/Latest-Release-Rockchip-Kernel-CVEs-Link.txt`，或访问 Redmine 平台获取最新信息。

### U-Boot 系统

U-Boot CVE 报告存放在 `sbom/u-boot/cve/` 目录下，提供中英文版本：

| 文件 | 说明 |
|:---|:---|
| `U-Boot-CVE-2026-06_CN.md` | U-Boot CVE 报告（中文） |
| `U-Boot-CVE-2026-06_EN.md` | U-Boot CVE 报告（英文） |

**最新 CVE 链接**: 参见 `sbom/u-boot/Latest-Release-Rockchip-U-Boot-CVEs-Link.txt`。

## 许可证合规说明

Buildroot 系统的许可证信息包含在 `copyright/` 目录下：

- **software_copyright_list.csv**: 目标端（Target）软件包的许可证列表，包含 SDK 默认编译的所有第三方包
- **host-software_copyright_list.csv**: 宿主端（Host）工具链和辅助工具的许可证列表
- **BUILDROOT_README**: Buildroot 许可证合规说明，包含源码获取、许可证文件位置等重要信息

> **注意**: 部分 Rockchip 本地包（如 rockchip-mpp、rknpu2、rkwifibt 等）由于技术限制未保存源码，开发者需根据许可证要求自行获取。

## 常见问题

### 如何查看 SPDX 格式的 SBOM？

SPDX JSON 文件可以使用以下工具查看：
- **在线工具**: [SPDX Online Tools](https://tools.spdx.org/)
- **命令行工具**: spdx-tools（可通过 pip 安装）
- **通用编辑器**: 任何支持 JSON 的文本编辑器

### 如何查看 CycloneDX 格式的 SBOM？

CycloneDX JSON 文件可以使用以下工具查看：
- **在线工具**: [CycloneDX BOM Validator](https://cyclonedx.org/tool-center/)
- **命令行工具**: cyclonedx-bom（可通过 npm 安装）
- **通用编辑器**: 任何支持 JSON 的文本编辑器

### 如何查询特定组件的 CVE 漏洞？

1. **Buildroot/Debian/Yocto 系统**: 在对应的 `security/cve-report.html` 中搜索组件名称
2. **SDK**: 在 `sdk/security/sdk-security-report.html` 中搜索组件名称
3. **内核 CVE**: 在 `kernel/cve/` 目录下的月度报告中查找
4. **U-Boot CVE**: 在 `u-boot/cve/` 目录下的报告中查找

### CVE 严重程度等级说明

| 等级 | CVSS 分数 | 说明 |
|:---|:---|:---|
| CRITICAL | 9.0 - 10.0 | 严重漏洞，需立即修复 |
| HIGH | 7.0 - 8.9 | 高危漏洞，建议尽快修复 |
| MEDIUM | 4.0 - 6.9 | 中危漏洞，建议评估后修复 |
| LOW | 0.1 - 3.9 | 低危漏洞，可视情况修复 |

## 后续更新计划

我们将持续更新 SBOM 和安全漏洞扫描报告，以解决安全相关问题：

1. **定期更新**: 每月更新内核和 U-Boot 的 CVE 报告
2. **新增格式支持**: 根据社区需求，考虑增加其他 SBOM 格式（如 SWID、SPDX Lite 等）
3. **自动化扫描**: 逐步实现安全扫描的自动化和持续集成
4. **漏洞修复跟踪**: 建立漏洞修复跟踪机制，及时更新修复状态
5. **文档完善**: 持续完善文档，增加使用示例和最佳实践

## 参考链接

- [SPDX 规范](https://spdx.dev/specifications/)
- [CycloneDX 规范](https://cyclonedx.org/specification/)
- [NVD - National Vulnerability Database](https://nvd.nist.gov/)
- [CVE 官方网站](https://cve.mitre.org/)
- [VEX 概述](https://www.cisa.gov/sbom/vex)
- [Rockchip Redmine 平台](https://redmine.rock-chips.com/)
