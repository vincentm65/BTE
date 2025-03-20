#ifndef AUTHENTICATE_H
#define AUTHENTICATE_H

#include <string>


// Helper functions
std::string urlDecode(const std::string& str);
std::string base64_encode(const std::string& input);
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response);

// Authentication & token functions
void openAuthUrl(const std::string& authUrl);
void exchangeAuthCodeForToken(const std::string& authCode);
std::string refreshAccessToken(const std::string& refreshToken);
void authenticate_client();
std::string getAccessToken();

#endif // AUTHENTICATE_H
