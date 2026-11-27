// Spectral waveform analysis (feature/spectral-wave): band mix, peaks/RMS, mip levels, disk-cache format.
// Included at the end of tests.cpp.

#include "core/WaveAnalysis.h"

namespace
{
std::vector<float> waveSine (double hz, float amplitude, std::size_t frames, double rate = 48000.0)
{
    std::vector<float> v (frames);
    for (std::size_t i = 0; i < frames; ++i)
        v[i] = amplitude * (float) std::sin (2.0 * 3.14159265358979323846 * hz * (double) i / rate);
    return v;
}

std::vector<float> waveNoise (float amplitude, std::size_t frames)
{
    std::vector<float> v (frames);
    std::uint32_t x = 0x12345678u;
    for (auto& s : v)
    {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        s = amplitude * ((float) (x & 0xffffff) / (float) 0x800000 - 1.0f);
    }
    return v;
}

lynx::wave::BandMix waveMixOf (const std::vector<float>& v)
{
    const auto s = lynx::wave::analyse (v.data(), nullptr, v.size(), 48000.0);
    // skip the first 0.1 s (filter settling)
    const auto p = s.query (4800.0, (double) v.size(), -1);
    return lynx::wave::bandMix (p.low, p.mid, p.high);
}
} // namespace

TEST (wave_band_mix_follows_the_spectrum)
{
    const auto frames = (std::size_t) 48000;
    const auto low = waveMixOf (waveSine (60.0, 0.8f, frames));
    const auto mid = waveMixOf (waveSine (1000.0, 0.8f, frames));
    const auto high = waveMixOf (waveSine (9000.0, 0.8f, frames));
    const auto noise = waveMixOf (waveNoise (0.8f, frames));

    CHECK (low.low > 0.95f && low.mid < 0.05f && low.high < 0.01f);
    CHECK (mid.mid > 0.95f && mid.low < 0.05f && mid.high < 0.05f);
    CHECK (high.high > 0.9f && high.low < 0.01f);
    // white noise: energy grows with bandwidth, so mostly "high", some "mid", little "low"
    CHECK (noise.high > noise.mid && noise.mid > noise.low && noise.mid > 0.05f && noise.low < 0.1f);
    CHECK (std::abs (noise.low + noise.mid + noise.high - 1.0f) < 1.0e-4f);

    // bass + hats mixed at musical levels: both colours present
    auto mix = waveSine (55.0, 0.6f, frames);
    const auto hats = waveNoise (0.08f, frames);
    for (std::size_t i = 0; i < frames; ++i)
        mix[i] += hats[i];
    const auto both = waveMixOf (mix);
    CHECK (both.low > 0.6f && both.high > 0.1f && both.high < 0.3f); // measured: 0.84 / 0.01 / 0.15

    // silence is neutral, never NaN
    const auto silent = lynx::wave::bandMix (0.0f, 0.0f, 0.0f);
    CHECK (silent.mid == 1.0f && silent.low == 0.0f && silent.high == 0.0f);
}

TEST (wave_spectral_colours_are_warm_to_cool)
{
    using namespace lynx::wave;
    const auto warm = spectralColour (BandMix { 1.0f, 0.0f, 0.0f });
    const auto neutral = spectralColour (BandMix { 0.0f, 1.0f, 0.0f });
    const auto cool = spectralColour (BandMix { 0.0f, 0.0f, 1.0f });
    CHECK (warm.r > 0.9f && warm.r > warm.g && warm.g > warm.b);
    CHECK (neutral.g > neutral.r && neutral.g > neutral.b);
    CHECK (cool.b > 0.9f && cool.b > cool.r);
    // a blend stays bright and between its anchors
    const auto half = spectralColour (BandMix { 0.5f, 0.0f, 0.5f });
    CHECK (std::max ({ half.r, half.g, half.b }) >= 0.89f);
    CHECK (half.r < warm.r + 1e-6f && half.b > warm.b && half.b < cool.b + 1e-6f);
    for (float l = 0.0f; l <= 1.0f; l += 0.1f)
        for (float h = 0.0f; l + h <= 1.0f; h += 0.1f)
        {
            const auto c = spectralColour (BandMix { l, 1.0f - l - h, h });
            CHECK (c.r >= 0.0f && c.r <= 1.0f && c.g >= 0.0f && c.g <= 1.0f && c.b >= 0.0f && c.b <= 1.0f);
        }
}

TEST (wave_peaks_rms_and_levels)
{
    using namespace lynx::wave;
    const auto frames = (std::size_t) 200000;
    const auto v = waveSine (440.0, 0.5f, frames);
    const auto s = analyse (v.data(), nullptr, frames, 48000.0);

    CHECK (s.channels == 1 && s.frames == frames && s.bucket == minBucket && s.scale == 1.0f);
    CHECK (std::abs (s.peak - 0.5f) < 1e-3f);
    CHECK (s.levels.size() > 5 && s.levels.back().size() == 1);
    CHECK (s.levels[0].size() == (frames + minBucket - 1) / minBucket);
    for (std::size_t l = 1; l < s.levels.size(); ++l)
        CHECK (s.levels[l].size() == (s.levels[l - 1].size() + levelFactor - 1) / levelFactor);

    // any zoom: min/max/RMS of a sine (RMS = A / sqrt 2)
    for (const double span : { 1000.0, 5000.0, 60000.0, 190000.0 })
    {
        const auto p = s.query (1000.0, 1000.0 + span, 0);
        CHECK (p.valid);
        CHECK (std::abs (p.hi - 0.5f) < 0.01f && std::abs (p.lo + 0.5f) < 0.01f);
        CHECK (std::abs (p.rms - 0.5f / std::sqrt (2.0f)) < 0.01f);
    }

    // closer than one column: interpolated, still within the sine's range and continuous
    float previous = -1.0f;
    for (double f = 50000.0; f < 50000.0 + 4.0 * minBucket; f += 4.0)
    {
        const auto p = s.query (f, f + 4.0, 0);
        CHECK (p.valid && p.hi <= 0.51f && p.lo >= -0.51f && p.rms > 0.3f);
        if (previous >= 0.0f)
            CHECK (std::abs (p.rms - previous) < 0.02f);
        previous = p.rms;
    }

    // past the end: invalid; a range overlapping the end is clamped
    CHECK (! s.query ((double) frames, (double) frames + 100.0, 0).valid);
    CHECK (s.query ((double) frames - 10.0, (double) frames + 1000.0, 0).valid);
    CHECK (! analyse (nullptr, nullptr, 0, 48000.0).query (0.0, 10.0, -1).valid);
    CHECK (s.memoryBytes() < frames / minBucket * sizeof (Column) * 3 / 2 + 4096);
}

TEST (wave_stereo_and_loud_float_files)
{
    using namespace lynx::wave;
    const auto frames = (std::size_t) 20000;
    const std::vector<float> silent (frames, 0.0f);
    auto loud = waveSine (100.0, 2.0f, frames); // float file over 0 dBFS
    const auto s = analyse (silent.data(), loud.data(), frames, 44100.0);

    CHECK (s.channels == 2 && std::abs (s.scale - 2.0f) < 0.01f);
    const auto l = s.query (0.0, (double) frames, 0), r = s.query (0.0, (double) frames, 1), both = s.query (0.0, (double) frames, -1);
    CHECK (l.rms < 1e-3f && l.hi < 1e-3f);
    CHECK (std::abs (r.hi - 2.0f) < 0.02f && std::abs (r.lo + 2.0f) < 0.02f);
    CHECK (std::abs (both.hi - 2.0f) < 0.02f && both.rms > l.rms && both.rms < r.rms);

    // the same pointer twice is mono
    CHECK (analyse (loud.data(), loud.data(), frames, 44100.0).channels == 1);

    // cancelled: empty
    std::atomic<bool> cancel { true };
    CHECK (analyse (loud.data(), nullptr, frames, 44100.0, &cancel).levels.empty());
}

TEST (wave_disk_cache_round_trip)
{
    using namespace lynx::wave;
    const auto v = waveNoise (0.7f, 77777);
    const auto s = analyse (v.data(), nullptr, v.size(), 44100.0);
    const auto bytes = serialise (s);

    Summary back;
    CHECK (deserialise (bytes.data(), bytes.size(), back));
    CHECK (back.frames == s.frames && back.channels == s.channels && back.bucket == s.bucket && back.sampleRate == s.sampleRate);
    CHECK (back.levels.size() == s.levels.size());
    bool same = true;
    for (std::size_t l = 0; l < s.levels.size(); ++l)
        same = same && std::memcmp (back.levels[l].data(), s.levels[l].data(), s.levels[l].size() * sizeof (Column)) == 0;
    CHECK (same);
    const auto a = s.query (100.0, 30000.0, 0), b = back.query (100.0, 30000.0, 0);
    CHECK (a.hi == b.hi && a.rms == b.rms && a.high == b.high);

    // damaged files are rejected (truncated, extra bytes, other version, wrong magic)
    Summary junk;
    CHECK (! deserialise (bytes.data(), bytes.size() - 1, junk));
    auto longer = bytes;
    longer.push_back (0);
    CHECK (! deserialise (longer.data(), longer.size(), junk));
    auto other = bytes;
    other[4] ^= 0xff;
    CHECK (! deserialise (other.data(), other.size(), junk));
    other = bytes;
    other[0] ^= 0xff;
    CHECK (! deserialise (other.data(), other.size(), junk));
    CHECK (! deserialise (nullptr, 0, junk));
}

// Edison follows a recording with a Builder: the same columns as analysing the whole take, at the cost of each new
// stretch (it used to analyse everything again five times a second, and a long take froze the window)
TEST (wave_builder_matches_analyse_whatever_the_chunks)
{
    using namespace lynx::wave;
    const auto frames = (std::size_t) 200003;
    const auto l = waveNoise (0.7f, frames);
    auto r = waveSine (3000.0, 0.4f, frames, 44100.0);
    for (std::size_t i = 0; i < frames; i += 7)
        r[i] += 0.1f * l[i];
    const auto whole = analyse (l.data(), r.data(), frames, 44100.0);

    Builder b (44100.0, 2);
    std::size_t at = 0, step = 1;
    while (at < frames)
    {
        const auto n = std::min (frames - at, step);
        b.append (l.data() + at, r.data() + at, n);
        at += n;
        step = step * 3 + 17; // 1, 20, 77, 248, ... (chunks across column boundaries, and ones much longer)
        if (step > 40000)
            step = 5;
    }
    const auto& s = b.summary();
    CHECK (s.frames == whole.frames && s.channels == 2 && s.bucket == whole.bucket && s.scale == whole.scale);
    CHECK (std::abs (s.peak - whole.peak) < 1e-6f);
    CHECK (s.levels.size() == whole.levels.size());
    bool same = s.levels.size() == whole.levels.size();
    for (std::size_t lv = 0; same && lv < s.levels.size(); ++lv)
        same = s.levels[lv].size() == whole.levels[lv].size()
               && std::memcmp (s.levels[lv].data(), whole.levels[lv].data(), s.levels[lv].size() * sizeof (Column)) == 0;
    CHECK (same);

    // between appends it is a summary of what is in so far (the open column included)
    Builder partial (44100.0, 1);
    partial.append (l.data(), nullptr, 1000);
    const auto upTo = analyse (l.data(), nullptr, 1000, 44100.0);
    CHECK (partial.frames() == 1000 && partial.summary().levels[0].size() == upTo.levels[0].size());
    CHECK (std::memcmp (partial.summary().levels[0].data(), upTo.levels[0].data(), upTo.levels[0].size() * sizeof (Column)) == 0);
    CHECK (partial.summary().levels.back().size() == 1);
    CHECK (! Builder (44100.0, 1).summary().query (0.0, 10.0, -1).valid); // (nothing yet)
}

TEST (wave_builder_grows_its_bucket_and_scale)
{
    using namespace lynx::wave;
    // level 0 full (a small limit here; maxBaseColumns in the app): two columns merge, the bucket doubles, the
    // peaks stay exact
    const auto frames = (std::size_t) 20603;
    const auto v = waveNoise (0.9f, frames);
    Builder b (48000.0, 1, 64);
    for (std::size_t at = 0; at < frames; at += 333)
        b.append (v.data() + at, nullptr, std::min<std::size_t> (333, frames - at));
    const auto& s = b.summary();
    CHECK (s.bucket == 512 && s.levels[0].size() == (frames + 511) / 512 && s.frames == frames);
    bool peaks = true, rms = true;
    for (std::size_t c = 0; c < s.levels[0].size(); ++c)
    {
        const auto from = c * s.bucket, to = std::min (frames, from + s.bucket);
        float lo = v[from], hi = v[from];
        double sq = 0.0;
        for (auto i = from; i < to; ++i)
        {
            lo = std::min (lo, v[i]);
            hi = std::max (hi, v[i]);
            sq += (double) v[i] * v[i];
        }
        const auto& col = s.levels[0][c];
        peaks = peaks && std::abs (col.lo[0] / 32767.0f - lo) < 1e-4f && std::abs (col.hi[0] / 32767.0f - hi) < 1e-4f;
        rms = rms && std::abs (col.rms[0] / 65535.0 - std::sqrt (sq / (double) (to - from))) < 2e-3;
    }
    CHECK (peaks);
    CHECK (rms);
    CHECK (s.levels.back().size() == 1);
    for (std::size_t lv = 1; lv < s.levels.size(); ++lv)
        CHECK (s.levels[lv].size() == (s.levels[lv - 1].size() + levelFactor - 1) / levelFactor);

    // a float input past full scale: the scale follows, what came before keeps its level
    auto quiet = waveSine (200.0, 0.5f, 30000, 48000.0);
    const auto loud = waveSine (200.0, 2.0f, 30000, 48000.0);
    Builder f (48000.0, 1);
    f.append (quiet.data(), nullptr, quiet.size());
    CHECK (f.summary().scale == 1.0f);
    f.append (loud.data(), nullptr, loud.size());
    CHECK (std::abs (f.summary().scale - 2.0f) < 0.01f);
    // (ranges well inside each part: a wide query reads coarser columns, which may reach into the other part)
    const auto q = f.summary().query (5000.0, 6000.0, 0), w = f.summary().query (40000.0, 41000.0, 0);
    CHECK (std::abs (q.hi - 0.5f) < 0.01f && std::abs (q.lo + 0.5f) < 0.01f);
    CHECK (std::abs (w.hi - 2.0f) < 0.02f && std::abs (w.lo + 2.0f) < 0.02f);
}
