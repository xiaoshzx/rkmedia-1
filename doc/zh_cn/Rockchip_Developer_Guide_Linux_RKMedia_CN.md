# Rockchip RKMedia Development Guide

文件标识：

发布版本：0.0.1

日       期：2020.8

文件密级：公开资料

---

免责声明

本文档按“现状”提供，瑞芯微电子股份有限公司（“本公司”，下同）不对本文档的任何陈述、信息和内容的准确性、可靠性、完整性、适销性、特定目的性和非侵权性提供任何明示或暗示的声明或保证。本文档仅作为使用指导的参考。

由于产品版本升级或其他原因，本文档将可能在未经任何通知的情况下，不定期进行更新或修改。

商标声明

“Rockchip”、“瑞芯微”、“瑞芯”均为本公司的注册商标，归本公司所有。

本文档可能提及的其他所有注册商标或商标，由其各自拥有者所有。

版权所有 © 2020 瑞芯微电子股份有限公司

超越合理使用范畴，非经本公司书面许可，任何单位和个人不得擅自摘抄、复制本文档内容的部分或全部，并不得以任何形式传播。

瑞芯微电子股份有限公司

Rockchip Electronics Co., Ltd.

地址：     福建省福州市铜盘路软件园A区18号

网址：     www.rock-chips.com

客户服务电话： +86-4007-700-590

客户服务传真： +86-591-83951833

客户服务邮箱： fae@rock-chips.com

---

## 前言

 概述

 本文主要描述了RKMedia 媒体开发参考

产品版本

| 芯片名称  | 内核版本 |
| ------------- | ------------ |
| RV1126/RV1109 | 4.19         |

读者对象

本文档（本指南）主要适用于以下工程师：

​        技术支持工程师

​        软件开发工程师

 修订记录

| 日期   | 版本 | 作者        | 修改说明 |
| ---------- | -------- | :-------------- | ------------ |
| 2020-08-28 | V0.0.1   | 范立创 / 余永镇 | 初始版本     |

---

## 目录

[TOC]

---

## 系统概述

### 概述

 RKMedia提供了一种媒体处理方案，可支持应用软件快速开发。RKMedia在各模块基础API上做进一步封装，简化了应用开发难度。该平台支持以下功能：VI(输入视频捕获)、VENC(H.265/H.264/JPEG/MJPEG 编码)、VDEC(H.265/H.264/JPEG、MJPEG 解码)、VO(视频输出显示)、RGA视频处理（包括旋转、缩放、裁剪）AI(音频采集）、AO（音频输出）、AENC（音频编码）、ADEC（音频解码）、MD（移动侦测）、OD（遮挡侦测）。

### 系统架构

 ![](resource/system-1.png)

### 系统资源数目表

| 模块名称       | 通道数量 |
| -------------- | -------- |
| VI             | 4        |
| VENC           | 16       |
| VDEC           | 16       |
| AI             | 1        |
| AO             | 1        |
| AENC           | 16       |
| ADEC           | 16       |
| MD（移动侦测） | 4        |
| OD（遮挡侦测） | 4        |
| RGA            | 16       |
| VO             | 2        |

## 系统控制

### 概述

系统控制基本系统的初始化工作，同时负责完成 各个模块的初始化、反初始化以及管理各个业务模块的绑定关系、提供当前系统版本、系统日志管理。

### 功能描述

#### 系统绑定

RKMedia提供系统绑定接口（RK_MPI_SYS_Bind），即通过数据接收者绑定数据源来建立
两者之间的关联关系（只允许数据接收者绑定数据源）。绑定后，数据源生成的数据将
自动发送给接收者。目前支持的绑定关系如表 2-1所示.

表4-1 RKMedia 支持的绑定关系

| 数据源 | 数据接受者        |
| ------ | ----------------- |
| VI     | VO/RGA/VENC/MD/OD |
| VDEC   | VO/RGA/VENC/MD/OD |
| RGA    | VO/VENC/MD/OD     |
| AI     | AO/AENC           |
| ADEC   | AO                |

### API参考

#### RK_MPI_SYS_Init

​                                     【描述】

​                                    初始化系统

​                                     【语法】

​                                    RK_S32 RK_MPI_SYS_Init();

#### RK_MPI_SYS_Bind

​                                     【描述】

​                                     数据源到数据接收者绑定接口。

​                                     【语法】

​                                    RK_S32 RK_MPI_SYS_Bind(const MPP_CHN_S *pstSrcChn,const MPP_CHN_S *pstDestChn);

#### RK_MPI_SYS_UnBind

​                                     【描述】

​                                    数据源到数据接收者解绑定接口。

​                                     【语法】

​                                    RK_MPI_SYS_UnBind(const MPP_CHN_S *pstSrcChn,const MPP_CHN_S *pstDestChn);

#### RK_MPI_SYS_RegisterEventCb

​                                     【描述】

​                                    注册事件回调，比如移动侦测事件

​                                     【语法】

​                                    RK_S32 RK_MPI_SYS_RegisterEventCb(const MPP_CHN_S *pstChn,EventCbFunc cb);

#### RK_MPI_SYS_RegisterOutCb

​                                     【描述】

​                                    注册数据输出回调。注意：回调函数不能处理耗时操作，否则对应通道数据流将被阻塞。

​                                     【语法】

​                                    RK_S32 RK_MPI_SYS_RegisterOutCb(const MPP_CHN_S *pstChn, OutCbFunc cb);

#### RK_MPI_SYS_SendMediaBuffer

​                                     【描述】

​                                    向指定通道输入数据，比如将本地yuv文件送入编码器编码

​                                     【语法】

​                                    RK_S32 RK_MPI_SYS_SendMediaBuffer(MOD_ID_E enModID, RK_S32 s32ChnID,

​                                     MEDIA_BUFFER buffer);

#### RK_MPI_SYS_GetMediaBuffer

​                                     【描述】

​                                    从指定通道中获取数据。注意：若该接口不能被及时调用，将发生丢帧现象。

​                                     【语法】

​                                    RK_S32 RK_MPI_SYS_SendMediaBuffer(MOD_ID_E enModID, RK_S32 s32ChnID,
​                                    MEDIA_BUFFER buffer);

## 视频输入

### 概述

视频输入（VI）模块实现的功能：ISPP驱动实现标准V4L2设备，通过对V4L2 API的封装，可以采集到ISPP多通道视频数据。VI 将接收到的数据存入到指定的内存区域，实现视频数据的采集。

### 功能描述

#### VI节点名称

VI的创建需要指定视频节点名称，比如“/dev/video0”。在RV1126/RV1109平台则比较特殊，对应节点名称如下所示

表5-1 ISPP节点名称

| ISPP 节点名称 |
| ------------- |
| ispp_bypass   |
| ispp_scal0    |
| ispp_scal1    |
| ispp_scal2    |

#### VI工作模式

VI有两种工作模式，如下表所示

| 模式名称 | 宏定义名称             | 功能说明                                                     |
| -------- | ---------------------- | ------------------------------------------------------------ |
| 正常模式 | VI_WORK_MODE_NORMAL    | 相对于“亮度模式”，该模式下正常读取Camera数据并发给后级       |
| 亮度模式 | VI_WORK_MODE_LUMA_ONLY | 亮度模式下，VI仅用于亮度统计。此时VI模块无法通过回调函数或者RK_MPI_SYS_GetMediaBuffer获取数据。 |

### API参考

#### RK_MPI_VI_EnableChn

​                                     【描述】

​                                    启用VI通道

​                                     【语法】

​                                    RK_S32 RK_MPI_VI_EnableChn(VI_PIPE ViPipe, VI_CHN ViChn);

#### RK_MPI_VI_DisableChn

​                                     【描述】

​                                     关闭VI通道

​                                     【语法】

​                                    RK_S32 RK_MPI_VI_DisableChn(VI_PIPE ViPipe, VI_CHN ViChn);

#### RK_MPI_VI_SetChnAttr

​                                     【描述】

​                                    设置VI通道属性

​                                     【语法】

​                                    RK_MPI_VI_SetChnAttr(VI_PIPE ViPipe, VI_CHN ViChn,const VI_CHN_ATTR_S *pstChnAttr);

#### RK_MPI_VI_GetChnRegionLuma

​                                     【描述】

​                                    获取区域亮度信息。该接口不支持FBC0/FBC2压缩格式。

​                                     【语法】

​                                    RK_S32 RK_MPI_VI_GetChnRegionLuma(VI_PIPE ViPipe, VI_CHN ViChn, const

​                                     VIDEO_REGION_INFO_S *pstRegionInfo, RK_U64 *pu64LumaData, RK_S32 s32MilliSec);

## 视频编码

### 概述

VENC 模块，即视频编码模块。本模块支持多路实时编码，且每路编码独立，编码协
议和编码 profile 可以不同。支持视频编码同时，调度 Region 模块对编码图像内
容进行叠加和遮挡。支持H264/H1265/MJPEG/JPEG编码。

### 功能描述

#### 数据流程图

![](resource/venc-1.png)

注：虚线框所述功能为可选，只有对编码器进行相应配置才会触发。

#### 码率控制

| 编码器类型 | 支持码控类型 |
| ---------- | ------------ |
| H265       | CBR / VBR    |
| H264       | CBR / VBR    |
| MJPEG      | CBR / VBR    |

#### GOP Mode

GOP Mode用于定制参考帧的依赖关系，目前支持如下模式。注：可根据需求定制。

| 名称             | 宏定义               | 描述                                                         |
| ---------------- | -------------------- | ------------------------------------------------------------ |
| 普通模式         | VENC_GOPMODE_NORMALP | 最常见场景，每隔GopSize一个I帧                               |
| 智能P帧模式      | VENC_GOPMODE_SMARTP  | 每隔GopSize一个虚拟I帧，每隔BgInterval一个I帧                |
| 多层时域参考模式 | VENC_GOPMODE_TSVC    | 编码依赖关系划分为多层，可根据RK_MPI_MB_GetTsvcLevel获取层信息，从而定制码流。比如只播放第0层码流，可实现快速预览。 |

#### 感兴趣区域(ROI)

通过配置编码器感兴趣区域，可实现指定区域QP的定制。比如一个对着走廊的镜头，用户真正感兴趣的是走廊中央。可通过配置ROI让走廊中央编码质量更高，图像更清晰，走廊的边框（墙体、天花板等）非感兴趣区域图像质量会偏低。通过这种方式实现保持码率基本不变情况下，突出显示用户关心区域。

系统提供8个感兴趣区域，优先级从REGION_ID_0~REGION_ID_7递增。在多个ROI重叠的区域，其QP策略会按照优先级高的区域进行配置。

```
  REGION_ID_0
  REGION_ID_1
  REGION_ID_2
  REGION_ID_3
  REGION_ID_4
  REGION_ID_5
  REGION_ID_6
  REGION_ID_7
```

#### 旋转(Rotation)

编码器支持4种类型的旋转，分别为0°，90°，180°，270°。编码器旋转目前不支持FBC格式，FBC格式的旋转则需要通过ISPP的旋转来实现。

### API参考

#### RK_MPI_VENC_CreateChn

​                                     【描述】

​                                    创建编码通道

​                                     【语法】

​                                    RK_MPI_VENC_CreateChn(VENC_CHN VeChn,VENC_CHN_ATTR_S *stVencChnAttr);

#### RK_MPI_VENC_DestroyChn

​                                     【描述】

​                                     销毁编码通道

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_DestroyChn(VENC_CHN VeChn);

#### RK_MPI_VENC_SetRcParam

​                                     【描述】

​                                    设置码率控制参数

​                                     【语法】

​                                    RK_MPI_VENC_SetRcParam(VENC_CHN VeChn,const VENC_RC_PARAM_S *pstRcParam);

#### RK_MPI_VENC_SetRcMode

​                                     【描述】

​                                    设置码率控制模式

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_SetRcMode(VENC_CHN VeChn, VENC_RC_MODE_E RcMode);

#### RK_MPI_VENC_SetRcQuality

​                                     【描述】

​                                    设置编码质量。用于H264 / H265编码器。

​                                     【语法】

​                                    RK_MPI_VENC_SetRcQuality(VENC_CHN VeChn,VENC_RC_QUALITY_E RcQuality);

#### RK_MPI_VENC_SetBitrate

​                                     【描述】

​                                    设置码率

​                                     【语法】

​                                    RK_MPI_VENC_SetBitrate(VENC_CHN VeChn, RK_U32 u32BitRate, RK_U32 u32MinBitRate,

​                                     RK_U32 u32MaxBitRate);

#### RK_MPI_VENC_RequestIDR

​                                     【描述】

​                                    请求IDR帧。调用该接口后，编码器立即刷新IDR帧。

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_RequestIDR(VENC_CHN VeChn, RK_BOOL bInstant);

#### RK_MPI_VENC_SetFps

​                                     【描述】

​                                    设置编码帧率。注意：输出帧率不能大于输入帧率。

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_SetFps(VENC_CHN VeChn, RK_U8 u8OutNum, RK_U8 u8OutDen,

​                                     RK_U8 u8InNum, RK_U8 u8InDen);

#### RK_MPI_VENC_SetGop

​                                     【描述】

​                                    设置GOP。用于H264 / H265编码器。

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_SetGop(VENC_CHN VeChn, RK_U32 u32Gop);

#### RK_MPI_VENC_SetAvcProfile

​                                     【描述】

​                                    设置 profile。用于H264 编码器。

​                                     【语法】

​                                    RK_MPI_VENC_SetAvcProfile(VENC_CHN VeChn, RK_U32 u32Profile,RK_U32 u32Level);

#### RK_MPI_VENC_InsertUserData

​                                     【描述】

​                                    插入用户数据，插入后的数据将在码流的SEI包中体现。用于H264 / H265编码器。

​                                     【语法】

​                                    RK_MPI_VENC_InsertUserData(VENC_CHN VeChn, RK_U8 *pu8Data,RK_U32 u32Len);

#### RK_MPI_VENC_SetRoiAttr

​                                     【描述】

​                                    设置ROI编码感兴趣区。用于H264 / H265编码器。

​                                     【语法】

​                                    RK_MPI_VENC_SetRoiAttr(VENC_CHN VeChn,const VENC_ROI_ATTR_S *pstRoiAttr);

#### RK_MPI_VENC_SetGopMode

​                                     【描述】

​                                    设置GopMode。用于H264 / H265编码器。

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_SetGopMode(VENC_CHN VeChn, VENC_GOP_MODE_E GopMode);

#### RK_MPI_VENC_InitOsd

​                                     【描述】

​                                    初始化OSD。在调用RK_MPI_VENC_SetBitMap或RK_MPI_VENC_RGN_SetCover之前，必须

​                                     先调用该接口，并且每个编码通道只能调用一次。

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_InitOsd(VENC_CHN VeChn);

#### RK_MPI_VENC_SetBitMap

​                                     【描述】

​                                    设置OSD位图

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_SetBitMap(VENC_CHN VeChn,const OSD_REGION_INFO_S *

​                                     pstRgnInfo, const BITMAP_S *pstBitmap);

#### RK_MPI_VENC_RGN_SetCover

​                                     【描述】

​                                    设置隐私遮挡

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_RGN_SetCover(VENC_CHN VeChn, const OSD_REGION_INFO_S

​                                     *pstRgnInfo, const COVER_INFO_S *pstCoverInfo);

#### RK_MPI_VENC_SetJpegParam

​                                     【描述】

​                                    设置JPEG编码参数

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_SetJpegParam(VENC_CHN VeChn, const VENC_JPEG_PARAM_S

​                                     *pstJpegParam);

#### RK_MPI_VENC_StartRecvFrame

​                                     【描述】

​                                    设置编码器接收帧的数量。默认创建编码器将持续不断的接收VI数据，通过

​                                     RK_MPI_VENC_StartRecvFrame接口可以设置接收帧数量，到达指定数目后，编码器将休

​                                     眠，直至下一次调用该接口改变接收帧数目。

​                                     【语法】

​                                    RK_S32 RK_MPI_VENC_StartRecvFrame(VENC_CHN VeChn, const

​                                     VENC_RECV_PIC_PARAM_S *pstRecvParam);

## 移动侦测

### 概述

移动侦测（MD）模块实现运动区域检测，最大支持4096个区域。

### 功能描述

MD算法由软件实现，输入的分辨率不易太大，典型分辨率640x480，分辨率越大，CPU负载也高。

### API参考

#### RK_MPI_ALGO_MD_CreateChn

​                                     【描述】

​                                    创建MD通道

​                                     【语法】

​                                    RK_S32 RK_MPI_ALGO_MD_CreateChn(ALGO_MD_CHN MdChn);

#### RK_MPI_ALGO_MD_DestroyChn

​                                     【描述】

​                                     销毁MD通道

​                                     【语法】

​                                    RK_S32 RK_MPI_ALGO_MD_DestroyChn(ALGO_MD_CHN MdChn);

#### RK_MPI_ALGO_MD_SetChnAttr

​                                     【描述】

​                                    设置MD属性

​                                     【语法】

​                                    RK_S32 RK_MPI_ALGO_MD_SetChnAttr(ALGO_MD_CHN MdChn,

​                                     const ALGO_MD_ATTR_S *pstChnAttr);

## 移动侦测

### 概述

遮挡侦测（Occlusion Detection）模块实现遮挡报警，最大支持10个区域。

### 功能描述

OD算法由软件实现，输入的分辨率不易太大，典型分辨率640x480，分辨率越大，CPU负载也高。

### API参考

#### RK_MPI_ALGO_OD_CreateChn

​                                     【描述】

​                                    创建OD通道

​                                     【语法】

​                                    RK_S32 RK_MPI_ALGO_OD_CreateChn(ALGO_OD_CHN OdChn, const ALGO_OD_ATTR_S

​                                     *pstChnAttr);

#### RK_MPI_ALGO_OD_DestroyChn

​                                     【描述】

​                                     销毁OD通道

​                                     【语法】

​                                    RK_S32 RK_MPI_ALGO_OD_DestroyChn(ALGO_OD_CHN OdChn);

## 音频

### 概述

AUDIO 模块包括音频输入、音频输出、音频编码、音频解码四个子模块。音频输入和
输出模块通过对Linux ALSA音频接口的封装，实现音频输入输出功能。音频编码和解码
模块通过对ffmpeg 音频编码器的封装实现。支持G711A/G711U/G726 /MP2。

### 功能描述

#### 音频输入输出

音频输入AI输出AO，用于和 Audio Codec 对接，完成声音的录制和播放。RKMedia AI/AO依赖于Linux ALSA设备，不同的声卡，只要支持ALSA驱动，就可以使用AI/AO接口。AI中集成了音频算法，可通过配置开启。开启算法后，AI输出经过算法处理后的PCM数据。

#### 音频编解码

音频编解码是通过对ffmpeg的封装实现，目前支持G711A/G711U/G726/MP2。

#### 音频算法

目前支持对讲场景AEC算法，录音场景ANR算法。

### API参考

#### RK_MPI_AI_EnableChn

​                                     【描述】

​                                    打开AI通道

​                                     【语法】

​                                    RK_S32 RK_MPI_AI_EnableChn(AI_CHN AiChn);

#### RK_MPI_AI_DisableChn

​                                     【描述】

​                                    关闭AI通道

​                                     【语法】

​                                    RK_S32 RK_MPI_AI_DisableChn(AI_CHN AiChn);

#### RK_MPI_AI_SetChnAttr

​                                     【描述】

​                                    设置AO通道属性

​                                     【语法】

​                                    RK_S32 RK_MPI_AI_SetChnAttr(AI_CHN AiChn, const AI_CHN_ATTR_S *pstAttr);

#### RK_MPI_AI_SetVolume

​                                     【描述】

​                                    设置音量

​                                     【语法】

​                                    RK_S32 RK_MPI_AI_SetVolume(AI_CHN AiChn, RK_S32 s32Volume);

#### RK_MPI_AI_GetVolume

​                                     【描述】

​                                    获取音量

​                                     【语法】

​                                    RK_S32 RK_MPI_AI_GetVolume(AI_CHN AiChn, RK_S32 *ps32Volume);

#### RK_MPI_AO_EnableChn

​                                     【描述】

​                                    打开AO通道

​                                     【语法】

​                                    RK_S32 RK_MPI_AO_EnableChn(AO_CHN AoChn);

#### RK_MPI_AO_DisableChn

​                                     【描述】

​                                    关闭AO通道

​                                     【语法】

​                                    RK_S32 RK_MPI_AO_DisableChn(AO_CHN AoChn);

#### RK_MPI_AO_SetChnAttr

​                                     【描述】

​                                    设置AO通道属性

​                                     【语法】

​                                    RK_S32 RK_MPI_AO_SetChnAttr(AO_CHN AoChn, const AO_CHN_ATTR_S *pstAttr);

#### RK_MPI_AO_SetVolume

​                                     【描述】

​                                    设置音量

​                                     【语法】

​                                    RK_S32 RK_MPI_AO_SetVolume(AO_CHN AoChn, RK_S32 s32Volume);

#### RK_MPI_AO_GetVolume

​                                     【描述】

​                                    获取音量

​                                     【语法】

​                                    RK_S32 RK_MPI_AO_GetVolume(AO_CHN AoChn, RK_S32 *ps32Volume);

#### RK_MPI_AENC_CreateChn

​                                     【描述】

​                                    创建音频编码通道

​                                     【语法】

​                                    RK_MPI_AENC_CreateChn(AENC_CHN AencChn,const AENC_CHN_ATTR_S *pstAttr);

#### RK_MPI_AENC_DestroyChn

​                                     【描述】

​                                    销毁音频编码通道

​                                     【语法】

​                                     RK_S32 RK_MPI_AENC_DestroyChn(AENC_CHN AencChn);

#### RK_MPI_ADEC_CreateChn

​                                     【描述】

​                                    创建音频解码通道

​                                     【语法】

​                                     RK_S32 RK_MPI_ADEC_CreateChn(ADEC_CHN AdecChn, const ADEC_CHN_ATTR_S

​                                     *pstAttr);

#### RK_MPI_ADEC_DestroyChn

​                                     【描述】

​                                    销毁音频解码通道

​                                     【语法】

​                                     RK_S32 RK_MPI_ADEC_DestroyChn(ADEC_CHN AdecChn);

## RGA

### 概述

RGA模块用于2D图像的裁剪、格式转换、缩放、旋转、图片叠加等。

### 功能描述

rkmedia中RGA通道仅支持格式转换、缩放、裁剪、旋转功能，图片叠加则需要单独调用librga.so库，参见《Rockchip_Developer_Guide_Linux_RGA_CN.pdf》

### API参考

#### RK_MPI_RGA_CreateChn

​                                     【描述】

​                                    创建RGA通道

​                                     【语法】

​                                    RK_S32 RK_MPI_RGA_CreateChn(RGA_CHN RgaChn, RGA_ATTR_S *pstRgaAttr);

#### RK_MPI_RGA_DestroyChn

​                                     【描述】

​                                     销毁RGA通道

​                                     【语法】

​                                    RK_S32 RK_MPI_RGA_DestroyChn(RGA_CHN RgaChn);

## 视频显示

### 概述

VO模块用于视频输出管理。

### 功能描述

VO模块是对DRM/KMS的封装，支持多VOP以及多图层显示。

### API参考

#### RK_MPI_VO_CreateChn

​                                     【描述】

​                                    创建VO通道

​                                     【语法】

​                                    RK_S32 RK_MPI_VO_CreateChn(VO_CHN VoChn, const VO_CHN_ATTR_S *pstAttr);

#### RK_MPI_VO_DestroyChn

​                                     【描述】

​                                     销毁VO通道

​                                     【语法】

​                                    RK_S32 RK_MPI_VO_DestroyChn(VO_CHN VoChn);