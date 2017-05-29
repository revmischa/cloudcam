#include "cloudcam/s3.h"

// get a fresh "easy" handle for performing CURL operations.
// TODO: reuse handle per-thread
CURL *cloudcam_get_curl_easy_handle(cloudcam_ctx *ctx) {
    return curl_easy_init();
}

long fsize(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return size;
}

void cloudcam_upload_file_to_s3_presigned(cloudcam_ctx *ctx, FILE *fp, char *url) {
    DEBUG("Uploading file to presigned S3 URL: %s", url);

    CURL *curleh = cloudcam_get_curl_easy_handle(ctx);

    curl_easy_setopt(curleh, CURLOPT_PUT, 1);
    curl_easy_setopt(curleh, CURLOPT_URL, url);
    curl_easy_setopt(curleh, CURLOPT_UPLOAD, 1);

    // callback userdata
    cloudcam_s3_curl_userdata ud = {
        .fp = fp,
        .ctx = ctx
    };

    // set callback for providing data to buffer
    curl_easy_setopt(curleh, CURLOPT_READDATA, &ud);
    curl_easy_setopt(curleh, CURLOPT_READFUNCTION, _cloudcam_curl_s3_upload_read);

    curl_easy_setopt(curleh, CURLOPT_INFILESIZE_LARGE, fsize(fp));

    // now do multipart POST...
    CURLcode res = curl_easy_perform(curleh);

    curl_easy_cleanup(curleh);

    if (res != CURLE_OK) {
        ERROR("S3 file upload failed: %s", curl_easy_strerror(res));
    }
}

// callback function to fill in upload buffer with our file to transfer
size_t _cloudcam_curl_s3_upload_read(char *bufptr, size_t size, size_t nitems, cloudcam_s3_curl_userdata *ud) {
    FILE *fp = ud->fp;
    assert(fp);

    size_t provided = 0;
    provided = fread(bufptr, size, nitems, fp);
    return provided;
}

