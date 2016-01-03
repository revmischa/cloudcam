#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>

#include <capture.h>

#ifdef DEBUG
#define D(x)    x
#else
#define D(x)
#endif

#define LOGINFO(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOGERR(fmt, args...)     { syslog(LOG_CRIT, fmt, ## args); fprintf(stderr, fmt, ## args); }

int
main(int argc, char** argv)
{
  media_stream *     stream;
  media_frame *      frame;
  struct timeval     tv_start, tv_end;
  int                msec;
  int                i          = 1;
  int                numframes  = 100;
  unsigned long long totalbytes = 0;

  if (argc < 3) {
#if defined __i386
    /* The host version */
    fprintf(stderr,
            "Usage: %s media_type media_props\n", argv[0]);
    fprintf(stderr,
            "Example: %s \"%s\"      \"resolution=2CIF&fps=15\"\n",
            argv[0], IMAGE_JPEG);
    fprintf(stderr,
            "Example: %s \"%s\" \"resolution=352x288&sdk_format=Y800&fps=15\"\n",
            argv[0], IMAGE_UNCOMPRESSED);
    fprintf(stderr,
            "Example: %s \"%s\" \"capture-cameraIP=192.168.0.90&capture-userpass=user:pass&sdk_format=Y800&resolution=2CIF&fps=15\"\n",
            argv[0], IMAGE_UNCOMPRESSED);
#else
    fprintf(stderr, "Usage: %s media_type media_props [numframes]\n",
            argv[0]);
    fprintf(stderr, "Example: %s %s \"resolution=352x288&fps=15\" 100\n",
            argv[0], IMAGE_JPEG);
    fprintf(stderr,
            "Example: %s %s \"resolution=160x120&sdk_format=Y800&fps=15\" 100\n",
            argv[0], IMAGE_UNCOMPRESSED);
#endif

    return 1;
  }
  /* is numframes specified ? */
  if (argc >= 4) {
    numframes = atoi(argv[3]);

    /* Need at least 2 frames for the achived fps calculation */
    if (numframes < 2) {
      numframes = 2;
    }
  }

  openlog("vidcap", LOG_PID | LOG_CONS, LOG_USER);

  stream = capture_open_stream(argv[1], argv[2]);
  if (stream == NULL) {
    LOGERR("Failed to open stream\n");
    closelog();
    return EXIT_FAILURE;
  }

  gettimeofday(&tv_start, NULL);

  /* print intital information */
  frame = capture_get_frame(stream);

  LOGINFO("Getting %d frames. resolution: %dx%d framesize: %d\n",
          numframes,
          capture_frame_width(frame),
          capture_frame_height(frame),
          capture_frame_size(frame));

  capture_frame_free(frame);

  while (i < numframes) {
    frame = capture_get_frame(stream);
    if (!frame) {
      /* nothing to read, this is serious  */
      fprintf(stderr, "Failed to read frame!");
      syslog(LOG_CRIT, "Failed to read frame!\n");

      return 1;
    } else {
      D(capture_time ts = capture_frame_timestamp(frame));
      D(LOGINFO("timestamp: %" CAPTURE_TIME_FORMAT "\n", ts));

      totalbytes += capture_frame_size(frame);
      capture_frame_free(frame);
      i++;
    }
  }
  gettimeofday(&tv_end, NULL);

  /* calculate fps */
  msec  = tv_end.tv_sec * 1000 + tv_end.tv_usec / 1000;
  msec -= tv_start.tv_sec * 1000 + tv_start.tv_usec / 1000;

  LOGINFO("Fetched %d images in %d milliseconds, fps:%0.3f MBytes/sec:%0.3f\n",
          i,
          msec,
          (float)(i / (msec / 1000.0f)),
          (float)(totalbytes / (msec * 1000.0f)));

  capture_close_stream(stream);
  closelog();

  return EXIT_SUCCESS;
}
