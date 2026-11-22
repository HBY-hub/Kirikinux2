#include "PSBStreamDecompress.h"

#include "UtilStreams.h"

#include <cstring>
#include <memory>

#include <zlib.h>

#include "PSBLz4Stream.h"

namespace PSB {
    namespace {

        std::unique_ptr<tTVPMemoryStream>
        decompressMdfStream(TJS::tTJSBinaryStream *orgStream, size_t readSize) {
            uLongf uncompressedSize = orgStream->ReadI32LE();
            std::vector<tjs_uint8> uncompressed(uncompressedSize);
            std::vector<tjs_uint8> compressed(readSize - 8);
            orgStream->Read(compressed.data(), readSize - 8);
            if(uncompress(uncompressed.data(), &uncompressedSize,
                          compressed.data(),
                          static_cast<uLong>(readSize - 8)) != Z_OK) {
                return nullptr;
            }
            auto stream = std::make_unique<tTVPMemoryStream>(
                nullptr, static_cast<tjs_uint>(uncompressedSize));
            std::memcpy(stream->GetInternalBuffer(), uncompressed.data(),
                        uncompressedSize);
            stream->SetPosition(0);
            return stream;
        }

    } // namespace

    std::unique_ptr<tTVPMemoryStream>
    openDecompressedStream(TJS::tTJSBinaryStream *input) {
        const size_t readSize = input->GetSize();
        if(readSize < 9) {
            return nullptr;
        }

        char sign[5]{};
        input->Read(sign, 5);
        sign[4] = '\0';
        const char lzfs[] = { 0x04, 0x22, 0x4d, 0x18, 0x00 };
        if(std::memcmp(sign, lzfs, 5) == 0) {
            input->SetPosition(0);
            return decompressLz4Stream(input);
        }

        // MDF containers use "MDF\0" or "mdf\0" depending on the exporter
        // (the KR ports ship lower-case).  Compare case-insensitively, like the
        // reference implementation does, or every .scn/.mdf scenario is
        // rejected as "Not a valid PSB file".
        sign[3] = '\0';
        if((sign[0] | 0x20) == 'm' && (sign[1] | 0x20) == 'd' &&
           (sign[2] | 0x20) == 'f') {
            input->SetPosition(4);
            return decompressMdfStream(input, readSize);
        }

        auto stream = std::make_unique<tTVPMemoryStream>(
            nullptr, static_cast<tjs_uint>(readSize));
        input->SetPosition(0);
        input->ReadBuffer(stream->GetInternalBuffer(), readSize);
        stream->SetPosition(0);
        return stream;
    }

} // namespace PSB
