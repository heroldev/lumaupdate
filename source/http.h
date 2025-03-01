#pragma once

#include "libs.h"

/*! \brief Optional extra httpGet informations */
struct HTTPResponseInfo {
	std::string etag; //!< ETag (for AWS S3 requests)
	std::string hash; //!< Hash (from content-md5 header)
};

/*! \brief Makes a GET HTTP request
 *  This function will throw an exception if it encounters any error
 *
 *  \param url     URL to download
 *  \param buf     Output buffer (will be allocated by the function)
 *  \param size    Output buffer size
 *  \param verbose OPTIONAL Write download progress to screen (via printf)
 *  \param info    OPTIONAL Pointer to HTTPResponseInfo struct to fill with extra data
 */
void httpGet(const char* url, u8** buf, u32* size, const bool verbose = false, HTTPResponseInfo* info = nullptr);

/*! \brief Check for file integrity via MD5 Hash content header
 *
 *  \param hash    hash from content-md5 header
 *  \param fileData Pointer to file data to check
 *  \param fileSize Size of the file to check
 *
 *  \return true if the check succeeds (md5 match), false otherwise
 */
bool httpCheckHash(std::string hash, const u8* fileData, const u32 fileSize);