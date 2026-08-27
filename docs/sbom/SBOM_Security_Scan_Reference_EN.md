[TOC]

# SBOM and Security Vulnerability Scan Reference

## Overview

A Software Bill of Materials (SBOM) is a structured inventory of the components contained in software. It provides detailed information about **open-source components, third-party libraries, version information, licenses, dependencies, and vulnerability mapping data** included in the software. SBOM is essential for software supply chain security management and compliance auditing.

The `sbom` directory in the Rockchip Linux SDK provides SBOM and security vulnerability scan reports for different operating systems (Buildroot, Debian, Yocto) and the SDK, helping developers understand the software components included in the SDK and their security status. **Each operating system and SDK provides SBOM and security analysis reports in 4 formats**: SPDX JSON, CycloneDX JSON, CSV, and HTML, to meet the needs of different tools and scenarios.

## Directory Structure

```
sbom/
├── buildroot/
│   ├── sbom/                               # Buildroot system SBOM files
│   │   ├── sbom.spdx.json                 # SPDX format
│   │   ├── sbom.cyclonedx.json            # CycloneDX format
│   │   ├── sbom.csv                       # CSV format
│   │   └── sbom.html                      # HTML format
│   └── security/                           # Buildroot security scan reports
│       ├── cve-report.spdx.json           # SPDX format
│       ├── cve-report.cyclonedx.json      # CycloneDX format
│       ├── cve-report.csv                 # CSV format
│       └── cve-report.html                # HTML format
├── debian/
│   ├── sbom/                               # Debian system SBOM files
│   │   ├── sbom.spdx.json                 # SPDX format
│   │   ├── sbom.cyclonedx.json            # CycloneDX format
│   │   ├── sbom.csv                       # CSV format
│   │   └── sbom.html                      # HTML format
│   └── security/                           # Debian security scan reports
│       ├── cve-report.spdx.json           # SPDX format
│       ├── cve-report.cyclonedx.json      # CycloneDX format
│       ├── cve-report.csv                 # CSV format
│       └── cve-report.html                # HTML format
├── kernel/
│   ├── Latest-Release-Rockchip-Kernel-CVEs-Link.txt  # Kernel CVE latest link
│   └── cve/                                # Monthly kernel CVE reports
│       ├── 2025-06.md
│       ├── 2025-07.md
│       ├── ...
│       └── 2026-05.md
├── sdk/
│   ├── sbom/                               # SDK SBOM files
│   │   ├── sdk-sbom.spdx.json             # SPDX format
│   │   ├── sdk-sbom.cyclonedx.json        # CycloneDX format
│   │   ├── sdk-sbom.csv                   # CSV format
│   │   └── sdk-sbom.html                  # HTML format
│   └── security/                           # SDK security scan reports
│       ├── sdk-security-report.spdx.json  # SPDX format
│       ├── sdk-security-report.cyclonedx.json # CycloneDX format
│       ├── sdk-security-report.csv        # CSV format
│       └── sdk-security-report.html       # HTML format
├── u-boot/
│   ├── Latest-Release-Rockchip-U-Boot-CVEs-Link.txt  # U-Boot CVE latest link
│   └── cve/                                # U-Boot CVE reports
│       ├── U-Boot-CVE-2026-06_CN.md
│       └── U-Boot-CVE-2026-06_EN.md
└── yocto/
    ├── sbom/                               # Yocto system SBOM files
    │   ├── sbom.spdx.json                 # SPDX format
    │   ├── sbom.cyclonedx.json            # CycloneDX format
    │   ├── sbom.csv                       # CSV format
    │   └── sbom.html                      # HTML format
    └── security/                           # Yocto security scan reports
        ├── cve-report.spdx.json           # SPDX format
        ├── cve-report.cyclonedx.json      # CycloneDX format
        ├── cve-report.csv                 # CSV format
        └── cve-report.html                # HTML format
```

## SBOM File Format Description

### SPDX Format

SPDX (Software Package Data Exchange) is an open standard maintained by the Linux Foundation for describing software bill of materials. SPDX files in the Rockchip SDK use JSON format and contain the following key information:

- **SPDXVersion**: SPDX specification version
- **packages**: List of software packages, including package name, version, license, etc.
- **relationships**: Dependency relationships between software packages
- **files**: File-level component information

**Use cases**: Suitable for compliance auditing, license management, and supply chain security analysis.

**Example files**:
- `buildroot/sbom/sbom.spdx.json` — Complete SBOM for Buildroot system
- `debian/sbom/sbom.spdx.json` — Complete SBOM for Debian system
- `yocto/sbom/sbom.spdx.json` — Complete SBOM for Yocto system
- `sdk/sbom/sdk-sbom.spdx.json` — Complete SBOM for SDK

### CycloneDX Format

CycloneDX is a lightweight SBOM standard focused on security and vulnerability management. CycloneDX files in the Rockchip SDK use JSON format and contain the following key information:

- **bomFormat**: BOM format identifier (CycloneDX)
- **components**: List of software components, including name, version, type, etc.
- **vulnerabilities**: Known vulnerability information
- **dependencies**: Dependency relationships between components

**Use cases**: Suitable for vulnerability management, security analysis, and DevSecOps workflows.

**Example files**:
- `buildroot/sbom/sbom.cyclonedx.json` — SBOM for Buildroot system
- `debian/sbom/sbom.cyclonedx.json` — SBOM for Debian system
- `yocto/sbom/sbom.cyclonedx.json` — SBOM for Yocto system
- `sdk/sbom/sdk-sbom.cyclonedx.json` — SBOM for SDK

### CSV Format

CSV (Comma-Separated Values) format SBOM files present component information in tabular form, making it easy to view and analyze in spreadsheet software.

**Typical fields**:

| Field | Description |
|:---|:---|
| Package | Software package name |
| Version | Version number |
| License | License type |
| Supplier | Supplier |
| CVE | Associated CVE ID |

**Example files**:
- `buildroot/sbom/sbom.csv` — Buildroot system component list
- `debian/sbom/sbom.csv` — Debian system component list
- `yocto/sbom/sbom.csv` — Yocto system component list
- `sdk/sbom/sdk-sbom.csv` — SDK component list

### HTML Format

HTML format reports provide visualized SBOM and security scan results that can be viewed directly in a browser, making it easy for quick browsing and analysis.

**Use cases**: Suitable for quick viewing, presentations, and understanding by non-technical personnel.

**Example files**:
- `buildroot/sbom/sbom.html` — Buildroot system SBOM visualization report
- `debian/sbom/sbom.html` — Debian system SBOM visualization report
- `yocto/sbom/sbom.html` — Yocto system SBOM visualization report
- `sdk/sbom/sdk-sbom.html` — SDK SBOM visualization report

## Security Vulnerability Scan Description

### Buildroot System

The Buildroot system provides security vulnerability scan reports through the `security/` directory, containing 4 formats:

| File | Format | Description |
|:---|:---|:---|
| `cve-report.spdx.json` | SPDX JSON | Security scan results (SPDX format) |
| `cve-report.cyclonedx.json` | CycloneDX JSON | Security scan results (CycloneDX format) |
| `cve-report.csv` | CSV | Security scan summary table |
| `cve-report.html` | HTML | Security scan visualization report |

**Usage**: Open `sbom/buildroot/security/cve-report.html` in a browser to view the full report.

### Debian System

The Debian system provides security vulnerability scan reports through the `security/` directory, containing 4 formats:

| File | Format | Description |
|:---|:---|:---|
| `cve-report.spdx.json` | SPDX JSON | Security scan results (SPDX format) |
| `cve-report.cyclonedx.json` | CycloneDX JSON | Security scan results (CycloneDX format) |
| `cve-report.csv` | CSV | Security scan summary table |
| `cve-report.html` | HTML | Security scan visualization report |

**Usage**: Open `sbom/debian/security/cve-report.html` in a browser to view the full report.

### Yocto System

The Yocto system provides security vulnerability scan reports through the `security/` directory, containing 4 formats:

| File | Format | Description |
|:---|:---|:---|
| `cve-report.spdx.json` | SPDX JSON | Security scan results (SPDX format) |
| `cve-report.cyclonedx.json` | CycloneDX JSON | Security scan results (CycloneDX format) |
| `cve-report.csv` | CSV | Security scan summary table |
| `cve-report.html` | HTML | Security scan visualization report |

**Usage**: Open `sbom/yocto/security/cve-report.html` in a browser to view the full report.

### SDK System

The SDK provides security vulnerability scan reports through the `security/` directory, containing 4 formats:

| File | Format | Description |
|:---|:---|:---|
| `sdk-security-report.spdx.json` | SPDX JSON | SDK security scan results (SPDX format) |
| `sdk-security-report.cyclonedx.json` | CycloneDX JSON | SDK security scan results (CycloneDX format) |
| `sdk-security-report.csv` | CSV | SDK security scan summary table |
| `sdk-security-report.html` | HTML | SDK security scan visualization report |

**Usage**: Open `sbom/sdk/security/sdk-security-report.html` in a browser to view the full report.

### Kernel System

Kernel CVE reports are organized on a monthly basis and stored in the `sbom/kernel/cve/` directory. Each report includes:

- CVE vulnerability list related to the kernel for that month
- Affected kernel versions
- Vulnerability severity and fix status

**Latest CVE link**: See `sbom/kernel/Latest-Release-Rockchip-Kernel-CVEs-Link.txt`, or visit the Redmine platform for the latest information.

### U-Boot System

U-Boot CVE reports are stored in the `sbom/u-boot/cve/` directory, available in both Chinese and English:

| File | Description |
|:---|:---|
| `U-Boot-CVE-2026-06_CN.md` | U-Boot CVE report (Chinese) |
| `U-Boot-CVE-2026-06_EN.md` | U-Boot CVE report (English) |

**Latest CVE link**: See `sbom/u-boot/Latest-Release-Rockchip-U-Boot-CVEs-Link.txt`.

## License Compliance Notes

License information for the Buildroot system is contained in the `copyright/` directory:

- **software_copyright_list.csv**: License list for target-side software packages, including all third-party packages compiled by default in the SDK
- **host-software_copyright_list.csv**: License list for host-side toolchain and auxiliary tools
- **BUILDROOT_README**: Buildroot license compliance notes, including important information such as source code access and license file locations

> **Note**: Some Rockchip local packages (such as rockchip-mpp, rknpu2, rkwifibt, etc.) have not saved source code due to technical limitations. Developers need to obtain them independently according to license requirements.

## Frequently Asked Questions

### How to view SPDX format SBOM?

SPDX JSON files can be viewed using the following tools:
- **Online tools**: [SPDX Online Tools](https://tools.spdx.org/)
- **Command-line tools**: spdx-tools (can be installed via pip)
- **General editors**: Any text editor that supports JSON

### How to view CycloneDX format SBOM?

CycloneDX JSON files can be viewed using the following tools:
- **Online tools**: [CycloneDX BOM Validator](https://cyclonedx.org/tool-center/)
- **Command-line tools**: cyclonedx-bom (can be installed via npm)
- **General editors**: Any text editor that supports JSON

### How to search for CVE vulnerabilities of a specific component?

1. **Buildroot/Debian/Yocto system**: Search for the component name in the corresponding `security/cve-report.html`
2. **SDK**: Search for the component name in `sdk/security/sdk-security-report.html`
3. **Kernel CVE**: Look in the monthly reports under the `kernel/cve/` directory
4. **U-Boot CVE**: Look in the reports under the `u-boot/cve/` directory

### CVE Severity Level Description

| Level | CVSS Score | Description |
|:---|:---|:---|
| CRITICAL | 9.0 - 10.0 | Critical vulnerability, requires immediate fix |
| HIGH | 7.0 - 8.9 | High severity vulnerability, recommended to fix as soon as possible |
| MEDIUM | 4.0 - 6.9 | Medium severity vulnerability, recommended to evaluate and fix |
| LOW | 0.1 - 3.9 | Low severity vulnerability, fix as appropriate |

## Future Update Plans

We will continuously update the SBOM and security vulnerability scan reports to address security-related issues:

1. **Regular Updates**: Update kernel and U-Boot CVE reports monthly
2. **New Format Support**: Consider adding other SBOM formats (such as SWID, SPDX Lite, etc.) based on community needs
3. **Automated Scanning**: Gradually implement automation and continuous integration for security scanning
4. **Vulnerability Fix Tracking**: Establish a vulnerability fix tracking mechanism to timely update fix status
5. **Documentation Improvement**: Continuously improve documentation, adding usage examples and best practices

## Reference Links

- [SPDX Specification](https://spdx.dev/specifications/)
- [CycloneDX Specification](https://cyclonedx.org/specification/)
- [NVD - National Vulnerability Database](https://nvd.nist.gov/)
- [CVE Official Website](https://cve.mitre.org/)
- [VEX Overview](https://www.cisa.gov/sbom/vex)
- [Rockchip Redmine Platform](https://redmine.rock-chips.com/)
