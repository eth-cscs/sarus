/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _ImageManager_Puller_hpp
#define _ImageManager_Puller_hpp

#include <memory>
#include <vector>
#include <string>

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "common/Config.hpp"
#include "common/Utility.hpp"
#include "common/Logger.hpp"
#include "image_manager/PulledImage.hpp"


namespace sarus {
namespace image_manager {

class Puller {
public:
    Puller(std::shared_ptr<const common::Config> config);
    PulledImage pull();
    web::json::value retrieveImageManifest();

private:    
    std::string makeImageManifestUri();
    std::string getParam(const std::string &header, const std::string& param);
    std::string getServerUri(const std::string &server);
    void saveImage(web::json::value fsLayers);
    void saveLayer(const std::string &digest);
    void downloadStream(const web::http::http_response &response, const boost::filesystem::path &filename);
    std::string requestAuthorizationToken(web::http::http_response& response);
    bool checkSum(const std::string &digest, const boost::filesystem::path &filename);
    void printLog(  const boost::format &message, common::LogLevel LogLevel,
                    std::ostream& outStream = std::cout, std::ostream& errStream = std::cerr);
    std::unique_ptr<web::http::client::http_client> setupHttpClientWithCredential(const std::string& server);
    std::tuple<std::string, std::string, std::string> parseWwwAuthenticateHeader(const std::string& auth_header);

private:
    std::shared_ptr<const common::Config> config;

    /** system name for logger */
    const std::string sysname = "Puller";

    /** digest when layer tarfile is empty */
    const std::string EMPTY_TAR_SHA256 = "sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4";

    const int MAX_DOWNLOAD_RETRIES = 3;
    std::string authorizationToken;
};

}
}

#endif
