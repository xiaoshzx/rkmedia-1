#ifndef __RKMEDIA_API_
#define __RKMEDIA_API_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "rkmedia_buffer.h"
#include "rkmedia_common.h"
#include "rkmedia_venc.h"
#include "rkmedia_vi.h"

// Platform resource number configuration
#define VI_MAX_DEV_NUM 4
#define VI_MAX_CHN_NUM VI_MAX_DEV_NUM
#define VENC_MAX_CHN_NUM 16

typedef RK_S32 VI_PIPE;
typedef RK_S32 VI_CHN;
typedef RK_S32 VENC_CHN;

typedef struct rkMPP_CHN_S {
  MOD_ID_E enModId;
  RK_S32 s32DevId;
  RK_S32 s32ChnId;
} MPP_CHN_S;

/********************************************************************
 * SYS Ctrl api
 ********************************************************************/
_CAPI RK_S32 RK_MPI_SYS_Init();
_CAPI RK_S32 RK_MPI_SYS_Bind(const MPP_CHN_S *pstSrcChn,
                             const MPP_CHN_S *pstDestChn);
_CAPI RK_S32 RK_MPI_SYS_UnBind(const MPP_CHN_S *pstSrcChn,
                               const MPP_CHN_S *pstDestChn);

_CAPI RK_S32 RK_MPI_SYS_RegisterOutCb(const MPP_CHN_S *pstChn, OutCbFunc cb);

/********************************************************************
 * Vi api
 ********************************************************************/
_CAPI RK_S32 RK_MPI_VI_SetChnAttr(VI_PIPE ViPipe, VI_CHN ViChn,
                                  const VI_CHN_ATTR_S *pstChnAttr);
_CAPI RK_S32 RK_MPI_VI_DisableChn(VI_PIPE ViPipe, VI_CHN ViChn);
_CAPI RK_S32 RK_MPI_VI_EnableChn(VI_PIPE ViPipe, VI_CHN ViChn);

/********************************************************************
 * Venc api
 ********************************************************************/
_CAPI RK_S32 RK_MPI_VENC_CreateChn(VENC_CHN VeChn,
                                   VENC_CHN_ATTR_S *stVencChnAttr);
_CAPI RK_S32 RK_MPI_VENC_SetRcParam(VENC_CHN VeChn,
                                    const VENC_RC_PARAM_S *pstRcParam);

_CAPI RK_S32 RK_MPI_VENC_SetRcMode(VENC_CHN VeChn, VENC_RC_MODE_E RcMode);
_CAPI RK_S32 RK_MPI_VENC_SetRcQuality(VENC_CHN VeChn,
                                      VENC_RC_QUALITY_E RcQuality);
_CAPI RK_S32 RK_MPI_VENC_SetBitrate(VENC_CHN VeChn, RK_U32 u32BitRate,
                                    RK_U32 u32MinBitRate, RK_U32 u32MaxBitRate);
_CAPI RK_S32 RK_MPI_VENC_RequestIDR(VENC_CHN VeChn, RK_BOOL bInstant);
_CAPI RK_S32 RK_MPI_VENC_SetFps(VENC_CHN VeChn, RK_U8 u8OutNum, RK_U8 u8OutDen,
                                RK_U8 u8InNum, RK_U8 u8InDen);
_CAPI RK_S32 RK_MPI_VENC_SetGop(VENC_CHN VeChn, RK_U32 u32Gop);
_CAPI RK_S32 RK_MPI_VENC_SetAvcProfile(VENC_CHN VeChn, RK_U32 u32Profile,
                                       RK_U32 u32Level);
_CAPI RK_S32 RK_MPI_VENC_InsertUserData(VENC_CHN VeChn, RK_U8 *pu8Data,
                                        RK_U32 u32Len);
_CAPI RK_S32 RK_MPI_VENC_SetRoiAttr(VENC_CHN VeChn,
                                    const VENC_ROI_ATTR_S *pstRoiAttr);

_CAPI RK_S32 RK_MPI_VENC_DestroyChn(VENC_CHN VeChn);

#ifdef __cplusplus
}
#endif

#endif //__RKMEDIA_API_
