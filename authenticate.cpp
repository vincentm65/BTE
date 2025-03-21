#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <curl/curl.h>
#include <json/json.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include <QDir>
#include <QCoreApplication>

#include "authenticate.h"


// ------------------------
// Helper Functions
// ------------------------
QString tokenFilePath = QDir(QCoreApplication::applicationDirPath()).filePath("tokens.txt");

std::string urlDecode(const std::string& str) {
    char* decoded = curl_easy_unescape(nullptr, str.c_str(), static_cast<int>(str.length()), nullptr);
    std::string result(decoded);
    curl_free(decoded);
    return result;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

std::string base64_encode(const std::string& input) {
    BIO* bio = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    bio = BIO_push(bio, bmem);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // No newlines in output
    BIO_write(bio, input.c_str(), static_cast<int>(input.length()));
    BIO_flush(bio);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(bio, &bptr);
    std::string encoded(bptr->data, bptr->length);
    BIO_free_all(bio);
    return encoded;
}

// ------------------------
// Authentication Functions Implementation
// ------------------------

void openAuthUrl(const std::string& authUrl) {
    system(("start " + authUrl).c_str());
}

void exchangeAuthCodeForToken(const std::string& authCode) {
    std::string clientId = "IlG3Rwq8RWkpmoHzPDrcE7QA1a5VxKWH";
    std::string clientSecret = "vlRMsI15ZRXNSo1M";
    std::string redirectUri = "https://127.0.0.1";
    std::string tokenUrl = "https://api.schwabapi.com/v1/oauth/token";
    if (authCode.empty()) {
        std::cerr << "Error: Authorization code is empty!" << std::endl;
        return;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL initialization failed!" << std::endl;
        return;
    }
    std::string decodedAuthCode = urlDecode(authCode);
    std::string credentials = clientId + ":" + clientSecret;
    std::string base64Credentials = base64_encode(credentials);
    std::string postFields = "grant_type=authorization_code"
                             "&code=" + decodedAuthCode +
                             "&redirect_uri=" + redirectUri;
    std::cout << "POST Fields: " << postFields << std::endl;
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, tokenUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, ("Authorization: Basic " + base64Credentials).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "CURL request failed: " << curl_easy_strerror(res) << std::endl;
    }
    else {
        std::cout << "Raw Response: " << response << std::endl;
        Json::Value jsonData;
        Json::CharReaderBuilder reader;
        std::string errs;
        std::istringstream stream(response);
        if (Json::parseFromStream(reader, stream, &jsonData, &errs)) {
            std::string accessToken = jsonData["access_token"].asString();
            std::string refreshToken = jsonData["refresh_token"].asString();
            std::cout << "Access Token: " << accessToken << std::endl;
            std::cout << "Refresh Token: " << refreshToken << std::endl;
            std::ofstream tokenFile(tokenFilePath.toStdString());
            if (tokenFile.is_open()) {
                tokenFile << "access_token=" << accessToken << "\n";
                tokenFile << "refresh_token=" << refreshToken << "\n";
                tokenFile.close();
                std::cout << "Tokens written to file!" << std::endl;
            }
            else {
                std::cerr << "Failed to open " << tokenFilePath.toStdString() << " for writing!" << std::endl;
            }
        }
        else {
            std::cerr << "Failed to parse JSON response: " << errs << std::endl;
        }
    }
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}

std::string refreshAccessToken(const std::string& refreshToken) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return "";
    }

    std::string clientId = "IlG3Rwq8RWkpmoHzPDrcE7QA1a5VxKWH";
    std::string clientSecret = "vlRMsI15ZRXNSo1M";
    std::string tokenUrl = "https://api.schwabapi.com/v1/oauth/token";

    // Build Basic Auth credentials
    std::string credentials = clientId + ":" + clientSecret;
    std::string base64Credentials = base64_encode(credentials);

    // Build the POST fields (note: client_id is no longer in the post body)
    std::string postFields = "grant_type=refresh_token&refresh_token=" + refreshToken;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, tokenUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Set up the headers with proper Content-Type and Authorization.
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, ("Authorization: Basic " + base64Credentials).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::cerr << "CURL request failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    // Parse JSON response using JsonCpp
    Json::Value jsonResponse;
    Json::CharReaderBuilder reader;
    std::string errors;
    std::istringstream responseStream(response);
    if (!Json::parseFromStream(reader, responseStream, &jsonResponse, &errors)) {
        std::cerr << "Failed to parse JSON response: " << errors << std::endl;
        return "";
    }

    std::string newAccessToken = jsonResponse["access_token"].asString();
    std::string newRefreshToken = jsonResponse["refresh_token"].asString();

    // Check for valid tokens before writing to file.
    if (newAccessToken.empty() || newRefreshToken.empty()) {
        std::cerr << "Error: Refresh response did not return valid tokens." << std::endl;
        std::cerr << "Response: " << response << std::endl;
        return "";
    }

    // Save new tokens to file.
    std::ofstream tokenFile(tokenFilePath.toStdString());
    if (!tokenFile) {
        std::cerr << "Failed to open " << tokenFilePath.toStdString() << " for writing!" << std::endl;
    }
    else {
        tokenFile << "access_token=" << newAccessToken << "\n";
        tokenFile << "refresh_token=" << newRefreshToken << "\n";
        tokenFile.close();
        std::cout << "Tokens refreshed and written to file!" << std::endl;
    }

    return newAccessToken;
}

void authenticate_client() {
    std::string clientId = "IlG3Rwq8RWkpmoHzPDrcE7QA1a5VxKWH";
    std::string redirectUri = "https://127.0.0.1";
    std::string authUrl = "https://api.schwabapi.com/v1/oauth/authorize?"
                          "client_id=" + clientId +
                          "&redirect_uri=" + redirectUri;
    std::cout << "Auth URL: " << authUrl << std::endl;
    std::cout << "\nPaste the full redirect URL after login: ";
    std::string returnedUrl;
    std::cin >> returnedUrl;
    size_t codePos = returnedUrl.find("code=");
    if (codePos == std::string::npos) {
        std::cerr << "Error: Authorization code not found!" << std::endl;
        return;
    }
    size_t endPos = returnedUrl.find('&', codePos + 5);
    if (endPos == std::string::npos) {
        endPos = returnedUrl.length();
    }
    std::string authCode = returnedUrl.substr(codePos + 5, endPos - (codePos + 5));
    std::string decodedAuthCode = urlDecode(authCode);
    std::cout << "Authorization Code: " << decodedAuthCode << std::endl;
    exchangeAuthCodeForToken(decodedAuthCode);
}

std::string getAccessToken() {
    std::ifstream tokenFile(tokenFilePath.toStdString());
    std::string accessToken;
    std::string refreshToken;

    if (tokenFile.is_open()) {
        std::string line;
        while (std::getline(tokenFile, line)) {
            if (line.find("access_token=") == 0) {
                accessToken = line.substr(13);
            }
            else if (line.find("refresh_token=") == 0) {
                refreshToken = line.substr(line.find("=") + 1);
            }
        }
        tokenFile.close();
    }

    // If access token is empty, attempt to refresh
    if (accessToken.empty() && !refreshToken.empty()) {
        std::cout << "Access token is expired or missing, refreshing token...\n";
        accessToken = refreshAccessToken(refreshToken);  // Refresh token
    }

    if (accessToken.empty()) {
        std::cerr << "Error: Could not retrieve or refresh access token." << std::endl;
    }

    return accessToken;
}
