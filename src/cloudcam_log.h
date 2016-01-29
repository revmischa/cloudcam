#ifndef _CLOUDCAM_LOG_H
#define _CLOUDCAM_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>


// this is ACAP-style logging
#define AXIS_LOGDEBUG(fmt, args...)    { syslog(LOG_DEBUG, fmt, ## args); printf(fmt, ## args); }
#define AXIS_LOGWARN(fmt, args...)     { syslog(LOG_WARNING, fmt, ## args); fprintf(stderr, fmt, ## args); }
#define AXIS_LOGINFO(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define AXIS_LOGERR(fmt, args...)     { syslog(LOG_CRIT, fmt, ## args); fprintf(stderr, fmt, ## args); }

#ifdef IOT_DEBUG
#define DEBUG(...)    \
    {\
    AXIS_LOGDEBUG("DEBUG:   %s L#%d ", __PRETTY_FUNCTION__, __LINE__);  \
    AXIS_LOGDEBUG(__VA_ARGS__); \
    AXIS_LOGDEBUG("\n"); \
    }
#else
#define DEBUG(...)
#endif

#ifdef IOT_INFO
#define INFO(...)    \
    {\
    AXIS_LOGINFO(__VA_ARGS__); \
    AXIS_LOGINFO("\n"); \
    }
#else
#define INFO(...)
#endif

#ifdef IOT_WARN
#define WARN(...)   \
    { \
    AXIS_LOGWARN("WARN:  %s L#%d ", __PRETTY_FUNCTION__, __LINE__);  \
    AXIS_LOGWARN(__VA_ARGS__); \
    AXIS_LOGWARN("\n"); \
    }
#else
#define WARN(...)
#endif

#ifdef IOT_ERROR
#define ERROR(...)  \
    { \
    AXIS_LOGERR("ERROR: %s L#%d ", __PRETTY_FUNCTION__, __LINE__); \
    AXIS_LOGERR(__VA_ARGS__); \
    AXIS_LOGERR("\n"); \
    }
#else
#define ERROR(...)
#endif

#endif // _CLOUDCAM_LOG_H
