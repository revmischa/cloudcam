#include "cloudcam/s3.h"
#include "cloudcam/cloudcam.h"

// get a fresh "easy" handle for performing CURL operations.
// TODO: reuse handle per-thread
CURL *cloudcam_get_curl_easy_handle(cloudcam_ctx *ctx) {
    return curl_easy_init();
}

typedef struct {
    FILE *fp;
} cloudcam_curl_userdata;

void cloudcam_upload_file_to_s3_presigned(cloudcam_ctx *ctx, FILE *fp, char *url) {
    DEBUG("Uploading file to presigned S3 URL: %s", url);

    CURL *curleh = cloudcam_get_curl_easy_handle(ctx);

    // callback userdata
    cloudcam_curl_userdata ud = {
        .fp = fp
    };
    // set callback to handle data received from transfer
    curl_easy_setopt(curleh, CURLOPT_WRITEDATA, &ud);
    curl_easy_setopt(curleh, CURLOPT_WRITEFUNCTION, _cloudcam_curl_s3_upload_write);

    // set callback for providing data to buffer
    curl_easy_setopt(easyhandle, CURLOPT_READDATA, &ud);
    curl_easy_setopt(easyhandle, CURLOPT_READFUNCTION, _cloudcam_curl_s3_upload_read);


    // doing upload yes
    curl_easy_setopt(easyhandle, CURLOPT_UPLOAD, 1L);


}


// callback function to receive data during/after transfer
size_t _cloudcam_curl_s3_upload_write(void *buffer, size_t size, size_t nmemb, cloudcam_curl_userdata *ud) {
    FILE *fp = ud->fp;
    assert(fp);

    size_t provided = 0;
    provided = fread(bufptr, size, nitems, fp);
    return provided;
}



// callback function to fill in upload buffer with our file to transfer
size_t _cloudcam_curl_s3_upload_read(char *bufptr, size_t size, size_t nitems, cloudcam_curl_userdata *ud) {
    FILE *fp = ud->fp;
    assert(fp);

    size_t provided = 0;
    provided = fread(bufptr, size, nitems, fp);
    return provided;
}

