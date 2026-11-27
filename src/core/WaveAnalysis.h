#pragma once

// Waveform analysis for drawing audio (Playlist audio clips, the Sampler editor, the Browser preview).
//
// A Summary is a multi-resolution peak cache: level 0 holds one column per `bucket` frames (min/max per
// channel, RMS per channel and the RMS of three frequency bands of the mono mix); each further level merges
// `levelFactor` columns of the one below. Drawing asks query() for any frame range and gets the right level,
// so zooming in or out stays detailed and costs a handful of columns per pixel.
//
// The bands (low < 250 Hz, mid 250 Hz - 4 kHz, high > 4 kHz) come from 4th-order Linkwitz-Riley-style
// crossovers (two cascaded 2nd-order Butterworth sections); spectralColour() turns a band mix into a colour of
// LYNX's palette (low = warm red-orange, mid = soft lime, high = cool cyan-white). Own design (clean-room).
//
// JUCE-free; analyse() is pure and runs on a worker thread.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lynx::wave
{
constexpr std::size_t minBucket = 64;          // frames per level-0 column (short files)
constexpr std::size_t maxBaseColumns = 1 << 18; // longer files use a bigger level-0 bucket (memory bound)
constexpr std::size_t levelFactor = 4;
constexpr double lowCrossoverHz = 250.0, highCrossoverHz = 4000.0;

struct Column
{
    std::int16_t lo[2] {}, hi[2] {}; // min/max sample, scaled by Summary::scale
    std::uint16_t rms[2] {};         // RMS per channel
    std::uint16_t band[3] {};        // RMS of the low / mid / high band (mono mix)
};

// One drawing column (any frame range), values relative to full scale (1 = 0 dBFS).
struct Point
{
    float lo = 0.0f, hi = 0.0f, rms = 0.0f;
    float low = 0.0f, mid = 0.0f, high = 0.0f;
    bool valid = false; // false: past the end of the audio
};

struct Summary
{
    double sampleRate = 44100.0;
    std::size_t frames = 0;
    int channels = 1;
    std::size_t bucket = minBucket; // frames per level-0 column
    float scale = 1.0f;             // full scale of the quantised values (>= 1 for float files over 0 dBFS)
    float peak = 0.0f;              // loudest sample (absolute)
    std::vector<std::vector<Column>> levels;

    // channel: 0 = left, 1 = right, -1 = both merged. Frames are fractional; a range shorter than one level-0
    // column is interpolated between neighbouring columns (smooth when zoomed far in).
    Point query (double fromFrame, double toFrame, int channel) const noexcept;

    std::size_t bucketOf (std::size_t level) const noexcept;
    std::size_t memoryBytes() const noexcept;
};

// right == nullptr: mono. cancel (optional) is polled; a cancelled analysis returns an empty Summary.
Summary analyse (const float* left, const float* right, std::size_t frames, double sampleRate, const std::atomic<bool>* cancel = nullptr);

namespace detail
{
// 2nd-order Butterworth section (RBJ cookbook, Q = 1/sqrt 2), transposed direct form II, mono
struct Section
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    static Section make (double hz, double rate, bool highPass) noexcept;
    double process (double x) noexcept
    {
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
} // namespace detail

// A Summary built while the audio arrives (Edison following a recording): append() costs what the new frames cost,
// however much is in already. Analysing the whole recording again each time made a long take slower and slower to
// follow, until the window stopped answering while the recording went on.
// Its columns are analyse()'s of everything appended, exactly, as long as the level-0 bucket did not have to grow
// (frames <= maxColumns * minBucket) and no sample went past full scale. Past maxColumns the bucket doubles
// (neighbouring columns merged); a louder peak quantises the columns again for the new scale. Both cost O(columns).
class Builder
{
public:
    Builder (double sampleRate, int channels, std::size_t maxColumns = maxBaseColumns);

    // right == nullptr (or a mono builder): mono
    void append (const float* left, const float* right, std::size_t frames);
    const Summary& summary() const noexcept { return s; }
    std::size_t frames() const noexcept { return s.frames; }

private:
    void writeOpenColumn();
    void doubleBucket();
    void rescale (float newScale);
    void refreshLevels (std::size_t firstChanged);

    Summary s;
    std::size_t maxColumns;
    detail::Section lowLp[2], midHp[2], midLp[2], highHp[2];
    // the column being filled (the last one of level 0 once it has frames)
    float lo[2] {}, hi[2] {};
    double sq[2] {}, band[3] {};
    std::size_t inColumn = 0, closedColumns = 0;
};

// Disk cache format (little-endian, versioned). deserialise() rejects anything inconsistent.
constexpr std::uint32_t formatVersion = 1;
std::vector<std::uint8_t> serialise (const Summary& s);
bool deserialise (const std::uint8_t* data, std::size_t size, Summary& out);

// --- colour --------------------------------------------------------------------------------------

// Share of each band in the column's colour (sums to 1). The bands are weighted first so typical music
// (energy falling with frequency) does not read as "all bass": mid x1.6, high x3.2 in amplitude.
struct BandMix
{
    float low = 0.0f, mid = 0.0f, high = 0.0f;
};
BandMix bandMix (float low, float mid, float high) noexcept;

struct Rgb
{
    float r = 0.0f, g = 0.0f, b = 0.0f; // 0..1
};
Rgb spectralColour (const BandMix& mix) noexcept;
inline Rgb spectralColour (const Point& p) noexcept { return spectralColour (bandMix (p.low, p.mid, p.high)); }

// The palette anchors (for the legend and the tests).
constexpr Rgb lowColour { 1.00f, 0.38f, 0.20f };  // warm red-orange
constexpr Rgb midColour { 0.70f, 0.88f, 0.42f };  // soft lime
constexpr Rgb highColour { 0.52f, 0.90f, 1.00f }; // cool cyan-white
} // namespace lynx::wave
