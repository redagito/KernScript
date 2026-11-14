#pragma once

#include "../../Support/Files.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cadence
{
constexpr int Tracks = 3;
constexpr int MaxSteps = 128;
constexpr int Rest = -1;
constexpr int NoteLow = 0;
constexpr int NoteHigh = 127;

struct Track
{
    bool muted = false;
    int instrument = 0;
    std::array<int, MaxSteps> notes{};
    Track() { notes.fill(Rest); }
    bool operator==(const Track &) const = default;
};

struct Song
{
    int bars = 2;
    int tempo = 100;
    int root = 0;
    int scale = 0; // 0 major, 1 natural minor, 2 pentatonic
    std::array<Track, Tracks> tracks{};
    int steps() const { return bars * 16; }
    bool operator==(const Song &) const = default;
};

inline void validateSong(const Song &song)
{
    if (song.bars != 2 && song.bars != 4 && song.bars != 8)
        throw std::runtime_error("Song length must be 2, 4, or 8 bars");
    if (song.tempo < 40 || song.tempo > 240 || song.root < 0 || song.root > 11 || song.scale < 0 || song.scale > 2)
        throw std::runtime_error("Song controls are outside their supported range");
    for (const auto &track : song.tracks)
    {
        if (track.instrument < 0 || track.instrument > 2)
            throw std::runtime_error("Track instrument is outside its supported range");
        for (int note : track.notes)
            if (note != Rest && (note < NoteLow || note > NoteHigh))
                throw std::runtime_error("Track note is outside the MIDI range");
    }
}

inline void saveSong(const Song &song, const std::filesystem::path &path)
{
    validateSong(song);
    samples::writeAtomically(path,
                             [&](std::ostream &out)
                             {
                                 out << "CADENCE 1\n"
                                     << song.bars << ' ' << song.tempo << ' ' << song.root << ' ' << song.scale << '\n';
                                 for (int t = 0; t < Tracks; ++t)
                                 {
                                     out << song.tracks[t].muted << ' ' << song.tracks[t].instrument << '\n';
                                     for (int s = 0; s < MaxSteps; ++s)
                                         out << song.tracks[t].notes[s] << (s + 1 == MaxSteps ? '\n' : ' ');
                                 }
                             });
}

inline Song openSong(const std::filesystem::path &path)
{
    std::istringstream in(samples::readText(path, 65536));
    std::string magic;
    int version = 0;
    Song song;
    if (!(in >> magic >> version) || magic != "CADENCE" || version != 1)
        throw std::runtime_error("Unsupported Cadence document");
    if (!(in >> song.bars >> song.tempo >> song.root >> song.scale) ||
        (song.bars != 2 && song.bars != 4 && song.bars != 8) || song.tempo < 40 || song.tempo > 240 || song.root < 0 ||
        song.root > 11 || song.scale < 0 || song.scale > 2)
        throw std::runtime_error("Invalid Cadence controls");
    for (int t = 0; t < Tracks; ++t)
    {
        int muted = 0;
        if (!(in >> muted >> song.tracks[t].instrument) || (muted != 0 && muted != 1) ||
            song.tracks[t].instrument < 0 || song.tracks[t].instrument > 2)
            throw std::runtime_error("Invalid Cadence track settings");
        song.tracks[t].muted = muted != 0;
        for (int s = 0; s < MaxSteps; ++s)
        {
            int n = Rest;
            if (!(in >> n) || (n != Rest && (n < NoteLow || n > NoteHigh)))
                throw std::runtime_error("Invalid note in Cadence document");
            song.tracks[t].notes[s] = n;
        }
    }
    in >> std::ws;
    if (!in.eof())
        throw std::runtime_error("Unexpected data after Cadence document");
    validateSong(song);
    return song;
}
} // namespace cadence
