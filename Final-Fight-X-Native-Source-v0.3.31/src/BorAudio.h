#pragma once
#include <cstdint>
#include <vector>

namespace ffx {
struct BorPcm {
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    std::vector<int16_t> samples;
};

// Decode the legacy OpenBOR "BOR music" ADPCM container to interleaved PCM16.
bool decodeBorMusic(const std::vector<unsigned char>& bytes, BorPcm& out);
}
