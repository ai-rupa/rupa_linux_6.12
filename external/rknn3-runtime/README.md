# 简介
  RKNN软件栈能够帮助用户快速将AI模型部署到瑞芯微（Rockchip）芯片上。
  为了使用RKNPU，用户需要先在计算机上运行RKNN3-Toolkit工具，将训练好的模型转换为RKNN格式，然后通过RKNN C API在开发板上进行推理。 

- **RKNN3-Toolkit** 是一个软件开发工具包，支持用户在PC和Rockchip NPU平台上进行模型转换、推理和性能评估。
- **RKNN3 API** 为Rockchip NPU平台提供C/C++编程接口，帮助用户部署RKNN模型并加速AI应用的实现。 
- **RKNN3 Model Zoo** 仓库提供了将LLM（大型语言模型）转换为RKNN格式并在开发板上部署的示例。



# 支持平台

  - RK1820
  - RK1828

**注意**： 

-  **对于RK3588/RK3576/RK3568/RK3566/RK3562系列、RV1103/RV1106、RV1103B/RV1106B、RV1126B、RK2118，请参考：** 

    https://github.com/airockchip/rknn-toolkit2      
    
- **对于RK1808/RV1109/RV1126/RK3399Pro，请参考：** 

    https://github.com/airockchip/rknn-toolkit  
    
    https://github.com/airockchip/rknpu 

    https://github.com/airockchip/RK3399Pro_npu  



# 支持模型  

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


# 注意事项  

- **RKNN3-Toolkit** 与 [RKNN-Toolkit](https://github.com/airockchip/rknn-toolkit) 和 [RKNN-Toolkit2](https://github.com/airockchip/rknn-toolkit2) **不兼容**。  



# 支持的Python版本：  

  - Python 3.10  
  - Python 3.12  
- 最新版本：V1.0.0



# 更新日志  

## V1.0.0
- 大幅提升LLM/ViT性能，LLM Decode性能整体提升超过15%
- 扩展模型支持范围，新增适配 Qwen3-VL / Qwen2.5-Omni(Thinker) / GLM Edge / SmolVLM 等模型
- 支持连板精度分析
- 支持数据传输与推理并行
- 支持 mRoPE
- 支持 Function Call 功能
- 支持 YUV 格式输入
- rkllm3 server 新增支持 embedding 模型，并兼容音频输入
- 支持多核多模型同时推理
- 支持用户在协处理器上自定义模型后处理
- 优化exSDPA、exMatMul、Resize、Transpose等算子实现
- 提供RKNN3 Toolkit Lite工具包，支持在开发板上进行Python API调用

## V0.4.0b0
 - SDK功能及稳定性优化，修复已知BUG
 - 支持更多模型，包括 CNN 模型，ViT 模型及 LLM/VLM 模型
 - 提高多线程推理稳定性
 - 修复部分算子在特定规格下精度异常问题，比如matmul/resize/gather等
 - 优化大模型内存占用，提高支持的最大上下文长度
 - 多模态模型支持 video/audio

## V0.3.0b0
 - SDK功能及稳定性优化，修复V0.2.0发布后的已知BUG
 - 支持更多模型，包括CNN模型及LLM/VLM模型
 - 降低模型转换对服务器的内存消耗，整体降低30%以上
 - RKNN3 Toolkit支持连板推理
 - RKNN3 Model Zoo支持更多的模型，特别是VLM相关示例，比如InternVL3-2B、Qwen2.5-VL-3B等
 - Yolo检测类模型性能优化，特别是多batch多核性能（模型需要修改，详见RKNN3 Model Zoo说明）
 - LLM推理KVCache支持滑窗，允许输出token数超过最大上下文长度
 - 支持USB通讯

## V0.2.0
 - 支持CNN、LLM、VLM模型的转换与部署

## V0.1.0  
 - 初始版本，支持模型性能评估



# 反馈与社区支持  

- [Redmine](https://redmine.rock-chips.com) (**推荐反馈问题，请联系销售或FAE获取Redmine账号**)  
- QQ群1：1025468710（已满，请加群4）  
- QQ群2：547021958（已满，请加群4）  
- QQ群3：469385426（已满，请加群4）  
- QQ群4：958083853  
<center class="half">  
  <img width="200" height="200"  src="res/QQGroupQRCode.png" title="QQ群二维码"/>  
  <img width="200" height="200"  src="res/QQGroup2QRCode.png" title="QQ群2二维码"/>  
  <img width="200" height="200"  src="res/QQGroup3QRCode.png" title="QQ群3二维码"/>  
  <img width="200" height="200"  src="res/QQGroup4QRCode.png" title="QQ群4二维码"/>  
</center>  
