#ifndef CLOUDCAM_GST_H
#define CLOUDCAM_GST_H

#include <gst/gst.h>

// internal state for the GStreamer thread/pipeline
typedef struct gst_thread_ctx {
  pthread_t thread_id;

  // main loop
  GMainLoop *loop;

  // stream pipeline
  GstElement *pipeline;
  GstElement *source;
  GstElement *source_filter;
  GstElement *timeoverlay;
  GstElement *tee;
  GstElement *x264enc;
  GstElement *enc_filter;
  GstElement *rtph264pay;
  GstElement *udpsink;
  GstElement *jpegenc;
  GstCaps *source_caps;
  GstCaps *enc_caps;
  GstPad *jpeg_sink_pad;
  GstPad *jpeg_src_pad;

  // snapshot-related values
  unsigned long current_frame;
  unsigned long snapshot_frame;
  void *snapshot_buf;
  size_t snapshot_buf_capacity;
  size_t snapshot_size;
  int snapshot_update_flag;
  pthread_mutex_t snapshot_mutex;
  pthread_cond_t snapshot_cond;

  // stream params
  guint h264_bitrate;
  char rtp_host[256];
  guint rtp_port;
  pthread_mutex_t params_mutex;
} gst_thread_ctx;

// public interface

// starts the GStreamer thread/stream
void gst_start_stream(gst_thread_ctx *ctx);

// updates stream parameters; thread-safe
void gst_update_stream_params(gst_thread_ctx *ctx, int h264_bitrate, const char *rtp_host, int rtp_port);

// creates a jpeg snapshot of the next available frame
// buffer returned in *data must be released via free()
size_t gst_get_jpeg_snapshot(gst_thread_ctx *ctx, void **data);

#endif // CLOUDCAM_GST_H
