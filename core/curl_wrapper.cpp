#include "global_include.h"
#include "curl_wrapper.h"
#include <sstream>
#include <iostream>
#include <stdio.h>

namespace dronelink {


CurlWrapper::CurlWrapper() :
    curl(std::shared_ptr<CURL>(curl_easy_init(), curl_easy_cleanup))
{
}

CurlWrapper::~CurlWrapper()
{
}

// converts curl output to string
// taken from https://stackoverflow.com/questions/9786150/save-curl-content-result-into-a-string-in-c
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

bool CurlWrapper::download_text(const std::string &url, std::string &content)
{
    std::string readBuffer;

    if (nullptr != curl) {
        CURLcode res;

        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl.get());
        content = readBuffer;

        if (res == CURLcode::CURLE_OK) {
            return true;
        } else {
            Debug() << "Error while downloading text, curl error code: " << curl_easy_strerror(res);
            return false;
        }
    } else {
        Debug() << "Error: cannot start uploading because of curl initialization error. ";
        return false;
    }
}

static int upload_progress_update(void *p, double dltotal, double dlnow, double ultotal,
                                  double ulnow)
{
    UNUSED(dltotal);
    UNUSED(dlnow);

    struct dl_up_progress *myp = (struct dl_up_progress *)p;

    if (myp->progress_callback == nullptr) {
        return 0;
    }

    if (ultotal == 0 || ulnow == 0) {
        return myp->progress_callback(0, Status::Idle, CURLcode::CURLE_OK);
    }

    int percentage = 100 / ultotal * ulnow;

    if (percentage > myp->progress_in_percentage) {
        myp->progress_in_percentage = percentage;
        return myp->progress_callback(percentage, Status::Uploading, CURLcode::CURLE_OK);
    }

    return 0;
}


bool CurlWrapper::upload_file(const std::string &url, const std::string &path, const
                              progress_callback_t &progress_callback)
{
    CURLcode res;

    if (nullptr != curl) {
        struct dl_up_progress prog;
        prog.progress_callback = progress_callback;

        curl_httppost *post = NULL;
        curl_httppost *last = NULL;

        struct curl_slist *chunk = NULL;

        // avoid sending 'Expect: 100-Continue' header, required by some server implementations
        chunk = curl_slist_append(chunk, "Expect:");

        curl_formadd(&post, &last,
                     CURLFORM_COPYNAME, "file",
                     CURLFORM_FILE, path.c_str(),
                     CURLFORM_END);

        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_PROGRESSFUNCTION, upload_progress_update);
        curl_easy_setopt(curl.get(), CURLOPT_PROGRESSDATA, &prog);
        curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_HTTPPOST, post);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);

        res = curl_easy_perform(curl.get());

        curl_slist_free_all(chunk);
        curl_formfree(post);

        if (res == CURLcode::CURLE_OK) {
            if (nullptr != progress_callback) {
                progress_callback(100, Status::Finished, CURLcode::CURLE_OK);
            }
            return true;
        } else {
            if (nullptr != progress_callback) {
                progress_callback(0, Status::Error, res);
            }
            Debug() << "Error while uploading file, curl error code: " << curl_easy_strerror(res);
            return false;
        }
    } else {
        Debug() << "Error: cannot start uploading because of curl initialization error.";
        return false;
    }
}

static int download_progress_update(void *p, double dltotal, double dlnow, double ultotal,
                                    double ulnow)
{
    UNUSED(ultotal);
    UNUSED(ulnow);

    struct dl_up_progress *myp = (struct dl_up_progress *)p;

    if (myp->progress_callback == nullptr) {
        return 0;
    }

    if (dltotal == 0 || dlnow == 0) {
        return myp->progress_callback(0, Status::Idle, CURLcode::CURLE_OK);
    }

    int percentage = 100 / dltotal * dlnow;

    if (percentage > myp->progress_in_percentage) {
        myp->progress_in_percentage = percentage;
        return myp->progress_callback(percentage, Status::Downloading, CURLcode::CURLE_OK);
    }

    return 0;
}

bool CurlWrapper::download_file_to_path(const std::string &url, const std::string &path, const
                                        progress_callback_t &progress_callback)
{
    FILE *fp;

    if (nullptr != curl) {
        CURLcode res;
        struct dl_up_progress prog;
        prog.progress_callback = progress_callback;

        fp = fopen(path.c_str(), "wb");
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_PROGRESSFUNCTION, download_progress_update);
        curl_easy_setopt(curl.get(), CURLOPT_PROGRESSDATA, &prog);
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        res = curl_easy_perform(curl.get());
        fclose(fp);

        if (res == CURLcode::CURLE_OK) {
            if (nullptr != progress_callback) {
                progress_callback(100, Status::Finished, res);
            }
            return true;
        } else {
            if (nullptr != progress_callback) {
                progress_callback(0, Status::Error, res);
            }
            remove(path.c_str());
            Debug() << "Error while downloading file, curl error code: " << curl_easy_strerror(res);
            return false;
        }
    } else {
        Debug() << "Error: cannot start downloading file because of curl initialization error. ";
        return false;
    }
}

} // namespace dronelink
