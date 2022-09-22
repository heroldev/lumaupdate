#include "http.h"
#include "httpc.h"

#include "base64.h"

#include "utils.h"

// libmd5-rfc includes
#include "md5/md5.h"

HTTPC httpc;
void httpGet(const char* url, u8** buf, u32* size, const bool verbose, HTTPResponseInfo* info) {
	httpcContext context;
	CHECK(httpc.OpenContext(&context, HTTPC_METHOD_GET, (char*)url, 0), "Could not open HTTP context");
	// Add User Agent field (required by Github API calls)
	CHECK(httpc.AddRequestHeaderField(&context, (char*)"User-Agent", (char*)"LUMA-UPDATER"), "Could not set User Agent");

	CHECK(httpc.BeginRequest(&context), "Could not begin request");

	u32 statuscode = 0;
	CHECK(httpc.GetResponseStatusCode(&context, &statuscode), "Could not get status code");
	if (statuscode != 200) {
		// Handle 3xx codes
		if (statuscode >= 300 && statuscode < 400) {
			char newUrl[1024];
			CHECK(httpc.GetResponseHeader(&context, (char*)"location", newUrl, 1024), "Could not get Location header for 3xx reply");
			CHECK(httpc.CloseContext(&context), "Could not close HTTP context");
			httpGet(newUrl, buf, size, verbose, info);
			return;
		}
		throw std::runtime_error(formatErrMessage("Non-200 status code", statuscode));
	}

	// Retrieve extra info if required
	if (info != nullptr) {
		char etagChr[512] = { 0 };
		if (httpc.GetResponseHeader(&context, (char*)"ETag", etagChr, 512) == 0) {
			info->etag = std::string(etagChr);
		}

		char hashChr[512] = { 0 };
		if (httpc.GetResponseHeader(&context, (char*)"Content-MD5", hashChr, 512) == 0) {
			info->hash = std::string(hashChr);
		}

	}

	u32 pos = 0;
	u32 dlstartpos = 0;
	u32 dlpos = 0;
	Result dlret = HTTPC_RESULTCODE_DOWNLOADPENDING;

	CHECK(httpc.GetDownloadSizeState(&context, &dlstartpos, size), "Could not get file size");

	*buf = (u8*)std::malloc(*size);
	if (*buf == NULL) throw std::runtime_error(formatErrMessage("Could not allocate enough memory", *size));
	std::memset(*buf, 0, *size);

	while (pos < *size && dlret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING)
	{
		u32 sz = *size - pos;
		dlret = httpc.ReceiveData(&context, *buf + pos, sz);
		CHECK(httpc.GetDownloadSizeState(&context, &dlpos, NULL), "Could not get file size");
		pos = dlpos - dlstartpos;
		if (verbose) {
			logPrintf("Download progress: %lu / %lu", dlpos, *size);
			gfxFlushBuffers();
		}
	}
	
	if (verbose) {
		logPrintf("\n");
	}

	CHECK(httpc.CloseContext(&context), "Could not close HTTP context");
}

/*
OLD ETAG METHOD THAT STOPPED WORKING, IT NOW CHECKS THE CONTENT-MD5 HEADER FOR A HASH
bool httpCheckETag(std::string etag, const u8* fileData, const u32 fileSize) {
	// Strip quotes from either side of the etag
	if (etag[0] == '"') {
		etag = etag.substr(1, etag.length() - 2);
	}

	// Get MD5 bytes from Etag header
	md5_byte_t expected[16];
	const char* etagchr = etag.c_str();
	for (u8 i = 0; i < 16; i++) {
		std::sscanf(etagchr + (i * 2), "%02x", &expected[i]);
	}

	// Calculate MD5 hash of downloaded archive
	md5_state_t state;
	md5_byte_t result[16];
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)fileData, fileSize);
	md5_finish(&state, result);

	return memcmp(expected, result, 16) == 0;
}
*/

bool httpCheckHash(std::string hash, const u8* fileData, const u32 fileSize) {

	// Calculate MD5 hash of downloaded archive
	md5_state_t state;
	md5_byte_t result[16];
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)fileData, fileSize);
	md5_finish(&state, result);

	// source: https://stackoverflow.com/a/261607
	std::vector<BYTE> md5Vector;
	md5Vector.insert(md5Vector.end(), &result[0], &result[16]);

	// source: https://stackoverflow.com/a/13935718
	std::string encodedHash = base64_encode(&md5Vector[0], md5Vector.size());

	std::string expectedmd5 = hash.c_str();
	std::string resultmd5 = encodedHash.c_str();

	// uncomment these for debugging this function
	//logPrintf("\nprovided hash: %s\n", hash.c_str());
	//logPrintf("encoded hash: %s\n", encodedHash.c_str());

	// return true if strings are equal
	return expectedmd5.compare(resultmd5) == 0;
}