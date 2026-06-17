# README

## 前言

**概述**

Rockchip多媒体处理平台(rockit)对外应用接口说明，移除TGI接口，只保留MPI接口

---

## 目录

[TOC]

---
## Rockit对外接口说明
Rockit对外 MPI(Media Process Interface)接口，用于应用程序开发。

### MPI

- MPI接口是模块化接口，包括VI/VO/VDEC/VENC/VPSS/VGS/TDE/SYS/MB/AI/AO/ADEC/AENC等模块，详见mpi/doc/目录文档说明。
- MPI支持单模块使用，也支持多模块组合使用，可采用模块间绑定或者手动取/送流方式，灵活构建应用Pipeline。

- 已应用于RV1109/1126 IPC、RK356X NVR类产品，并在后续RK3588等新芯片上持续支持。

## Rockit对外接口特点

|          | MPI                  |
| -------- | -------------------- |
| 灵活性   | 灵活，模块可灵活拼接 |
| 使用场景 | NVR、IPC             |

## release note

