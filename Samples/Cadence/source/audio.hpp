#pragma once
#include "song.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace cadence
{
inline void put16(std::vector<unsigned char> &b, unsigned v)
{
    b.push_back(static_cast<unsigned char>(v));
    b.push_back(static_cast<unsigned char>(v >> 8));
}
inline void put32(std::vector<unsigned char> &b, unsigned v)
{
    put16(b, v);
    put16(b, v >> 16);
}
inline std::vector<unsigned char> renderWav(const Song &song)
{
    validateSong(song);
    constexpr int rate = 22050;
    const double stepSeconds = 15.0 / song.tempo;
    const int count = static_cast<int>(song.steps() * stepSeconds * rate + 0.5);
    std::vector<double> mix(static_cast<std::size_t>(count), 0.0);
    const double twoPi = 8.0 * std::atan(1.0);
    for (int t = 0; t < Tracks; ++t)
    {
        if (song.tracks[t].muted)
            continue;
        for (int s = 0; s < song.steps(); ++s)
        {
            const int note = song.tracks[t].notes[s];
            if (note == Rest)
                continue;
            const double hz = 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
            const double duration = stepSeconds * 0.92;
            const int begin = static_cast<int>(s * stepSeconds * rate + 0.5);
            const int end = std::min(count, begin + static_cast<int>(duration * rate + 0.5));
            const double amp = t == 0 ? 0.34 : (t == 1 ? 0.20 : 0.26);
            for (int i = begin; i < end; ++i)
            {
                const double x = static_cast<double>(i - begin) / rate;
                const double phase = twoPi * hz * x;
                double wave = std::sin(phase);
                const int instrument = song.tracks[t].instrument;
                if (instrument == 1)
                    wave = 0.68 * wave + 0.22 * std::sin(phase * 2.0) + 0.1 * std::sin(phase * 3.0);
                if (instrument == 2)
                    wave = std::sin(phase) * std::sin(phase * 0.5);
                mix[static_cast<std::size_t>(i)] += amp * wave * std::exp(-2.5 * x / duration);
            }
        }
    }
    std::vector<unsigned char> bytes;
    bytes.reserve(static_cast<std::size_t>(count) * 2 + 44);
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    put32(bytes, 36u + static_cast<unsigned>(count) * 2u);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(bytes, 16);
    put16(bytes, 1);
    put16(bytes, 1);
    put32(bytes, rate);
    put32(bytes, rate * 2);
    put16(bytes, 2);
    put16(bytes, 16);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    put32(bytes, static_cast<unsigned>(count) * 2u);
    for (double sample : mix)
    {
        sample = std::max(-1.0, std::min(1.0, sample));
        put16(bytes, static_cast<unsigned>(static_cast<std::int16_t>(std::lround(sample * 32767.0))));
    }
    return bytes;
}
inline void exportWav(const Song &song, const std::filesystem::path &path)
{
    const auto bytes = renderWav(song);
    samples::writeAtomically(
        path, [&](std::ostream &out)
        { out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())); });
}
} // namespace cadence
