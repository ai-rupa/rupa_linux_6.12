#!/usr/bin/env python3
"""
CSV to SPDX 2.3 Converter Script (Enhanced Version)

This script converts Buildroot legal-info manifest CSV files to SPDX 2.3 JSON format.
Supports NTIA minimum elements compliance with:
- Supplier information
- SHA256 checksums
- CPE and PURL external references
- Copyright text
- Dependency relationships
- Package type classification

Usage:
    python csv_to_spdx_v2.py <input_csv> <output_spdx_json> [--spdx-version 2.3]

Example:
    python csv_to_spdx_v2.py manifest.csv sbom.spdx.json --spdx-version 2.3
"""

import csv
import json
import uuid
import hashlib
import argparse
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import urlparse

# Optional YAML support for external package info
try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False

SPDX_LICENSE_LIST_VERSION = "3.28.0"

# Auto-inference rules for supplier (based on package name patterns)
SUPPLIER_INFER_RULES = [
    # GNU projects
    (r'^gcc|^glibc|^binutils|^bash|^coreutils|^grep|^sed|^awk|^gawk|^tar|^make|^bison|^flex|^m4', 'Free Software Foundation'),
    (r'^grub|^gdb|^gmp|^mpfr|^mpc', 'Free Software Foundation'),
    # Linux Foundation
    (r'^(linux|linux-kernel|linux-headers|kernel)$|^libdrm|^libpciaccess',
     'The Linux Foundation'),
    # Freedesktop.org
    (r'^dbus|^wayland|^weston|^xorg|^mesa|^dri|^libxcb|^xserver|^at-spi', 'Freedesktop.org'),
    # GNOME
    (r'^glib|^gtk|^gstreamer|^pango|^cairo|^atk|^gdk|^harfbuzz|^libffi|^libfribidi', 'GNOME Foundation'),
    # Apache Software Foundation
    (r'^apache', 'Apache Software Foundation'),
    # Python Software Foundation
    (r'^python', 'Python Software Foundation'),
    # OpenSSL
    (r'^openssl|^libopenssl', 'OpenSSL Software Foundation'),
    # BusyBox
    (r'^busybox', 'BusyBox.net'),
    # DENX (U-Boot)
    (r'^uboot|^u-boot', 'DENX Software Engineering'),
    # zlib
    (r'^zlib', 'zlib.net'),
    # SQLite
    (r'^sqlite', 'D. Richard Hipp'),
    # curl
    (r'^libcurl|^curl', 'Daniel Stenberg'),
    # PNG
    (r'^libpng', 'PNG Reference Library'),
    # FreeType
    (r'^freetype', 'The FreeType Project'),
    # Expat
    (r'^expat', 'The Expat maintainers'),
    # systemd
    (r'^systemd|^udev|^eudev', 'systemd Project'),
    # BlueZ
    (r'^bluez', 'BlueZ Project'),
    # ALSA
    (r'^alsa', 'The ALSA Project'),
    # Netfilter
    (r'^iptables|^netfilter|^libnetfilter', 'Netfilter Project'),
    # OpenSSH
    (r'^openssh', 'OpenSSH'),
    # Dropbear
    (r'^dropbear', 'Matt Johnston'),
    # Vim
    (r'^vim', 'Bram Moolenaar'),
    # XZ
    (r'^xz', 'Lasse Collin'),
    # BZip2
    (r'^bzip2', 'Julian Seward'),
    # LZO
    (r'^lzo', 'Markus F.X.J. Oberhumer'),
    # Rockchip (local packages)
    (r'^rockchip|^rk(bin|npu|rga|mpp|[0-9])', 'Rockchip Electronics Co., Ltd.'),
    # Common libraries
    (r'^libbsd', 'FreeBSD Foundation'),
    (r'^libcap', 'Andrew G. Morgan'),
    (r'^libinput', 'Freedesktop.org'),
    (r'^libevdev', 'Freedesktop.org'),
    (r'^libical', 'Free Software Foundation'),
    (r'^libidn', 'Free Software Foundation'),
    (r'^libfuse', 'libfuse developers'),
    (r'^libgudev', 'GNOME Foundation'),
    (r'^kmod', 'Professional Open Source'),
    (r'^e2fsprogs', 'Theodore Ts\'o'),
    (r'^dosfstools', 'Werner Almesberger'),
    (r'^exfat', 'Andrew Nayenko'),
    (r'^dnsmasq', 'Simon Kelley'),
    (r'^chrony', 'Richard Cochran'),
    (r'^dhcpcd', 'Roy Marples'),
    (r'^iputils', 'iputils developers'),
    (r'^iperf', 'ESnet'),
    (r'^iw', 'Linux Wireless'),
    (r'^hostapd', 'Jouni Malinen'),
    (r'^evtest', 'Arnd Bergmann'),
    (r'^i2c-tools', 'Jean Delvare'),
    (r'^ca-certificates', 'Mozilla Foundation'),
    (r'^fontconfig', 'Freedesktop.org'),
    (r'^dejavu', 'DejaVu fonts'),
    (r'^liberation', 'Red Hat'),
    (r'^font-awesome', 'Fonticons Inc'),
    (r'^flac', 'Xiph.Org Foundation'),
    (r'^fluidsynth', 'FluidSynth developers'),
    (r'^faad2', 'FAAD2 developers'),
    (r'^jpeg-turbo', 'libjpeg-turbo Project'),
    (r'^gst1-plugins', 'GStreamer Project'),
    (r'^glmark2', 'Canonical'),
    (r'^hwdata', 'Vojtech Pavlik'),
]

# Auto-inference rules for copyright
COPYRIGHT_INFER_RULES = [
    (r'^gcc|^glibc|^binutils|^bash|^coreutils|^grep|^sed|^awk|^gawk|^tar|^make', 'Copyright (c) Free Software Foundation, Inc.'),
    (r'^(linux|linux-kernel|linux-headers|kernel)$',
     'Copyright (c) Linus Torvalds and contributors'),
    (r'^dbus|^wayland|^weston|^xorg|^mesa|^at-spi', 'Copyright (c) Freedesktop.org contributors'),
    (r'^glib|^gtk|^gstreamer|^pango|^cairo|^libffi|^libfribidi|^harfbuzz', 'Copyright (c) GNOME Foundation contributors'),
    (r'^python', 'Copyright (c) Python Software Foundation'),
    (r'^openssl', 'Copyright (c) The OpenSSL Project'),
    (r'^busybox', 'Copyright (c) Erik Andersen and contributors'),
    (r'^uboot|^u-boot', 'Copyright (c) DENX Software Engineering'),
    (r'^zlib', 'Copyright (c) Jean-loup Gailly and Mark Adler'),
    (r'^sqlite', 'Copyright (c) D. Richard Hipp'),
    (r'^libcurl|^curl', 'Copyright (c) Daniel Stenberg'),
    (r'^libpng', 'Copyright (c) PNG Reference Library'),
    (r'^freetype', 'Copyright (c) The FreeType Project'),
    (r'^expat', 'Copyright (c) Thai Open Source Software Center Ltd'),
    (r'^systemd|^udev|^eudev', 'Copyright (c) Lennart Poettering'),
    (r'^bluez', 'Copyright (c) BlueZ Project contributors'),
    (r'^alsa', 'Copyright (c) Jaroslav Kysela <perex@perex.cz>'),
    (r'^iptables|^netfilter', 'Copyright (c) Netfilter Project contributors'),
    (r'^openssh', 'Copyright (c) OpenSSH contributors'),
    (r'^dropbear', 'Copyright (c) Matt Johnston'),
    (r'^vim', 'Copyright (c) Bram Moolenaar'),
    (r'^xz', 'Copyright (c) Lasse Collin'),
    (r'^bzip2', 'Copyright (c) Julian Seward'),
    (r'^rockchip|^rk(bin|npu|rga|mpp|[0-9])',
     'Copyright (c) Rockchip Electronics Co., Ltd.'),
    (r'^libbsd', 'Copyright (c) FreeBSD Foundation'),
    (r'^libcap', 'Copyright (c) Andrew G. Morgan'),
    (r'^libinput|^libevdev', 'Copyright (c) Freedesktop.org'),
    (r'^libical', 'Copyright (c) Free Software Foundation'),
    (r'^libfuse', 'Copyright (c) libfuse developers'),
    (r'^libgudev', 'Copyright (c) GNOME Foundation'),
    (r'^kmod', 'Copyright (c) Professional Open Source'),
    (r'^e2fsprogs', 'Copyright (c) Theodore Ts\'o'),
    (r'^dosfstools', 'Copyright (c) Werner Almesberger'),
    (r'^exfat', 'Copyright (c) Andrew Nayenko'),
    (r'^dnsmasq', 'Copyright (c) Simon Kelley'),
    (r'^chrony', 'Copyright (c) Richard Cochran'),
    (r'^dhcpcd', 'Copyright (c) Roy Marples'),
    (r'^iputils', 'Copyright (c) iputils developers'),
    (r'^iperf', 'Copyright (c) ESnet'),
    (r'^iw', 'Copyright (c) Linux Wireless'),
    (r'^hostapd', 'Copyright (c) Jouni Malinen'),
    (r'^evtest', 'Copyright (c) Arnd Bergmann'),
    (r'^i2c-tools', 'Copyright (c) Jean Delvare'),
    (r'^ca-certificates', 'Copyright (c) Mozilla Foundation'),
    (r'^fontconfig', 'Copyright (c) Freedesktop.org'),
    (r'^dejavu', 'Copyright (c) DejaVu fonts'),
    (r'^liberation', 'Copyright (c) Red Hat'),
    (r'^font-awesome', 'Copyright (c) Fonticons Inc'),
    (r'^flac', 'Copyright (c) Xiph.Org Foundation'),
    (r'^fluidsynth', 'Copyright (c) FluidSynth developers'),
    (r'^faad2', 'Copyright (c) FAAD2 developers'),
    (r'^jpeg-turbo', 'Copyright (c) libjpeg-turbo Project'),
    (r'^gst1-plugins', 'Copyright (c) GStreamer Project'),
    (r'^glmark2', 'Copyright (c) Canonical'),
    (r'^hwdata', 'Copyright (c) Vojtech Pavlik'),
]


def generate_spdx_id(prefix="SPDXRef"):
    """Generate a unique SPDX identifier."""
    return f"{prefix}-{uuid.uuid4().hex[:8]}"


def generate_deterministic_id(name, version=""):
    """Generate deterministic SPDX ID based on name and version."""
    hash_val = hashlib.sha1(f"{name}-{version}".encode()).hexdigest()[:8]
    return f"SPDXRef-{hash_val}"


def normalize_license_id(license_str):
    """
    Normalize license string to SPDX license identifier.
    Maps common Buildroot license names to SPDX standard identifiers.
    """
    if not license_str or license_str.strip() == "":
        return "NOASSERTION"

    license_str = license_str.strip()

    # License mapping table - Buildroot to SPDX
    license_map = {
        "GPL-1.0-only": "GPL-1.0-only",
        "GPL-1.0-or-later": "GPL-1.0-or-later",
        "GPL-2.0-only": "GPL-2.0-only",
        "GPL-2.0-or-later": "GPL-2.0-or-later",
        "GPL-3.0-only": "GPL-3.0-only",
        "GPL-3.0-or-later": "GPL-3.0-or-later",
        "LGPL-2.0-only": "LGPL-2.0-only",
        "LGPL-2.0-or-later": "LGPL-2.0-or-later",
        "LGPL-2.1-only": "LGPL-2.1-only",
        "LGPL-2.1-or-later": "LGPL-2.1-or-later",
        "LGPL-3.0-only": "LGPL-3.0-only",
        "LGPL-3.0-or-later": "LGPL-3.0-or-later",
        "GPL-2.0": "GPL-2.0-only",
        "GPL-2.0+": "GPL-2.0-or-later",
        "GPL-3.0": "GPL-3.0-only",
        "GPL-3.0+": "GPL-3.0-or-later",
        "LGPL-2.0": "LGPL-2.0-only",
        "LGPL-2.1": "LGPL-2.1-only",
        "LGPL-2.1+": "LGPL-2.1-or-later",
        "LGPL-3.0": "LGPL-3.0-only",
        "LGPL-3.0+": "LGPL-3.0-or-later",
        "MIT": "MIT",
        "BSD-2-Clause": "BSD-2-Clause",
        "BSD-3-Clause": "BSD-3-Clause",
        "Apache-2.0": "Apache-2.0",
        "Zlib": "Zlib",
        "ISC": "ISC",
        "Public Domain": "LicenseRef-Public-Domain",
        "Public domain": "LicenseRef-Public-Domain",
        "Python-2.0": "Python-2.0",
        "OFL-1.1": "OFL-1.1",
        "MPL-2.0": "MPL-2.0",
        "Artistic": "Artistic-1.0",
        "Artistic or GPL-1.0+": "Artistic-1.0 OR GPL-1.0-or-later",
        "FTL or GPL-2.0+": "FTL OR GPL-2.0-or-later",
        "Xiph BSD-like": "BSD-3-Clause",
        "IJG": "IJG",
        "Libpng-2.0": "Libpng",
        "tiff license": "libtiff",
        "fontconfig license": "MIT",
        "blessing": "blessing",
        "bzip2-1.0.4": "LicenseRef-bzip2-1.0.4",

        "lmbench-license": "GPL-2.0-only",
        "lmbench license": "GPL-2.0-only",
        "MIT-like": "MIT",
        "XFree86-1.0": "XFree86-1.1",
        "DejaVu-Fonts": "Bitstream-Vera",
        "lmbench": "GPL-2.0-only",
        "glmark2": "GPL-3.0-only",
        "unknown": "LicenseRef-UNKNOWN",
        "ROCKCHIP": "LicenseRef-ROCKCHIP",
        "ARM": "LicenseRef-ARM-END-USER",
        "LicenseRef-ROCKCHIP": "LicenseRef-ROCKCHIP",
        "LicenseRef-ARM-END-USER": "LicenseRef-ARM-END-USER",
        "LicenseRef-UNKNOWN": "LicenseRef-UNKNOWN",
        "GPL-3.0-with-GCC-exception": "GPL-3.0-or-later WITH GCC-exception-3.1",
        "GPL-2.0, MIT-like with advertising clause (libss and libet)": "GPL-2.0-only AND MIT-advertising",
        "AFL-2.1 or GPL-2.0+ (library, tools), GPL-2.0+ (tools)": "AFL-2.1 OR GPL-2.0-or-later",
        "GPL-2.0+ (programs), LGPL-2.1+ (libraries)": "GPL-2.0-or-later AND LGPL-2.1-or-later",
        "LGPL-2.1 or MPL-1.1 (library)": "LGPL-2.1-only OR MPL-1.1",
        "LGPL-2.0+": "LGPL-2.0-or-later",
        "GPL-2.0 or GPL-3.0": "GPL-2.0-only OR GPL-3.0-only",
        "GPL-2.0 or BSD-3-Clause": "GPL-2.0-only OR BSD-3-Clause",
        "BSD-2-Clause (library), GPL-2.0+ (programs)": "BSD-2-Clause AND GPL-2.0-or-later",
        "MIT with advertising clause": "MIT-advertising",
        "MIT, BSD-2-Clause, Public domain, Unlicense, WTFPL": "MIT AND BSD-2-Clause AND LicenseRef-Public-Domain AND Unlicense AND WTFPL",
        "MIT/X11": "MIT",
        "MIT, ISC (ucdn library)": "MIT AND ISC",
        "GPL-3.0+ with exceptions": "GPL-3.0-or-later WITH GCC-exception",
        "GPL-3.0+ with exception": "GPL-3.0-or-later WITH Autoconf-exception",
        "FLEX": "LicenseRef-FLEX",
        "pkgconf license": "LicenseRef-pkgconf",
        "BSD-4-Clause": "BSD-4-Clause",
        "BSD-5-Clause": "BSD-3-Clause",
        "Beerware": "Beerware",
        "GPL-1.0+": "GPL-1.0-or-later",
        "MPL-1.0 or LGPL-2.1": "MPL-1.0 OR LGPL-2.1-only",
        "SGIv1": "SGI-B-1.0",
        "BSD-3-Clause or GPL-2.0": "BSD-3-Clause OR GPL-2.0-only",
        "Apache-2.0, MIT": "Apache-2.0 AND MIT",
        "Apache-2.0 or BSD-2-Clause": "Apache-2.0 OR BSD-2-Clause",
        "MIT, GPL-3.0+ (extra gettext)": "MIT AND GPL-3.0-or-later",
        "GPL-2.0+, LGPL-2.0+": "GPL-2.0-or-later AND LGPL-2.0-or-later",
        "LGPL-3.0+ or GPL-2.0+": "LGPL-3.0-or-later OR GPL-2.0-or-later",
        "LGPL-3.0+": "LGPL-3.0-or-later",
        "GPL-2.0+": "GPL-2.0-or-later",
        "GPL-3.0+": "GPL-3.0-or-later",
        "CC0-1.0": "CC0-1.0",
        "CC-BY-3.0": "CC-BY-3.0",
        "CC-BY-4.0": "CC-BY-4.0",
        "CC-BY-SA-4.0": "CC-BY-SA-4.0",
        "Unlicense": "Unlicense",
        "WTFPL": "WTFPL",
        # Legacy internal LicenseRef values found in earlier SBOM CSV/SPDX
        # revisions. These are mapped to SPDX-listed licenses so generated
        # files do not contain undefined LicenseRefs or placeholder texts.
        "LicenseRef-DejaVu-Fonts": "Bitstream-Vera",
        "LicenseRef-SGIv1": "SGI-B-1.0",
        "LicenseRef-MIT-like": "MIT",
        "LicenseRef-lmbench-license": "GPL-2.0-only",
        "LicenseRef-TIFF": "libtiff",
        "LicenseRef-Fontconfig": "MIT",
        "LicenseRef-BSD-5-Clause": "BSD-2-Clause AND BSD-3-Clause AND BSD-4-Clause AND MIT AND ISC AND Beerware",
        "LicenseRef-XFree86-1.0": "XFree86-1.1",
    }

    # Direct match
    if license_str in license_map:
        return license_map[license_str]
    if license_str.startswith("LicenseRef-"):
        return license_str

    def normalize_part(part):
        part = part.strip()
        if part in license_map:
            return license_map[part]
        if part.startswith("LicenseRef-"):
            return part

        # Buildroot often annotates a standard license with a component role:
        # "GPL-2.0+ (programs)", "LGPL-2.1+ (libraries)", etc.
        base = re.sub(r'\s*\([^)]*\)\s*$', '', part).strip()
        if base in license_map:
            return license_map[base]
        if base.startswith("LicenseRef-"):
            return base

        # Keep clearly non-SPDX but named licenses readable instead of hashing
        # them into opaque LicenseRef values.
        readable_refs = {
            "MIT-like with advertising clause": "LicenseRef-MIT-like-advertising",
            "XFree86 1.0": "XFree86-1.1",
            "SGIv1": "SGI-B-1.0",
            "SGI-B-2.0": "SGI-B-2.0",
            "Xiph BSD-like": "BSD-3-Clause",
            "IJG": "IJG",
            "tiff license": "libtiff",
            "fontconfig license": "MIT",
            "blessing": "blessing",
            "Public domain": "LicenseRef-Public-Domain",
            "Public Domain": "LicenseRef-Public-Domain",
            "lmbench-license": "GPL-2.0-only",
            "MIT-like": "MIT",
            "XFree86-1.0": "X11",
            "DejaVu-Fonts": "Bitstream-Vera",
            "ARM": "LicenseRef-ARM-END-USER",
            "lmbench": "GPL-2.0-only",
            "glmark2": "GPL-3.0-only",
            "unknown": "LicenseRef-UNKNOWN",
        }
        if part in readable_refs:
            return readable_refs[part]
        if base in readable_refs:
            return readable_refs[base]

        safe = re.sub(r'[^A-Za-z0-9.-]+', '-', base or part).strip('-')
        return f"LicenseRef-{safe[:48] or hashlib.md5(part.encode()).hexdigest()[:8]}"

    # Handle compound licenses (comma separated, respecting parentheses)
    if ", " in license_str:
        parts = []
        # Split by comma but respect parentheses
        depth = 0
        current = ""
        for char in license_str:
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            elif char == "," and depth == 0:
                if current.strip():
                    parts.append(current.strip())
                current = ""
                continue
            current += char
        if current.strip():
            parts.append(current.strip())

        normalized_parts = []
        for part in parts:
            normalized_parts.append(normalize_part(part))
        return join_unique_license_parts(normalized_parts, "AND")

    # Handle "or" licenses
    if " or " in license_str.lower():
        parts = []
        for part in re.split(r'\s+or\s+', license_str, flags=re.IGNORECASE):
            parts.append(normalize_part(part))
        return join_unique_license_parts(parts, "OR")

    # Handle "and" licenses
    if " and " in license_str.lower():
        parts = []
        for part in re.split(r'\s+and\s+', license_str, flags=re.IGNORECASE):
            parts.append(normalize_part(part))
        return join_unique_license_parts(parts, "AND")

    return normalize_part(license_str)


def generate_cpe_string(cpe_id, pkg_name, version):
    """
    Generate CPE 2.3 string from CPE ID components or infer from package info.
    Format: cpe:2.3:a:vendor:product:version:*:*:*:*:*:*:*
    """
    if cpe_id and cpe_id != "NOASSERTION":
        # CPE ID is already in proper format
        if cpe_id.startswith("cpe:"):
            return cpe_id
        # Otherwise construct from components
        return f"cpe:2.3:a:{cpe_id}:*:{version}:*:*:*:*:*:*:*"

    # Infer CPE from package name
    vendor = "unknown"
    product = pkg_name.lower().replace("_", "-")

    # Common vendor mappings
    vendor_mappings = {
        "gnu": "gnu",
        "linux": "linux",
        "alsa": "alsa-project",
        "busybox": "busybox",
        "openssl": "openssl",
        "python": "python",
        "perl": "perl",
        "ruby": "ruby-lang",
    }

    for key, val in vendor_mappings.items():
        if key in pkg_name.lower():
            vendor = val
            break

    return f"cpe:2.3:a:{vendor}:{product}:{version}:*:*:*:*:*:*:*"


def generate_purl(pkg_name, version, source_site, source_archive):
    """
    Generate Package URL (purl) for the package.
    Format: pkg:type/namespace/name@version
    """
    if not source_site or source_site == "NOASSERTION" or source_site == "not saved":
        return None

    # Determine package type based on source site
    purl_type = "generic"
    namespace = ""

    if "gnu.org" in source_site or "gnu." in source_site:
        purl_type = "gnu"
    elif "github.com" in source_site:
        purl_type = "github"
        # Extract namespace from URL
        match = re.search(r'github\.com/([^/]+)/', source_site)
        if match:
            namespace = match.group(1)
    elif "pypi" in source_site:
        purl_type = "pypi"
    elif "npm" in source_site or "npmjs" in source_site:
        purl_type = "npm"
    elif "rubygems" in source_site:
        purl_type = "gem"
    elif "maven" in source_site:
        purl_type = "maven"

    # Clean package name for purl
    clean_name = pkg_name.lower().replace("_", "-")

    purl = f"pkg:{purl_type}/"
    if namespace:
        purl += f"{namespace}/"
    purl += f"{clean_name}@{version}"

    return purl


def run_git_value(repo_path, args):
    """Return a single git value for local source trees, or None if unavailable."""
    try:
        result = subprocess.run(
            ["git", "-C", repo_path, *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=15,
        )
    except Exception:
        return None
    value = result.stdout.strip().splitlines()
    return value[0].strip() if value else None


def canonical_git_url(remote_url):
    if not remote_url:
        return None
    remote_url = remote_url.strip()
    if remote_url.endswith(".git"):
        remote_url = remote_url[:-4]
    return remote_url


def github_archive_download(source_site, version):
    parsed = urlparse(source_site)
    if parsed.netloc.lower() != "github.com":
        return None
    parts = [part for part in parsed.path.strip("/").split("/") if part]
    if len(parts) < 2:
        return None
    org, repo = parts[0], parts[1]
    if not version or version == "NOASSERTION":
        return f"https://github.com/{org}/{repo}"
    branch_names = {"master", "main", "develop"}
    if version in branch_names or version.startswith(("release-", "release/")):
        return f"https://github.com/{org}/{repo}/archive/refs/heads/{version}.tar.gz"
    if re.fullmatch(r"[0-9a-f]{7,40}", version):
        return f"https://github.com/{org}/{repo}/archive/{version}.tar.gz"
    return f"https://github.com/{org}/{repo}/archive/refs/tags/{version}.tar.gz"


def resolve_source_path(source_site, legal_info_dir=None, sdk_root=None):
    source_path = Path(source_site)
    candidates = [source_path]
    if not source_path.is_absolute():
        if legal_info_dir:
            candidates.append(Path(legal_info_dir) / source_path)
        if sdk_root:
            candidates.append(Path(sdk_root) / source_path)
    for candidate in candidates:
        if candidate.exists() and candidate.is_dir():
            return candidate
    return None


def build_download_location(
    source_site,
    source_archive,
    version,
    legal_info_dir=None,
    sdk_root=None,
):
    """Build an SPDX downloadLocation with an exact retrieval method when possible."""
    if not source_site or source_site == "not saved":
        return "NOASSERTION", None

    if source_site.startswith("output/"):
        return "NONE", f"Generated product artifact evidence path: {source_site.rstrip('/')}/{source_archive}"

    source_path = resolve_source_path(source_site, legal_info_dir, sdk_root)
    if source_path:
        remote = canonical_git_url(run_git_value(str(source_path), ["remote", "get-url", "rk"]))
        if not remote:
            remote = canonical_git_url(run_git_value(str(source_path), ["remote", "get-url", "origin"]))
        commit = run_git_value(str(source_path), ["rev-parse", "HEAD"])
        if remote and commit:
            short_commit = commit[:12]
            return (
                f"git+{remote}@{short_commit}",
                f"Acquisition method: local SDK Git tree; remote: {remote}; commit: {commit}",
            )
        return "NOASSERTION", f"Acquisition method: local SDK path; evidence path: {source_path}"

    if source_site.startswith("http"):
        github_download = github_archive_download(source_site, version)
        if github_download:
            return github_download, f"Acquisition method: GitHub archive generated from repository URL and version {version or 'NOASSERTION'}"
        if source_archive:
            return f"{source_site.rstrip('/')}/{source_archive}", "Acquisition method: Buildroot source site plus source archive filename"
        return source_site, "Acquisition method: upstream source site URL"

    if source_archive:
        return "NOASSERTION", f"Acquisition method: Buildroot source archive filename only; source archive: {source_archive}; source site: {source_site}"

    return source_site, "Acquisition method: Buildroot source site value"


def spdx_file_name(file_name):
    if not file_name:
        return "./NOASSERTION"
    if file_name.startswith("./") or file_name.startswith("/"):
        return file_name
    return f"./{file_name}"


def infer_file_types(file_name):
    suffixes = "".join(Path(file_name).suffixes).lower()
    if any(suffixes.endswith(ext) for ext in (".tar.gz", ".tar.xz", ".tar.bz2", ".tgz", ".zip", ".rar", ".gz", ".xz", ".bz2")):
        return ["ARCHIVE"]
    if any(suffixes.endswith(ext) for ext in (".img", ".bin", ".elf", ".itb")):
        return ["BINARY"]
    if any(suffixes.endswith(ext) for ext in (".json", ".spdx", ".spdx.json")):
        return ["SPDX"]
    return ["SOURCE"]


def get_pkg_type_category(pkg_type):
    """Map PKG_TYPE to SPDX primaryPackagePurpose."""
    type_map = {
        "BOOTLOADER": "FIRMWARE",
        "OPERATING_SYSTEM": "OPERATING-SYSTEM",
        "COMPILER": "LIBRARY",  # SPDX doesn't have COMPILER, use LIBRARY
        "LIBRARY": "LIBRARY",
        "APPLICATION": "APPLICATION",
        "FRAMEWORK": "LIBRARY",
        "KERNEL": "OPERATING-SYSTEM",
        "FIRMWARE": "FIRMWARE",
        "DATA": "FILE",
        "DEVICE_DRIVER": "LIBRARY",
        "BUILD_SYSTEM": "SOURCE",
    }
    return type_map.get(pkg_type.upper(), "SOURCE")


def parse_dependencies(dep_string):
    """
    Parse dependencies string from CSV.
    Format: "dep1 [license1] dep2 [license2] ..."
    Returns list of (dep_name, dep_license) tuples.
    """
    if not dep_string or dep_string.strip() == "":
        return []

    dependencies = []
    # Match pattern: name [license]
    pattern = r'(\S+)\s*\[([^\]]+)\]'
    matches = re.findall(pattern, dep_string)

    for name, license_str in matches:
        dependencies.append((name.strip(), license_str.strip()))

    return dependencies


def load_package_info(yaml_path):
    """Load external package info mapping from YAML file."""
    if not yaml_path:
        return []
    if not HAS_YAML:
        print(
            f"Warning: PyYAML is not installed; ignoring {yaml_path}",
            file=sys.stderr,
        )
        return []
    try:
        with open(yaml_path, 'r', encoding='utf-8') as f:
            return yaml.safe_load(f) or []
    except Exception as e:
        print(f"Warning: Could not load package info from {yaml_path}: {e}")
        return []


def match_package_info(pkg_name, info_list):
    """Match package name against info list (supports regex patterns)."""
    for info in info_list:
        if not isinstance(info, dict):
            print(f"Warning: ignoring invalid package info entry: {info!r}", file=sys.stderr)
            continue
        pattern = info.get('name_pattern', '')
        if not pattern:
            continue
        try:
            if re.match(pattern, pkg_name, re.IGNORECASE):
                return info
        except re.error as exc:
            print(f"Warning: ignoring invalid package info regex {pattern!r}: {exc}", file=sys.stderr)
    return None


def infer_supplier(pkg_name):
    """Infer supplier from package name using built-in rules."""
    for pattern, supplier in SUPPLIER_INFER_RULES:
        if re.search(pattern, pkg_name, re.IGNORECASE):
            return supplier
    return None


def infer_copyright(pkg_name):
    """Infer copyright text from package name using built-in rules."""
    for pattern, copyright_text in COPYRIGHT_INFER_RULES:
        if re.search(pattern, pkg_name, re.IGNORECASE):
            return copyright_text
    return None


def format_supplier(value):
    """Return an SPDX supplier string."""
    if not value or value == "NOASSERTION":
        return "NOASSERTION"
    if value.startswith(("Organization:", "Person:")):
        return value
    return f"Organization: {value}"


def license_refs_in_expression(license_expression):
    """Extract all LicenseRef identifiers from an SPDX license expression."""
    return set(re.findall(r'LicenseRef-[A-Za-z0-9.-]+', license_expression or ""))


def license_ref_see_alsos(ref):
    """Return public reference URLs for extracted LicenseRef texts when available."""
    see_also_map = {
        "LicenseRef-ARM-END-USER": [
            "https://developer.arm.com/downloads/-/mali-drivers",
        ],
        "LicenseRef-DejaVu-Fonts": [
            "https://github.com/dejavu-fonts/dejavu-fonts/blob/master/LICENSE",
        ],
        "LicenseRef-Dhrystone": [
            "http://www.netlib.org/benchmark/dhry-c",
        ],
        "LicenseRef-SGIv1": [
            "https://github.com/glmark2/glmark2/blob/master/COPYING.SGI",
        ],
        "LicenseRef-MIT-like": [
            "https://sourceforge.net/projects/iperf2/",
        ],
        "LicenseRef-lmbench-license": [
            "https://github.com/intel/lmbench/blob/master/COPYING",
            "https://github.com/intel/lmbench/blob/master/COPYING-2",
        ],
        "LicenseRef-ROCKCHIP": [
            "https://github.com/rockchip-linux/rkbin/blob/master/LICENSE",
        ],
        "LicenseRef-Whetstone": [
            "http://www.netlib.org/benchmark/whetstone.c",
        ],
        "LicenseRef-Product-Image-Composite": [
            "https://spdx.github.io/spdx-spec/v2.3/package-information/",
        ],
        "LicenseRef-bzip2-1.0.4": [
            "https://sourceware.org/bzip2/",
        ],
        "LicenseRef-Public-Domain": [
            "https://spdx.org/licenses/",
        ],
        "LicenseRef-TIFF": [
            "https://libtiff.gitlab.io/libtiff/",
        ],
        "LicenseRef-Fontconfig": [
            "https://gitlab.freedesktop.org/fontconfig/fontconfig",
        ],
        "LicenseRef-BSD-5-Clause": [
            "https://spdx.org/licenses/BSD-4-Clause.html",
        ],
        "LicenseRef-XFree86-1.0": [
            "https://spdx.org/licenses/XFree86-1.1.html",
        ],
    }
    return see_also_map.get(ref, [])


def built_in_license_ref_text(ref):
    """Return curated fallback text for known non-SPDX LicenseRef IDs."""
    text_map = {
        "LicenseRef-Public-Domain": (
            "Public domain notice. This package contains timezone database materials that are "
            "generally distributed as public domain data. This LicenseRef records the "
            "package-specific public-domain notice for SPDX tooling that does not accept "
            "Public-Domain as a listed license identifier."
        ),
        "LicenseRef-Dhrystone": (
            "Dhrystone Benchmark Program, Version 2.1.\n"
            "Author: Reinhold P. Weicker, Siemens AG.\n"
            "The source package does not provide a standalone license file. The package text "
            "identifies Dhrystone as public domain benchmark software; the SPDX file records "
            "this package-specific licensing notice without embedding the full source archive."
        ),
        "LicenseRef-ROCKCHIP": (
            "Rockchip proprietary software license notice. Rockchip grants a non-exclusive "
            "license to use, copy and distribute the software, and to modify and sublicense "
            "source-code portions where provided. The software is provided on an AS IS basis, "
            "without express, implied or statutory warranties. Except as expressly authorized "
            "by Rockchip in writing, decompilation, reverse engineering, disassembly, attempts "
            "to derive source code, and removal of copyright, patent or trademark notices are "
            "not permitted."
        ),
        "LicenseRef-Whetstone": (
            "C Converted Whetstone Double Precision Benchmark, Version 1.2, 22 March 1998.\n"
            "Copyright 1998 Painter Engineering, Inc. All Rights Reserved.\n"
            "Permission is granted to use, duplicate, and publish this text and program as long "
            "as it includes the entire original comment block and limited rights reference."
        ),
        "LicenseRef-Product-Image-Composite": (
            "Product image composite licensing notice.\n"
            "This LicenseRef is used only for assembled Rockchip deliverable image artifacts such "
            "as update.img, rootfs.img, boot.img, uboot.img, MiniLoaderAll.bin, partition images "
            "and update manifests. These artifacts are generated from multiple open-source and "
            "Rockchip-supplied components. The applicable license obligations are inherited from "
            "the component packages, files and dependency relationships listed elsewhere in this "
            "SPDX document. This notice does not replace the underlying component license "
            "declarations."
        ),
        "LicenseRef-ARM-END-USER": (
            "ARM End User License Agreement (EULA). This is a proprietary license from ARM/Mali "
            "for GPU binary drivers."
        ),
        "LicenseRef-bzip2-1.0.4": (
            "bzip2 license version 1.0.4. This LicenseRef represents the "
            "bzip2 license text referenced by packages that embed or reuse "
            "bzip2 code under the original bzip2 license terms."
        ),
    }
    return text_map.get(ref)


def atomic_license_infos(license_expression):
    """Return simple license identifiers for licenseInfoFromFiles fields."""
    if not license_expression or license_expression == "NOASSERTION":
        return ["NOASSERTION"]
    expression = re.sub(r'\s+WITH\s+[A-Za-z0-9+.-]+', '', license_expression)
    expression = expression.replace("(", " ").replace(")", " ")
    parts = re.split(r'\s+(?:AND|OR|WITH)\s+', expression)
    infos = []
    for part in parts:
        part = part.strip()
        if not part or part in {"AND", "OR", "WITH"}:
            continue
        if part not in infos:
            infos.append(part)
    return infos or ["NOASSERTION"]


def join_unique_license_parts(parts, operator):
    """Join license expression parts while preserving order and removing duplicates."""
    unique = []
    for part in parts:
        if part and part not in unique:
            unique.append(part)
    return f" {operator} ".join(unique)


def read_text_file(path, max_chars=None):
    try:
        text = Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    if max_chars and len(text) > max_chars:
        return text[:max_chars] + "\n\n[TRUNCATED: license text exceeds converter limit]"
    return text


def split_license_files(license_files):
    if not license_files:
        return []
    return [part.strip() for part in re.split(r'[\s,]+', license_files) if part.strip()]


def package_specific_license_text(legal_info_dir, sdk_root, pkg_name):
    legal_info_dir = Path(legal_info_dir) if legal_info_dir else None
    sdk_root = Path(sdk_root) if sdk_root else Path.cwd()

    if pkg_name == "lmbench" and legal_info_dir:
        base = legal_info_dir / "licenses" / "lmbench-3.0-a9"
        gpl = read_text_file(base / "COPYING").strip()
        restrictions = read_text_file(base / "COPYING-2")
        restrictions = restrictions.split("=========================================================================")[0].strip()
        restrictions = restrictions.split("If you have modifications or enhancements")[0].strip()
        return "\n\n".join(part for part in (gpl, restrictions) if part)

    if pkg_name == "dhrystone":
        return (
            "Dhrystone Benchmark Program, Version 2.1.\n"
            "Author: Reinhold P. Weicker, Siemens AG.\n"
            "The source package does not provide a standalone license file. "
            "The package text identifies Dhrystone as public domain benchmark software; "
            "the SPDX file records this package-specific licensing notice without embedding "
            "the full source archive."
        )

    if pkg_name == "whetstone":
        return (
            "C Converted Whetstone Double Precision Benchmark, Version 1.2, 22 March 1998.\n"
            "Copyright 1998 Painter Engineering, Inc. All Rights Reserved.\n"
            "Permission is granted to use, duplicate, and publish this text and program "
            "as long as it includes the entire original comment block and limited rights reference."
        )

    if pkg_name == "tzdata":
        return (
            "Public domain notice. This package contains timezone database "
            "materials that are generally distributed as public domain data. "
            "This LicenseRef records the package-specific public-domain notice "
            "for SPDX tooling that does not accept Public-Domain as a listed "
            "license identifier."
        )

    product_image_suffixes = (
        "-update-image",
        "-update-raw-image",
        "-boot-image",
        "-recovery-image",
        "-uboot-image",
        "-miniloader",
        "-misc-image",
        "-oem-image",
        "-userdata-image",
        "-parameter-file",
        "-package-file",
    )
    if pkg_name.endswith(product_image_suffixes) or pkg_name == "buildroot-rootfs-image":
        return (
            "Product image composite licensing notice.\n"
            "This LicenseRef is used only for assembled Rockchip deliverable image artifacts "
            "such as update.img, rootfs.img, boot.img, uboot.img, MiniLoaderAll.bin, "
            "partition images and update manifests. These artifacts are generated from "
            "multiple open-source and Rockchip-supplied components. The applicable license "
            "obligations are inherited from the component packages, files and dependency "
            "relationships listed elsewhere in this SPDX document. This notice does not "
            "replace the underlying component license declarations."
        )

    return ""


def collect_license_text(legal_info_dir, sdk_root, pkg_name, version, license_files):
    """Collect full license text for LicenseRef extractedText."""
    specific_text = package_specific_license_text(legal_info_dir, sdk_root, pkg_name)
    if specific_text:
        return specific_text

    legal_info_dir = Path(legal_info_dir) if legal_info_dir else None
    sdk_root = Path(sdk_root) if sdk_root else Path.cwd()
    file_names = split_license_files(license_files)
    candidate_files = []

    if legal_info_dir:
        for license_dir in (
            legal_info_dir / "licenses" / f"{pkg_name}-{version}",
            legal_info_dir / "licenses" / pkg_name,
        ):
            if license_dir.exists():
                if file_names:
                    for name in file_names:
                        candidate = license_dir / name
                        if candidate.is_file():
                            candidate_files.append(candidate)
                if not candidate_files:
                    candidate_files.extend(sorted(p for p in license_dir.rglob("*") if p.is_file()))

    for name in file_names:
        for candidate in (sdk_root / name, Path(name)):
            if candidate.is_file():
                candidate_files.append(candidate)

    # SDK component fallbacks.
    fallback_by_package = {
        "rkbin": [sdk_root / "rkbin" / "LICENSE"],
        "dhrystone": [legal_info_dir / "sources" / "dhrystone-2" / "dhry-c"] if legal_info_dir else [],
        "whetstone": [legal_info_dir / "sources" / "whetstone-1.2" / "whetstone.c"] if legal_info_dir else [],
    }
    candidate_files.extend(fallback_by_package.get(pkg_name, []))
    rockchip_firmware_suffixes = (
        "-ddr-init",
        "-trustzone-bl31",
        "-trustzone-bl32",
        "-secure-boot-trust-config",
    )
    if pkg_name.endswith(rockchip_firmware_suffixes):
        candidate_files.append(sdk_root / "rkbin" / "LICENSE")

    seen = set()
    sections = []
    for candidate in candidate_files:
        candidate = candidate.resolve()
        if candidate in seen or not candidate.is_file():
            continue
        seen.add(candidate)
        text = read_text_file(candidate)
        if text.strip():
            sections.append(text.strip())

    return "\n\n".join(sections)


def parse_csv_to_spdx(
    csv_path,
    document_name=None,
    creator_organization="Rockchip",
    spdx_version="2.3",
    package_info_yaml=None,
    legal_info_dir=None,
    sdk_root=None,
    document_namespace_prefix="https://spdx.rockchip.com/documents",
):
    """
    Parse Buildroot legal-info manifest CSV file and convert to SPDX 2.2/2.3 JSON.

    Expected CSV columns:
    PACKAGE, VERSION, LICENSE, LICENSE FILES, SOURCE ARCHIVE, SOURCE SITE,
    DEPENDENCIES WITH LICENSES, CPE_ID, SHA256, COPYRIGHT_TEXT, SUPPLIER, PKG_TYPE

    Args:
        csv_path: Path to the input CSV file
        document_name: Name for the SPDX document
        creator_organization: Organization name for the creator field
        spdx_version: "2.2" or "2.3"

    Returns:
        SPDX compliant JSON dictionary
    """
    packages = []
    files = []
    relationships = []
    annotations = []
    extracted_licensing_info = []
    license_refs_defined = set()

    # Load external package info mapping
    package_info_list = load_package_info(package_info_yaml)
    if package_info_list:
        print(f"Loaded {len(package_info_list)} package info entries from {package_info_yaml}")

    # Read CSV file
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        rows = list(reader)

        package_id_by_name = {}
        for row in rows:
            pkg_name = row.get('PACKAGE', '').strip('"').strip()
            version = row.get('VERSION', '').strip('"').strip()
            if pkg_name:
                package_id_by_name[pkg_name] = generate_deterministic_id(pkg_name, version)

        for row_idx, row in enumerate(rows):
            pkg_name = row.get('PACKAGE', '').strip('"').strip()
            if not pkg_name:
                continue

            version = row.get('VERSION', '').strip('"').strip()
            license_declared = row.get('LICENSE', '').strip('"').strip()
            license_files = row.get('LICENSE FILES', '').strip('"').strip()
            source_archive = row.get('SOURCE ARCHIVE', '').strip('"').strip()
            source_site = row.get('SOURCE SITE', '').strip('"').strip()
            dependencies_str = row.get('DEPENDENCIES WITH LICENSES', '').strip('"').strip()

            # New fields (may not exist in old CSV)
            cpe_id = (row.get('CPE_ID', '') or row.get('CPE23_TYPE', '')).strip('"').strip()
            purl_value = (row.get('PURL', '') or row.get('PACKAGE_URL', '')).strip('"').strip()
            sha256_hash = row.get('SHA256', '').strip('"').strip()
            copyright_text = row.get('COPYRIGHT_TEXT', '').strip('"').strip()
            supplier = row.get('SUPPLIER', '').strip('"').strip()
            pkg_type = row.get('PKG_TYPE', '').strip('"').strip()

            # Supplement with external package info if available
            if package_info_list:
                pkg_info = match_package_info(pkg_name, package_info_list)
                if pkg_info:
                    if not cpe_id and pkg_info.get('cpe'):
                        cpe_id = pkg_info['cpe']
                    if not supplier and pkg_info.get('supplier'):
                        supplier = pkg_info['supplier']
                    if not copyright_text and pkg_info.get('copyright'):
                        copyright_text = pkg_info['copyright']
                    if not pkg_type and pkg_info.get('pkg_type'):
                        pkg_type = pkg_info['pkg_type']

            # Generate SPDX ID
            pkg_spdx_id = generate_deterministic_id(pkg_name, version)

            # Normalize license
            normalized_license = normalize_license_id(license_declared)
            file_license_infos = atomic_license_infos(normalized_license)

            # Track LicenseRef definitions
            if "LicenseRef-" in normalized_license:
                refs = license_refs_in_expression(normalized_license)
                for ref in refs:
                    extracted_text = collect_license_text(legal_info_dir, sdk_root, pkg_name, version, license_files)
                    if not extracted_text:
                        extracted_text = built_in_license_ref_text(ref)
                    if not extracted_text:
                        extracted_text = f"Package-specific license notice for {ref}. Package: {pkg_name}; license files: {license_files or 'N/A'}"
                    if ref not in license_refs_defined:
                        license_refs_defined.add(ref)
                        extracted_licensing_info.append({
                            "licenseId": ref,
                            "name": f"License {ref}",
                            "extractedText": extracted_text,
                            "seeAlsos": license_ref_see_alsos(ref),
                        })
                    else:
                        for info in extracted_licensing_info:
                            if info.get("licenseId") == ref and info.get("extractedText", "").startswith("License text for"):
                                info["extractedText"] = extracted_text
                                break

            # Generate CPE string
            cpe_string = generate_cpe_string(cpe_id, pkg_name, version)

            # Generate PURL
            purl = purl_value or generate_purl(pkg_name, version, source_site, source_archive)
            if not purl:
                purl = f"pkg:generic/{pkg_name.lower()}@{version or '0'}"

            # Build external references
            external_refs = []
            if cpe_string and cpe_string != "NOASSERTION":
                external_refs.append({
                    "referenceCategory": "SECURITY",
                    "referenceType": "cpe23Type",
                    "referenceLocator": cpe_string
                })

            if purl:
                external_refs.append({
                    "referenceCategory": "PACKAGE-MANAGER",
                    "referenceType": "purl",
                    "referenceLocator": purl
                })

            # Build download location
            download_location, acquisition_evidence = build_download_location(
                source_site,
                source_archive,
                version,
                legal_info_dir,
                sdk_root,
            )

            # Build supplier - try external info, then auto-infer, then NOASSERTION
            if not supplier or supplier == "NOASSERTION":
                inferred_supplier = infer_supplier(pkg_name)
                if inferred_supplier:
                    supplier = inferred_supplier
                else:
                    supplier = "NOASSERTION"
            supplier = format_supplier(supplier)

            # Build copyright text - try external info, then auto-infer, then NOASSERTION
            if not copyright_text or copyright_text == "NOASSERTION":
                inferred_copyright = infer_copyright(pkg_name)
                if inferred_copyright:
                    copyright_text = inferred_copyright
                else:
                    copyright_text = "NOASSERTION"

            package = {
                "SPDXID": pkg_spdx_id,
                "name": pkg_name,
                "versionInfo": version if version else "NOASSERTION",
                "downloadLocation": download_location,
                "filesAnalyzed": False,
                "licenseConcluded": normalized_license,
                "licenseDeclared": normalized_license,
                "copyrightText": copyright_text,
                "supplier": supplier,
                "licenseInfoFromFiles": file_license_infos,
            }

            # Add SHA256 checksum if available
            if sha256_hash and sha256_hash != "NOASSERTION":
                package["checksums"] = [
                    {
                        "algorithm": "SHA256",
                        "checksumValue": sha256_hash
                    }
                ]

            # Add external references
            if external_refs:
                package["externalRefs"] = external_refs

            # Add SPDX 2.3 primaryPackagePurpose
            if spdx_version == "2.3" and pkg_type:
                package["primaryPackagePurpose"] = get_pkg_type_category(pkg_type)

            # Add license files as comment if available
            if license_files:
                package["comment"] = f"License files: {license_files}"
            package["comment"] = (
                f"{package.get('comment', '')}; CPE: {cpe_string}; PURL: {purl}"
            ).strip("; ")
            if acquisition_evidence:
                package["comment"] = f"{package.get('comment', '')}; {acquisition_evidence}".strip("; ")

            packages.append(package)

            # Parse and create dependency relationships
            dependencies = parse_dependencies(dependencies_str)
            for dep_name, dep_license in dependencies:
                if dep_name == pkg_name:
                    continue
                if dep_name not in package_id_by_name:
                    continue
                dep_spdx_id = package_id_by_name[dep_name]
                relationship = {
                    "spdxElementId": pkg_spdx_id,
                    "relationshipType": "DEPENDS_ON",
                    "relatedSpdxElement": dep_spdx_id
                }
                relationships.append(relationship)

    # Generate document name
    if not document_name:
        document_name = Path(csv_path).stem

    # Create main package to describe the overall product
    main_pkg_id = "SPDXRef-Package-main"
    main_pkg = {
        "SPDXID": main_pkg_id,
        "name": document_name,
        "versionInfo": "1.0",
        "downloadLocation": "NOASSERTION",
        "filesAnalyzed": False,
        "licenseConcluded": "NOASSERTION",
        "licenseDeclared": "NOASSERTION",
        "copyrightText": "NOASSERTION",
        "supplier": format_supplier(creator_organization),
    }
    if spdx_version == "2.3":
        main_pkg["primaryPackagePurpose"] = "SOURCE"
    packages.insert(0, main_pkg)

    relationships.insert(0, {
        "spdxElementId": "SPDXRef-DOCUMENT",
        "relationshipType": "DESCRIBES",
        "relatedSpdxElement": main_pkg_id
    })

    # Add DESCRIBES relationship from main package to all other packages
    for pkg in packages[1:]:
        relationships.append({
            "spdxElementId": main_pkg_id,
            "relationshipType": "DESCRIBES",
            "relatedSpdxElement": pkg["SPDXID"]
        })

    created_at = datetime.now(timezone.utc)
    namespace_uuid = uuid.uuid4()

    namespace_prefix = (
        document_namespace_prefix or "https://spdx.rockchip.com/documents"
    ).rstrip("/")
    safe_document_name = re.sub(r"[^A-Za-z0-9._-]+", "-", document_name).strip("-")

    # Create SPDX document. Keep relationships before packages so reviewers
    # and tools that preview only the leading document fields still see the
    # document-level DESCRIBES relationship immediately.
    spdx_document = {
        "spdxVersion": f"SPDX-{spdx_version}",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": document_name,
        "documentNamespace": f"{namespace_prefix}/{safe_document_name}-{created_at.strftime('%Y%m%d%H%M%S')}-{namespace_uuid}",
        "creationInfo": {
            "created": created_at.strftime('%Y-%m-%dT%H:%M:%SZ'),
            "creators": [
                f"Organization: {creator_organization}",
                "Tool: csv-to-spdx-v2.py"
            ],
            "licenseListVersion": SPDX_LICENSE_LIST_VERSION
        },
    }

    if files:
        # Deliberately added after relationships below.
        pass

    if relationships:
        spdx_document["relationships"] = relationships

    spdx_document["packages"] = packages

    if files:
        spdx_document["files"] = files

    # Add extracted licensing info if any
    if extracted_licensing_info:
        spdx_document["hasExtractedLicensingInfos"] = extracted_licensing_info

    return spdx_document


def main():
    parser = argparse.ArgumentParser(
        description="Convert Buildroot legal-info manifest CSV to SPDX 2.2/2.3 JSON format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('input_csv', help='Input CSV file path')
    parser.add_argument('output_json', help='Output SPDX JSON file path')
    parser.add_argument('--document-name', '-n', help='SPDX document name')
    parser.add_argument('--creator', '-c', default='Rockchip',
                        help='Creator organization name')
    parser.add_argument('--spdx-version', '-v', default='2.3', choices=['2.2', '2.3'],
                        help='SPDX version (default: 2.3)')
    parser.add_argument('--package-info', '-p',
                        help='YAML file with package info mapping (supplier, CPE, etc.)')
    parser.add_argument('--legal-info-dir',
                        help='Buildroot legal-info directory used to read full license text')
    parser.add_argument('--sdk-root', default='.',
                        help='SDK root directory used to read local license text')
    parser.add_argument('--document-namespace-prefix',
                        default='https://spdx.rockchip.com/documents',
                        help='SPDX document namespace URI prefix')

    args = parser.parse_args()

    # Validate input file
    if not Path(args.input_csv).exists():
        print(f"Error: Input file '{args.input_csv}' not found")
        return 1

    # Convert
    print(f"Converting {args.input_csv} to SPDX {args.spdx_version} format...")
    spdx_doc = parse_csv_to_spdx(
        args.input_csv,
        args.document_name,
        args.creator,
        args.spdx_version,
        args.package_info,
        args.legal_info_dir,
        args.sdk_root,
        args.document_namespace_prefix
    )

    # Write output
    with open(args.output_json, 'w', encoding='utf-8') as f:
        json.dump(spdx_doc, f, indent=2, ensure_ascii=False)

    print(f"Successfully created SPDX document: {args.output_json}")
    print(f"  - SPDX Version: {spdx_doc['spdxVersion']}")
    print(f"  - Packages: {len(spdx_doc['packages'])}")
    if 'relationships' in spdx_doc:
        print(f"  - Relationships: {len(spdx_doc['relationships'])}")
    if spdx_doc.get('hasExtractedLicensingInfos'):
        print(f"  - Extracted Licenses: {len(spdx_doc['hasExtractedLicensingInfos'])}")

    # Print statistics
    component_packages = [p for p in spdx_doc['packages'] if p.get('SPDXID') != 'SPDXRef-Package-main']
    pkgs_with_cpe = sum(1 for p in component_packages if 'externalRefs' in p and any(r.get('referenceType') == 'cpe23Type' for r in p['externalRefs']))
    pkgs_with_purl = sum(1 for p in component_packages if 'externalRefs' in p and any(r.get('referenceType') == 'purl' for r in p['externalRefs']))
    pkgs_with_checksum = sum(1 for p in component_packages if 'checksums' in p)
    pkgs_with_supplier = sum(1 for p in component_packages if p.get('supplier') != 'NOASSERTION')
    pkgs_with_copyright = sum(1 for p in component_packages if p.get('copyrightText') != 'NOASSERTION')

    total = len(component_packages)
    print(f"\n--- Compliance Statistics ---")
    print(f"  - Packages with CPE: {pkgs_with_cpe}/{total}")
    print(f"  - Packages with PURL: {pkgs_with_purl}/{total}")
    print(f"  - Packages with SHA256: {pkgs_with_checksum}/{total}")
    print(f"  - Packages with Supplier: {pkgs_with_supplier}/{total}")
    print(f"  - Packages with Copyright: {pkgs_with_copyright}/{total}")

    return 0


if __name__ == '__main__':
    exit(main())
