#include "cloudcam/s3.h"

// get a fresh "easy" handle for performing CURL operations.
// TODO: reuse handle per-thread
CURL *cloudcam_get_curl_easy_handle(cloudcam_ctx *ctx) {
  return curl_easy_init();
}

unsigned long fsize(FILE *fp) {
  fseek(fp, 0, SEEK_END);
  unsigned long size = (unsigned long)ftell(fp);
  fseek(fp, 0, SEEK_SET);
  return size;
}

// callback function to fill in upload buffer with our file to transfer
size_t curl_file_read_cb(char *bufptr, size_t size, size_t nitems, cloudcam_s3_curl_userdata *ud) {
  assert(bufptr != NULL);
  assert(ud != NULL);
  assert(ud->fp);
  size_t provided = 0;
  provided = fread(bufptr, size, nitems, ud->fp);
  return provided;
}

IoT_Error_t cloudcam_upload_file_to_s3_presigned(cloudcam_ctx *ctx, FILE *fp, const char *url) {
  assert(ctx != NULL);
  assert(fp != NULL);
  assert(url != NULL);
  assert(strlen(url) > 0);

  DEBUG("Uploading file to presigned S3 URL: %s", url);

  CURL *curl = cloudcam_get_curl_easy_handle(ctx);
  assert(curl != NULL);

  curl_easy_setopt(curl, CURLOPT_PUT, 1);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);

  // callback userdata
  cloudcam_s3_curl_userdata ud = {
    .fp = fp,
    .ctx = ctx
  };

  // set callback for providing data to buffer
  curl_easy_setopt(curl, CURLOPT_READDATA, &ud);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_file_read_cb);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, fsize(fp));

  CURLcode rc = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (rc != CURLE_OK) {
    ERROR("S3 file upload failed: %s", curl_easy_strerror(rc));
    return FAILURE;
  }

  return SUCCESS;
}

static size_t min(size_t a, size_t b) {
  return a < b ? a : b;
}

// callback function to fill in upload buffer with our buffer to transfer
size_t curl_buffer_read_cb(char *bufptr, size_t size, size_t nitems, cloudcam_s3_curl_userdata *ud) {
  assert(bufptr != NULL);
  assert(ud != NULL);
  assert(ud->data != NULL);

  size_t n = min(ud->length - ud->read_offset, size * nitems);
  memcpy(bufptr, (const unsigned char*)ud->data + ud->read_offset, n);

  ud->read_offset += n;

  return n;
}

IoT_Error_t cloudcam_upload_buffer_to_s3_presigned(cloudcam_ctx *ctx, const void *data, size_t length, const char *url) {
  assert(ctx != NULL);
  assert(data != NULL);
  assert(url != NULL);
  assert(strlen(url) > 0);

  DEBUG("Uploading buffer to presigned S3 URL: %s", url);

  CURL *curl = cloudcam_get_curl_easy_handle(ctx);
  assert(curl != NULL);

  curl_easy_setopt(curl, CURLOPT_PUT, 1);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);

  // callback userdata
  cloudcam_s3_curl_userdata ud = {
    .data = data,
    .length = length,
    .read_offset = 0,
    .ctx = ctx
  };

  // set callback for providing data to buffer
  curl_easy_setopt(curl, CURLOPT_READDATA, &ud);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_buffer_read_cb);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, length);

  CURLcode rc = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (rc != CURLE_OK) {
    ERROR("S3 buffer upload failed: %s", curl_easy_strerror(rc));
    return FAILURE;
  }

  return SUCCESS;
}

