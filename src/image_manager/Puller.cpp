/*
 * Sarus
 *
 * Copyright (c) 2018-2022, ETH Zurich. All rights reserved.
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
#include <boost/algorithm/string.hpp>

#include "common/Error.hpp"
#include "common/Logger.hpp"
#include "common/Utility.hpp"
#include "image_manager/ImageManager.hpp"
#include "image_manager/Utility.hpp"

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
        printLog( boost::format("# image            : %s") % config->imageReference, common::LogLevel::GENERAL);
        printLog( boost::format("# cache directory  : %s") % config->directories.cache, common::LogLevel::GENERAL);
        printLog( boost::format("# temp directory   : %s") % config->directories.temp, common::LogLevel::GENERAL);
        printLog( boost::format("# images directory : %s") % config->directories.images, common::LogLevel::GENERAL);

        std::string imageDigest = retrieveImageDigest();
        printLog( boost::format("# image digest : %s") % imageDigest, common::LogLevel::GENERAL);

        auto skopeoPath = config->json["skopeoPath"].GetString();
        auto skopeoArgs = common::CLIArguments{skopeoPath};

        auto skopeoVerbosity = utility::getSkopeoVerbosityOption();
        if (!skopeoVerbosity.empty()) {
            skopeoArgs.push_back(skopeoVerbosity);
        }

        auto ociImagePath = common::makeUniquePathWithRandomSuffix(config->directories.temp / "ociImage");
        printLog( boost::format("Creating temporary OCI image in: %s") % ociImagePath, common::LogLevel::GENERAL);
        skopeoArgs += common::CLIArguments{"copy",
                                           "docker://"+config->imageReference.string(),
                                           "oci:"+ociImagePath.string()+":sarus-pull"};

        start = std::chrono::system_clock::now();
        auto status = common::forkExecWait(skopeoArgs);
        if(status != 0) {
            auto message = boost::format("%s exited with code %d") % skopeoArgs % status;
            printLog(message, common::LogLevel::INFO);
            exit(status);
        }

        end = std::chrono::system_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / double(1000);

        printLog(boost::format("Elapsed time on pulling    : %s [sec]") % elapsed, common::LogLevel::INFO);
        printLog(boost::format("Successfully pulled image"), common::LogLevel::INFO);

        return PulledImage{config, imageDigest, ociImagePath};
    }

    /*
     * Retrieve the image's digest using 'skopeo inspect'
     */
    std::string Puller::retrieveImageDigest() const {
        auto skopeoPath = config->json["skopeoPath"].GetString();
        auto skopeoVerbosity = utility::getSkopeoVerbosityOption();
        auto inspectCommand = boost::format("%s %s inspect docker://%s") % skopeoPath % skopeoVerbosity % config->imageReference;
        auto inspectOutput = std::string();
        try {
            inspectOutput = common::executeCommand(inspectCommand.str());
        }
        catch(common::Error& e) {
            // Confirm skopeo failed because of image non existent or unauthorized access
            auto errorMessage = std::string(e.what());
            auto exitCodeString = std::string{"Process terminated with status 1"};
            auto manifestErrorString = std::string{"Error reading manifest"};

            /**
             * Registries often respond differently to the same incorrect requests, making it very hard to understand
             * whether an image is not present in the registry or is just private.
             * For example, Docker Hub responds both "denied" and "unauthorized", regardless if the image is private or non-existent;
             * Quay.io responds "unauthorized" regardless if the image is private or non-existent.
             * Additionally, the error strings might have different contents depending on the registry.
             */
            auto deniedAccessString = std::string{"denied:"};
            auto unauthorizedAccessString = std::string{"unauthorized:"};

            if(errorMessage.find(exitCodeString) != std::string::npos
               && errorMessage.find(manifestErrorString) != std::string::npos) {
                auto messageHeader = boost::format{"Failed to pull image '%s'"
                                                   "\nError reading manifest from registry."} % config->imageReference;
                printLog(messageHeader, common::LogLevel::GENERAL, std::cerr);

                if(errorMessage.find(unauthorizedAccessString) != std::string::npos
                   || errorMessage.find(deniedAccessString) != std::string::npos) {
                    auto message = boost::format{"%s\nThe image may be private or not present in the remote registry."
                                                 "\nDid you perform a login with the proper credentials?"
                                                 "\nSee 'sarus help pull' (--login option)"} % messageHeader;
                    printLog(message, common::LogLevel::GENERAL, std::cerr);
                }

                SARUS_THROW_ERROR(errorMessage, common::LogLevel::INFO);
            }
            else {
                SARUS_RETHROW_ERROR(e, "Error accessing image in the remote registry.");
            }
        }

        auto registryImageData = common::parseJSON(inspectOutput);
        return registryImageData["Digest"].GetString();
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

        std::unique_ptr<web::http::client::http_client> client = setupHttpClient(getServerUri(config->imageReference.server));
        web::http::http_request  request(methods::GET);
        web::http::http_response response;

        // make a copy of the authorization token to avoid data races because
        // multiple instances of this function could be running in parallel
        std::string authorizationToken = this->authorizationToken;

        printLog( boost::format("> %-15.15s: %s") % "pulling" % digest, common::LogLevel::GENERAL);

        for(int retry = 0; retry < MAX_DOWNLOAD_RETRIES; ++retry) {
            if ( retry > 0 ) {
                printLog( boost::format("> %-15.15s: %s") % "retry" % digest, common::LogLevel::GENERAL);
            }
            
            std::string path = (boost::format("v2/%s/%s/blobs/%s")
                                % config->imageReference.repositoryNamespace
                                % config->imageReference.image
                                % digest).str();
            request.set_request_uri(path);

            request.headers().clear();
            std::string authorizationHeader;
            if (!authorizationToken.empty()) {
                authorizationHeader = (boost::format("Bearer %s") % authorizationToken).str();
                request.headers().add(header_names::authorization, U(authorizationHeader) );
            }

            printLog( boost::format("httpclient: uri=%s, path=%s, auth-header=%25.25s..., digest=%25.25s...")
                        % getServerUri(config->imageReference.server) % path % authorizationHeader % digest, common::LogLevel::DEBUG);

            response = client->request(request).get();
            printLog( boost::format("Received HTTP response status code (%s): %s, digest=%s")
                % response.status_code() % response.reason_phrase() % digest, common::LogLevel::DEBUG);

            // layer content is in the response body
            if ( response.status_code() == status_codes::OK ) {
                printLog( boost::format("Layer %s: blob content is directly available") % digest, common::LogLevel::DEBUG);
                try {
                    downloadStream(response, layerFileTemp);
                }
                catch(common::Error& e) {
                    printLog( boost::format("> %-15.15s: %s") % "failed" % digest, common::LogLevel::GENERAL);
                    common::Logger::getInstance().logErrorTrace(e, sysname);
                    continue; // retry download
                }
            }
            // when unauthorized response arrives, request new token
            else if (response.status_code() == status_codes::Unauthorized || response.status_code() == status_codes::Forbidden) {
                printLog( boost::format("> %-15.15s: %s") % "authorization token expired" % digest, common::LogLevel::GENERAL);

                try {
                    authorizationToken = requestAuthorizationToken(response);
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
                    auto message = boost::format("Failed to parse redirected download location: %s") % location;
                    SARUS_THROW_ERROR(message.str());
                }
                std::string downloadUri = matches[1].str() + "://" + matches[2].str();
                path = matches[3];

                printLog( boost::format("Layer %s: download redirected to %s/%s") % digest % downloadUri % path, common::LogLevel::DEBUG);

                try {
                    std::unique_ptr<web::http::client::http_client> redirectClient = setupHttpClient(downloadUri);
                    web::http::http_request        redirectRequest(methods::GET);
                    web::http::http_response       redirectResponse;

                    redirectRequest.set_request_uri(path);
                    redirectResponse = redirectClient->request(redirectRequest).get();

                    if ( redirectResponse.status_code() != status_codes::OK ) {
                        auto message = boost::format("Received HTTP response status code (%s): %s, uri=%s, path=%s, digest=%s")
                            % response.status_code() % response.reason_phrase() % downloadUri % path % digest;
                        SARUS_THROW_ERROR(message.str());
                    }
                    downloadStream(redirectResponse, layerFileTemp);
                }
                catch(common::Error& e) {
                    printLog( boost::format("> %-15.15s: %s") % "failed" % digest, common::LogLevel::GENERAL);
                    common::Logger::getInstance().logErrorTrace(e, sysname);
                    continue; // retry download
                }
            }
            // other HTTP responses are not supported
            else {
                printLog( boost::format("> %-15.15s: %s") % "failed" % digest, common::LogLevel::GENERAL);
                auto message =  boost::format("Unexpected HTTP response status code (%s): %s, uri=%s, path=%s, digest=%s")
                                    % response.status_code() % response.reason_phrase() % getServerUri(config->imageReference.server)
                                    % path % digest;
                printLog(message, common::LogLevel::INFO);
                continue;
            }

            // if we get here, layer has been downloaded into temporary file: proceed with verifying checksum
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
        auto message = boost::format("Failed to download image layer %s. Exceeded max number of retries (%s).")
            % digest % MAX_DOWNLOAD_RETRIES;
        SARUS_THROW_ERROR(message.str());
    }
    
    /**
     * Download the HTTP response body to a file
     * 
     * @param response      The HTTP response whose body will be saved
     * @param filename      Path to the file where to save the response body
     */
    void Puller::downloadStream(const web::http::http_response &response, const boost::filesystem::path &filename)
    {
        printLog( boost::format("Starting download stream to %s") % filename, common::LogLevel::DEBUG);
        auto fileStream = std::make_shared<ostream>();
        // open stream
        pplx::task<void> downloadTask = fstream::open_ostream(U(filename.string())).then([=](ostream outFile) {
            *fileStream = outFile;
            return;
        })
        // handle response
        .then([=]() {
            return response.body().read_to_end(fileStream->streambuf());
        })
        // close stream
        .then([=](size_t) {
            return fileStream->close();
        });

        try {
            downloadTask.wait();
        }
        catch (const std::exception &e) {
            boost::filesystem::remove(filename);
            SARUS_RETHROW_ERROR(e, "Download stream error");
        }

        printLog( boost::format("Finished download stream to %s") % filename, common::LogLevel::DEBUG);
    }

    /**
     * Retrieve image manifest from server
     */
    web::json::value Puller::retrieveImageManifest() {
        printLog(boost::format("Retrieving image manifest from %s") % config->imageReference.server,
                 common::LogLevel::INFO);

        // Attempt to retrieve manifest
        std::unique_ptr<web::http::client::http_client> client = setupHttpClient(getServerUri(config->imageReference.server));
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
            SARUS_RETHROW_ERROR(e, "Error while sending request for manifest to remote registry");
        }

        // If the registry responds we're unauthorized, send a new request with an authorization header
        if(response.status_code() == status_codes::Unauthorized) {
            auto message = boost::format("Received http_response status code(%s): %s")
                % response.status_code() %  response.reason_phrase();
            printLog(message, common::LogLevel::DEBUG);

            authorizationToken = requestAuthorizationToken(response);

            printLog(boost::format("Constructing new manifest request"), common::LogLevel::DEBUG);
            web::http::http_request authHeaderRequest(methods::GET);
            authHeaderRequest.set_request_uri( makeImageManifestUri() );
            std::string authorizationString = (boost::format("Bearer %s") % authorizationToken).str();
            authHeaderRequest.headers().add(header_names::authorization, U(authorizationString) );

            printLog( boost::format("server      : %s") % getServerUri(config->imageReference.server), common::LogLevel::DEBUG);
            printLog( boost::format("request_uri : %s") % makeImageManifestUri(), common::LogLevel::DEBUG);
            printLog( boost::format("header      : %s") % U(authorizationString), common::LogLevel::DEBUG);
            printLog( boost::format("full request: %s") % authHeaderRequest.to_string(), common::LogLevel::DEBUG);

            response = client->request(authHeaderRequest).get();

            // If an Unauthorized response is returned again, inform the user and suggest to use login credentials
            if(response.status_code() == status_codes::Unauthorized || response.status_code() == status_codes::Forbidden) {
                auto message = boost::format{"Failed to pull image '%s'"
                    "\nThe image may be private or not present in the remote registry."
                    "\nDid you perform a login with the proper credentials?"
                    "\nSee 'sarus help pull' (--login option)"} % config->imageReference;
                printLog(message, common::LogLevel::GENERAL, std::cerr);

                message = boost::format{"Failed to pull manifest. Received http_response status code(%s): %s"}
                    % response.status_code() % response.reason_phrase();
                SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
            }
        }

        // Throw an error if response is not successful
        if(response.status_code() != status_codes::OK) {
            auto message = boost::format{"Failed to pull image '%s'"}  % config->imageReference;
            if(response.status_code() == status_codes::NotFound){
                message = boost::format{"%s\nThe image is not present in the remote registry."} % message.str();
            }
            else {
                message = boost::format{"%s\nUnexpected response from remote registry."} % message.str();
            }
            printLog(message, common::LogLevel::GENERAL, std::cerr);

            message = boost::format{"Failed to pull manifest. Received http_response status code(%s): %s"}
                % response.status_code() % response.reason_phrase();
            SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);
        }
        
        auto manifest = response.extract_json(true).get();

        // check manifest
        if ( manifest.has_field(U("errors")) ) {
            auto message = boost::format("Failed to retrieve manifest for image %s."
                                         " Downloaded manifest has 'errors' field: %s")
                % config->imageReference % manifest.serialize();
            SARUS_THROW_ERROR(message.str());
        }

        printLog(boost::format("Retrieved image manifest:\n%s") % manifest.serialize(),
                 common::LogLevel::DEBUG);

        printLog(boost::format("Successfully retrieved image manifest"), common::LogLevel::INFO);

        return manifest;
    }

    std::string Puller::makeImageManifestUri() {
        return (boost::format("v2/%s/%s/manifests/%s")
                % config->imageReference.repositoryNamespace
                % config->imageReference.image
                % config->imageReference.tag).str();
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

    std::string Puller::requestAuthorizationToken(web::http::http_response& response) {
        printLog(boost::format("Getting new authorization token from %s") % config->imageReference.server,
                 common::LogLevel::DEBUG);

        std::string realm;
        std::string service;
        std::string scope;
        auto auth_header = response.headers()[U("Www-Authenticate")];
        std::tie(realm, service, scope) = parseWwwAuthenticateHeader(auth_header);

        // get authorized token
        std::unique_ptr<web::http::client::http_client> tokenClient = setupHttpClient( realm );
        web::http::http_request tokenReq(methods::GET);
        web::uri_builder tokenUriBuilder("");
        web::http::http_response tokenResp;
        std::string token;
    
        if (!scope.empty()){
            tokenUriBuilder.append_query(U("scope"), scope);
        }
        if (!service.empty()){
            tokenUriBuilder.append_query(U("service"), service);
        }

        tokenReq.set_request_uri(tokenUriBuilder.to_string());

        printLog( boost::format("Full auth token request: %s") % tokenReq.to_string(), common::LogLevel::DEBUG);
        tokenResp = tokenClient->request(tokenReq).get();
        if(tokenResp.status_code() != status_codes::OK) {
            if (tokenResp.status_code() == status_codes::Unauthorized) {
                auto message = boost::format{"Authorization failed when retrieving token for image '%s'"
                        "\nPlease check the entered credentials."}  % config->imageReference;
                printLog(message, common::LogLevel::GENERAL, std::cerr);

                message = boost::format("Failed to get token. Received http_response status code(%s): %s")
                    % tokenResp.status_code() % tokenResp.reason_phrase();
                SARUS_THROW_ERROR(message.str(), common::LogLevel::INFO);

            }
            else {
                auto message = boost::format("Failed to get token. Received http_response status code(%s): %s")
                    % tokenResp.status_code() %  tokenResp.reason_phrase();
                SARUS_THROW_ERROR(message.str());
            }
        }

        try {
            json::value respJson = tokenResp.extract_json(true).get();
            token = respJson[U("token")].serialize();
            token = common::eraseFirstAndLastDoubleQuote(token);
        }
        catch (const std::exception& e) {
            auto message = boost::format("Failed to get Token: %s") % e.what();
            SARUS_RETHROW_ERROR(e, message.str());
        }

        printLog( boost::format("Got token: %s") % token, common::LogLevel::DEBUG);
        printLog( boost::format("Successfully got new authorization token"), common::LogLevel::DEBUG);

        return token;
    }

    std::unique_ptr<web::http::client::http_client> Puller::setupHttpClient(const std::string& server) {
        web::http::client::http_client_config clientConfig;
        clientConfig.set_validate_certificates(config->enforceSecureServer);
        setProxyIfNecessary(clientConfig);

        // if repository is private, add credential configurations
        if(config->authentication.isAuthenticationNeeded) {
            web::credentials creds( U(config->authentication.username), U(config->authentication.password) );
            clientConfig.set_credentials(creds);
        }

        return std::unique_ptr<web::http::client::http_client>( new web::http::client::http_client( server, clientConfig ));
    }

    void Puller::setProxyIfNecessary(web::http::client::http_client_config& clientConfig) {
        auto proxyURI = getProxy();
        if (!proxyURI.empty()) {
            printLog( boost::format("Setting proxy for HTTP client: %s") % proxyURI, common::LogLevel::DEBUG);
            clientConfig.set_proxy(web::web_proxy(proxyURI));
        }
    }

    std::string Puller::getProxy() {
        // Prefer lower case variable name like Python's urllib
        auto noProxyVar = config->commandRun.hostEnvironment.find("no_proxy");
        if (noProxyVar != config->commandRun.hostEnvironment.end() && !noProxyVar->second.empty()) {
            if (isRegistryInNoProxyList(noProxyVar->second)) {
                return std::string{};
            }
        }
        else {
            noProxyVar = config->commandRun.hostEnvironment.find("NO_PROXY");
            if (noProxyVar != config->commandRun.hostEnvironment.end() && !noProxyVar->second.empty()) {
                if (isRegistryInNoProxyList(noProxyVar->second)) {
                    return std::string{};
                }
            }
        }

        auto proxyVar = config->commandRun.hostEnvironment.find("ALL_PROXY");
        if (proxyVar != config->commandRun.hostEnvironment.end() && !proxyVar->second.empty()) {
            return proxyVar->second;
        }

        if (config->enforceSecureServer) {
            // Prefer lower case like curl and Python's urllib
            auto proxyVar = config->commandRun.hostEnvironment.find("https_proxy");
            if (proxyVar != config->commandRun.hostEnvironment.end() && !proxyVar->second.empty()) {
                return proxyVar->second;
            }
            proxyVar = config->commandRun.hostEnvironment.find("HTTPS_PROXY");
            if (proxyVar != config->commandRun.hostEnvironment.end() && !proxyVar->second.empty()) {
                return proxyVar->second;
            }
        }
        else {
            // Only check lower case to avoid security issues with upper case version in CGI environments
            auto proxyVar = config->commandRun.hostEnvironment.find("http_proxy");
            if (proxyVar != config->commandRun.hostEnvironment.end() && !proxyVar->second.empty()) {
                return proxyVar->second;
            }
        }

        return std::string{};
    }

    bool Puller::isRegistryInNoProxyList(const std::string& noProxyList) {
        if (noProxyList == "*") {
            return true;
        }
        auto hostnames = std::vector<std::string>{};
        boost::split(hostnames, noProxyList, boost::is_any_of(","));
        for (const auto& host : hostnames) {
            if (host == config->imageReference.server) {
                return true;
            }
        }
        return false;
    }

    /**
     * Parse the Www-Authenticate header from an HTTP response to extract realm,
     * service and scope parameters (if present)
     */
    std::tuple<std::string, std::string, std::string> Puller::parseWwwAuthenticateHeader(const std::string& auth_header) {
        printLog( boost::format("Parsing Www-Authenticate header from unauthorized response"), common::LogLevel::DEBUG);

        auto realm   = getParam(auth_header, "realm");
        std::string service;
        std::string scope;

        if (realm.find("service=") == std::string::npos){
            service = getParam(auth_header, "service");
        }

        if (realm.find("scope=") == std::string::npos){
            scope = getParam(auth_header, "scope");
        }

        printLog( boost::format("realm   : %s") % realm, common::LogLevel::DEBUG);
        printLog( boost::format("service : %s") % service, common::LogLevel::DEBUG);
        printLog( boost::format("scope   : %s") % scope, common::LogLevel::DEBUG);

        return std::tuple<std::string, std::string, std::string>(realm, service, scope);
    }

    /**
     * Get params from the HTTP response header(Www-Authenticate)
     */
    std::string Puller::getParam(const std::string &header, const std::string& param)
    {
        auto paramPosition = header.find(param + "=");
        if (paramPosition == std::string::npos){
            return std::string();
        }
        int i = paramPosition + param.size() + 2;
        int j = header.substr(i, header.size()).find('\"');
        return header.substr(i, j);
    }

    std::string Puller::getServerUri(const std::string &server) {
        return (config->enforceSecureServer) ? "https://" + server : "http://" + server ;
    }

    void Puller::printLog(  const boost::format &message, common::LogLevel LogLevel,
                            std::ostream& outStream, std::ostream& errStream) const
    {
        common::Logger::getInstance().log(message.str(), sysname, LogLevel, outStream, errStream);
    }
    
} // namespace
} // namespace
