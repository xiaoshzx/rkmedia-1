#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "rkmedia_common.h"

#ifdef RKAIQ
#include <rk_aiq_user_api_imgproc.h>
#include <rk_aiq_user_api_sysctl.h>
/*
 * stream on:
 * 1) ISP Init
 * 2) ISP Stop
 * 3) VI enable and stream on
 *
 * stream off:
 * 4) ISP Stop
 * 5) VI disable
 */
/*
typedef enum {
 RK_AIQ_WORKING_MODE_NORMAL,
 RK_AIQ_WORKING_MODE_ISP_HDR2    = 0x10,
 RK_AIQ_WORKING_MODE_ISP_HDR3    = 0x20,
 //RK_AIQ_WORKING_MODE_SENSOR_HDR = 10, // sensor built-in hdr mode
} rk_aiq_working_mode_t;
*/

RK_S32 SAMPLE_COMM_ISP_Init(rk_aiq_working_mode_t WDRMode);
RK_VOID SAMPLE_COMM_ISP_Stop(void);
RK_S32 SAMPLE_COMM_ISP_Run(void); // isp stop before vi streamoff

RK_VOID SAMPLE_COMM_ISP_FEC_Set(RK_BOOL open);
#endif
