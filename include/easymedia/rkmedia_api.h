#ifndef __RKMEDIA_API_
#define __RKMEDIA_API_

//#ifdef __cplusplus
//extern "C"
//{
//#endif

#include <stddef.h>

#if defined(_WIN32) && !defined(__MINGW32CE__)

typedef unsigned char           RK_U8;
typedef unsigned short          RK_U16;
typedef unsigned int            RK_U32;
typedef unsigned long           RK_ULONG;
typedef unsigned __int64        RK_U64;

typedef signed char             RK_S8;
typedef signed short            RK_S16;
typedef signed int              RK_S32;
typedef signed long             RK_LONG;
typedef signed __int64          RK_S64;

#else

typedef unsigned char           RK_U8;
typedef unsigned short          RK_U16;
typedef unsigned int            RK_U32;
typedef unsigned long           RK_ULONG;
typedef unsigned long long int  RK_U64;


typedef signed char             RK_S8;
typedef signed short            RK_S16;
typedef signed int              RK_S32;
typedef signed long             RK_LONG;
typedef signed long long int    RK_S64;

#endif

typedef enum rkMOD_ID_E {
    RK_ID_CMPI    = 0,
    RK_ID_VB      = 1,
    RK_ID_SYS     = 2,
    RK_ID_RGN      = 3,
    RK_ID_CHNL    = 4,
    RK_ID_VDEC    = 5,
    RK_ID_AVS     = 6,
    RK_ID_VPSS    = 7,
    RK_ID_VENC    = 8,
    RK_ID_SVP     = 9,
    RK_ID_H264E   = 10,
    RK_ID_JPEGE   = 11,
    RK_ID_MPEG4E  = 12,
    RK_ID_H265E   = 13,
    RK_ID_JPEGD   = 14,
    RK_ID_VO      = 15,
    RK_ID_VI      = 16,
    RK_ID_DIS     = 17,
    RK_ID_VALG    = 18,
    RK_ID_RC      = 19,
    RK_ID_AIO     = 20,
    RK_ID_AI      = 21,
    RK_ID_AO      = 22,
    RK_ID_AENC    = 23,
    RK_ID_ADEC    = 24,
    RK_ID_VPU    = 25,
    RK_ID_PCIV    = 26,
    RK_ID_PCIVFMW = 27,
    RK_ID_ISP      = 28,
    RK_ID_IVE      = 29,
    RK_ID_USER    = 30,
    RK_ID_DCCM    = 31,
    RK_ID_DCCS    = 32,
    RK_ID_PROC    = 33,
    RK_ID_LOG     = 34,
    RK_ID_VFMW    = 35,
    RK_ID_H264D   = 36,
    RK_ID_GDC     = 37,
    RK_ID_PHOTO   = 38,
    RK_ID_FB      = 39,
    RK_ID_HDMI    = 40,
    RK_ID_VOIE    = 41,
    RK_ID_TDE     = 42,
    RK_ID_HDR      = 43,
    RK_ID_PRORES  = 44,
    RK_ID_VGS     = 45,

    RK_ID_FD      = 47,
    RK_ID_ODT      = 48, //Object detection trace
    RK_ID_VQA      = 49, //Video quality  analysis
    RK_ID_LPR      = 50, //Object detection trace
    RK_ID_SVP_NNIE     = 51,
    RK_ID_SVP_DSP      = 52,
    RK_ID_DPU_RECT     = 53,
    RK_ID_DPU_MATCH    = 54,

    RK_ID_MOTIONSENSOR = 55,
    RK_ID_MOTIONFUSION = 56,

    RK_ID_GYRODIS      = 57,
    RK_ID_PM           = 58,
    RK_ID_SVP_ALG      = 59,
    RK_ID_IVP          = 60,

    RK_ID_BUTT,
} MOD_ID_E;

typedef enum rkEN_ERR_CODE_E {
    EN_ERR_INVALID_DEVID = 1, /* invlalid device ID                           */
    EN_ERR_INVALID_CHNID = 2, /* invlalid channel ID                          */
    EN_ERR_ILLEGAL_PARAM = 3, /* at lease one parameter is illagal
                               * eg, an illegal enumeration value             */
    EN_ERR_EXIST         = 4, /* resource exists                              */
    EN_ERR_UNEXIST       = 5, /* resource unexists                            */

    EN_ERR_NULL_PTR      = 6, /* using a NULL point                           */

    EN_ERR_NOT_CONFIG    = 7, /* try to enable or initialize system, device
                              ** or channel, before configing attribute       */

    EN_ERR_NOT_SUPPORT   = 8, /* operation or type is not supported by NOW    */
    EN_ERR_NOT_PERM      = 9, /* operation is not permitted
                              ** eg, try to change static attribute           */
    EN_ERR_INVALID_PIPEID = 10, /* invlalid pipe ID                           */
    EN_ERR_INVALID_STITCHGRPID  = 11, /* invlalid stitch group ID                           */

    EN_ERR_NOMEM         = 12,/* failure caused by malloc memory              */
    EN_ERR_NOBUF         = 13,/* failure caused by malloc buffer              */

    EN_ERR_BUF_EMPTY     = 14,/* no data in buffer                            */
    EN_ERR_BUF_FULL      = 15,/* no buffer for new data                       */

    EN_ERR_SYS_NOTREADY  = 16,/* System is not ready,maybe not initialed or
                              ** loaded. Returning the error code when opening
                              ** a device file failed.                        */

    EN_ERR_BADADDR       = 17,/* bad address,
                              ** eg. used for copy_from_user & copy_to_user   */

    EN_ERR_BUSY          = 18,/* resource is busy,
                              ** eg. destroy a venc chn without unregister it */
    EN_ERR_SIZE_NOT_ENOUGH = 19, /* buffer size is smaller than the actual size required */

    EN_ERR_BUTT          = 63,/* maxium code, private error code of all modules
                              ** must be greater than it                      */
}EN_ERR_CODE_E;

#define RK_ERR_SYS_NULL_PTR         -1
#define RK_ERR_SYS_NOTREADY         -2
#define RK_ERR_SYS_NOT_PERM         -3
#define RK_ERR_SYS_NOMEM            -4
#define RK_ERR_SYS_ILLEGAL_PARAM    -5
#define RK_ERR_SYS_BUSY             -6
#define RK_ERR_SYS_NOT_SUPPORT      -7

/* invlalid channel ID */
#define RK_ERR_VENC_INVALID_CHNID     0
/* at lease one parameter is illagal ,eg, an illegal enumeration value  */
#define RK_ERR_VENC_ILLEGAL_PARAM     0
/* channel exists */
#define RK_ERR_VENC_EXIST             0
/* channel exists */
#define RK_ERR_VENC_UNEXIST           0
/* using a NULL point */
#define RK_ERR_VENC_NULL_PTR          0
/* try to enable or initialize system,device or channel, before configing attrib */
#define RK_ERR_VENC_NOT_CONFIG        0
/* operation is not supported by NOW */
#define RK_ERR_VENC_NOT_SUPPORT       0
/* operation is not permitted ,eg, try to change stati attribute */
#define RK_ERR_VENC_NOT_PERM          0
/* failure caused by malloc memory */
#define RK_ERR_VENC_NOMEM             0
/* failure caused by malloc buffer */
#define RK_ERR_VENC_NOBUF             0
/* no data in buffer */
#define RK_ERR_VENC_BUF_EMPTY         0
/* no buffer for new data */
#define RK_ERR_VENC_BUF_FULL          0
/* system is not ready,had not initialed or loaded*/
#define RK_ERR_VENC_SYS_NOTREADY      0
/* system is busy*/
#define RK_ERR_VENC_BUSY              0


//Platform resource number configuration
#define VI_MAX_DEV_NUM      4
#define VI_MAX_CHN_NUM      4
#define VENC_MAX_CHN_NUM    16

typedef struct rkMPP_CHN_S {
    MOD_ID_E    enModId;
    RK_S32      s32DevId;
    RK_S32      s32ChnId;
} MPP_CHN_S;


typedef RK_S32 VI_PIPE;
typedef RK_S32 VI_CHN;
typedef RK_S32 VENC_CHN;


/* the attribute of the venc chnl*/
typedef struct hiVENC_CHN_ATTR_S {
    int stVencAttr;                   /*the attribute of video encoder*/
    int stRcAttr;                     /*the attribute of rate  ctrl*/
    int stGopAttr;                    /*the attribute of gop*/
} VENC_CHN_ATTR_S;

#define _CAPI __attribute__((visibility("default")))

/********************************************************************
 * SYS Ctrl api
 ********************************************************************/
_CAPI RK_S32  RK_MPI_SYS_Bind(const MPP_CHN_S *pstSrcChn, const MPP_CHN_S *pstDestChn);

/********************************************************************
 * Vi api
 ********************************************************************/
_CAPI RK_S32 RK_MPI_VI_DisableChn(VI_PIPE ViPipe, VI_CHN ViChn);
_CAPI RK_S32 RK_MPI_VI_EnableChn(VI_PIPE ViPipe, VI_CHN ViChn);


/********************************************************************
 * Venc api
 ********************************************************************/
_CAPI RK_S32 RK_MPI_VENC_CreateChn(VENC_CHN VeChn, VENC_CHN_ATTR_S *stVencChnAttr);
_CAPI RK_S32 RK_MPI_VENC_DestroyChn(VENC_CHN VeChn);

//#ifdef __cplusplus
//}
//#endif

#endif //__RKMEDIA_API_
