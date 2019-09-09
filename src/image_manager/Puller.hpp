/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
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
    web::json::value getManifest();
    std::string getManifestPath();
    PulledImage pull();

private:    
    web::json::value getManifest(const std::string &token);
    std::string getParam(std::string &header, const std::string& param);
    std::string getUri(const std::string &server);
    void saveImage(web::json::value fsLayers);
    void saveLayer(const std::string &digest);
    void downloadStream(const std::string &uri, const std::string &path, const boost::filesystem::path &filename);
    std::string requestAuthToken();
    bool checkSum(const std::string &digest, const boost::filesystem::path &filename);
    void printLog(  const boost::format &message, common::LogLevel LogLevel,
                    std::ostream& outStream = std::cout, std::ostream& errStream = std::cerr);
    std::unique_ptr<web::http::client::http_client> setupHttpClientWithCredential(const std::string& server);

private:
    std::shared_ptr<const common::Config> config;

    /** system name for logger */
    const std::string sysname = "Puller";

    /** digest when layer tarfile is empty */
    const std::string EMPTY_TAR_SHA256 = "sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4";

    /** max download retry */
    const int RETRY_MAX = 3;

    /** image manifest */
    web::json::value manifest;

    /** authorization token */
    std::string token;

};

}
}

#endif
