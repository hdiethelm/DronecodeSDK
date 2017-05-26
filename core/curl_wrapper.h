#pragma once

#include <string>
#include <memory>
#include "curl_include.h"

#ifdef TESTING
#include <gmock/gmock.h>
using namespace testing;
#endif // TESTING

namespace dronelink {

enum class Status {
    Idle = 0,
    Downloading = 1,
    Uploading = 2,
    Finished = 3,
    Error = 4
};

typedef std::function<int(int progress, Status status, CURLcode curl_code)> progress_callback_t;

struct dl_up_progress {
    int progress_in_percentage = 0.0;
    progress_callback_t progress_callback;
};

class ICurlWrapper
{
public:
    virtual bool download_text(const std::string &url, std::string &content) = 0;
    virtual bool download_file_to_path(const std::string &url, const std::string &path,
                                       const progress_callback_t &progress_callback) = 0;
    virtual bool upload_file(const std::string &url, const std::string &path, const
                             progress_callback_t &progress_callback) = 0;

    virtual ~ICurlWrapper() {}
};

class CurlWrapper : public ICurlWrapper
{
public:
    CurlWrapper();
    ~CurlWrapper();

    // ICurlWrapper
    bool download_text(const std::string &url, std::string &content) override;
    bool download_file_to_path(const std::string &url, const std::string &path,
                               const progress_callback_t &progress_callback) override;
    bool upload_file(const std::string &url, const std::string &path, const
                     progress_callback_t &progress_callback) override;

private:
    std::shared_ptr<CURL> curl;
};

#ifdef TESTING
class CurlWrapperMock : public ICurlWrapper
{
public:
    MOCK_METHOD2(download_text, bool(const std::string &url, std::string &content));
    MOCK_METHOD3(download_file_to_path, bool(const std::string &url, const std::string &path,
                                             const progress_callback_t &progress_callback));
    MOCK_METHOD3(upload_file, bool(const std::string &url, const std::string &path, const
                                   progress_callback_t &progress_callback));
};
#endif // TESTING

} // namespace dronelink
