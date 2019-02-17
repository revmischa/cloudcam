// GStreamer RTP streaming implementation

#include <signal.h>
#include <pthread.h>
#include <assert.h>
#include "cloudcam/cloudcam.h"
#include "cloudcam/log.h"
#include "cloudcam/ccgst.h"

// creates a jpeg snapshot of the next available frame
// returned buffer must be released via free()
void *ccgst_get_jpeg_snapshot(ccgst_thread_ctx *ctx, size_t *snapshot_size)
{
  assert(ctx != NULL);
  assert(snapshot_size != NULL);
  pthread_mutex_lock(&ctx->snapshot_mutex);
  unsigned long prev_snapshot_frame = ctx->snapshot_frame;
  // set the update flag and wait until the snapshot is created
  ctx->snapshot_update_flag = 1;
  do {
    pthread_cond_wait(&ctx->snapshot_cond, &ctx->snapshot_mutex);
  }
  while (ctx->snapshot_frame == prev_snapshot_frame); // wait until snapshot was actually updated
  // copy the snapshot and return
  void *snapshot_buf = NULL;
  if (ctx->snapshot_size != 0) {
    snapshot_buf = malloc(ctx->snapshot_size);
    memcpy(snapshot_buf, ctx->snapshot_buf, ctx->snapshot_size);
  }
  *snapshot_size = ctx->snapshot_size;
  pthread_mutex_unlock(&ctx->snapshot_mutex);
  return snapshot_buf;
}

// this pad probe passes the video frame to jpeg encoder or drops it depending on if a snapshot was requested
// also keeps the frame count in ctx->current_frame
static GstPadProbeReturn jpeg_sink_pad_probe_cb(GstPad          *pad,
                                                GstPadProbeInfo *info,
                                                gpointer         user_data)
{
  ccgst_thread_ctx *ctx = (ccgst_thread_ctx *)user_data;
  assert(ctx != NULL);
  GstPadProbeReturn probe_return = GST_PAD_PROBE_DROP; // by default, drop the buffer
  pthread_mutex_lock(&ctx->snapshot_mutex);
  ++ctx->current_frame; // update the total frame count
  // check if a snapshot was requested and clear the flag if so
  if (ctx->snapshot_update_flag == 1) {
    ctx->snapshot_update_flag = 0;
    probe_return = GST_PAD_PROBE_OK; // pass the buffer to jpeg encoder
  }
  pthread_mutex_unlock(&ctx->snapshot_mutex);
  return probe_return;
}

// this pad probe receives the jpeg-encoded snapshot (if it was requested and allowed through by jpeg_sink_pad_probe_cb)
static GstPadProbeReturn jpeg_src_pad_probe_cb(GstPad          *pad,
                                               GstPadProbeInfo *info,
                                               gpointer         user_data)
{
  ccgst_thread_ctx *ctx = (ccgst_thread_ctx *)user_data;
  assert(ctx != NULL);
  GstMapInfo map;
  GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
  if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
    ERROR("jpeg_src_pad_probe_cb(): gst_buffer_map() failed");
    return GST_PAD_PROBE_OK;
  }
  pthread_mutex_lock(&ctx->snapshot_mutex);
  // (re)allocate the snapshot buffer
  if (map.size > ctx->snapshot_buf_capacity || ctx->snapshot_buf == NULL) {
    if (ctx->snapshot_buf != NULL) {
      free(ctx->snapshot_buf);
    }
    ctx->snapshot_buf = malloc(map.size);
    ctx->snapshot_buf_capacity = map.size;
  }
  // copy jpeg data into ctx->snapshot_buf
  memcpy(ctx->snapshot_buf, map.data, map.size);
  ctx->snapshot_size = map.size;
  ctx->snapshot_frame = ctx->current_frame;
  DEBUG("jpeg_src_pad_probe_cb(): updated snapshot of frame %lu, %lu bytes", ctx->snapshot_frame, ctx->snapshot_size);
  // wake up any threads waiting for a snapshot
  pthread_cond_broadcast(&ctx->snapshot_cond);
  pthread_mutex_unlock(&ctx->snapshot_mutex);
  gst_buffer_unmap(buffer, &map);
  return GST_PAD_PROBE_OK;
}

// updates stream parameters
void ccgst_update_stream_params(ccgst_thread_ctx *ctx, int h264_bitrate, const char *rtp_host, int rtp_port) {
  assert(ctx != NULL);
  assert(h264_bitrate != 0);
  assert(rtp_host != NULL);
  assert(rtp_port != 0);
  pthread_mutex_lock(&ctx->params_mutex);
  // copy values into the ccgst_thread_ctx
  ctx->h264_bitrate = h264_bitrate;
  strncpy(ctx->rtp_host, rtp_host, sizeof(ctx->rtp_host));
  ctx->rtp_port = rtp_port;
  pthread_mutex_unlock(&ctx->params_mutex);
}

static gboolean update_stream_params_cb(gpointer user_data) {
  ccgst_thread_ctx *ctx = (ccgst_thread_ctx *)user_data;
  assert(ctx != NULL);
  pthread_mutex_lock(&ctx->params_mutex);
  guint prev_h264_bitrate;
  g_object_get(G_OBJECT(ctx->x264enc), "bitrate", &prev_h264_bitrate, NULL);
  if (prev_h264_bitrate != ctx->h264_bitrate) {
    INFO("update_stream_params_cb(): bitrate: %dkbps", ctx->h264_bitrate);
    g_object_set(G_OBJECT(ctx->x264enc), "bitrate", ctx->h264_bitrate, NULL);
  }
  gchar *prev_rtp_host;
  gint prev_rtp_port;
  g_object_get(G_OBJECT(ctx->udpsink), "host", &prev_rtp_host, "port", &prev_rtp_port, NULL);
  if (strcmp(prev_rtp_host, ctx->rtp_host) != 0 || prev_rtp_port != ctx->rtp_port) {
    INFO("update_stream_params_cb(): RTP sink: %s:%d", ctx->rtp_host, ctx->rtp_port);
    g_object_set(G_OBJECT(ctx->udpsink), "host", ctx->rtp_host, "port", (gint)ctx->rtp_port, NULL);
  }
  // FIXME:
  //  g_free(prev_rtp_host);
  pthread_mutex_unlock(&ctx->params_mutex);
  return G_SOURCE_CONTINUE;
}

static void *ccgst_thread_fn(void *param) {
  ccgst_thread_ctx *ctx = (ccgst_thread_ctx *)param;
  assert(ctx != NULL);

  // initialize ccgst_thread_ctx
  pthread_mutex_init(&ctx->snapshot_mutex, NULL);
  pthread_cond_init(&ctx->snapshot_cond, NULL);
  ctx->snapshot_buf = NULL;
  ctx->snapshot_buf_capacity = ctx->snapshot_size = 0;
  ctx->current_frame = 0;
  ctx->snapshot_frame = -1;
  pthread_mutex_init(&ctx->params_mutex, NULL);
  ctx->h264_bitrate = 256;
  strcpy(ctx->rtp_host, "localhost");
  ctx->rtp_port = 20000;

  // initialize GStreamer/main loop
  gst_init(NULL, NULL);
  ctx->loop = g_main_loop_new(NULL, FALSE);

  // create the stream pipeline
  ctx->pipeline = gst_pipeline_new("default");
  ctx->source = gst_element_factory_make("videotestsrc", "source");
  ctx->source_filter = gst_element_factory_make ("capsfilter", "source-filter");
  ctx->timeoverlay = gst_element_factory_make("timeoverlay", "overlay");
  ctx->tee = gst_element_factory_make ("tee", "tee");
  ctx->x264enc = gst_element_factory_make("x264enc", "x264-encoder");
  g_object_set(G_OBJECT(ctx->x264enc), "bitrate", ctx->h264_bitrate, NULL);
  ctx->enc_filter = gst_element_factory_make ("capsfilter", "enc-filter");
  ctx->rtph264pay = gst_element_factory_make("rtph264pay", "rtph264");
  g_object_set(G_OBJECT(ctx->rtph264pay), "config-interval", 1, NULL);
  g_object_set(G_OBJECT(ctx->rtph264pay), "pt", 96, NULL);
  ctx->udpsink = gst_element_factory_make("udpsink", "udpsink");
  g_object_set(G_OBJECT(ctx->udpsink), "host", ctx->rtp_host, NULL);
  g_object_set(G_OBJECT(ctx->udpsink), "port", ctx->rtp_port, NULL);
  ctx->jpegenc = gst_element_factory_make("jpegenc", "jpeg-encoder");

  gst_bin_add_many(GST_BIN(ctx->pipeline), ctx->source, ctx->source_filter, ctx->timeoverlay, ctx->tee, ctx->x264enc,
                   ctx->enc_filter, ctx->rtph264pay, ctx->udpsink, ctx->jpegenc, NULL);
  gst_element_link_many(ctx->source, ctx->source_filter, ctx->timeoverlay, ctx->tee, NULL);
  gst_element_link_many(ctx->tee, ctx->x264enc, ctx->enc_filter, ctx->rtph264pay, ctx->udpsink, NULL);
  gst_element_link_many(ctx->tee, ctx->jpegenc, NULL);

  ctx->source_caps = gst_caps_new_simple("video/x-raw",
                                         "format", G_TYPE_STRING, "I420",
                                         "width", G_TYPE_INT, 480,
                                         "height", G_TYPE_INT, 320,
                                         "framerate", GST_TYPE_FRACTION, 30, 1,
                                         NULL);
  g_object_set(G_OBJECT(ctx->source_filter), "caps", ctx->source_caps, NULL);
  gst_caps_unref(ctx->source_caps);

  ctx->enc_caps = gst_caps_new_simple("video/x-h264", "profile", G_TYPE_STRING, "baseline", NULL);
  g_object_set(G_OBJECT(ctx->enc_filter), "caps", ctx->enc_caps, NULL);
  gst_caps_unref(ctx->enc_caps);

  // create the pads used for snapshot creation/extraction
  ctx->jpeg_sink_pad = gst_element_get_static_pad(ctx->jpegenc, "sink");
  gst_pad_add_probe(ctx->jpeg_sink_pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)jpeg_sink_pad_probe_cb, ctx, NULL);
  gst_object_unref(ctx->jpeg_sink_pad);

  ctx->jpeg_src_pad = gst_element_get_static_pad(ctx->jpegenc, "src");
  gst_pad_add_probe(ctx->jpeg_src_pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)jpeg_src_pad_probe_cb, ctx, NULL);
  gst_object_unref(ctx->jpeg_src_pad);

  // run the pipeline
  gst_element_set_state(ctx->pipeline, GST_STATE_PLAYING);

  if (gst_element_get_state(ctx->pipeline, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE) {
    ERROR("Failed to go into PLAYING state");
    return NULL;
  }

  // run update_stream_params_cb every once in a while so stream params are updated in a thread-safe way
  g_timeout_add(500, &update_stream_params_cb, ctx);

  INFO("GStreamer pipeline started");
  g_main_loop_run(ctx->loop);

  gst_element_set_state(ctx->pipeline, GST_STATE_NULL);
  gst_object_unref(ctx->pipeline);

  INFO("GStreamer pipeline finished");

  // release remaining resources from the ccgst_thread_ctx
  pthread_mutex_destroy(&ctx->params_mutex);
  pthread_mutex_lock(&ctx->snapshot_mutex);
  ctx->snapshot_buf_capacity = ctx->snapshot_size = 0;
  free(ctx->snapshot_buf);
  ctx->snapshot_buf = 0;
  pthread_cond_destroy(&ctx->snapshot_cond);
  pthread_mutex_unlock(&ctx->snapshot_mutex);
  pthread_mutex_destroy(&ctx->snapshot_mutex);

  return NULL;
}

// starts the GStreamer thread/stream
void ccgst_start_stream(ccgst_thread_ctx *ctx) {
  assert(ctx != NULL);
  int r = pthread_create(&ctx->thread_id, NULL, &ccgst_thread_fn, ctx);
  if (r != 0) {
    ERROR("ccgst_start_stream(): pthread_create failed (%d)", r);
    return;
  }
}
