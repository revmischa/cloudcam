#include <curl/curl.h>
#include "cloudcam/cloudcam.h"
#include "cloudcam/log.h"

void cloudcam_upload_file_to_s3_presigned(cloudcam_ctx *ctx, FILE *fp, char *url);
