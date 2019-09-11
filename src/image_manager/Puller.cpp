/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/**
 * Pulling container image from registry service using CPPRESTSDK
 */

#include "image_manager/Puller.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <future>
#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdio>
#include <memory>
#include <array>
#include <chrono>

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "image_manager/ImageManager.hpp"

using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace concurrency::streams;       // Asynchronous streams

namespace sarus {
namespace image_manager {

    Puller::Puller(std::shared_ptr<const common::Config> config)
        : config{std::move(config)}
    {}

    /**
     * Pull the container image layer tarfile using configurations (config)
     */
    PulledImage Puller::pull() {
        printLog(boost::format("Pulling image"), common::LogLevel::INFO);

        /** main function  */
        std::chrono::system_clock::time_point start, end;
        double elapsed;

        // output params
        printLog( boost::format("# image            : %s") % config->imageID, common::LogLevel::GENERAL);
        printLog( boost::format("# cache directory  : %s") % config->directories.cache, common::LogLevel::GENERAL);
        printLog( boost::format("# temp directory   : %s") % config->directories.temp, common::LogLevel::GENERAL);
        printLog( boost::format("# images directory : %s") % config->directories.images, common::LogLevel::GENERAL);

        auto manifest = retrieveImageManifest();

        if (!manifest.has_field(U("fsLayers"))) {
            SARUS_THROW_ERROR("manifest does not have \"fsLayers\" field.");
        }
        web::json::value fsLayers = manifest[U("fsLayers")];

        start = std::chrono::system_clock::now();

        saveImage(fsLayers);

        end = std::chrono::system_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);

        printLog(boost::format("Elapsed time on pulling    : %s [sec]") % elapsed, common::LogLevel::INFO);
        printLog(boost::format("Successfully pulled image"), common::LogLevel::INFO);

        return PulledImage{config, manifest};
    }

    /**
     * Save the container image using parallel threads
     * 
     * @param fsLaters      The list of digests of container images (manifest[fslayers])
     */
    void Puller::saveImage(json::value fsLayers)
    {
        printLog( boost::format("> save image layers ..."), common::LogLevel::GENERAL);
        printLog( boost::format("Create download threads."), common::LogLevel::DEBUG);
    
        common::createFoldersIfNecessary(config->directories.cache);
    
        std::vector<std::future<void>> results;
        for(size_t idx = 0; idx < fsLayers.size(); ++idx)
        {
            std::string digest = fsLayers[idx]["blobSum"].serialize();
            digest = common::eraseFirstAndLastDoubleQuote(digest);
    
            // EMPTY_TAR_SHA256 has no members, can be skipped.
            if(digest == EMPTY_TAR_SHA256 ) {
                continue;
            }
            // launch download thread
            results.push_back(
                std::async(std::launch::async, &Puller::saveLayer, this, digest)
            );
        }

        try {
            // check that all download threads exited normally (without throwing exceptions)
            for (auto &result : results){ 
                result.get();
            }
        }
        catch(common::Error& e) {
            SARUS_RETHROW_ERROR(e, "Failed to download image. An error occurred in one of the download threads.");
        }

        printLog(boost::format("Successfully downloaded image."), common::LogLevel::DEBUG);
    }

    /**
     * Download the layer tarfile (handle error response, retry)
     * 
     * @param digest        The digest of the target download layer
     */
    void Puller::saveLayer(const std::string &digest)
    {
        printLog( boost::format("Download the layer: %s") % digest, common::LogLevel::DEBUG);

        auto layerFile = config->directories.cache / boost::filesystem::path(digest + ".tar");
        auto layerFileTemp = common::makeUniquePathWithRandomSuffix(layerFile);

        // check if layer is already in cache
        if(boost::filesystem::exists(layerFile)) {
            printLog( boost::format("> %-15.15s: %s") % "found in cache" % digest, common::LogLevel::GENERAL);
            return;
        }
    
        web::http::client::http_client       client( getServerUri(config->imageID.server) );
        web::http::http_request              request(methods::GET);
        web::http::http_response             response;

        // make a copy of the authorization token to avoid data races because
        // multiple instances of this function could be running in parallel
        std::string authorizationToken = this->authorizationToken;

        for(int retry = 0; retry < MAX_DOWNLOAD_RETRIES; ++retry) {
            if ( retry > 0 ) {
                printLog( boost::format("> %-15.15s: %s") % "retry" % digest, common::LogLevel::GENERAL);
            }
            
            std::string path = (boost::format("v2/%s/%s/blobs/%s")
                                % config->imageID.repositoryNamespace
                                % config->imageID.image
                                % digest).str();
            request.set_request_uri(path);

            request.headers().clear();
            std::string header = (boost::format("Bearer %s") % authorizationToken).str();
            request.headers().add(header_names::authorization, U(header) );

            printLog( boost::format("httpclient: uri=%s, path=%s, header=%25.25s..., digest=%25.25s...")
                        % getServerUri(config->imageID.server) % path % header % digest, common::LogLevel::DEBUG);

            response = client.request(request).get();
            printLog( boost::format("Received http_response status code (%s): %s, digest=%s")
                % response.status_code() % response.reason_phrase() % digest, common::LogLevel::DEBUG);

            if ( response.status_code() == status_codes::OK ) {
                break;
            }
            // when unauthorized response arrives, request new token
            else if (response.status_code() == 401) {
                printLog( boost::format("> %-15.15s: %s") % "tokenExpired" % digest, common::LogLevel::GENERAL);

                try {
                    authorizationToken = requestAuthorizationToken();
                } catch (const std::exception &e) {
                    printLog( boost::format("Failed to get authorized token."), common::LogLevel::ERROR);
                }
                continue;
            }
            // handle redirect to download layer
            else if (response.status_code() > 300 && response.status_code() < 309) {
                // parse redirected location
                std::string location = response.headers()[U("Location")];
                boost::cmatch matches;
                boost::regex re("(https?)://(.*?)(/.*)");
                if (!boost::regex_match(location.c_str(), matches, re)) {
                    auto message = boost::format("Failed to parse image location: %s") % location;
                    SARUS_THROW_ERROR(message.str());
                }
                std::string downloadUri = matches[1].str() + "://" + matches[2].str();
                path = matches[3];
    
                printLog( boost::format("> %-15.15s: %s") % "pulling" % digest, common::LogLevel::GENERAL);

                try {
                    downloadStream(downloadUri, path, layerFileTemp);
                }
                catch(common::Error& e) {
                    printLog( boost::format("> %-15.15s: %s") % "failed" % digest, common::LogLevel::GENERAL);
                    common::Logger::getInstance().logErrorTrace(e, sysname);
                    continue; // retry download
                }

                if(!checkSum(digest, layerFileTemp)) {
                    printLog( boost::format("> %-15.15s: %s") % "bad checksum" % digest, common::LogLevel::GENERAL);
                    boost::filesystem::remove(layerFileTemp);
                    continue; // retry download
                }
                // if checksum succeeded, finish download process
                boost::filesystem::rename(layerFileTemp, layerFile); // atomically create/replace layer file
                printLog( boost::format("> %-15.15s: %s") % "completed" % digest, common::LogLevel::GENERAL);
                printLog( boost::format("Success to download : %s") % digest, common::LogLevel::DEBUG);
                return;
            }
            // other http response means irregal status
            else {
                printLog( boost::format("Unexpected http_response (%s): %s, digest=%s")
                    % response.status_code() % response.reason_phrase() % digest, common::LogLevel::ERROR);
                continue;
            }
        }
        auto message = boost::format("Failed to download image layer %s. Exceeded max number of retries (%s).")
            % digest % MAX_DOWNLOAD_RETRIES;
        SARUS_THROW_ERROR(message.str());
    }
    
    /**
     * Download the http response body
     * 
     * @param uri       The base uri location of HTTP client
     * @param path          The request uri of the download stream
     * @param filename      The filename to save
     */
    void Puller::downloadStream(const std::string &uri, const std::string &path, const boost::filesystem::path &filename)
    {
        printLog( boost::format("Start downloadStream: uri=%s, path=%s, filename=%s") % uri % path % filename, common::LogLevel::DEBUG);
        auto fileStream = std::make_shared<ostream>();
        
        // Open stream
        pplx::task<void> requestTask = fstream::open_ostream(U(filename.string())).then([=](ostream outFile) {
            *fileStream = outFile;
            web::http::client::http_client client( uri );
            web::http::http_request        request(methods::GET);

            request.set_request_uri(path);
            return client.request(request);
        })
        // handle response
        .then([=](http_response response) {
            if ( response.status_code() != status_codes::OK ) {
                auto message = boost::format("Received http_response status code (%s): %s, uri=%s, path=%s, filename=%s")
                    % response.status_code() % response.reason_phrase() % uri % path % filename;
                SARUS_THROW_ERROR(message.str());
            }
            return response.body().read_to_end(fileStream->streambuf());
        })
        // close stream
        .then([=](size_t) {
            return fileStream->close();
        });

        try {
            requestTask.wait();
        }
        catch (const std::exception &e) {
            boost::filesystem::remove(filename);
            SARUS_RETHROW_ERROR(e, "Download stream error");
        }

        printLog( boost::format("Finished download Stream: uri=%s, path=%s, filename=%s") % uri % path % filename, common::LogLevel::DEBUG);
    }

    /**
     * Retrieve image manifest from server
     */
    web::json::value Puller::retrieveImageManifest() {
        printLog(boost::format("Retrieving image manifest from %s") % config->imageID.server,
                 common::LogLevel::INFO);

        // get authorization token
        try {
            authorizationToken = requestAuthorizationToken();
        }
        catch(common::Error& e) {
            auto message = boost::format{"Failed authentication for image '%s'"
                    "\nDid you perform a login with the proper credentials?"
                    "\nSee 'sarus help pull' (--login option)"}
                    % config->imageID;
            printLog(message, common::LogLevel::GENERAL, std::cerr);
            SARUS_RETHROW_ERROR(e, message.str(), common::LogLevel::INFO);
        }

        web::http::client::http_client      client( getServerUri(config->imageID.server) );
        web::http::http_request             request(methods::GET);
        web::http::http_response            response;
        
        request.set_request_uri( makeImageManifestUri() );
        std::string header = (boost::format("Bearer %s") % authorizationToken).str();
        request.headers().add(header_names::authorization, U(header) );

        printLog( boost::format("server      : %s") % getServerUri(config->imageID.server), common::LogLevel::DEBUG);
        printLog( boost::format("request_uri : %s") % makeImageManifestUri(), common::LogLevel::DEBUG);
        printLog( boost::format("header      : %s") % U(header), common::LogLevel::DEBUG);
        
        response = client.request(request).get();

        if(response.status_code() != status_codes::OK) {
            auto message = boost::format{"Failed to pull image '%s'\nIs the image ID correct?"} % config->imageID;
            printLog(message, common::LogLevel::GENERAL, std::cerr);

            message = boost::format{"Failed to pull manifest. Received http_response status code(%s): %s"}
                % response.status_code() % response.reason_phrase();
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
        
        auto manifest = response.extract_json(true).get();

        // check manifest
        if ( manifest.has_field(U("errors")) ) {
            auto message = boost::format(   "Failed to get manifest. Possible reasons: bad image ID specified"
                                            " or access to repository denied (try with --login)."
                                            " Downloaded manifest has 'errors' field: %s") % manifest.serialize();
            SARUS_THROW_ERROR(message.str());
        }

        printLog(boost::format("Retrieved image manifest:\n%s") % manifest.serialize(),
                 common::LogLevel::DEBUG);

        printLog(boost::format("Successfully retrieved image manifest"), common::LogLevel::INFO);

        return manifest;
    }

    std::string Puller::makeImageManifestUri() {
        return (boost::format("v2/%s/%s/manifests/%s")
                % config->imageID.repositoryNamespace
                % config->imageID.image
                % config->imageID.tag).str();
    }

    /**
     * Test the checksum of file content and digest
     */
    bool Puller::checkSum(const std::string &digest, const boost::filesystem::path &filename)
    {
        /** check target filename sum and return result */
        printLog( boost::format("checksum: %s") % filename, common::LogLevel::DEBUG);
        printLog( boost::format("checksum: digest=%s, filename=%s") % digest % filename, common::LogLevel::DEBUG);

        boost::cmatch matches;
        boost::regex re("(.*?):(.*?)");
        if (!boost::regex_match(digest.c_str(), matches, re)) {
            printLog( boost::format("Failed to parse filename: %s") % digest, common::LogLevel::ERROR);
            return false;
        }
        std::string hash_type = matches[1].str();
        std::string value     = matches[2].str();
        std::string command   = hash_type + "sum";

        // checksum
        std::string result = common::executeCommand(command + " " + filename.string());
        std::string checksum = result.substr(0, result.find(" "));
    
        if(checksum != value) {
            printLog( boost::format("Failed to test the checksum of layer %s") % filename, common::LogLevel::ERROR);
            printLog( boost::format("expected checksum=%s, actually computed checksum=%s") % value % checksum, common::LogLevel::DEBUG);
            return false;
        }
        printLog( boost::format("successfully verified checksum of layer %s") % filename, common::LogLevel::DEBUG);
        return true;
    }

    /**
     * Get params from the HTTP response header(Www-Authenticate)
     */
    std::string Puller::getParam(std::string &header, const std::string& param)
    {
        int i = header.find(param + "=") + param.size() + 2;
        int j = header.substr(i, header.size()).find('\"');
        return header.substr(i, j);
    }

    std::string Puller::requestAuthorizationToken() {
        printLog(boost::format("Getting new authorization token from %s") % config->imageID.server,
                 common::LogLevel::DEBUG);

        // get unauthorized header
        std::unique_ptr<web::http::client::http_client> client = setupHttpClientWithCredential(getServerUri(config->imageID.server));
        web::http::http_request              request(methods::GET);
        pplx::task<web::http::http_response> responseTask;
        web::http::http_response             response;

        request.set_request_uri( makeImageManifestUri() );
        responseTask = client->request(request);
        
        try {
            responseTask.wait();
            response = responseTask.get();
        }
        catch (const std::exception& e) {
            SARUS_RETHROW_ERROR(e, "Failed to get token");
        }

        if(response.status_code() != 401) {
            auto message = boost::format("Received http_response status code(%s): %s") 
                % response.status_code() %  response.reason_phrase();
            SARUS_THROW_ERROR(message.str());
        }

        // parse header
        auto auth_header = response.headers()[U("Www-Authenticate")];
        auto realm   = getParam(auth_header, "realm");
        auto service = getParam(auth_header, "service");
        auto scope   = getParam(auth_header, "scope");

        printLog( boost::format("real   : %s") % realm, common::LogLevel::DEBUG);
        printLog( boost::format("service: %s") % service, common::LogLevel::DEBUG);
        printLog( boost::format("scope  : %s") % scope, common::LogLevel::DEBUG);

        // get authorized token
        std::unique_ptr<web::http::client::http_client> tokenClient = setupHttpClientWithCredential( realm );
        web::http::http_request tokenReq(methods::GET);
        web::uri_builder tokenUriBuilder("");
        web::http::http_response tokenResp;
        std::string token;
    
        tokenUriBuilder.append_query(U("scope"), scope);
        tokenUriBuilder.append_query(U("service"), service);
        tokenReq.set_request_uri(tokenUriBuilder.to_string());

        tokenResp = tokenClient->request(tokenReq).get();
        if(tokenResp.status_code() != status_codes::OK) {
            auto message = boost::format("Failed to get token. Received http_response status code(%s): %s")
                % tokenResp.status_code() %  tokenResp.reason_phrase();
            SARUS_THROW_ERROR(message.str());
        }

        try {
            json::value respJson = tokenResp.extract_json().get();
            token = respJson[U("token")].serialize();
            token = common::eraseFirstAndLastDoubleQuote(token);
        }
        catch (const std::exception& e) {
            SARUS_RETHROW_ERROR(e, "Failed to get Token: %s");
        }

        printLog( boost::format("Got token: %s") % token, common::LogLevel::DEBUG);
        printLog( boost::format("Successfully got new authorization token"), common::LogLevel::DEBUG);

        return token;
    }

    std::unique_ptr<web::http::client::http_client> Puller::setupHttpClientWithCredential(const std::string& server) {
        // if repository is private, add credential configrations
        if(config->authentication.isAuthenticationNeeded) {
            web::http::client::http_client_config clientConfig;
            web::credentials creds( U(config->authentication.username), U(config->authentication.password) );
            clientConfig.set_credentials(creds);
            return std::unique_ptr<web::http::client::http_client>( new web::http::client::http_client( server, clientConfig ));
        }

        return std::unique_ptr<web::http::client::http_client>( new web::http::client::http_client( server ));
    }

    std::string Puller::getServerUri(const std::string &server) {
        return "https://" + server;
    }

    void Puller::printLog(  const boost::format &message, common::LogLevel LogLevel,
                            std::ostream& outStream, std::ostream& errStream)
    {
        common::Logger::getInstance().log(message.str(), sysname, LogLevel, outStream, errStream);
    }
    
} // namespace
} // namespace
