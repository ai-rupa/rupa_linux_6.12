# Rockchip RKAI Developer Guide

ID: RK-KF-YF-A61

Release Version: V1.0.0

Release Date: 2025-09-30

Security Level: □Top-Secret   □Secret   □Internal   ■Public

**DISCLAIMER**

THIS DOCUMENT IS PROVIDED “AS IS”. ROCKCHIP ELECTRONICS CO., LTD.(“ROCKCHIP”)DOES NOT PROVIDE ANY WARRANTY OF ANY KIND, EXPRESSED, IMPLIED OR OTHERWISE, WITH RESPECT TO THE ACCURACY, RELIABILITY, COMPLETENESS,MERCHANTABILITY, FITNESS FOR ANY PARTICULAR PURPOSE OR NON-INFRINGEMENT OF ANY REPRESENTATION, INFORMATION AND CONTENT IN THIS DOCUMENT. THIS DOCUMENT IS FOR REFERENCE ONLY. THIS DOCUMENT MAY BE UPDATED OR CHANGED WITHOUT ANY NOTICE AT ANY TIME DUE TO THE UPGRADES OF THE PRODUCT OR ANY OTHER REASONS.

**Trademark Statement**

"Rockchip", "瑞芯微", "瑞芯" shall be Rockchip’s registered trademarks and owned by Rockchip. All the other trademarks or registered trademarks mentioned in this document shall be owned by their respective owners.

**All rights reserved. ©2025. Rockchip Electronics Co., Ltd.**

Beyond the scope of fair use, neither any entity nor individual shall extract, copy, or distribute this document in any form in whole or in part without the written approval of Rockchip.

Rockchip Electronics Co., Ltd.

No.18 Building, A District, No.89, software Boulevard Fuzhou, Fujian,PRC

Website:     [www.rock-chips.com](http://www.rock-chips.com)

Customer service Tel:  +86-4007-700-590

Customer service Fax:  +86-591-83951833

Customer service e-Mail:  [fae@rock-chips.com](mailto:fae@rock-chips.com)

---

**Preface**

**Overview**

This document presents the steps and guides for rapid verification and development of large language models and visual large models.

**Product Version**

| **SoC** | **Kernel Version** |
| ------------- | ----------------- |
| RV1126B       | Kernel 6.1        |

**Target Audience**

This document (this guide) is mainly intended for:

Technical support engineers

Software development engineers

**Revision Record**

| **Version** | **Author**    | **Date** | **Modification Description** |
| ----------------- | ------------ | -------------------- | -------------------------- |
| V1.0.0           | Cherry Chen  | 2025-09-30           | Initial Version            |

---

**Contents**

[TOC]

---

## Introduction

The RKAI module is a rapid verification module and adaptation example for SDK integration with Large Language Models (LLM) and Vision Language Models (VLM). The source code of this module is open-source and can be found at the path: `app/rkai`. The directory structure is as follows:

```SHELL
tree  rkai -L 1
rkai
├── app_conf #Configuration files and model files
├── CMakeLists.txt
├── common  #General code
├── LICENSE
├── README.md
├── rkai_LLM  # LLM model API source code
├── rkai_VLM  # VLM model API source code
└── samples   # Adaptation use cases

5 directories, 3 files
```

- app_conf is the general configuration path, which includes three parts: model files, model configuration files, and model input files. This part can be directly integrated into the board for use.
- common contains general code for the RKAI module, such as `CNPY`, etc.;
- rkai_LLM is the general API for LLM deployment;
- rkai_VLM is the general API for VLM deployment;
- samples provide verification cases for LLM/Vision/VLM.

## Quick Verification

The SDK comes with the RKAI module pre-compiled by default and integrates the `Qwen3-0.6B` model, allowing users to quickly verify the process of large models.

### Quick Verification of LLM

The LLM adaptation program is `rkai_LLM_Demo`, with the following Usage:

```shell
Usage: rkai_LLM_Demo [options]
Options:
  --llm_model <path>   RKLLM model file
  --config    <path>   Configuration ini file
Example:
  rkai_LLM_Demo --config <path>/rkai_cfg.ini
  rkai_LLM_Demo --config <path>/rkai_cfg.ini --llm_model <your_LLM_model_path>
```

The default inference command is:

```shell
rkai_LLM_Demo --config <path>/rkai_cfg.ini
```

The program comes with 5 pre-set questions, from which users can select by entering numbers, or input questions directly through the terminal.

```shell
**********************Enter the corresponding number for the following questions to get answers/ or input custom questions********************
[0] Translate the following modern text into classical Chinese: When the spring breeze is gentle and the sun is bright...
[1] Write an ancient poem with the theme of "Ode to Plum Blossoms", including elements such as plum blossoms and snow.
[2] Upper couplet: Used to watch thousands of sails pass by the riverbank
[3] Translate this sentence into Chinese: Knowledge can be acquired from many sources...
[4] Translate this sentence into English: RK3588是新一代高端处理器...

*************************************************************************
```

The model output consists of two parts: 1. Model response; 2. Performance.

![image-20250930100305410](./resource/1-llm-out.png)

You can also convert the model yourself and use `rkai_LLM_Demo` for model inference verification. Detailed instructions are in the third section.

### VLM Rapid Verification

VLM actually refers to Vision and LLM, which can be verified separately or together. Due to the large memory footprint of the models, it is not convenient to integrate them into the SDK. Therefore, users can quickly verify the VLM model by downloading from the following URLs.

The Vision model download link is: [Vision Model](https://www.modelscope.cn/models/eliasning/Qwen3-0.6B-w4a16.rv1126b.rkllm/resolve/master/fastvlm-0.5b-fp16.rv1126b.rknn);

The LLM download link is: [LLM Model](https://www.modelscope.cn/models/eliasning/Qwen3-0.6B-w4a16.rv1126b.rkllm/resolve/master/fastvlm-0.5B-w8a8_level0.rv1126b.rkllm).

Push the downloaded models to the board's `app_conf/models` directory. Then, you can use `rkai_VLM_Demo` for rapid verification. `Usage` is as follows:

```shell
Usage: rkai_VLM_Demo [options]
Options:
  --vision_model <path>   Vision RKNN model file
  --llm_model    <path>   LLM RKLLM model file
  --image_path   <path>   Input image file
  --config       <path>   Configuration ini file
Example:
  rkai_VLM_Demo --config <path>/rkai_cfg.ini
  rkai_VLM_Demo --vision_model <path>/fastvlm-0.5b-fp16.rv1126b.rknn \
     --llm_model <path>/fastvlm-0.5B-w8a8_level0.rv1126b.rkllm \
     --image_path <path>/demo.jpg \
     --config <path>/rkai_cfg.ini
```

If the downloaded models are pushed to the `app_conf/models` path, then you can use the following command for inference:

```shell
rkai_VLM_Demo --config rkai_cfg.ini
```

If the downloaded models are pushed to other paths, then you need to manually specify:

```shell
rkai_VLM_Demo --vision_model xxx.rknn \
              --llm_model xxx.rkllm \
              --config <path>rkai_cfg.ini
```

The model output consists of two parts: 1. Model response; 2. Performance.

![image-20250930100054558](./resource/2-vlm-out.png)

## Custom Models

Users can convert models on their own and perform inference using `rkai_LLM_Demo` and `rkai_VLM_Demo`. The model conversion tool download link is: <https://github.com/airockchip/rknn-llm>. If users customize their models, then `rkai_cfg.ini` needs to be modified accordingly.

1. In `rkai_cfg.ini`, the [llm_cfg] section contains LLM parameters, including model hyperparameters that users can adjust. The parameters are as follows:

| Parameter            | Default Value                                                | Description                           |
| -------------------- | ----------------------------------------------------------- | ------------------------------------- |
| llm_model            | models/Qwen3-0.6B_W4A16_RV1126B.rkllm                     | Can be defined externally with `--llm_model` |
| max_context_len      | 1024                                                       | Maximum context length                |
| max_new_tokens       | 128                                                         | Maximum output tokens                 |
| skip_special_token   | 1                                                           | Whether to skip special characters    |
| base_domain_id       | 0                                                           | Memory usage method, default is 0      |
| top_k                | 1                                                           | Less than or equal to 1                |

2. In `rkai_cfg.ini`, the [vlm_cfg] section contains VLM parameters, including model hyperparameters that users can adjust. The parameters are as follows:

| Parameter            | Default Value                                                | Description                              |
| -------------------- | ----------------------------------------------------------- | ---------------------------------------- |
| vision_model         | models/fastvlm-0.5b-fp16.rv1126b.rknn                      | Can also be defined externally with `--vision_model` |
| llm_model            | models/fastvlm-0.5B-w8a8_level0.rv1126b.rkllm               | Can be defined externally with `--llm_model`     |
| image_path           | inputs/image_1024x1024.npy                                 | Supports input jpg images, specified with `–image_path` |
| max_context_len      | 512                                                         | Maximum context length                   |
| skip_special_token   | 1                                                           | Whether to skip special characters      |
| base_domain_id       | 0                                                           | Memory usage method, default is 0        |
| top_k                | 1                                                           | Less than or equal to 1                  |

3. In `rkai_cfg.ini`, the [prompt] section is the system prompt, and users need to modify this part when customizing their models. The parameters are as follows:

| Parameter           | Default Value                                                                 | Description                      |
| ------------------- | ---------------------------------------------------------------------------- | -------------------------------- |
| system_prompt       | <\|im_start\|>system\nYou are a helpful assistant.<\|im_end\|>\n                 | Default system prompt for qwen3   |
| prompt_prefix       | <\|im_start\|>user\n                                                         | Can be obtained from the model's config file |
| prompt_postfix      | <\|im_end\|>\n<\|im_start\|>assistant\n                                        | Can be obtained from the model's config file |

4. In `rkai_cfg.ini`, the [tensor] section is the image placeholder for the VLM model, and users need to modify this part when customizing their models. The parameters are as follows:

   | Parameter       | Default Value  | Description |
   | --------------- | -------------- | ----------- |
   | img_start      | <\|im_start\|>  |             |
   | img_end        | <\|im_end\|>    |             |
   | img_content    | <\|im_content\|>|             |

## Detailed API Documentation

Users can integrate applications by calling APIs themselves. The APIs are fully open-source, and the following will introduce the APIs.

### LLM API

The APIs for initializing and deinitializing the LLM model are as follows:

| API  | rkai_llm_init             |
| ---- | ------------------------- |
| Description | LLM Initialization |
| Parameters | rkaiCtxLLM* ctx: LLM handle |
|            | rkaiCfg* cfg: LLM parameter configuration |

| API  | rkai_llm_release |
| ---- | ---------------- |
| Description | LLM Deinitialization |

The APIs for LLM inference deployment are as follows:

| API  | rkai_llm_run              |
| ---- | ------------------------- |
| Description | LLM Inference |
| Parameters | rkaiCtxLLM* ctx: LLM handle |
|            | rkaiCfg* cfg: LLM parameter configuration |

### VLM API

VLM's Vision Model API:

| API  | rkai_vlm_vision_init                                |
| ---- | --------------------------------------------------- |
| Description | Vision Initialization                                    |
| Parameters | const char *model_path: Vision model path, in .rknn format |
|            | rkaiCfg* cfg : LLM parameter configuration             |

| API  | rkai_vlm_vision_release |
| ---- | ----------------------- |
| Description | Vision De-initialization          |

APIs for Vision Inference are as follows:

| API  | rkai_vlm_vision_run                                  |
| ---- | ---------------------------------------------------- |
| Description | VLM Inference                                              |
| Parameters | rkaiCtxVision *rkai_ctx: Handle                       |
|            | void *img_data: Input image buffer, in rgb888 format |
|            | rkaiVisionToLLM *vtol: Parameters and buffer for vision to llm |

| API  | dump_tensor_attr                        |
| ---- | --------------------------------------- |
| Description | Dump basic information of the model                      |
| Parameters | rknn_tensor_attr *attr: Model tensor information |
|            | int32_t is_inputs: Whether to dump inputs        |

VLM's LLM API:

| API  | rkai_vlm_llm_init         |
| ---- | ------------------------- |
| Description | LLM Initialization                 |
| Parameters | rkaiCtxLLM* ctx: LLM handle |
|            | rkaiCfg* cfg :LLM parameter configuration |

| API  | rkai_vlm_llm_release |
| ---- | -------------------- |
| Description | LLM De-initialization          |

APIs for LLM Inference Deployment are as follows:

| API  | rkai_vlm_llm_run                         |
| ---- | ---------------------------------------- |
| Description | LLM Inference                                  |
| Parameters | rkaiCtxLLM* ctx: LLM handle                |
|            | rkaiCfg* cfg :LLM parameter configuration                |
|            | rkaiVisionToLLM *vtl: Receive information from Vision |