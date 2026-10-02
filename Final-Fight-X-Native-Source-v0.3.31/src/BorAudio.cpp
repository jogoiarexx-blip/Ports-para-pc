#include "BorAudio.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace ffx {
namespace {
uint32_t readU32(const std::vector<unsigned char>& b, size_t o) {
    uint32_t v = 0;
    if (o + sizeof(v) <= b.size()) std::memcpy(&v, b.data() + o, sizeof(v));
    return v;
}

constexpr std::array<int, 16> kIndexTable = {
    -1,-1,-1,-1, 2,4,6,8,
    -1,-1,-1,-1, 2,4,6,8
};
constexpr std::array<int, 89> kStepTable = {
    7,8,9,10,11,12,13,14,16,17,
    19,21,23,25,28,31,34,37,41,45,
    50,55,60,66,73,80,88,97,107,118,
    130,143,157,173,190,209,230,253,279,307,
    337,371,408,449,494,544,598,658,724,796,
    876,963,1060,1166,1282,1411,1552,1707,1878,2066,
    2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,
    5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,
    15289,16818,18500,20350,22385,24623,27086,29794,32767
};

struct AdpcmState { int predictor = 0; int index = 0; };

int16_t decodeNibble(unsigned char nibble, AdpcmState& state) {
    const int step = kStepTable[(size_t)std::clamp(state.index, 0, 88)];
    const int delta = nibble & 7;
    int diff = step >> 3;
    if (delta & 4) diff += step;
    if (delta & 2) diff += step >> 1;
    if (delta & 1) diff += step >> 2;

    state.predictor += (nibble & 8) ? -diff : diff;
    state.predictor = std::clamp(state.predictor, -32768, 32767);
    state.index = std::clamp(state.index + kIndexTable[(size_t)(nibble & 15)], 0, 88);
    return (int16_t)state.predictor;
}
}

bool decodeBorMusic(const std::vector<unsigned char>& b, BorPcm& out) {
    constexpr uint32_t kMono = 0x00010000u;
    constexpr uint32_t kStereo = 0x00010001u;
    out = {};

    if (b.size() < 160 || std::memcmp(b.data(), "BOR music", 9) != 0) return false;
    const uint32_t version = readU32(b, 0x90);
    const uint32_t rate = readU32(b, 0x94);
    const uint32_t channels = readU32(b, 0x98);
    const uint32_t dataStart = readU32(b, 0x9c);

    if ((version != kMono && version != kStereo) ||
        (channels != 1 && channels != 2) ||
        rate < 11025 || rate > 44100 ||
        dataStart < 160 || dataStart >= b.size()) return false;
    if ((version == kMono && channels != 1) || (version == kStereo && channels != 2)) return false;

    const size_t compressedBytes = b.size() - dataStart;
    if (compressedBytes > std::numeric_limits<size_t>::max() / 2u) return false;

    out.sampleRate = rate;
    out.channels = (uint16_t)channels;
    out.samples.reserve(compressedBytes * 2u);
    AdpcmState state[2]{};

    if (channels == 1) {
        for (size_t i = dataStart; i < b.size(); ++i) {
            const unsigned char v = b[i];
            out.samples.push_back(decodeNibble((v >> 4) & 0x0f, state[0]));
            out.samples.push_back(decodeNibble(v & 0x0f, state[0]));
        }
    } else {
        // OpenBOR stereo ADPCM stores left in the high nibble and right in the low nibble.
        for (size_t i = dataStart; i < b.size(); ++i) {
            const unsigned char v = b[i];
            out.samples.push_back(decodeNibble((v >> 4) & 0x0f, state[0]));
            out.samples.push_back(decodeNibble(v & 0x0f, state[1]));
        }
    }
    return !out.samples.empty();
}
}
