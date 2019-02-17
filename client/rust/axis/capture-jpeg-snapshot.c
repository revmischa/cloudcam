#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/prctl.h>

#include <capture.h>

// capture-jpeg-snapshot: captures a jpeg snapshot from an Axis camera via libcapture.so API and writes it to stdout
// this is necessary since the main Rust executable is static and can't currently link to libcapture.so

int main(int argc, char** argv)
{
  // request SIGTERM to be delivered if parent process exits
  prctl(PR_SET_PDEATHSIG, SIGTERM);
  // capture one jpeg frame and write it to stdout
  const char *media_props = argc > 1 ? argv[1] : "";
  media_stream *stream = capture_open_stream(IMAGE_JPEG, strlen(media_props) > 0 ? media_props : "?");
  if (stream == NULL) {
    return EXIT_FAILURE;
  }
  media_frame *frame = capture_get_frame(stream);
  fwrite(capture_frame_data(frame), 1, capture_frame_size(frame), stdout);
  // clean up
  capture_frame_free(frame);
  capture_close_stream(stream);
  return EXIT_SUCCESS;
}
