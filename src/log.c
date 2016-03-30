#ifdef LINUX

#define _GNU_SOURCE   // for cookie_io_functions_t
#include "cloudcam/log.h"

static char const *priov[] = {
  "EMERG:",   "ALERT:",  "CRIT:", "ERR:", "WARNING:", "NOTICE:", "INFO:", "DEBUG:"
};

static FILE *old_outfh = NULL;

static size_t writer(void *cookie, char const *data, size_t leng) {
  (void)cookie;
  int     p = LOG_DEBUG, len;
  do len = strlen(priov[p]);
  while (memcmp(data, priov[p], len) && --p >= 0);

  if (p < 0) p = LOG_INFO;
  else data += len, leng -= len;
  while (*data == ' ') ++data, --leng;

  syslog(p, "%.*s", (int)leng, data);
  #ifdef CLOUDCAM_LOG_DUPOUT
  if (old_outfh)
    fprintf(old_outfh, "%.*s", (int)leng, data);
  #endif
  return  leng;
}

static int noop(void) {return 0;}
static cookie_io_functions_t log_fns = {
  (void*) noop, (void*) writer, (void*) noop, (void*) noop
};

void tolog(FILE **pfp) {
  old_outfh = *pfp;
  setvbuf(*pfp = fopencookie(NULL, "w", log_fns), NULL, _IOLBF, 0);
}

#else //LINUX

#include "cloudcam/log.h"
void tolog(FILE **pfp) {
// no-op
}

#endif //LINUX

