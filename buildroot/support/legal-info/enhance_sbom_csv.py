#!/usr/bin/env python3
"""
Enhance a Buildroot legal-info manifest CSV into a customer-facing SBOM CSV.

The generated CSV keeps the original Buildroot columns and adds the fields
needed by the SPDX converter and customer review:
SHA256, SUPPLIER, COPYRIGHT_TEXT, CPE_ID, PURL, CPE23_TYPE,
PACKAGE_URL, PKG_TYPE, COMPONENT_SCOPE.
It also appends SDK-level components not produced by Buildroot legal-info,
such as Linux kernel, U-Boot, rkbin and Buildroot itself.
"""

import argparse
import csv
import hashlib
import os
import re
import sys
from pathlib import Path
from urllib.parse import quote

SCRIPT_DIR = Path(__file__).resolve().parent

try:
    from csv_to_spdx_v2 import (
        generate_cpe_string,
        generate_purl,
        infer_copyright,
        infer_supplier,
        load_package_info,
        match_package_info,
    )
except ImportError as exc:
    raise SystemExit(f"Cannot import csv_to_spdx_v2.py helpers: {exc}")


BASE_COLUMNS = [
    "PACKAGE",
    "VERSION",
    "LICENSE",
    "LICENSE FILES",
    "SOURCE ARCHIVE",
    "SOURCE SITE",
    "DEPENDENCIES WITH LICENSES",
]

EXTRA_COLUMNS = [
    "SHA256",
    "SUPPLIER",
    "COPYRIGHT_TEXT",
    "CPE_ID",
    "PURL",
    "CPE23_TYPE",
    "PACKAGE_URL",
    "PKG_TYPE",
    "COMPONENT_SCOPE",
]


PKG_TYPE_RULES = [
    (r"^(gcc|gcc-final|gcc-initial|binutils|make|cmake|meson|ninja|pkgconf)", "COMPILER"),
    (r"^(linux|linux-headers|kernel)", "KERNEL"),
    (r"^(u-boot|uboot|rkbin)", "BOOTLOADER"),
    (r"^(rockchip-mali|rknpu|rk.*firmware)", "FIRMWARE"),
    (r"^(busybox|bash|coreutils|dropbear|openssh|weston|chromium)", "APPLICATION"),
    (r"(font|tzdata|hwdata|ca-certificates)", "DATA"),
]


LICENSE_BY_PACKAGE = {
    "linux-kernel": "GPL-2.0-only",
    "u-boot": "GPL-2.0-or-later",
    "rkbin": "LicenseRef-ROCKCHIP",
    "buildroot": "GPL-2.0-or-later",
}


PACKAGE_OVERRIDES = {
    "ncurses": {
        "SUPPLIER": "Thomas E. Dickey",
        "CPE_ID": "cpe:2.3:a:gnu:ncurses:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) Thomas E. Dickey and contributors",
    },
    "libogg": {
        "SUPPLIER": "Xiph.Org Foundation",
        "CPE_ID": "cpe:2.3:a:xiph:libogg:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) Xiph.Org Foundation",
    },
    "libunistring": {
        "SUPPLIER": "Free Software Foundation",
        "CPE_ID": "cpe:2.3:a:gnu:libunistring:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) Free Software Foundation, Inc.",
    },
    "util-linux": {
        "SUPPLIER": "kernel.org util-linux project",
        "CPE_ID": "cpe:2.3:a:kernel:util-linux:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) util-linux contributors",
    },
    "util-linux-libs": {
        "SUPPLIER": "kernel.org util-linux project",
        "CPE_ID": "cpe:2.3:a:kernel:util-linux-libs:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) util-linux contributors",
    },
    "chromium-wayland": {
        "LICENSE": "BSD-3-Clause",
        "SUPPLIER": "Google LLC",
        "CPE_ID": "cpe:2.3:a:google:chrome:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) Google LLC and Chromium contributors",
        "SOURCE SITE": "https://github.com/rockchip-linux/chromium",
        "PURL": "pkg:generic/rockchip/chromium-wayland",
    },
    "dejavu": {
        "LICENSE": "LicenseRef-DejaVu-Fonts",
        "SUPPLIER": "DejaVu Fonts project",
        "CPE_ID": "cpe:2.3:a:dejavu_fonts_project:dejavu_fonts:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) Bitstream, Inc. and DejaVu Fonts contributors",
    },
    "dhrystone": {
        "LICENSE": "LicenseRef-Dhrystone",
        "SUPPLIER": "Reinhold P. Weicker",
        "CPE_ID": "cpe:2.3:a:dhrystone_project:dhrystone:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) Reinhold P. Weicker and Dhrystone contributors",
    },
    "whetstone": {
        "LICENSE": "LicenseRef-Whetstone",
        "LICENSE FILES": "whetstone.c",
        "SUPPLIER": "Painter Engineering, Inc.",
        "CPE_ID": "cpe:2.3:a:whetstone_project:whetstone:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) 1998 Painter Engineering, Inc.",
    },
    "gst1-plugins-bad": {
        "LICENSE": "LGPL-2.0-or-later AND GPL-2.0-or-later",
    },
    "rknpu2": {
        "SUPPLIER": "Rockchip Electronics Co., Ltd.",
        "CPE_ID": "cpe:2.3:a:rockchip:rknpu2:*:*:*:*:*:*:*:*",
        "SOURCE SITE": "https://github.com/rockchip-linux/rknpu2",
        "PURL": "pkg:generic/rockchip/rknpu2",
    },
    "rockchip-mali": {
        "SOURCE SITE": "https://github.com/rockchip-linux/libmali",
        "PURL": "pkg:generic/rockchip/rockchip-mali",
    },
    "rockchip-rga": {
        "SOURCE SITE": "https://github.com/airockchip/librga",
        "PURL": "pkg:generic/rockchip/rockchip-rga",
    },
    "rockchip-mpp": {
        "SOURCE SITE": "https://github.com/rockchip-linux/mpp",
        "PURL": "pkg:generic/rockchip/rockchip-mpp",
    },
    "host-buildroot": {
        "SUPPLIER": "Buildroot project",
        "CPE_ID": "cpe:2.3:a:buildroot:buildroot:*:*:*:*:*:*:*:*",
        "COPYRIGHT_TEXT": "Copyright (c) Buildroot contributors",
        "PKG_TYPE": "BUILD_SYSTEM",
        "PURL": "pkg:generic/buildroot/buildroot",
    },
}


# Curated CPE vendor/product corrections aligned with the NVD CPE Dictionary.
# These values intentionally override the generic "<package>_project" fallback
# for common packages where NVD uses a different vendor or product token.
CPE_NVD_OVERRIDES = {
    "alsa-lib": ("a", "alsa-project", "alsa-lib"),
    "alsa-plugins": ("a", "alsa-project", "alsa-plugins"),
    "alsa-utils": ("a", "alsa-project", "alsa-utils"),
    "alsa-ucm-conf": ("a", "alsa-project", "alsa-ucm-conf"),
    "bluez-alsa": ("a", "arkq", "bluez-alsa"),
    "dropbear": ("a", "dropbear_ssh_project", "dropbear_ssh"),
    "libglib2": ("a", "gnome", "glib"),
    "pcre2": ("a", "pcre", "pcre2"),
    "libzlib": ("a", "zlib", "zlib"),
    "zlib": ("a", "zlib", "zlib"),
    "readline": ("a", "gnu", "readline"),
    "eudev": ("a", "gentoo", "eudev"),
    "libsndfile": ("a", "libsndfile", "libsndfile"),
    "flac": ("a", "flac_project", "flac"),
    "libvorbis": ("a", "xiph", "libvorbis"),
    "opus": ("a", "opus-codec", "opus"),
    "libtool": ("a", "gnu", "libtool"),
    "libxml2": ("a", "xmlsoft", "libxml2"),
    "libxslt": ("a", "xmlsoft", "libxslt"),
    "tiff": ("a", "libtiff", "libtiff"),
    "jpeg-turbo": ("a", "libjpeg-turbo", "libjpeg-turbo"),
    "dosfstools": ("a", "dosfstools_project", "dosfstools"),
    "e2fsprogs": ("a", "e2fsprogs_project", "e2fsprogs"),
    "i2c-tools": ("a", "i2c-tools_project", "i2c-tools"),
    "iputils": ("a", "iputils_project", "iputils"),
    "libidn2": ("a", "gnu", "libidn2"),
    "libtirpc": ("a", "libtirpc_project", "libtirpc"),
    "pciutils": ("a", "mj.ucw", "pciutils"),
    "procps-ng": ("a", "procps-ng_project", "procps-ng"),
    "fontconfig": ("a", "fontconfig", "fontconfig"),
    "freetype": ("a", "freetype", "freetype"),
    "libpng": ("a", "libpng", "libpng"),
    "libdrm": ("a", "dri", "libdrm"),
    "pixman": ("a", "pixman", "pixman"),
    "chrony": ("a", "chrony_project", "chrony"),
    "libnss": ("a", "mozilla", "network_security_services"),
    "libnspr": ("a", "mozilla", "netscape_portable_runtime"),
    "dhcpcd": ("a", "dhcpcd_project", "dhcpcd"),
    "dnsmasq": ("a", "thekelleys", "dnsmasq"),
    "evtest": ("a", "freedesktop", "evtest"),
    "faad2": ("a", "audacious-media-player", "faad2"),
    "parted": ("a", "gnu", "parted"),
    "fluidsynth": ("a", "fluidsynth", "fluidsynth"),
    "gdk-pixbuf": ("a", "gnome", "gdk-pixbuf"),
    "glmark2": ("a", "glmark2_project", "glmark2"),
    "gst1-plugins-bad": ("a", "gstreamer", "gst-plugins-bad"),
    "gst1-plugins-base": ("a", "gstreamer", "gst-plugins-base"),
    "gst1-plugins-good": ("a", "gstreamer", "gst-plugins-good"),
    "gst1-plugins-ugly": ("a", "gstreamer", "gst-plugins-ugly"),
    "gstreamer1": ("a", "gstreamer", "gstreamer"),
    "gstreamer1-rockchip": ("a", "gstreamer", "gstreamer"),
    "libtheora": ("a", "xiph", "libtheora"),
    "libmpeg2": ("a", "libmpeg2_project", "libmpeg2"),
    "mjpegtools": ("a", "mjpegtools_project", "mjpegtools"),
    "rtmpdump": ("a", "rtmpdump_project", "rtmpdump"),
    "libsoup3": ("a", "gnome", "libsoup"),
    "libpsl": ("a", "rockdaboot", "libpsl"),
    "nghttp2": ("a", "nghttp2", "nghttp2"),
    "libv4l": ("a", "linuxtv", "v4l-utils"),
    "harfbuzz": ("a", "harfbuzz_project", "harfbuzz"),
    "hostapd": ("a", "w1.fi", "hostapd"),
    "libnl": ("a", "libnl_project", "libnl"),
    "iperf": ("a", "iperf2_project", "iperf2"),
    "libevdev": ("a", "freedesktop", "libevdev"),
    "libfribidi": ("a", "gnu", "fribidi"),
    "libinput": ("a", "freedesktop", "libinput"),
    "mtdev": ("a", "bitmath", "mtdev"),
    "libmad": ("a", "underbit", "libmad"),
    "libvpx": ("a", "webmproject", "libvpx"),
    "libxkbcommon": ("a", "xkbcommon", "libxkbcommon"),
    "make": ("a", "gnu", "make"),
    "memtester": ("a", "pyropus", "memtester"),
    "ntfs-3g": ("a", "tuxera", "ntfs-3g"),
    "numactl": ("a", "numactl_project", "numactl"),
    "perl": ("a", "perl", "perl"),
    "wpa_supplicant": ("a", "w1.fi", "wpa_supplicant"),
    "rt-tests": ("a", "linuxfoundation", "rt-tests"),
    "sox": ("a", "sox_project", "sox"),
    "strace": ("a", "strace_project", "strace"),
    "stress-ng": ("a", "stress-ng_project", "stress-ng"),
    "stress": ("a", "stress_project", "stress"),
    "tzdata": ("a", "iana", "time_zone_database"),
    "webp": ("a", "webmproject", "libwebp"),
    "wireless_tools": ("a", "hewlettpackard", "wireless_tools"),
    "xkeyboard-config": ("a", "freedesktop", "xkeyboard-config"),
    "wayland": ("a", "wayland", "wayland"),
    "wayland-utils": ("a", "wayland", "wayland"),
    "weston": ("a", "wayland", "weston"),
}


def read_make_version(makefile):
    values = {}
    if not makefile.exists():
        return "NOASSERTION"
    for line in makefile.read_text(errors="ignore").splitlines():
        match = re.match(r"^(VERSION|PATCHLEVEL|SUBLEVEL|EXTRAVERSION)\s*=\s*(.*)$", line)
        if match:
            values[match.group(1)] = match.group(2).strip()
    parts = [values.get("VERSION"), values.get("PATCHLEVEL"), values.get("SUBLEVEL")]
    version = ".".join([p for p in parts if p])
    return f"{version}{values.get('EXTRAVERSION', '')}" if version else "NOASSERTION"


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_tree(path):
    """Return a deterministic digest of file paths and file contents."""
    digest = hashlib.sha256()
    excluded_dirs = {
        ".git",
        ".repo",
        "__pycache__",
        "output",
        "dl",
        "build",
        "host",
        "target",
        "images",
        "legal-info",
    }
    if not path.exists():
        return ""
    if path.is_file():
        return sha256_file(path)
    for file_path in sorted(p for p in path.rglob("*") if p.is_file()):
        if any(part in excluded_dirs for part in file_path.relative_to(path).parts):
            continue
        rel = file_path.relative_to(path).as_posix()
        digest.update(rel.encode("utf-8"))
        digest.update(b"\0")
        digest.update(sha256_file(file_path).encode("ascii"))
        digest.update(b"\0")
    return digest.hexdigest()


def sha256_evidence(paths):
    """Return a deterministic digest over a small set of evidence files."""
    digest = hashlib.sha256()
    found = False
    for file_path in sorted(Path(p) for p in paths):
        if not file_path.exists() or not file_path.is_file():
            continue
        found = True
        digest.update(file_path.as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(sha256_file(file_path).encode("ascii"))
        digest.update(b"\0")
    return digest.hexdigest() if found else "NOASSERTION"


def warn(message):
    print(f"Warning: {message}", file=sys.stderr)


def first_existing_dir(root, names, pattern=None):
    for name in names:
        candidate = root / name
        if candidate.is_dir():
            return candidate
    if pattern:
        matches = sorted(p for p in root.glob(pattern) if p.is_dir())
        if matches:
            return matches[0]
    return None


def resolve_existing_path(path):
    """Resolve symlinks and return the path only when the target exists."""
    path = Path(path)
    try:
        resolved = path.resolve(strict=True)
    except OSError:
        return None
    return resolved if resolved.exists() and resolved.is_file() else None


def sha256_license_evidence(legal_info_dir, package, version):
    candidates = [
        legal_info_dir / "licenses" / f"{package}-{version}",
        legal_info_dir / "licenses" / package,
        legal_info_dir / "host-licenses" / f"{package}-{version}",
        legal_info_dir / "host-licenses" / package,
    ]
    files = []
    for candidate in candidates:
        if candidate.exists() and candidate.is_dir():
            files.extend(p for p in candidate.rglob("*") if p.is_file())
    return sha256_evidence(files)


def build_source_archive_index(legal_info_dir):
    index = {}
    for source_root in (legal_info_dir / "sources", legal_info_dir / "host-sources"):
        if not source_root.exists():
            continue
        for candidate in source_root.glob("*/*"):
            if candidate.is_file():
                index.setdefault(candidate.name, candidate)
    return index


def find_source_archive(legal_info_dir, package, version, source_archive, archive_index):
    if not source_archive:
        return None
    candidates = []
    package_dir = legal_info_dir / "sources" / f"{package}-{version}"
    candidates.append(package_dir / source_archive)
    indexed = archive_index.get(source_archive)
    if indexed:
        candidates.append(indexed)
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate
    return None


def infer_pkg_type(name):
    for pattern, pkg_type in PKG_TYPE_RULES:
        if re.search(pattern, name, re.IGNORECASE):
            return pkg_type
    return "LIBRARY" if name.startswith("lib") else "APPLICATION"


def format_supplier(value):
    if not value or value == "NOASSERTION":
        return "NOASSERTION"
    if value.startswith(("Organization:", "Person:")):
        return value
    return f"Organization: {value}"


def cpe_token(value):
    token = re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")
    return token or "upstream"


def product_token(value):
    """Normalize the board/chip product name used by SDK pseudo packages."""
    token = re.sub(r"[^a-z0-9]+", "-", str(value).lower()).strip("-")
    if token.startswith("rockchip-"):
        token = token[len("rockchip-"):]
    return token or "rockchip-product"


def cpe_version(value):
    token = re.sub(r"[^A-Za-z0-9._+~-]+", "_", str(value)).strip("_")
    return token or "na"


def normalize_cpe23(row):
    """Make CPEs concrete enough for vulnerability matching and review."""
    cpe = row.get("CPE_ID", "")
    parts = cpe.split(":")
    if len(parts) < 13 and len(parts) >= 6 and parts[0] == "cpe" and parts[1] == "2.3":
        parts.extend(["*"] * (13 - len(parts)))
    elif len(parts) > 13 and parts[0] == "cpe" and parts[1] == "2.3":
        warn(f"truncating malformed CPE with too many fields: {cpe}")
        parts = parts[:13]
    if len(parts) != 13 or parts[0] != "cpe" or parts[1] != "2.3":
        return row

    version = row.get("VERSION") or ""
    if parts[4] in ("", "*"):
        parts[4] = cpe_token(row["PACKAGE"])
    if parts[5] in ("", "*") and version and version != "NOASSERTION":
        parts[5] = cpe_version(version)

    override = CPE_NVD_OVERRIDES.get(row["PACKAGE"])
    if override:
        parts[2], parts[3], parts[4] = override

    # These attributes are not asserted by the Buildroot/SDK evidence. Customer
    # requested retaining CPE ANY ("*") for non-specific trailing attributes.
    for index in range(6, 13):
        if parts[index] in ("", "-"):
            parts[index] = "*"

    row["CPE_ID"] = ":".join(parts)
    return row


def remove_self_dependency(dependencies, package):
    """Remove manifest entries where a package depends on itself."""
    if not dependencies:
        return dependencies
    pattern = re.compile(rf"(?<!\S){re.escape(package)}\s+\[[^\]]*\]\s*")
    cleaned = pattern.sub("", dependencies)
    return re.sub(r"\s+", " ", cleaned).strip()


def apply_package_info(row, package_info):
    package = row["PACKAGE"]
    info = match_package_info(package, package_info) if package_info else None
    if info:
        row["SUPPLIER"] = row["SUPPLIER"] or info.get("supplier", "")
        row["COPYRIGHT_TEXT"] = row["COPYRIGHT_TEXT"] or info.get("copyright", "")
        row["CPE_ID"] = row["CPE_ID"] or info.get("cpe", "")
        row["PKG_TYPE"] = row["PKG_TYPE"] or info.get("pkg_type", "")

    row["SUPPLIER"] = row["SUPPLIER"] or infer_supplier(package) or "Unknown upstream project"
    row["COPYRIGHT_TEXT"] = row["COPYRIGHT_TEXT"] or infer_copyright(package) or f"Copyright information is provided by the upstream {package} license files."
    row["PKG_TYPE"] = row["PKG_TYPE"] or infer_pkg_type(package)
    row["CPE_ID"] = row["CPE_ID"] or generate_cpe_string("", package, row["VERSION"] or "*")
    row["PURL"] = row["PURL"] or generate_purl(package, row["VERSION"] or "0", row["SOURCE SITE"], row["SOURCE ARCHIVE"])
    if not row["PURL"]:
        row["PURL"] = f"pkg:generic/{quote(package.lower())}@{quote(row['VERSION'] or '0')}"
    row = normalize_cpe23(row)
    row["CPE23_TYPE"] = row["CPE_ID"]
    row["PACKAGE_URL"] = row["PURL"]
    row["SUPPLIER"] = format_supplier(row["SUPPLIER"])
    row["DEPENDENCIES WITH LICENSES"] = remove_self_dependency(
        row["DEPENDENCIES WITH LICENSES"],
        package,
    )
    return row


def apply_overrides(row, sdk_root):
    override = PACKAGE_OVERRIDES.get(row["PACKAGE"])
    if override:
        for key, value in override.items():
            row[key] = value
    if row["PACKAGE"] == "linux-headers":
        kernel_dir = first_existing_dir(sdk_root, ["kernel"], "kernel-*")
        kernel_version = (
            read_make_version(kernel_dir / "Makefile") if kernel_dir else row["VERSION"]
        )
        if row["VERSION"] == "custom":
            row["VERSION"] = kernel_version
        row["SOURCE SITE"] = "https://github.com/rockchip-linux/kernel"
        row["SOURCE ARCHIVE"] = f"linux-headers-{row['VERSION']}.tar.gz"
        row["CPE_ID"] = f"cpe:2.3:o:linux:linux_headers:{row['VERSION']}:*:*:*:*:*:*:*"
        row["PURL"] = f"pkg:generic/rockchip/linux-headers@{quote(row['VERSION'])}"
        if row["SHA256"] == "NOASSERTION" and kernel_dir:
            row["SHA256"] = sha256_evidence([
                kernel_dir / "Makefile",
                kernel_dir / "COPYING",
            ])
    if "Unknown upstream project" in row["SUPPLIER"]:
        row["SUPPLIER"] = f"{row['PACKAGE']} upstream project"
    if ":unknown:" in row["CPE_ID"]:
        product = cpe_token(row["PACKAGE"])
        row["CPE_ID"] = f"cpe:2.3:a:{product}_project:{product}:{row['VERSION'] or '*'}:*:*:*:*:*:*:*"
    row = normalize_cpe23(row)
    row["CPE23_TYPE"] = row["CPE_ID"]
    row["PACKAGE_URL"] = row["PURL"]
    row["SUPPLIER"] = format_supplier(row["SUPPLIER"])
    return row


def read_manifest_rows(
    input_csv,
    legal_info_dir,
    sdk_root,
    package_info,
    archive_index,
    component_scope,
    host_manifest=False,
):
    rows = []
    with input_csv.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for original in reader:
            source_package = (original.get("PACKAGE", "") or "").strip()

            row = {column: (original.get(column, "") or "").strip() for column in BASE_COLUMNS}
            row.update({column: "" for column in EXTRA_COLUMNS})
            if host_manifest and not row["PACKAGE"].startswith("host-"):
                row["PACKAGE"] = f"host-{row['PACKAGE']}"
            row["COMPONENT_SCOPE"] = component_scope
            if host_manifest:
                row["PKG_TYPE"] = "BUILD_TOOL"

            archive = find_source_archive(
                legal_info_dir,
                source_package,
                row["VERSION"],
                row["SOURCE ARCHIVE"],
                archive_index,
            )
            if archive:
                row["SHA256"] = sha256_file(archive)
            elif row["SOURCE SITE"].startswith("/") and Path(row["SOURCE SITE"]).exists():
                row["SHA256"] = sha256_license_evidence(
                    legal_info_dir,
                    source_package,
                    row["VERSION"],
                )
            else:
                row["SHA256"] = "NOASSERTION"
            if row["PACKAGE"] == "host-buildroot":
                row["SHA256"] = sha256_evidence([
                    legal_info_dir / "buildroot.config",
                    legal_info_dir / "README",
                ])

            rows.append(apply_overrides(apply_package_info(row, package_info), sdk_root))
    return rows


def enhance_manifest(
    input_csv,
    output_csv,
    legal_info_dir,
    sdk_root,
    package_info_yaml,
    host_manifest=None,
    require_product_artifacts=True,
    product_name="rockchip-product",
):
    package_info = load_package_info(package_info_yaml)
    archive_index = build_source_archive_index(legal_info_dir)
    rows = read_manifest_rows(
        input_csv,
        legal_info_dir,
        sdk_root,
        package_info,
        archive_index,
        "buildroot-target",
    )
    if host_manifest:
        rows.extend(read_manifest_rows(
            host_manifest,
            legal_info_dir,
            sdk_root,
            package_info,
            archive_index,
            "buildroot-host",
            host_manifest=True,
        ))

    rows.extend(create_sdk_component_rows(sdk_root, package_info, product_name))
    rows.extend(create_product_artifact_rows(
        sdk_root,
        package_info,
        require_product_artifacts,
        product_name,
    ))

    output_csv.parent.mkdir(parents=True, exist_ok=True)
    with output_csv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=BASE_COLUMNS + EXTRA_COLUMNS, quoting=csv.QUOTE_ALL)
        writer.writeheader()
        writer.writerows(rows)

    return rows


def create_sdk_component_rows(sdk_root, package_info, product_name):
    kernel_dir = first_existing_dir(sdk_root, ["kernel"], "kernel-*")
    uboot_dir = first_existing_dir(sdk_root, ["u-boot", "uboot"])
    rkbin_dir = first_existing_dir(sdk_root, ["rkbin"])
    buildroot_dir = first_existing_dir(sdk_root, ["buildroot"])
    product = product_token(product_name)
    product_cpe = cpe_token(product)
    product_upper = product.upper()
    device_tree_pkg = f"{product}-device-tree"
    ddr_pkg = f"{product}-ddr-init"
    bl31_pkg = f"{product}-trustzone-bl31"
    bl32_pkg = f"{product}-trustzone-bl32"
    trust_config_pkg = f"{product}-secure-boot-trust-config"
    kernel_version = read_make_version(kernel_dir / "Makefile") if kernel_dir else "NOASSERTION"
    uboot_version = read_make_version(uboot_dir / "Makefile") if uboot_dir else "NOASSERTION"
    kernel_source = kernel_dir.name if kernel_dir else "kernel"
    uboot_source = uboot_dir.name if uboot_dir else "u-boot"
    rkbin_source = rkbin_dir.name if rkbin_dir else "rkbin"
    components = [
        {
            "PACKAGE": "linux-kernel",
            "VERSION": kernel_version,
            "LICENSE": LICENSE_BY_PACKAGE["linux-kernel"],
            "LICENSE FILES": "COPYING LICENSES/",
            "SOURCE ARCHIVE": kernel_source,
            "SOURCE SITE": "https://github.com/rockchip-linux/kernel",
            "DEPENDENCIES WITH LICENSES": "gcc-final [GPL-3.0-with-GCC-exception] linux-headers [GPL-2.0]",
            "PKG_TYPE": "KERNEL",
            "COMPONENT_SCOPE": "sdk-kernel",
            "SUPPLIER": "The Linux Foundation",
            "COPYRIGHT_TEXT": "Copyright (c) Linus Torvalds and Linux kernel contributors",
            "CPE_ID": f"cpe:2.3:o:linux:linux_kernel:{kernel_version}:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": "u-boot",
            "VERSION": uboot_version,
            "LICENSE": LICENSE_BY_PACKAGE["u-boot"],
            "LICENSE FILES": "Licenses/ README",
            "SOURCE ARCHIVE": uboot_source,
            "SOURCE SITE": "https://github.com/rockchip-linux/u-boot",
            "DEPENDENCIES WITH LICENSES": "gcc-final [GPL-3.0-with-GCC-exception] rkbin [LicenseRef-ROCKCHIP]",
            "PKG_TYPE": "BOOTLOADER",
            "COMPONENT_SCOPE": "sdk-bootloader",
            "SUPPLIER": "DENX Software Engineering",
            "COPYRIGHT_TEXT": "Copyright (c) DENX Software Engineering and U-Boot contributors",
            "CPE_ID": f"cpe:2.3:a:denx:u-boot:{uboot_version}:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": "rkbin",
            "VERSION": "sdk-release",
            "LICENSE": LICENSE_BY_PACKAGE["rkbin"],
            "LICENSE FILES": "LICENSE",
            "SOURCE ARCHIVE": rkbin_source,
            "SOURCE SITE": "https://github.com/rockchip-linux/rkbin",
            "DEPENDENCIES WITH LICENSES": "",
            "PKG_TYPE": "FIRMWARE",
            "COMPONENT_SCOPE": "sdk-firmware",
            "SUPPLIER": "Rockchip Electronics Co., Ltd.",
            "COPYRIGHT_TEXT": "Copyright (c) Rockchip Electronics Co., Ltd.",
            "CPE_ID": "cpe:2.3:a:rockchip:rkbin:sdk-release:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": "buildroot",
            "VERSION": "sdk-release",
            "LICENSE": LICENSE_BY_PACKAGE["buildroot"],
            "LICENSE FILES": "COPYING",
            "SOURCE ARCHIVE": "buildroot",
            "SOURCE SITE": "https://github.com/buildroot/buildroot",
            "DEPENDENCIES WITH LICENSES": "gcc-final [GPL-3.0-with-GCC-exception]",
            "PKG_TYPE": "BUILD_SYSTEM",
            "COMPONENT_SCOPE": "sdk-build-system",
            "SUPPLIER": "Buildroot project",
            "COPYRIGHT_TEXT": "Copyright (c) Buildroot contributors",
            "CPE_ID": "cpe:2.3:a:buildroot:buildroot:sdk-release:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": device_tree_pkg,
            "VERSION": kernel_version,
            "LICENSE": "GPL-2.0-only",
            "LICENSE FILES": f"{kernel_source}/COPYING",
            "SOURCE ARCHIVE": device_tree_pkg,
            "SOURCE SITE": "https://github.com/rockchip-linux/kernel",
            "DEPENDENCIES WITH LICENSES": "linux-kernel [GPL-2.0-only]",
            "PKG_TYPE": "DATA",
            "COMPONENT_SCOPE": "sdk-device-tree",
            "SUPPLIER": "Rockchip Electronics Co., Ltd.",
            "COPYRIGHT_TEXT": "Copyright (c) Rockchip Electronics Co., Ltd. and Linux kernel contributors",
            "CPE_ID": f"cpe:2.3:o:rockchip:{product_cpe}_device_tree:{kernel_version}:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": "u-boot-spl",
            "VERSION": uboot_version,
            "LICENSE": "GPL-2.0-or-later",
            "LICENSE FILES": f"{uboot_source}/Licenses/",
            "SOURCE ARCHIVE": "u-boot-spl",
            "SOURCE SITE": "https://github.com/rockchip-linux/u-boot",
            "DEPENDENCIES WITH LICENSES": "u-boot [GPL-2.0-or-later] rkbin [LicenseRef-ROCKCHIP]",
            "PKG_TYPE": "BOOTLOADER",
            "COMPONENT_SCOPE": "sdk-bootloader-spl",
            "SUPPLIER": "DENX Software Engineering",
            "COPYRIGHT_TEXT": "Copyright (c) DENX Software Engineering and U-Boot contributors",
            "CPE_ID": f"cpe:2.3:a:denx:u-boot_spl:{uboot_version}:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": ddr_pkg,
            "VERSION": "1.09",
            "LICENSE": "LicenseRef-ROCKCHIP",
            "LICENSE FILES": "rkbin/LICENSE",
            "SOURCE ARCHIVE": f"{product}_ddr_1332MHz_v1.09.bin",
            "SOURCE SITE": "https://github.com/rockchip-linux/rkbin",
            "DEPENDENCIES WITH LICENSES": "rkbin [LicenseRef-ROCKCHIP]",
            "PKG_TYPE": "FIRMWARE",
            "COMPONENT_SCOPE": "sdk-ddr-firmware",
            "SUPPLIER": "Rockchip Electronics Co., Ltd.",
            "COPYRIGHT_TEXT": "Copyright (c) Rockchip Electronics Co., Ltd.",
            "CPE_ID": f"cpe:2.3:a:rockchip:{product_cpe}_ddr_init:1.09:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": bl31_pkg,
            "VERSION": "1.23",
            "LICENSE": "LicenseRef-ROCKCHIP",
            "LICENSE FILES": "rkbin/LICENSE",
            "SOURCE ARCHIVE": f"{product}_bl31_v1.23.elf",
            "SOURCE SITE": "https://github.com/rockchip-linux/rkbin",
            "DEPENDENCIES WITH LICENSES": "rkbin [LicenseRef-ROCKCHIP]",
            "PKG_TYPE": "FIRMWARE",
            "COMPONENT_SCOPE": "sdk-trustzone",
            "SUPPLIER": "Rockchip Electronics Co., Ltd.",
            "COPYRIGHT_TEXT": "Copyright (c) Rockchip Electronics Co., Ltd.",
            "CPE_ID": f"cpe:2.3:a:rockchip:{product_cpe}_bl31:1.23:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": bl32_pkg,
            "VERSION": "1.08",
            "LICENSE": "LicenseRef-ROCKCHIP",
            "LICENSE FILES": "rkbin/LICENSE",
            "SOURCE ARCHIVE": f"{product}_bl32_v1.08.bin",
            "SOURCE SITE": "https://github.com/rockchip-linux/rkbin",
            "DEPENDENCIES WITH LICENSES": "rkbin [LicenseRef-ROCKCHIP]",
            "PKG_TYPE": "FIRMWARE",
            "COMPONENT_SCOPE": "sdk-trustzone",
            "SUPPLIER": "Rockchip Electronics Co., Ltd.",
            "COPYRIGHT_TEXT": "Copyright (c) Rockchip Electronics Co., Ltd.",
            "CPE_ID": f"cpe:2.3:a:rockchip:{product_cpe}_bl32:1.08:*:*:*:*:*:*:*",
        },
        {
            "PACKAGE": trust_config_pkg,
            "VERSION": "sdk-release",
            "LICENSE": "LicenseRef-ROCKCHIP",
            "LICENSE FILES": "rkbin/LICENSE",
            "SOURCE ARCHIVE": f"{product_upper}TRUST.ini",
            "SOURCE SITE": "https://github.com/rockchip-linux/rkbin",
            "DEPENDENCIES WITH LICENSES": f"{bl31_pkg} [LicenseRef-ROCKCHIP] {bl32_pkg} [LicenseRef-ROCKCHIP]",
            "PKG_TYPE": "FIRMWARE",
            "COMPONENT_SCOPE": "sdk-secure-boot",
            "SUPPLIER": "Rockchip Electronics Co., Ltd.",
            "COPYRIGHT_TEXT": "Copyright (c) Rockchip Electronics Co., Ltd.",
            "CPE_ID": f"cpe:2.3:a:rockchip:{product_cpe}_secure_boot_config:sdk-release:*:*:*:*:*:*:*",
        },
    ]

    for row in components:
        for column in BASE_COLUMNS + EXTRA_COLUMNS:
            row.setdefault(column, "")
        package = row["PACKAGE"]
        if package == "linux-kernel":
            evidence = [kernel_dir / "Makefile", kernel_dir / "COPYING"] if kernel_dir else []
        elif package == "u-boot":
            evidence = [uboot_dir / "Makefile", uboot_dir / "Licenses" / "README"] if uboot_dir else []
        elif package == "rkbin":
            evidence = [rkbin_dir / "LICENSE"] if rkbin_dir else []
            if rkbin_dir:
                evidence.extend(rkbin_dir.glob(f"RKTRUST/{product_upper}TRUST.ini"))
        elif package == "buildroot":
            evidence = [
                buildroot_dir / "COPYING",
                buildroot_dir / "Makefile",
                buildroot_dir / "Config.in",
            ] if buildroot_dir else []
        elif package == device_tree_pkg:
            dts_dir = (
                kernel_dir / "arch" / "arm64" / "boot" / "dts" / "rockchip"
                if kernel_dir else None
            )
            evidence = list(dts_dir.glob(f"{product}*.dts*")) if dts_dir and dts_dir.is_dir() else []
        elif package == "u-boot-spl":
            evidence = []
            if uboot_dir:
                evidence.extend([
                    uboot_dir / "include" / "spl.h",
                    uboot_dir / "include" / "config" / "spl.h",
                ])
            if rkbin_dir:
                evidence.extend(rkbin_dir.glob(f"bin/**/{product}_spl*.bin"))
        elif package == ddr_pkg:
            evidence = list(rkbin_dir.glob(f"bin/**/{product}_ddr*.bin")) if rkbin_dir else []
        elif package == bl31_pkg:
            evidence = list(rkbin_dir.glob(f"bin/**/{product}_bl31*.elf")) if rkbin_dir else []
        elif package == bl32_pkg:
            evidence = list(rkbin_dir.glob(f"bin/**/{product}_bl32*.bin")) if rkbin_dir else []
        elif package == trust_config_pkg:
            evidence = list(rkbin_dir.glob(f"RKTRUST/{product_upper}TRUST.ini")) if rkbin_dir else []
        else:
            evidence = [Path(row["SOURCE SITE"])]
        row["SHA256"] = sha256_evidence(evidence)
        row["PURL"] = f"pkg:generic/rockchip/{quote(row['PACKAGE'])}@{quote(row['VERSION'])}"
        row = apply_overrides(apply_package_info(row, package_info), sdk_root)
    return components


def create_product_artifact_rows(
    sdk_root,
    package_info,
    require_all=True,
    product_name="rockchip-product",
):
    """Add final deliverable images so the SBOM covers product artifacts too."""
    image_dir = sdk_root / "output" / "update" / "Image"
    product = product_token(product_name)
    update_pkg = f"{product}-update-image"
    device_tree_pkg = f"{product}-device-tree"
    ddr_pkg = f"{product}-ddr-init"
    artifacts = [
        (update_pkg, "update.img", "product-update-image", "DATA", "product-image", "buildroot-rootfs-image [LicenseRef-Product-Image-Composite] linux-kernel [GPL-2.0-only] u-boot [GPL-2.0-or-later] rkbin [LicenseRef-ROCKCHIP]"),
        (f"{product}-update-raw-image", "update.raw.img", "product-update-raw-image", "DATA", "product-image", f"{update_pkg} [LicenseRef-Product-Image-Composite]"),
        ("buildroot-rootfs-image", "rootfs.img", "product-rootfs", "OPERATING_SYSTEM", "product-rootfs", "busybox [GPL-2.0-only] glibc [LGPL-2.1-or-later] linux-kernel [GPL-2.0-only]"),
        (f"{product}-boot-image", "boot.img", "product-boot", "OPERATING_SYSTEM", "product-boot", f"linux-kernel [GPL-2.0-only] {device_tree_pkg} [GPL-2.0-only]"),
        (f"{product}-recovery-image", "recovery.img", "product-recovery", "OPERATING_SYSTEM", "product-recovery", "linux-kernel [GPL-2.0-only] buildroot-rootfs-image [LicenseRef-Product-Image-Composite]"),
        (f"{product}-uboot-image", "uboot.img", "product-bootloader", "BOOTLOADER", "product-bootloader", "u-boot [GPL-2.0-or-later] rkbin [LicenseRef-ROCKCHIP]"),
        (f"{product}-miniloader", "MiniLoaderAll.bin", "product-bootloader", "BOOTLOADER", "product-bootloader", f"u-boot-spl [GPL-2.0-or-later] {ddr_pkg} [LicenseRef-ROCKCHIP] rkbin [LicenseRef-ROCKCHIP]"),
        (f"{product}-misc-image", "misc.img", "product-partition", "DATA", "product-partition", ""),
        (f"{product}-oem-image", "oem.img", "product-partition", "DATA", "product-partition", ""),
        (f"{product}-userdata-image", "userdata.img", "product-partition", "DATA", "product-partition", ""),
        (f"{product}-parameter-file", "parameter.txt", "product-partition-table", "DATA", "product-partition-table", ""),
        (f"{product}-package-file", "package-file", "product-update-manifest", "DATA", "product-update-manifest", ""),
    ]
    rows = []
    missing = []
    for package, image_name, version, pkg_type, scope, dependencies in artifacts:
        image_path = resolve_existing_path(image_dir / image_name)
        if not image_path:
            missing.append(image_dir / image_name)
            continue
        cpe_product = cpe_token(package)
        cpe_id = f"cpe:2.3:o:rockchip:{cpe_product}:{version}:*:*:*:*:*:*:*"
        row = {
            "PACKAGE": package,
            "VERSION": version,
            "LICENSE": "LicenseRef-Product-Image-Composite",
            "LICENSE FILES": "see component licenses",
            "SOURCE ARCHIVE": image_name,
            "SOURCE SITE": "output/update/Image",
            "DEPENDENCIES WITH LICENSES": dependencies,
            "SHA256": sha256_file(image_path),
            "SUPPLIER": "Rockchip Electronics Co., Ltd.",
            "COPYRIGHT_TEXT": "Copyright and license obligations are inherited from the contained product image components listed in this SBOM.",
            "CPE_ID": cpe_id,
            "PURL": f"pkg:generic/rockchip/{quote(package)}@{quote(version)}",
            "CPE23_TYPE": cpe_id,
            "PACKAGE_URL": f"pkg:generic/rockchip/{quote(package)}@{quote(version)}",
            "PKG_TYPE": pkg_type,
            "COMPONENT_SCOPE": scope,
        }
        row = apply_overrides(apply_package_info(row, package_info), sdk_root)
        rows.append(row)
    if missing:
        message = "missing product artifacts: " + ", ".join(str(path) for path in missing)
        if require_all:
            raise RuntimeError(message)
        warn(message)
    return rows


def main():
    parser = argparse.ArgumentParser(description="Enhance Buildroot legal-info manifest CSV for SBOM delivery")
    parser.add_argument("input_csv", type=Path, help="Buildroot legal-info manifest.csv")
    parser.add_argument("output_csv", type=Path, help="Enhanced SBOM CSV output path")
    parser.add_argument("--host-manifest", type=Path, help="Buildroot host-manifest.csv")
    parser.add_argument("--allow-missing-product-artifacts", action="store_true",
                        help="Warn instead of failing when expected product images are missing")
    parser.add_argument("--legal-info-dir", type=Path, default=Path.cwd())
    parser.add_argument("--sdk-root", type=Path, default=Path.cwd())
    parser.add_argument("--product-name", default="rockchip-product",
                        help="Product/chip name used for SDK artifact package names")
    parser.add_argument("--package-info", type=Path, default=SCRIPT_DIR / "sbom_package_info.yaml")
    args = parser.parse_args()

    if not args.input_csv.exists():
        print(f"Error: Input file '{args.input_csv}' not found", file=sys.stderr)
        return 1
    if args.host_manifest and not args.host_manifest.exists():
        print(f"Error: Host manifest '{args.host_manifest}' not found", file=sys.stderr)
        return 1

    try:
        rows = enhance_manifest(
            args.input_csv,
            args.output_csv,
            args.legal_info_dir,
            args.sdk_root,
            args.package_info,
            args.host_manifest,
            not args.allow_missing_product_artifacts,
            args.product_name,
        )
    except Exception as exc:
        print(f"Error: failed to generate enhanced SBOM CSV: {exc}", file=sys.stderr)
        return 1

    print(f"Enhanced CSV written: {args.output_csv}")
    print(f"Packages: {len(rows)}")
    print(f"Columns: {', '.join(BASE_COLUMNS + EXTRA_COLUMNS)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
