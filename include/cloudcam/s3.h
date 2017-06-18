#ifndef CLOUDCAM_S3_H
#define CLOUDCAM_S3_H value

#include <assert.h>
#include <curl/curl.h>
#include "cloudcam/cloudcam.h"
#include "cloudcam/log.h"

// state passed to callback handlers when doing long-runningcurl requests
typedef struct {
    FILE *fp;  // file we're uploading (if any)
    cloudcam_ctx *ctx;  // client doing the request
} cloudcam_s3_curl_userdata;

IoT_Error_t cloudcam_upload_file_to_s3_presigned(cloudcam_ctx *ctx, FILE *fp, const char *url);

#endif
