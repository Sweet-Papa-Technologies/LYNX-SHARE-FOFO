#pragma once

// Audio tap for the audio editor's recording (round 86): the audio thread pushes the frames of one chosen source
// (the sound card input, the Master or a mixer insert) into a lock-free single-producer/single-consumer ring; the
// UI pulls them on a timer. Nothing is pushed while no source is chosen, so it costs one atomic load per block.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace lynx
{
class AudioCapture
{
public:
    static constexpr int off = -1;
    static constexpr int input = -2; // the sound card input; 0 = Master, n = mixer insert n
    static constexpr int maxStreams = 8;
    static constexpr std::size_t capacity = std::size_t (1) << 18; // ~5.5 s at 48 kHz
    static constexpr std::size_t maxWraps = 256;

    AudioCapture() = default;

    // UI: allocate / release a dedicated capture stream for an editor instance (0 is the default shared stream).
    int acquireStream() noexcept
    {
        for (int s = 1; s < maxStreams; ++s)
        {
            bool expected = false;
            if (streams[(std::size_t) s].inUse.compare_exchange_strong (expected, true, std::memory_order_acq_rel))
            {
                streams[(std::size_t) s].setSource (off);
                return s;
            }
        }
        return 0;
    }

    void releaseStream (int stream) noexcept
    {
        if (stream >= 0 && stream < maxStreams)
        {
            streams[(std::size_t) stream].setSource (off);
            if (stream > 0)
                streams[(std::size_t) stream].inUse.store (false, std::memory_order_release);
        }
    }

    // UI: what to capture (off stops). Choosing a source drops whatever was left in the ring.
    void setSource (int stream, int newSource) noexcept
    {
        if (stream >= 0 && stream < maxStreams)
            streams[(std::size_t) stream].setSource (newSource);
    }
    void setSource (int newSource) noexcept
    {
        streams[0].setSource (newSource);
        if (newSource == off)
            for (int s = 1; s < maxStreams; ++s)
                if (! streams[(std::size_t) s].inUse.load (std::memory_order_relaxed))
                    streams[(std::size_t) s].setSource (off);
    }

    int getSource (int stream = 0) const noexcept
    {
        return (stream >= 0 && stream < maxStreams) ? streams[(std::size_t) stream].source.load (std::memory_order_relaxed) : off;
    }

    bool isCapturing (int from) const noexcept
    {
        for (const auto& st : streams)
            if (st.source.load (std::memory_order_relaxed) == from)
                return true;
        return false;
    }

    bool isAnyActive() const noexcept
    {
        for (const auto& st : streams)
            if (st.source.load (std::memory_order_relaxed) != off)
                return true;
        return false;
    }

    // audio thread. `r` may equal `l` (mono).
    void push (int from, const float* l, const float* r, int n) noexcept
    {
        if (l == nullptr || n <= 0)
            return;
        for (auto& st : streams)
            st.push (from, l, r, n);
    }

    // audio thread: a silent block (the source was inactive), so the recording keeps its timing
    void pushSilence (int from, int n) noexcept
    {
        if (! isCapturing (from) || n <= 0)
            return;
        static constexpr std::array<float, 256> zeros {};
        for (int done = 0; done < n; done += (int) zeros.size())
            push (from, zeros.data(), zeros.data(), std::min (n - done, (int) zeros.size()));
    }

    // audio thread: marks a transport loop wrap at the current write position (lock-free, no allocation)
    void markLoopWrap() noexcept
    {
        for (auto& st : streams)
            st.markLoopWrap();
    }

    // UI: appends what arrived since the last call. Returns the number of frames appended.
    // When `loopWraps` is given, appends the sample index in `l` (after this call) of every loop wrap that occurred
    // within the pulled frames.
    int pull (int stream, std::vector<float>& l, std::vector<float>& r, std::vector<std::int64_t>* loopWraps = nullptr)
    {
        if (stream < 0 || stream >= maxStreams)
            return 0;
        return streams[(std::size_t) stream].pull (l, r, loopWraps);
    }
    int pull (std::vector<float>& l, std::vector<float>& r, std::vector<std::int64_t>* loopWraps = nullptr)
    {
        return pull (0, l, r, loopWraps);
    }

    // frames lost because the UI did not pull in time (since the source was chosen)
    int droppedFrames (int stream = 0) const noexcept
    {
        return (stream >= 0 && stream < maxStreams) ? streams[(std::size_t) stream].dropped.load (std::memory_order_relaxed) : 0;
    }

private:
    static constexpr std::uint64_t mask = capacity - 1;
    static constexpr std::uint64_t wrapMask = maxWraps - 1;

    struct Stream
    {
        Stream() : left (capacity), right (capacity) {}

        void setSource (int newSource) noexcept
        {
            source.store (off, std::memory_order_release);
            readIndex.store (writeIndex.load (std::memory_order_acquire), std::memory_order_release);
            wrapRead.store (wrapWrite.load (std::memory_order_acquire), std::memory_order_release);
            dropped.store (0, std::memory_order_relaxed);
            source.store (newSource, std::memory_order_release);
        }

        void push (int from, const float* l, const float* r, int n) noexcept
        {
            if (source.load (std::memory_order_relaxed) != from)
                return;
            if (r == nullptr)
                r = l;
            const auto w = writeIndex.load (std::memory_order_relaxed);
            const auto rd = readIndex.load (std::memory_order_acquire);
            const auto space = (std::int64_t) capacity - (std::int64_t) (w - rd);
            const auto count = (int) std::min<std::int64_t> (n, space);
            for (int i = 0; i < count; ++i)
            {
                left[(std::size_t) ((w + (std::uint64_t) i) & mask)] = l[i];
                right[(std::size_t) ((w + (std::uint64_t) i) & mask)] = r[i];
            }
            writeIndex.store (w + (std::uint64_t) count, std::memory_order_release);
            if (count < n)
                dropped.fetch_add (n - count, std::memory_order_relaxed);
        }

        void markLoopWrap() noexcept
        {
            if (source.load (std::memory_order_relaxed) == off)
                return;
            const auto ww = wrapWrite.load (std::memory_order_relaxed);
            const auto wr = wrapRead.load (std::memory_order_acquire);
            if (ww - wr < maxWraps)
            {
                wraps[(std::size_t) (ww & wrapMask)].store (writeIndex.load (std::memory_order_relaxed), std::memory_order_relaxed);
                wrapWrite.store (ww + 1, std::memory_order_release);
            }
        }

        int pull (std::vector<float>& l, std::vector<float>& r, std::vector<std::int64_t>* loopWraps)
        {
            const auto w = writeIndex.load (std::memory_order_acquire);
            const auto rd = readIndex.load (std::memory_order_relaxed);
            const auto base = (std::int64_t) l.size();
            const auto n = (int) (w - rd);
            for (int i = 0; i < n; ++i)
            {
                l.push_back (left[(std::size_t) ((rd + (std::uint64_t) i) & mask)]);
                r.push_back (right[(std::size_t) ((rd + (std::uint64_t) i) & mask)]);
            }
            const auto ww = wrapWrite.load (std::memory_order_acquire);
            auto wr = wrapRead.load (std::memory_order_relaxed);
            for (; wr != ww; ++wr)
            {
                const auto pos = wraps[(std::size_t) (wr & wrapMask)].load (std::memory_order_relaxed);
                if (pos > w)
                    break;
                if (loopWraps != nullptr && pos >= rd)
                    loopWraps->push_back (base + (std::int64_t) (pos - rd));
            }
            wrapRead.store (wr, std::memory_order_release);
            readIndex.store (w, std::memory_order_release);
            return n;
        }

        std::vector<float> left, right;
        std::atomic<int> source { off };
        std::atomic<bool> inUse { false };
        std::atomic<std::uint64_t> writeIndex { 0 }, readIndex { 0 };
        std::array<std::atomic<std::uint64_t>, maxWraps> wraps {};
        std::atomic<std::uint64_t> wrapWrite { 0 }, wrapRead { 0 };
        std::atomic<int> dropped { 0 };
    };

    std::array<Stream, maxStreams> streams {};
};
} // namespace lynx
