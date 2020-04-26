// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v4l2_stream.h"

#include <fcntl.h>
#include <cstring>

#include "control.h"

namespace easymedia {

V4L2Context::V4L2Context(enum v4l2_buf_type cap_type, v4l2_io io_func,
                         char *nodename)
    : fd(-1), capture_type(cap_type), vio(io_func), started(false)
#ifndef NDEBUG
      ,
      path(device)
#endif
{
  char *dev = nodename;
  fd = v4l2_open(dev, O_RDWR | O_CLOEXEC, 0);
  if (fd < 0)
    LOG("open %s failed %m\n", dev);
  LOGD("open %s, fd %d\n", dev, fd);
}

V4L2Context::~V4L2Context() {
  if (fd >= 0) {
    SetStarted(false);
    v4l2_close(fd);
    LOGD("close %s, fd %d\n", path.c_str(), fd);
  }
}

bool V4L2Context::SetStarted(bool val) {
  std::lock_guard<std::mutex> _lk(mtx);
  if (started == val)
    return true;
  enum v4l2_buf_type cap_type = capture_type;
  unsigned int request = val ? VIDIOC_STREAMON : VIDIOC_STREAMOFF;
  if (IoCtrl(request, &cap_type) < 0) {
    LOG("ioctl(%d): %m\n", (int)request);
    return false;
  }
  started = val;
  return true;
}

int V4L2Context::IoCtrl(unsigned long int request, void *arg) {
  if (fd < 0) {
    errno = EINVAL;
    return -1;
  }
  return V4L2IoCtl(&vio, fd, request, arg);
}

V4L2MediaCtl::V4L2MediaCtl(char* entity_name, ispp_media_info *ispp_info)
{
  const char* mdev_path = "/dev/media1";
  int ret;
  struct media_device *mdev;

  mdev = media_device_new (mdev_path);
  if (!mdev)
    LOG("new media device failed %m\n");

  /* Enumerate entities, pads and links. */
  ret = media_device_enumerate (mdev);
  ret = GetNodeName(mdev, entity_name, ispp_info->sd_ispp_path);
  if (ret){
    media_device_unref (mdev);
    return;
  }
}

int V4L2MediaCtl::GetNodeName(struct media_device * mdev, char* ent_name,
	char * nod_name)
{
  const char *devname;
  struct media_entity *entity =  NULL;

  entity = media_get_entity_by_name(mdev, ent_name, strlen(ent_name));
  if (!entity)
    return -1;

  devname = media_entity_get_devname(entity);

  if (!devname) {
    fprintf(stderr, "can't find %s device path!", ent_name);
    return -1;
  }

  strncpy(nod_name, devname, FILE_PATH_LEN);
  LOG("get %s devname: %s\n", ent_name, nod_name);

  return 0;
};

V4L2Stream::V4L2Stream(const char *param)
    : use_libv4l2(false), fd(-1), capture_type(V4L2_BUF_TYPE_VIDEO_CAPTURE) {
  memset(&vio, 0, sizeof(vio));
  std::map<std::string, std::string> params;
  std::list<std::pair<const std::string, std::string &>> req_list;
  std::string str_libv4l2;
  req_list.push_back(std::pair<const std::string, std::string &>(
      KEY_USE_LIBV4L2, str_libv4l2));
  req_list.push_back(
      std::pair<const std::string, std::string &>(KEY_DEVICE, device));
  req_list.push_back(
      std::pair<const std::string, std::string &>(KEY_SUB_DEVICE, sub_device));
  std::string cap_type;
  req_list.push_back(
      std::pair<const std::string, std::string &>(KEY_V4L2_CAP_TYPE, cap_type));
  int ret = parse_media_param_match(param, params, req_list);
  if (ret == 0)
    return;
  if (!str_libv4l2.empty())
    use_libv4l2 = !!std::stoi(str_libv4l2);
  if (!cap_type.empty())
    capture_type =
        static_cast<enum v4l2_buf_type>(GetV4L2Type(cap_type.c_str()));
}

int V4L2Stream::Open() {
  ispp_media_info ispp_info;
  if (!SetV4L2IoFunction(&vio, use_libv4l2))
    return -EINVAL;
  if (!sub_device.empty()) {
    // TODO:
  }
  v4l2_medctl = std::make_shared<V4L2MediaCtl>((char *)device.c_str(), &ispp_info);
  v4l2_ctx = std::make_shared<V4L2Context>(capture_type, vio, ispp_info.sd_ispp_path);
  if (!v4l2_ctx)
    return -ENOMEM;
  fd = v4l2_ctx->GetDeviceFd();
  if (fd < 0) {
    v4l2_ctx = nullptr;
    return -1;
  }
  return 0;
}

int V4L2Stream::Close() {
  if (v4l2_ctx) {
    v4l2_ctx->SetStarted(false);
    v4l2_ctx = nullptr; // release reference
  }
  fd = -1;
  return 0;
}

int V4L2Stream::IoCtrl(unsigned long int request, ...) {
  va_list vl;
  va_start(vl, request);
  void *arg = va_arg(vl, void *);
  va_end(vl);
  switch (request) {
  case S_SUB_REQUEST: {
    auto sub = (SubRequest *)arg;
    return V4L2IoCtl(&vio, fd, sub->sub_request, sub->arg);
  }
  case S_STREAM_OFF: {
    return v4l2_ctx->SetStarted(false) ? 0 : -1;
  }
  }
  return -1;
}

} // namespace easymedia
