#pragma once

#include <curl/curl.h>
#include <string_view>
#include <zlib.h>

#include "bosim/base/log.h"

#include "server/runner/curl.h"

#include "mqc3_cloud/program/v1/quantum_program.pb.h"

/**
 * Gzip-compress a byte sequence using zlib (gzip header/trailer).
 * Robust to inputs > 4 GiB by chunking avail_in to uInt.
 */
inline bool GzipCompressZlib(std::string_view input, std::string* out,
                             int level = Z_DEFAULT_COMPRESSION) {
    if (out == nullptr) return false;
    out->clear();

    z_stream zs{};
    // 15: max windowBits, +16: enable gzip wrapper.
    if (deflateInit2(&zs, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        LOG_ERROR("deflateInit2 failed.");
        return false;
    }

    constexpr auto OutBufsz = std::size_t{32} * std::size_t{1024};
    char outbuf[OutBufsz];

    // Feed input in chunks that fit into uInt.
    const auto* in_ptr = reinterpret_cast<const unsigned char*>(input.data());
    auto remaining = input.size();

    auto pump = [&](int flush) -> bool {
        int ret;
        do {
            zs.next_out = reinterpret_cast<Bytef*>(outbuf);
            zs.avail_out = static_cast<uInt>(OutBufsz);

            ret = deflate(&zs, flush);
            if (ret == Z_STREAM_ERROR) {
                LOG_ERROR("deflate returned Z_STREAM_ERROR.");
                return false;
            }
            const std::size_t have = OutBufsz - zs.avail_out;
            out->append(outbuf, have);
        } while (zs.avail_out == 0);
        return true;
    };

    while (remaining > 0) {
        const uInt chunk =
            static_cast<uInt>(std::min<std::size_t>(remaining, std::numeric_limits<uInt>::max()));
        zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in_ptr));
        zs.avail_in = chunk;

        in_ptr += chunk;
        remaining -= chunk;

        // Drain output while we have input (Z_NO_FLUSH).
        if (!pump(Z_NO_FLUSH)) {
            deflateEnd(&zs);
            return false;
        }
    }

    // Finish the stream.
    if (!pump(Z_FINISH)) {
        deflateEnd(&zs);
        return false;
    }

    deflateEnd(&zs);
    return true;
}

/**
 * Serialize result protobuf and gzip-compress it.
 */
inline bool CompressByGzip(std::string* o_compressed, std::size_t* o_raw_size_bytes,
                           std::size_t* o_encoded_size_bytes,
                           const mqc3_cloud::program::v1::QuantumProgramResult& result) {
    if (o_compressed == nullptr || o_raw_size_bytes == nullptr || o_encoded_size_bytes == nullptr) {
        return false;
    }

    std::string uncompressed;
    if (!result.SerializeToString(&uncompressed)) {
        LOG_ERROR("SerializeToString failed.");
        return false;
    }

    if (!GzipCompressZlib(uncompressed, o_compressed, Z_DEFAULT_COMPRESSION)) {
        LOG_ERROR("GzipCompressZlib failed.");
        return false;
    }

    *o_raw_size_bytes = uncompressed.size();
    *o_encoded_size_bytes = o_compressed->size();

    const double raw_mib = static_cast<double>(*o_raw_size_bytes) / 1024.0 / 1024.0;
    const double enc_mib = static_cast<double>(*o_encoded_size_bytes) / 1024.0 / 1024.0;
    const double ratio = (*o_raw_size_bytes != 0) ? enc_mib / raw_mib : 0.0;

    LOG_DEBUG_ALWAYS(
        "original_size={:.2f}[MiB] compressed_size={:.2f}[MiB] compression_ratio={:.3f}", raw_mib,
        enc_mib, ratio);
    return true;
}

/**
 * PUT the given compressed buffer to a pre-signed URL.
 * Uses CURLOPT_POSTFIELDS + CURLOPT_POSTFIELDSIZE_LARGE (no read callbacks).
 */
inline bool TryUploadBinary(const std::string& upload_url, const std::string& compressed,
                            double timeout_seconds = 900.0) {
    if (!curl_global::EnsureInitialized()) {
        LOG_ERROR("curl_global_init failed: {}", curl_global::LastErrorStr());
        return false;
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        LOG_ERROR("curl_easy_init failed");
        return false;
    }

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/protobuf");
    headers = curl_slist_append(headers, "Content-Encoding: gzip");
    headers = curl_slist_append(headers, "Content-Disposition: attachment");
    headers = curl_slist_append(headers, "Expect:");  // disable 100-continue to reduce latency

    std::string response_body;
    curl_easy_setopt(curl, CURLOPT_URL, upload_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

    // Provide body buffer directly; Content-Length set via POSTFIELDSIZE.
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, compressed.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(compressed.size()));

    // Network options.
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    // Capture response body for diagnostics.
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION, +[](char* p, size_t s, size_t n, void* u) -> size_t {
            static_cast<std::string*>(u)->append(p, s * n);
            return s * n;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    const CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    if (rc == CURLE_OK) { curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code); }

    const bool ok = (rc == CURLE_OK) && (http_code >= 200 && http_code < 300);
    if (!ok) {
        if (rc != CURLE_OK) {
            LOG_ERROR("CURL failed: {}", curl_easy_strerror(rc));
        } else {
            LOG_ERROR("HTTP error {} body: {}", http_code, response_body);
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ok;
}
