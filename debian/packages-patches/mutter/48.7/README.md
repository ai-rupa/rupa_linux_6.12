# mutter patches — NV12/P010/NV15 GPU Compositing via GL_OES_EGL_image_external

Enable direct GPU compositing of NV12/P010/NV15 DMA-buf in GNOME/mutter for
GPUs that do not support per-plane EGL import of the NV12 UV-plane as GR88.

Tested on RK3572 with Mali G310 (Debian 13 / GNOME 48).

## Problem

Mali G310 cannot import the NV12 UV-plane as GR88 via EGL per-plane import.
The standard two-plane NV12 path in mutter fails, forcing a CPU SHM fallback.
NV15 (NV12_10LE40, from MPP H.264 High 10-bit decoder) has no mutter support
at all in upstream.

## Solution

Import the full NV12/P010/NV15 DMA-buf as a single EGL image via
`GL_OES_EGL_image_external`, sampling through `samplerExternalOES`. Same
approach as Weston's `is_mali_egl` branch in `import_simple_dmabuf()`.

Key implementation notes:
- Mali G310 requires `EGL_YUV_COLOR_SPACE_HINT_EXT` and
  `EGL_SAMPLE_RANGE_HINT_EXT` for YUV EGL import (Weston patch 0018).
- NV12/P010 use BT.601; NV15 uses BT.709 (H.264 High 10-bit standard).
- Both planes must be passed: PLANE0(fd[0]) + PLANE1(fd[1]), matching
  Weston's `import_simple_dmabuf` which uses `attributes->n_planes`.
  Note: Weston's `pixel-formats.c` has `NV15 .num_planes=1` but that
  entry is only for DRM FB import, not for EGL GL import.
- `DRM_FORMAT_MOD_INVALID` is normalised to `DRM_FORMAT_MOD_LINEAR`.
- Vertex shader must NOT declare `samplerExternalOES` (causes GLSL link
  error); only the fragment shader declares it.
- `meta_multi_texture_new` takes transfer-full of the textures array;
  must use `g_new()`, not a stack address.

## Patches

| Patch | Description |
|-------|-------------|
| `0001-cogl-gl-oes-egl-image-external.patch` | cogl: detect GL_OES_EGL_image_external; add samplerExternalOES shader path |
| `0002-mutter-nv12-p010-gpu-compositing.patch` | mutter: NV12/P010 full-buffer EGL import via GL_OES_EGL_image_external |
| `0003-mutter-nv15-external-oes.patch` | mutter: NV15 (NV12_10LE40) direct EGL import; BT.709; PLANE0+PLANE1 |
| `0004-mutter-afbc-yuv420.patch` | mutter: AFBC YUV420_8BIT/10BIT external-OES GPU compositing (single-plane) |

## Modified Files

| File | Change |
|------|--------|
| `cogl-driver-gles2.c` | Detect `GL_OES_EGL_image_external`; set feature flag |
| `cogl-pipeline-fragend-glsl.c` | Emit `samplerExternalOES` for external OES textures |
| `cogl-pipeline-vertend-glsl.c` | Skip sampler declaration in vertex shader |
| `meta-multi-texture-format.{c,h,private.h}` | Add `NV12_EXT_OES` / `P010_EXT_OES` format entries |
| `meta-wayland-dma-buf.c` | `realize_nv12/p010/nv15_external_oes()` + `init_formats()` fallback |
| `meta-wayland-dma-buf.c` (0004) | `realize_afbc_external_oes()` + AFBC format advertising in `init_formats()` |

## AFBC compositing (0004)

Rockchip waylandsink sends AFBC-compressed NV12/NV15 as a **single-plane**
buffer re-typed to `DRM_FORMAT_YUV420_8BIT` (8-bit) / `DRM_FORMAT_YUV420_10BIT`
(10-bit) carrying `DRM_AFBC_MODIFIER`.  mutter has no `format_info` for these
Mali pseudo-formats, so without this patch negotiation fails with
`not-negotiated (-4)` (same situation NV15 had).

Key implementation notes:
- **AFBC block size is per-SoC.** The GStreamer stack tags every AFBC buffer
  16x16 (`0x0800000000000041`), correct for most Rockchip SoCs, but RK3572's
  rkvdec emits **32x8** (`0x0800000000000042`) — cf. the kernel's dedicated
  `format_modifiers_afbc_tiled_for_rk3572[]` in `rockchip_vop2_reg.c`. Importing
  32x8 data as 16x16 makes Mali read twice the superblock columns → a
  side-by-side **double image**. `meta_detect_rk_soc()` reads
  `/proc/device-tree/compatible` and selects 32x8 on RK3572, 16x16 elsewhere.
- **Single plane only** — AFBC packs Y+UV into one compressed buffer; pass
  PLANE0 with the AFBC modifier, never PLANE1 (unlike the linear NV12 path).
- fourcc passed to EGL is the original `dma_buf->drm_format`
  (`YUV420_8BIT` / `_10BIT`). No colorimetry travels with the dma-buf, so the
  YUV→RGB hint uses a resolution heuristic: `>= 720p` (and all 10-bit) → BT.709,
  else BT.601.
- Reuses `META_MULTI_TEXTURE_FORMAT_NV12_EXT_OES` + `alloc_nv12_external_oes`
  + the samplerExternalOES shader (0002/0003) — no new format/shader.
- `init_formats()` advertises `YUV420_8BIT`/`_10BIT` via `add_format()`:
  Mali G310 reports the AFBC modifier through `eglQueryDmaBufModifiers`, so the
  automatic path works (no manual format/modifier injection needed).
- The Mali texture unit decompresses AFBC during sampling, roughly halving
  texture-read bandwidth when the compositor downscales the source.

Verified on RK3572 (Mali G310, 1080p panel): AV1 8K30 through waylandsink
renders a correct **single image** with the 32x8 modifier (16x16 produced the
side-by-side double image) and holds ~30fps with performance DVFS governors,
versus heavy frame drops on the linear NV12 path. H264 8K30 unaffected.
(Residual jitter at default `simple_ondemand`/`schedutil` governors is a DVFS
matter, not an AFBC one — GPU sat at 300MHz; out of scope for this patch.)

To exercise this path the GStreamer `mppvideodec` must emit AFBC, which requires
`GST_MPP_VIDEODEC_DEFAULT_ARM_AFBC=1` in the environment. The debian overlay
enables it per-SoC in `/etc/profile.d/gst.sh` (gated to RK3572).

## Base

- mutter 48.7-0+deb13u1, arm64

## Apply

```bash
apt-get source mutter=48.7-0+deb13u1
cd mutter-48.7
git init && git add -A && git commit -q -m upstream
git config user.email "you@example.com" && git config user.name "Your Name"
git am ../packages-patches/mutter/48.7/0001-cogl-gl-oes-egl-image-external.patch
git am ../packages-patches/mutter/48.7/0002-mutter-nv12-p010-gpu-compositing.patch
git am ../packages-patches/mutter/48.7/0003-mutter-nv15-external-oes.patch
git am ../packages-patches/mutter/48.7/0004-mutter-afbc-yuv420.patch
DEB_BUILD_OPTIONS="nocheck" dpkg-buildpackage -b -uc -us --no-check-builddeps
```

## Post-build: remove libmali dependency

`dh_shlibdeps` automatically adds `libmali-valhall-g310-g29p1` to `Depends`
because `libmutter-16.so` links against the Mali EGL driver. This dependency is
GPU-board-specific and must be removed before distribution. (Note the package
name has no `-wayland-gbm` suffix, so the `debian/rules` `-x` exclusion below —
which names that suffix — does not match; strip it post-build instead.)

**Build note (deb13u1):** this source's `debian/rules` has an
`override_dh_shlibdeps` passing `-- -x libmali-valhall-g310-g29p1-wayland-gbm`
(space-separated). `dpkg-shlibdeps` expects `-x<package>` attached, so the
space form is parsed as a file to scan and the build aborts with
`cannot read libmali-...: No such file`. Fix before building:

```bash
sed -i 's/-- -x libmali-valhall-g310-g29p1-wayland-gbm --ignore-missing-info/-- --ignore-missing-info/' debian/rules
# build without dpkg-buildpackage's clean step (avoids a full recompile when
# resuming an already-compiled tree); board shell is root so no fakeroot:
debian/rules binary
```

`debian/rules` then links fine but `dh_shlibdeps` adds the Mali dependency,
which must be removed by editing the `DEBIAN/control` of the built package:

```bash
dpkg-deb -R libmutter-16-0_*.deb /tmp/repack
python3 -c "
import re, sys
c = open('/tmp/repack/DEBIAN/control').read()
c = re.sub(r',\s*libmali[^,\n]*', '', c)
c = re.sub(r'libmali[^,\n]*,\s*', '', c)
open('/tmp/repack/DEBIAN/control', 'w').write(c)
"
dpkg-deb -b /tmp/repack libmutter-16-0_*.deb
```

## Install

```bash
dpkg -i libmutter-16-0_*.deb gir1.2-mutter-16_*.deb \
        mutter-common_*.deb mutter-common-bin_*.deb
```
