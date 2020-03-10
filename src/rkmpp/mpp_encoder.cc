// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mpp_encoder.h"

#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include <memory>

#include "utils.h"
#include "buffer.h"

namespace easymedia {

MPPEncoder::MPPEncoder()
    : coding_type(MPP_VIDEO_CodingAutoDetect),
      output_mb_flags(0), encoder_sta_en(false),
      stream_size_1s(0), frame_cnt_1s(0), last_ts(0), cur_ts(0),
      osd_mask(0), osd_refresh_mask(0), osd_thread(NULL),
      osd_thread_loop(NULL) {
  //reset osd data.
  memset(osd_data, 0, sizeof(osd_data));
  memset(&enc_osd_data, 0, sizeof(enc_osd_data));
}

MPPEncoder::~MPPEncoder() {
  if (osd_thread) {
    LOGD("MPP Encoder: destructor stop osd thread...\n");
    osd_thread_loop = false;
    osd_thread->join();
    LOGD("MPP Encoder: destructor stop osd thread sucess!\n");
    osd_thread = NULL;
  }
  for (int i = 0; i < 2; i++) {
    if (osd_data[i].buf) {
      LOGD("MPP Encoder: free osd[%d] buff\n", i);
      mpp_buffer_put(osd_data[i].buf);
      osd_data[i].buf = NULL;
    }
  }
  if (enc_osd_data.buf) {
    LOGD("MPP Encoder: free enc osd buff\n");
    mpp_buffer_put(enc_osd_data.buf);
    enc_osd_data.buf = NULL;
  }
}

void MPPEncoder::SetMppCodeingType(MppCodingType type) {
  coding_type = type;
  if (type == MPP_VIDEO_CodingMJPEG)
    codec_type = CODEC_TYPE_JPEG;
  else if (type == MPP_VIDEO_CodingAVC)
    codec_type = CODEC_TYPE_H264;
  else if (type == MPP_VIDEO_CodingHEVC)
    codec_type = CODEC_TYPE_H265;
  // mpp always return a single nal
  if (type == MPP_VIDEO_CodingAVC || type == MPP_VIDEO_CodingHEVC)
    output_mb_flags |= MediaBuffer::kSingleNalUnit;
}

bool MPPEncoder::Init() {
  if (coding_type == MPP_VIDEO_CodingUnused)
    return false;
  mpp_ctx = std::make_shared<MPPContext>();
  if (!mpp_ctx)
    return false;
  MppCtx ctx = NULL;
  MppApi *mpi = NULL;
  int ret = mpp_create(&ctx, &mpi);
  if (ret) {
    LOG("mpp_create failed\n");
    return false;
  }
  mpp_ctx->ctx = ctx;
  mpp_ctx->mpi = mpi;
  ret = mpp_init(ctx, MPP_CTX_ENC, coding_type);
  if (ret != MPP_OK) {
    LOG("mpp_init failed with type %d\n", coding_type);
    mpp_destroy(ctx);
    ctx = NULL;
    mpi = NULL;
    return false;
  }
  return true;
}

int MPPEncoder::PrepareMppFrame(const std::shared_ptr<MediaBuffer> &input,
                                std::shared_ptr<MediaBuffer> &mdinfo,
                                MppFrame &frame) {
  MppBuffer pic_buf = nullptr;
  if (input->GetType() != Type::Image) {
    LOG("mpp encoder input source only support image buffer\n");
    return -EINVAL;
  }
  PixelFormat fmt = input->GetPixelFormat();
  if (fmt == PIX_FMT_NONE) {
    LOG("mpp encoder input source invalid pixel format\n");
    return -EINVAL;
  }
  ImageBuffer *hw_buffer = static_cast<ImageBuffer *>(input.get());

  assert(input->GetValidSize() > 0);
  mpp_frame_set_pts(frame, hw_buffer->GetUSTimeStamp());
  mpp_frame_set_dts(frame, hw_buffer->GetUSTimeStamp());
  mpp_frame_set_width(frame, hw_buffer->GetWidth());
  mpp_frame_set_height(frame, hw_buffer->GetHeight());
  mpp_frame_set_fmt(frame, ConvertToMppPixFmt(fmt));

  if (fmt == PIX_FMT_YUYV422 || fmt == PIX_FMT_UYVY422)
    mpp_frame_set_hor_stride(frame, hw_buffer->GetVirWidth() * 2);
  else
    mpp_frame_set_hor_stride(frame, hw_buffer->GetVirWidth());
  mpp_frame_set_ver_stride(frame, hw_buffer->GetVirHeight());

  MppMeta meta = mpp_frame_get_meta(frame);
  auto &related_vec = input->GetRelatedSPtrs();
  if (!related_vec.empty()) {
     mdinfo = std::static_pointer_cast<MediaBuffer>(related_vec[0]);
    LOGD("MPP Encoder: set mdinfo(%p, %zuBytes) to frame\n",
      mdinfo->GetPtr(), mdinfo->GetValidSize());
    mpp_meta_set_ptr(meta, KEY_MV_LIST, mdinfo->GetPtr());
  }

  while (osd_mask) {
    uint8_t data_id = (osd_mask & 0x02) ? 1 : 0;
    LOGD("MPP Encoder: set osd[%d] to frame, osd_mask:0x%x\n",
      data_id, osd_mask);
    osd_mutex[data_id].read_lock();
    MppEncOSDData *osd = &osd_data[data_id];
    size_t src_size = mpp_buffer_get_size(osd->buf);
    size_t dst_size = 0;
    if (enc_osd_data.buf)
      dst_size = mpp_buffer_get_size(enc_osd_data.buf);

    if (dst_size < src_size) {
      if (enc_osd_data.buf) {
        LOG("MPP Encoder: osd resize enc osd buff from %zu to %zu\n",
          dst_size, src_size);
        mpp_buffer_put(enc_osd_data.buf);
      }
      enc_osd_data.buf = NULL;
      if (mpp_buffer_get(NULL, &enc_osd_data.buf, src_size)) {
        LOG("ERROR: MPP Encoder: get enc osd %dBytes buffer failed\n",
          src_size);
        break;
      }
    }

    // copy osd info and buf.
    for (int i = 0; i < OSD_REGIONS_CNT; i++) {
      enc_osd_data.region[i].enable = osd->region[i].enable;
      enc_osd_data.region[i].inverse = osd->region[i].inverse;
      enc_osd_data.region[i].start_mb_x = osd->region[i].start_mb_x;
      enc_osd_data.region[i].start_mb_y = osd->region[i].start_mb_y;
      enc_osd_data.region[i].num_mb_x = osd->region[i].num_mb_x;
      enc_osd_data.region[i].num_mb_y = osd->region[i].num_mb_y;
      enc_osd_data.region[i].buf_offset = osd->region[i].buf_offset;
    }
    enc_osd_data.num_region = osd->num_region;
    void *src_ptr = mpp_buffer_get_ptr(osd->buf);
    void *dst_ptr = mpp_buffer_get_ptr(enc_osd_data.buf);
    memcpy(dst_ptr, src_ptr, src_size);
    osd_mutex[data_id].unlock();

    mpp_meta_set_ptr(meta, KEY_OSD_DATA, (void*)&enc_osd_data);
    break;
  }

  MPP_RET ret = init_mpp_buffer_with_content(pic_buf, input);
  if (ret) {
    LOG("prepare picture buffer failed\n");
    return ret;
  }

  mpp_frame_set_buffer(frame, pic_buf);
  if (input->IsEOF())
    mpp_frame_set_eos(frame, 1);

  mpp_buffer_put(pic_buf);

  return 0;
}

int MPPEncoder::PrepareMppPacket(std::shared_ptr<MediaBuffer> &output,
                                 MppPacket &packet) {
  MppBuffer mpp_buf = nullptr;

  if (!output->IsHwBuffer())
    return 0;

  MPP_RET ret = init_mpp_buffer(mpp_buf, output, 0);
  if (ret) {
    LOG("import output stream buffer failed\n");
    return ret;
  }

  if (mpp_buf) {
    mpp_packet_init_with_buffer(&packet, mpp_buf);
    mpp_buffer_put(mpp_buf);
  }

  return 0;
}

int MPPEncoder::PrepareMppExtraBuffer(std::shared_ptr<MediaBuffer> extra_output,
                                      MppBuffer &buffer) {
  MppBuffer mpp_buf = nullptr;
  if (!extra_output || !extra_output->IsValid())
    return 0;
  MPP_RET ret =
      init_mpp_buffer(mpp_buf, extra_output, extra_output->GetValidSize());
  if (ret) {
    LOG("import extra stream buffer failed\n");
    return ret;
  }
  buffer = mpp_buf;
  return 0;
}

class MPPPacketContext {
public:
  MPPPacketContext(std::shared_ptr<MPPContext> ctx, MppPacket p)
      : mctx(ctx), packet(p) {}
  ~MPPPacketContext() {
    if (packet)
      mpp_packet_deinit(&packet);
  }

private:
  std::shared_ptr<MPPContext> mctx;
  MppPacket packet;
};

static int __free_mpppacketcontext(void *p) {
  assert(p);
  delete (MPPPacketContext *)p;
  return 0;
}

int MPPEncoder::Process(const std::shared_ptr<MediaBuffer> &input,
                        std::shared_ptr<MediaBuffer> &output,
                        std::shared_ptr<MediaBuffer> extra_output) {
  MppFrame frame = nullptr;
  MppPacket packet = nullptr;
  MppPacket import_packet = nullptr;
  MppBuffer mv_buf = nullptr;
  size_t packet_len = 0;
  RK_U32 packet_flag = 0;
  RK_U32 out_eof = 0;
  RK_S64 pts = 0;
  std::shared_ptr<MediaBuffer> mdinfo;

  Type out_type;

  if (!input)
    return 0;
  if (!output)
    return -EINVAL;

  // all changes must set before encode and among the same thread
  if (HasChangeReq()) {
    auto change = PeekChange();
    if (change.first && !CheckConfigChange(change))
      return -1;
  }

  int ret = mpp_frame_init(&frame);
  if (MPP_OK != ret) {
    LOG("mpp_frame_init failed\n");
    goto ENCODE_OUT;
  }

  ret = PrepareMppFrame(input, mdinfo, frame);
  if (ret) {
    LOG("PrepareMppFrame failed\n");
    goto ENCODE_OUT;
  }

  if (output->IsValid()) {
    ret = PrepareMppPacket(output, packet);
    if (ret) {
      LOG("PrepareMppPacket failed\n");
      goto ENCODE_OUT;
    }
    import_packet = packet;
  }

  ret = PrepareMppExtraBuffer(extra_output, mv_buf);
  if (ret) {
    LOG("PrepareMppExtraBuffer failed\n");
    goto ENCODE_OUT;
  }

  ret = Process(frame, packet, mv_buf);
  if (ret)
    goto ENCODE_OUT;

  packet_len = mpp_packet_get_length(packet);
  packet_flag = (mpp_packet_get_flag(packet) & MPP_PACKET_FLAG_INTRA)
                    ? MediaBuffer::kIntra
                    : MediaBuffer::kPredicted;
  out_eof = mpp_packet_get_eos(packet);
  pts = mpp_packet_get_pts(packet);
  if (pts <= 0)
    pts = mpp_packet_get_dts(packet);

  // out fps < in fps ?
  if (packet_len == 0) {
    output->SetValidSize(0);
    if (extra_output)
      extra_output->SetValidSize(0);
    goto ENCODE_OUT;
  }

  // Calculate bit rate statistics.
  if (encoder_sta_en) {
    MediaConfig &cfg = GetConfig();
    int target_fps = cfg.vid_cfg.frame_rate;
    int target_bps = cfg.vid_cfg.bit_rate;
    frame_cnt_1s += 1;
    stream_size_1s += packet_len;
    //Refresh every second
    if ((frame_cnt_1s + 1) % target_fps == 0) {
        // Calculate the frame rate based on the system time.
        cur_ts = gettimeofday() / 1000;
        encoded_fps = ((float)target_fps / (cur_ts - last_ts)) * 1000;
        last_ts = cur_ts;
        // convert bytes to bits
        encoded_bps = stream_size_1s * 8;
        // reset 1s variable
        stream_size_1s = 0;
        frame_cnt_1s = 0;
        LOG("[INFO: MPP ENCODER] bps:%d, actual_bps:%d, "
          "fps:%d, actual_fps:%f\n",
          target_bps, encoded_bps, target_fps, encoded_fps);
    }
  } else if (cur_ts) {
    // clear tmp statistics variable.
    stream_size_1s = 0;
    frame_cnt_1s = 0;
    cur_ts = 0;
    last_ts = 0;
  }

  if (output->IsValid()) {
    if (!import_packet) {
      // !!time-consuming operation
      void *ptr = output->GetPtr();
      assert(ptr);
      LOGD("extra time-consuming memcpy to cpu!\n");
      memcpy(ptr, mpp_packet_get_data(packet), packet_len);
      // sync to cpu?
    }
  } else {
    MPPPacketContext *ctx = new MPPPacketContext(mpp_ctx, packet);
    if (!ctx) {
      LOG_NO_MEMORY();
      ret = -ENOMEM;
      goto ENCODE_OUT;
    }
    output->SetFD(mpp_buffer_get_fd(mpp_packet_get_buffer(packet)));
    output->SetPtr(mpp_packet_get_data(packet));
    output->SetSize(mpp_packet_get_size(packet));
    output->SetUserData(ctx, __free_mpppacketcontext);
    packet = nullptr;
  }
  output->SetValidSize(packet_len);
  output->SetUserFlag(packet_flag | output_mb_flags);
  output->SetUSTimeStamp(pts);
  output->SetEOF(out_eof ? true : false);
  out_type = output->GetType();
  if (out_type == Type::Image) {
    auto out_img = std::static_pointer_cast<ImageBuffer>(output);
    auto &info = out_img->GetImageInfo();
    const auto &in_cfg = GetConfig();
    info = (coding_type == MPP_VIDEO_CodingMJPEG)
               ? in_cfg.img_cfg.image_info
               : in_cfg.vid_cfg.image_cfg.image_info;
    //info.pix_fmt = codec_type;
  } else {
    output->SetType(Type::Video);
  }

  if (mv_buf) {
    if (extra_output->GetFD() < 0) {
      void *ptr = extra_output->GetPtr();
      assert(ptr);
      memcpy(ptr, mpp_buffer_get_ptr(mv_buf), mpp_buffer_get_size(mv_buf));
    }
    extra_output->SetValidSize(mpp_buffer_get_size(mv_buf));
    extra_output->SetUserFlag(packet_flag);
    extra_output->SetUSTimeStamp(pts);
  }

ENCODE_OUT:
  if (frame)
    mpp_frame_deinit(&frame);
  if (packet)
    mpp_packet_deinit(&packet);
  if (mv_buf)
    mpp_buffer_put(mv_buf);

  return ret;
}

int MPPEncoder::Process(MppFrame frame, MppPacket &packet, MppBuffer &mv_buf) {
  MppTask task = NULL;
  MppCtx ctx = mpp_ctx->ctx;
  MppApi *mpi = mpp_ctx->mpi;
  int ret = mpi->poll(ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
  if (ret) {
    LOG("input poll ret %d\n", ret);
    return ret;
  }

  ret = mpi->dequeue(ctx, MPP_PORT_INPUT, &task);
  if (ret || NULL == task) {
    LOG("mpp task input dequeue failed\n");
    return ret;
  }

  mpp_task_meta_set_frame(task, KEY_INPUT_FRAME, frame);
  if (packet)
    mpp_task_meta_set_packet(task, KEY_OUTPUT_PACKET, packet);
  if (mv_buf)
    mpp_task_meta_set_buffer(task, KEY_MOTION_INFO, mv_buf);
  ret = mpi->enqueue(ctx, MPP_PORT_INPUT, task);
  if (ret) {
    LOG("mpp task input enqueue failed\n");
    return ret;
  }
  task = NULL;

  ret = mpi->poll(ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
  if (ret) {
    LOG("output poll ret %d\n", ret);
    return ret;
  }

  ret = mpi->dequeue(ctx, MPP_PORT_OUTPUT, &task);
  if (ret || !task) {
    LOG("mpp task output dequeue failed, ret %d \n", ret);
    return ret;
  }
  if (task) {
    MppPacket packet_out = nullptr;
    mpp_task_meta_get_packet(task, KEY_OUTPUT_PACKET, &packet_out);
    ret = mpi->enqueue(ctx, MPP_PORT_OUTPUT, task);
    if (ret != MPP_OK) {
      LOG("enqueue task output failed, ret = %d\n", ret);
      return ret;
    }
    if (!packet) {
      // the buffer comes from mpp
      packet = packet_out;
      packet_out = nullptr;
    } else {
      assert(packet == packet_out);
    }
    // should not go follow
    if (packet_out && packet != packet_out) {
      mpp_packet_deinit(&packet);
      packet = packet_out;
    }
  }

  return 0;
}

int MPPEncoder::SendInput(const std::shared_ptr<MediaBuffer> &) {
  errno = ENOSYS;
  return -1;
}
std::shared_ptr<MediaBuffer> MPPEncoder::FetchOutput() {
  errno = ENOSYS;
  return nullptr;
}

int MPPEncoder::EncodeControl(int cmd, void *param) {
  MpiCmd mpi_cmd = (MpiCmd)cmd;
  int ret = mpp_ctx->mpi->control(mpp_ctx->ctx, mpi_cmd, (MppParam)param);

  if (ret) {
    LOG("mpp control cmd 0x%08x param %p failed\n", cmd, param);
    return ret;
  }

  return 0;
}

void MPPEncoder::set_statistics_switch(bool value) {
  LOG("[INFO] MPP ENCODER %s statistics\n", value?"enable":"disable");
  encoder_sta_en = value;
}

int MPPEncoder::get_statistics_bps() {
  if (!encoder_sta_en)
    LOG("[WARN] MPP ENCODER statistics should enable first!\n");
  return encoded_bps;
}

int MPPEncoder::get_statistics_fps() {
    if (!encoder_sta_en)
    LOG("[WARN] MPP ENCODER statistics should enable first!\n");
  return encoded_fps;
}

#define OSD_PTL_SIZE 1024 //Bytes.

#ifndef NDEBUG
static void OsdDummpRegions(OsdRegionData r_data[]) {
  for (int i = 0; i < OSD_REGIONS_CNT; i++) {
    OsdRegionData *rdata = &r_data[i];
    if (!rdata->enable)
      continue;
    LOGD("Region Data[%d]:%p:\n", i, rdata);
    LOG("\t region_id:%d\n", rdata->region_id);
    LOG("\t inverse:%d\n", rdata->inverse);
    LOG("\t offset_x:%d\n", rdata->offset_x);
    LOG("\t offset_y:%d\n", rdata->offset_y);
    LOG("\t width:%d\n", rdata->width);
    LOG("\t height:%d\n", rdata->height);
    LOG("\t str:%s\n", rdata->str);
    LOG("\t str_corlor:%d\n", rdata->str_corlor);
    LOG("\t path:%s\n", rdata->path);
    LOG("\t is_ts:%d\n", rdata->is_ts);
  }
}

static void OsdDummpMppOsd(MppEncOSDData *osd) {
  for (int i = 0; i < OSD_REGIONS_CNT; i++) {
    LOGD("MPP OsdRegion[%d]:\n", i);
    if (!osd->region[i].enable)
      continue;

    LOG("\t inverse:%d\n", osd->region[i].inverse);
    LOG("\t offset_x:%d\n", osd->region[i].start_mb_x * 16);
    LOG("\t offset_y:%d\n", osd->region[i].start_mb_y * 16);
    LOG("\t width:%d\n", osd->region[i].num_mb_x * 16);
    LOG("\t height:%d\n", osd->region[i].num_mb_y * 16);
    LOG("\t buf_offset:%d\n", osd->region[i].buf_offset);
  }
}

static void SaveOsdImg(osd_data_s *_data, int index) {
  char _path[64] = {0};
  sprintf(_path, "/tmp/osd_img%d", index);
  LOGD("MPP Encoder: save osd img to %s\n", _path);
  int fd = open(_path, O_WRONLY | O_CREAT);
  if (fd <= 0)
    return;

  for (int j = 0; j < _data->height; j++) {
    for (int k = 0; k < _data->width; k++) {
      if ( _data->buffer[j * _data->width + k] == 0xFF)
        write(fd, "-", 1);
      else
        write(fd, "*", 1);
    }
    write(fd, "\n", 1);
  }
  close(fd);
}

#endif

int MPPEncoder::OsdPaletteSet(uint32_t *ptl_data) {
  if (!ptl_data)
    return -1;

  LOGD("MPP Encoder: setting yuva palette...\n");
  MppEncOSDPlt osd_plt;

  //TODO rgba plt to yuva plt.
  for (int k = 0; k < 256; k++)
    osd_plt.buf[k] = *(ptl_data + k);

  MppCtx ctx = mpp_ctx->ctx;
  MppApi *mpi = mpp_ctx->mpi;
  int ret = mpi->control(ctx, MPP_ENC_SET_OSD_PLT_CFG, &osd_plt);
  if (ret)
    LOG("ERROR: MPP Encoder: set osd plt failed ret %d\n", ret);

  return ret;
}

static void OsdUpdateRegionsInfo(MppEncOSDData *osd,
  OsdRegionData region_data[]) {
  LOGD("MPP Encoder: osd udpate info of regions!\n");
  int update = 0;
  for (int i=0; i < OSD_REGIONS_CNT; i++)
    update |= (osd->region[i].enable != region_data[i].enable) ||
        (osd->region[i].inverse != region_data[i].inverse) ||
        (osd->region[i].start_mb_x != region_data[i].offset_x / 16) ||
        (osd->region[i].start_mb_y != region_data[i].offset_y / 16) ||
        (osd->region[i].num_mb_x != region_data[i].width / 16) ||
        (osd->region[i].num_mb_y != region_data[i].height / 16);
  if (!update) {
    LOGD("MPP Encoder: osd udpate info: nothing to do!\n");
    return;
  }

  if (osd->buf) {
    mpp_buffer_put(osd->buf);
    osd->buf = NULL;
  }
  memset(osd, 0, sizeof(MppEncOSDData));
  // set new osd data info.
  int buffer_size = 0;
  for (int i = 0; i < OSD_REGIONS_CNT; i++) {
    if (region_data[i].enable) {
      osd->region[i].enable = 1;
      osd->region[i].inverse = region_data[i].inverse;
      osd->region[i].start_mb_x = region_data[i].offset_x / 16;
      osd->region[i].start_mb_y = region_data[i].offset_y / 16;
      osd->region[i].num_mb_x = region_data[i].width / 16;
      osd->region[i].num_mb_y = region_data[i].height / 16;

      osd->region[i].buf_offset = buffer_size;
      buffer_size += region_data[i].width * region_data[i].height;
      osd->num_region++;
    }
  }

  if (osd->num_region == 0) {
    LOGD("MPP Encoder: osd udpate info: all regions close!\n");
    return;
  }
#ifndef NDEBUG
  OsdDummpMppOsd(osd);
#endif

  LOGD("MPP Encoder: osd region num:%d, buffer size:%d\n",
    osd->num_region, buffer_size);
  // malloc new osd buff.
  int ret = mpp_buffer_get(NULL, &osd->buf, buffer_size);
  if (ret) {
      LOG("ERROR: MPP Encoder: get osd %dBytes buffer failed(%d)\n",
        buffer_size, ret);
      return;
  }
}

static void get_formate_time_string(const char *fmt, wchar_t *result, int r_size, time_t curtime) {
  char year[8] = {0}, month[4] = {0}, day[4] = {0};
  wchar_t ymd[16] = {0};
  wchar_t w_week[16];
  char week[16] = {0}, hms[12] = {0};
  int wid = -1;
  int wchar_cnt = 0;

  strftime(year, sizeof(year), "%Y", localtime(&curtime));
  strftime(month, sizeof(month), "%m", localtime(&curtime));
  strftime(day, sizeof(day), "%d", localtime(&curtime));

  if (strstr(fmt, "TIME24")) {
    strftime(hms, sizeof(hms), "%H:%M:%S", localtime(&curtime));
    LOGD("MPP Encoder: TIME24: %s\n", hms);
  } else if (strstr(fmt, "TIME12")) {
    strftime(hms, sizeof(hms), "%I:%M:%S", localtime(&curtime));
    LOGD("MPP Encoder: TIME12: %s\n", hms);
  }

  wchar_cnt = sizeof(w_week) / sizeof(wchar_t);
  if (strstr(fmt, "WEEKCN")) {
    strftime(week, sizeof(week), "%u", localtime(&curtime));
    wid = week[0] - '0';
    LOGD("MPP Encoder: week id:%d\n", week[0] - '0');
    if ((wid >= 0) && (wid < 7)) {
      switch (wid) {
        case 1: //Monday
          swprintf(w_week, wchar_cnt, L" 星期一");
          break;
        case 2:
          swprintf(w_week, wchar_cnt, L" 星期二");
          break;
        case 3:
          swprintf(w_week, wchar_cnt, L" 星期三");
          break;
        case 4:
          swprintf(w_week, wchar_cnt, L" 星期四");
          break;
        case 5:
          swprintf(w_week, wchar_cnt, L" 星期五");
          break;
        case 6:
          swprintf(w_week, wchar_cnt, L" 星期六");
          break;
        case 0:
          swprintf(w_week, wchar_cnt, L" 星期日");
          break;
      }
    }
  } else if (strstr(fmt, "WEEK")) {
      strftime(week, sizeof(week), "%A", localtime(&curtime));
      LOGD("MPP Encoder: week:%s\n", week);
      swprintf(w_week, wchar_cnt, L" %s", week);
  }

  wchar_cnt = sizeof(ymd) / sizeof(wchar_t);
  if (strstr(fmt, "CHR")) {
    if (strstr(fmt, "YYYY-MM-DD"))
      swprintf(ymd, wchar_cnt, L"%s-%s-%s", year, month, day);
    else if (strstr(fmt, "MM-DD-YYYY"))
      swprintf(ymd, wchar_cnt, L"%s-%s-%s", month, day, year);
    else if (strstr(fmt, "DD-MM-YYYY"))
      swprintf(ymd, wchar_cnt, L"%s-%s-%s", day, month, year);
    else if (strstr(fmt, "YYYY/MM/DD"))
      swprintf(ymd, wchar_cnt, L"%s/%s/%s", year, month, day);
    else if (strstr(fmt, "MM/DD/YYYY"))
      swprintf(ymd, wchar_cnt, L"%s/%s/%s", month, day, year);
    else if (strstr(fmt, "DD/MM/YYYY"))
      swprintf(ymd, wchar_cnt, L"%s/%s/%s", day, month, year);
  } else {
    if (strstr(fmt, "YYYY-MM-DD"))
      swprintf(ymd, wchar_cnt, L"%s年%s月%s日", year, month, day);
    else if (strstr(fmt, "MM-DD-YYYY"))
      swprintf(ymd, wchar_cnt, L"%s月%s日%s年", month, day, year);
    else if (strstr(fmt, "DD-MM-YYYY"))
      swprintf(ymd, wchar_cnt, L"%s日%s月%s年", day, month, year);
  }

  swprintf(result, r_size, L"%ls%ls %s", ymd, w_week, hms);
}

static void OsdUpdateRegionsBuffer(MppEncOSDData *osd,
  OsdRegionData region_data[], bool refresh_ts, bool refresh_img) {
  LOGD("MPP Encoder: osd udpate buffer of regions!\n");
  uint8_t *ptr = NULL;
  time_t curtime;
  wchar_t *w_s = NULL;

  if (!refresh_ts && !refresh_img)
    return;
  else if (refresh_ts)
    curtime = time(0);

  OSDProducer osd_producer;

  // fill osd buff by rk drawing interface.
  for (int i = 0; i < OSD_REGIONS_CNT; i++) {
    if (!region_data[i].enable)
      continue;

    if (refresh_ts && region_data[i].is_ts) {
      wchar_t ts_wstr[128];
      LOGD("MPP Encoder: osd generating buffer: %s ...\n",
        region_data[i].str);
      get_formate_time_string(region_data[i].str,
        ts_wstr, 128, curtime);
      ptr = (uint8_t *)mpp_buffer_get_ptr(osd->buf) +
        osd->region[i].buf_offset;
      memset(ptr, 0xFF, region_data[i].width * region_data[i].height);
      osd_data_s draw_data;
      draw_data.buffer = ptr;
      draw_data.width = region_data[i].width;
      draw_data.height = region_data[i].height;
      draw_data.text.wch = ts_wstr;
      draw_data.size = draw_data.width * draw_data.height;
      draw_data.type = OSD_TYPE_DATE;
      draw_data.text.font_color = region_data[i].str_corlor;
      draw_data.text.font_size = 32;
      draw_data.text.font_path = "/etc/simsun.ttc";
      osd_producer.FillYuvMap(&draw_data);
#ifndef NDEBUG
      SaveOsdImg(&draw_data,  i);
#endif
      continue;
    }

    if (refresh_img) {
      ptr = (uint8_t *)mpp_buffer_get_ptr(osd->buf) +
        osd->region[i].buf_offset;;
      memset(ptr, 0xFF, region_data[i].width * region_data[i].height);
      osd_data_s draw_data;
      draw_data.buffer = ptr;
      draw_data.width = region_data[i].width;
      draw_data.height = region_data[i].height;
      draw_data.size = draw_data.width * draw_data.height;
      if (strlen(region_data[i].path)) {
        LOGD("MPP Encoder: osd generating img buffer:%s ...\n",
          region_data[i].path);
        draw_data.type = OSD_TYPE_IMAGE;
        draw_data.image = region_data[i].path;
      } else {
        LOGD("MPP Encoder: osd generating text buffer:%s ...\n",
          region_data[i].str);
        int char_size = strlen(region_data[i].str) + 1;
        w_s = (wchar_t *)malloc(char_size * sizeof(wchar_t));
        swprintf(w_s, char_size, L"%s", region_data[i].str);
        draw_data.type = OSD_TYPE_TEXT;
        draw_data.text.wch = w_s;
        draw_data.text.font_color = region_data[i].str_corlor;
        draw_data.text.font_size = 32;
        draw_data.text.font_path = "/etc/simsun.ttc";
      }
      osd_producer.FillYuvMap(&draw_data);
#ifndef NDEBUG
      SaveOsdImg(&draw_data, i);
#endif
      if (w_s) {
        free(w_s);
        w_s = NULL;
      }
    }
  }
}

void MPPEncoder::OsdSyncUpdateRegions() {
  LOGD("MPP Encoder: osd syn update osd_data[0]!\n");

  // clear old osd data.
  MppEncOSDData *osd = &osd_data[0];
  OsdUpdateRegionsInfo(osd, region_data);

  if (osd->num_region) {
    OsdUpdateRegionsBuffer(osd, region_data, false, true);
    osd_mask = 0x01;
  } else {
    osd_mask = 0;
    if (osd->buf) {
      mpp_buffer_put(osd->buf);
      osd->buf = NULL;
    }
  }
  osd_refresh_mask = 0;
}

void MPPEncoder::OsdAsyncUpdateRegions() {
  LOGD("MPP Encoder: osd thread start!\n");
  uint64_t start_time;
  int sleep_time; //us.
  uint8_t data_id;
  MppEncOSDData *osd;
  bool need_refresh;

  while (osd_thread_loop) {
    sleep_time = 1000000;
    start_time = gettimeofday();

    // ping pong opration.
    data_id = (osd_mask & 0x01) ? 1 : 0;
    osd = &osd_data[data_id];
    need_refresh = (osd_refresh_mask & (0x01 << data_id)) ? true : false;
    LOGD("MPP Encoder: osd thread data_id:%d, refresh_mask:0x%d!\n",
      data_id, osd_refresh_mask);

    osd_mutex[data_id].lock();
    if (need_refresh) {
      region_mutex.read_lock();
      OsdUpdateRegionsInfo(osd, region_data);
      region_mutex.unlock();
      if (osd->num_region == 0) {
        osd_mask = 0;
        osd_refresh_mask = 0;
        osd_mutex[data_id].unlock();
        break;
      } else {
        bool ts_valid = false;
        for (int i = 0; i < OSD_REGIONS_CNT; i++) {
          if (region_data[i].is_ts) {
            ts_valid = true;
            break;
          }
        }
        osd_thread_loop = ts_valid;
        if (!osd_thread_loop) {
          osd_mutex[data_id].unlock();
          break;
        }
      }
    }
    OsdUpdateRegionsBuffer(osd, region_data, true, need_refresh);
    osd_mutex[data_id].unlock();
    osd_mask = 0x01 << data_id;
    if (osd_refresh_mask)
      osd_refresh_mask &= (0x01 << (1 - data_id));

    if ((int)(gettimeofday() - start_time) > sleep_time) {
      LOG("ERROR: MPP Encoder: osd update cost too long(%d)\n",
        (uint32_t)(gettimeofday() - start_time));
    } else
      sleep_time -= (int)(gettimeofday() - start_time);

    LOGD("MPP Encoder: osd thread sleep %fs\n", sleep_time / 1000000.0);
    usleep(sleep_time);
  }
  LOGD("MPP Encoder: osd thread stoped!\n");
}

int MPPEncoder::OsdRegionSet(OsdRegionData *rdata) {
  if (!rdata)
    return -EINVAL;

  LOGD("MPP Encoder: setting osd regions...\n");
  if ((rdata->region_id <= 0) || (rdata->region_id > 8)) {
    LOG("ERROR: MPP Encoder: invalid region id(%d), should be [1, 8].\n",
      rdata->region_id);
    return -EINVAL;
  }

  // close region[region_id].
  int region_id = rdata->region_id - 1;
  if (!rdata->enable && !region_data[region_id].enable)
    return 0;

  if (rdata->enable && !strlen(rdata->path) &&
    !strlen(rdata->str)) {
    LOG("ERROR: MPP Encoder: invalid region data");
    return -EINVAL;
  }

#if 0
  if (osd_refresh) {
    LOG("ERROR: MPP Encoder: osd buffer is refreshing...\n");
    return -EBUSY;
  }
#endif

  if ((rdata->width % 16) || (rdata->height % 16) ||
    (rdata->offset_x % 16) || (rdata->offset_y % 16)) {
    LOG("WARN: MPP Encoder: osd size must be 16 aligned\n");
    rdata->width = UPALIGNTO16(rdata->width);
    rdata->height = UPALIGNTO16(rdata->height);
    rdata->offset_x = UPALIGNTO16(rdata->offset_x);
    rdata->offset_y = UPALIGNTO16(rdata->offset_y);
  }

  int need_updata = (region_data[region_id].enable != rdata->enable) ||
    (region_data[region_id].path != rdata->path) ||
    (region_data[region_id].str != rdata->str) ||
    (region_data[region_id].str_corlor != rdata->str_corlor) ||
    (region_data[region_id].inverse != rdata->inverse) ||
    (region_data[region_id].is_ts != rdata->is_ts) ||
    (region_data[region_id].width != rdata->width) ||
    (region_data[region_id].height != rdata->height) ||
    (region_data[region_id].offset_x != rdata->offset_x) ||
    (region_data[region_id].offset_y != rdata->offset_y);

  // nothing to do.
  if (!need_updata) {
    LOGD("MPP Encoder: osd regions no updates required!\n");
    return 0;
  }

  // update region_data[].
  region_mutex.lock();
  memcpy(&region_data[region_id], rdata, sizeof(OsdRegionData));
  region_mutex.unlock();
  // osd_data[0] & osd_data[1] all need refresh.
  osd_refresh_mask = 0x03;
#ifndef NDEBUG
  OsdDummpRegions(region_data);
#endif
  // Check the update time thread.
  bool ts_valid = false;
  for (int i = 0; i < OSD_REGIONS_CNT; i++) {
    if (region_data[i].is_ts) {
      ts_valid = true;
      break;
    }
  }

  if (ts_valid && !osd_thread) {
    LOG("INFO: MPP Encoder: start osd thread for update ts\n");
    osd_thread_loop = true;
    osd_thread = new std::thread(&MPPEncoder::OsdAsyncUpdateRegions, this);
    if (!osd_thread) {
      LOG("ERROR: MPP Encoder: start osd thread failed!\n");
      return -EINVAL;
    }
  } else if (!ts_valid && osd_thread) {
    LOG("INFO: MPP Encoder: osd thread stoping...\n");
    osd_thread_loop = false;
    osd_thread->join();
    LOG("INFO: MPP Encoder: osd thread stoped!\n");
    osd_thread = NULL;
  }

  if (!osd_thread)
    OsdSyncUpdateRegions();

  return 0;
}

int MPPEncoder::OsdRegionGet(OsdRegionData *rdata) {
  LOG("ToDo...%p\n", rdata);
  return 0;
}

} // namespace easymedia
