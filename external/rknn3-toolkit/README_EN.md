# Introduction

The RKNN software stack helps users quickly deploy AI models on Rockchip's chips.

To use the RKNPU, users first need to run the RKNN3-Toolkit tool on a computer to convert their trained models into the RKNN format. Then, they can perform inference on a development board using the RKNN C API.

- **RKNN3-Toolkit** is a software development kit that allows users to perform model conversion, inference, and performance evaluation on PCs and Rockchip NPU platforms.
- **RKNN3 API** provides C/C++ programming interfaces for the Rockchip NPU platform, helping users deploy RKNN models and accelerate the implementation of AI applications.
- The **RKNN3 Model Zoo** repository provides examples for converting Large Language Models (LLMs) to the RKNN format and deploying them on development boards.



# Supported Platforms

- RK1820
- RK1828

**Note**:

- **For RK3588/RK3576/RK3568/RK3566/RK3562 series, RV1103/RV1106, RV1103B/RV1106B, RV1126B, RK2118, please refer to:**

  https://github.com/airockchip/rknn-toolkit2

- **For RK1808/RV1109/RV1126/RK3399Pro, please refer to:**

  https://github.com/airockchip/rknn-toolkit

  https://github.com/airockchip/rknpu

  https://github.com/airockchip/RK3399Pro_npu



# Supported Models

 - [x] Qwen2.5-0.5B / 1.5B / 3B / 7B
 - [x] Qwen3-0.6B / 1.7B / 4B / 8B
 - [x] HY-MT1.5-1.8B
 - [x] Youtu-LLM-2B
 - [x] Qwen2.5-VL-3B / 7B
 - [x] Qwen2.5-Omni-3B (Thinker)
 - [x] Qwen3-VL-2B / 4B
 - [x] FastVLM-1.6B
 - [x] InternVL3-2B
 - [x] InternVL3.5-4B
 - [x] MiMo-VL-7B-RL
 - [x] Qwen3-Reranker-0.6B / 4B
 - [x] Qwen3-Embedding-4B
 - [x] GLM-Edge
 - [x] SmolVLM
 - [x] UI_TARS
 - [x] gme-Qwen2-VL-2B
 - [x] SigLIP
 - [x] SigLIP2
 - [x] DINOv2
 - [x] DINOv3
 - [x] MobileNetV1 / V2
 - [x] ResNet-50
 - [x] YOLOv5 / YOLOv6 / YOLOv8
 - [x] YOLO-World
 - [x] Diffusion Policy
 - [x] Whisper
 - [x] SenceVoice

# Notes

- **RKNN3-Toolkit** is **not compatible** with [RKNN-Toolkit](https://github.com/airockchip/rknn-toolkit) and [RKNN-Toolkit2](https://github.com/airockchip/rknn-toolkit2).



# Supported Python Versions:

- Python 3.10
- Python 3.12
- Latest Version: V1.0.0



# Changelog

## V1.0.0
- Significantly improved LLM/ViT performance; overall LLM decode performance improved by more than 15%.
- Expanded model support range, adding models such as Qwen3-VL / Qwen2.5-Omni(Thinker) / GLM Edge / SmolVLM.
- Added support for cross-board accuracy analysis.
- Added support for overlapping data transfer and inference.
- Added support for mRoPE.
- Added support for Function Call.
- Added support for YUV-format input.
- `rkllm3-server` now supports embedding models and audio input.
- Added support for concurrent multi-core, multi-model inference.
- Added support for custom model post-processing on the coprocessor.
- Optimized implementation of exSDPA, exMatMul, Resize, Transpose operators.
- Provides RKNN3 Toolkit Lite package to support Python API calls on development boards.

## v0.4.0b0
- Optimization of SDK functions and stability; fixed known bugs.
- Support for more models, including CNN models, ViT models, and LLM/VLM models.
- Improved stability of multi-threaded inference.
- Fixed precision anomalies of some operators under specific specifications, such as matmul, resize, gather, etc.
- Optimized memory usage of large models and increased the maximum supported context length.
- Support for video/audio in multimodal models.

## v0.3.0b0

- Optimized SDK functionality and stability; fixed known bugs from the V0.2.0 release.
- Support for more models, including CNN, LLM, and VLM models.
- Reduced server memory consumption for model conversion by over 30%.
- RKNN3 Toolkit now supports inference on connected development boards.
- RKNN3 Model Zoo supports more models, especially VLM-related examples like InternVL3-2B and Qwen2.5-VL-3B.
- Performance optimization for Yolo-series detection models, particularly for multi-batch and multi-core performance (requires modification for models, referring to RKNN3 Model Zoo for details).
- LLM inference KVCache now supports a sliding window, allowing the number of output tokens to exceed the maximum context length.
- Support for USB communication.

## v0.2.0

- Added support for the conversion and deployment of CNN, LLM, and VLM models.

## v0.1.0

- Initial version, supporting model performance evaluation.



# Feedback and Community Support

- [Redmine](https://redmine.rock-chips.com/) (**Recommended for reporting issues. Please contact sales or an FAE to get a Redmine account**).
- QQ Group 1: 1025468710 (Full, please join group 4)
- QQ Group 2: 547021958 (Full, please join group 4)
- QQ Group 3: 469385426 (Full, please join group 4)
- QQ Group 4: 958083853
<center class="half">
  <img width="200" height="200"  src="res/QQGroupQRCode.png" title="QQ Group QR Code"/>
  <img width="200" height="200"  src="res/QQGroup2QRCode.png" title="QQ Group 2 QR Code"/>
  <img width="200" height="200"  src="res/QQGroup3QRCode.png" title="QQ Group 3 QR Code"/>
  <img width="200" height="200"  src="res/QQGroup4QRCode.png" title="QQ Group 4 QR Code"/>
</center>
