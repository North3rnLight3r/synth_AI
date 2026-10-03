#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <mutex>

// Periodic, Fourier-limited tables, constructed off the audio thread.
// X morphs sine -> saw, Y morphs that pair toward triangle -> square.
// This is a deterministic engine. No trained neural model is bundled.
class WavetableBank
{
public:
    static constexpr int size = 2048, levels = 9;
    using Table = std::array<float, size + 1>;
    WavetableBank()
    {
        constexpr double tau = 6.2831853071795864769;
        for (int level = 0; level < levels; ++level)
        {
            for (int i = 0; i < size; ++i)
            {
                const double phase = tau * i / size;
                double saw = 0, square = 0, triangle = 0;
                for (int h = 1; h <= (1 << level); ++h)
                {
                    const double s = std::sin(phase * h);
                    saw += s / h;
                    if ((h & 1) != 0)
                    {
                        square += s / h;
                        triangle += s * ((h % 4 == 1) ? 1.0 : -1.0) / (h * h);
                    }
                }
                tables[0][level][i] = static_cast<float>(std::sin(phase));
                tables[1][level][i] = static_cast<float>(saw * 0.58);
                tables[2][level][i] = static_cast<float>(triangle * 0.81);
                tables[3][level][i] = static_cast<float>(square * 1.08);
            }
            for (auto& corner : tables)
                corner[level][size] = corner[level][0];
        }
    }
    int levelFor(double frequency, double sampleRate) const noexcept
    {
        const double maxHarmonic = sampleRate * 0.45 / std::max(1.0, frequency);
        return std::clamp(static_cast<int>(std::floor(std::log2(std::max(1.0, maxHarmonic)))), 0, levels - 1);
    }
    float sample(double phase, int level, float x, float y) const noexcept
    {
        const auto pos = static_cast<float>(phase * size);
        const auto i = std::clamp(static_cast<int>(pos), 0, size - 1);
        const auto fraction = pos - static_cast<float>(i);
        const auto read = [&](int corner)
        {
            const auto& t = tables[corner][level];
            return t[i] + fraction * (t[i + 1] - t[i]);
        };
        const float bottom = read(0) + x * (read(1) - read(0));
        const float top = read(2) + x * (read(3) - read(2));
        return bottom + y * (top - bottom);
    }
private:
    std::array<std::array<Table, levels>, 4> tables {};
};

// Single UI/file-import producer and audio-thread consumer. A three-buffer
// mailbox transfers ownership without locks or waiting in the audio callback.
class CustomWavetable
{
public:
    using Bank = std::array<WavetableBank::Table, WavetableBank::levels>;
    void publish(const std::array<float, WavetableBank::size>& source)
    {
        const std::lock_guard<std::mutex> writerLock(writerMutex);
        auto& destination = tables[static_cast<size_t>(back)];
        std::array<double, WavetableBank::size / 2 + 1> real {}, imaginary {};
        for (int h = 1; h <= WavetableBank::size / 2; ++h)
        {
            double r = 0, im = 0;
            for (int i = 0; i < WavetableBank::size; ++i)
            {
                const double phase = 6.2831853071795864769 * h * i / WavetableBank::size;
                r += source[static_cast<size_t>(i)] * std::cos(phase);
                im -= source[static_cast<size_t>(i)] * std::sin(phase);
            }
            real[static_cast<size_t>(h)] = 2 * r / WavetableBank::size;
            imaginary[static_cast<size_t>(h)] = 2 * im / WavetableBank::size;
        }
        for (int level = 0; level < WavetableBank::levels; ++level)
        {
            for (int i = 0; i < WavetableBank::size; ++i)
            {
                double value = 0;
                for (int h = 1; h <= (1 << level); ++h)
                {
                    const double phase = 6.2831853071795864769 * h * i / WavetableBank::size;
                    value += real[static_cast<size_t>(h)] * std::cos(phase) - imaginary[static_cast<size_t>(h)] * std::sin(phase);
                }
                destination[static_cast<size_t>(level)][static_cast<size_t>(i)] = static_cast<float>(value);
            }
            destination[static_cast<size_t>(level)][WavetableBank::size] = destination[static_cast<size_t>(level)][0];
        }
        const int published = back;
        back = middle.exchange(published | ready, std::memory_order_acq_rel) & indexMask;
    }
    void consume() noexcept
    {
        const int published = middle.load(std::memory_order_acquire);
        if ((published & ready) != 0) front = middle.exchange(front, std::memory_order_acq_rel) & indexMask;
    }
    float sample(double phase, int level) const noexcept
    {
        const auto& table = tables[static_cast<size_t>(front)][static_cast<size_t>(level)];
        const float position = static_cast<float>(phase * WavetableBank::size);
        const auto index = static_cast<size_t>(std::clamp(static_cast<int>(position), 0, WavetableBank::size - 1));
        const float fraction = position - static_cast<float>(index);
        return table[index] + fraction * (table[index + 1] - table[index]);
    }
private:
    static constexpr int ready = 4, indexMask = 3;
    std::mutex writerMutex;
    std::array<Bank, 3> tables {};
    std::atomic<int> middle {1};
    int front = 0, back = 2;
};
