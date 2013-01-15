#include "api.h"
#include "log.h"
#include "options.h"

int api_init() {
    curl_global_init(CURL_GLOBAL_ALL);
    req.curl = curl_easy_init();
    curl_easy_setopt(req.curl, CURLOPT_USERAGENT, "Floobits Diffshipper");
    /* TODO: verify cert! */
    curl_easy_setopt(req.curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(req.curl, CURLOPT_SSL_VERIFYHOST, 0L);

    return 0;
}


void api_cleanup() {
    curl_global_cleanup();
}


int api_create_room() {
    char *url;
    long http_status;
    CURLcode res;

    curl_easy_setopt(req.curl, CURLOPT_HTTPPOST, req.p_first);
    curl_easy_setopt(req.curl, CURLOPT_URL, url);

    res = curl_easy_perform(req.curl);

    if (res)
        die("Request failed: %s", curl_easy_strerror(res));

    curl_easy_getinfo(req.curl, CURLINFO_RESPONSE_CODE, &http_status);

    log_debug("Got HTTP status code %li\n", http_status);

    if (http_status <= 199 || http_status > 299) {
      if (http_status == 403) {
          log_err("Access denied. Probably a bad username or API secret.");
      } else if (http_status == 409) {
          log_err("You already have a room with the same name!");
      }
      return -1;
    }

    free(url);
    return 0;
}