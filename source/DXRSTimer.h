#pragma once

#include "Common.h"

#include <cmath>
#include <exception>
#include <stdint.h>

class DXRSTimer
{
public:
    DXRSTimer()
        :
        mElapsedTicks(0),
        mTotalTicks(0),
        mLeftOverTicks(0),
        mFrameCount(0),
        mFramesPerSecond(0),
        mFramesThisSecond(0),
        mQPCSecondCounter(0),
        mIsFixedTimeStep(false),
        mTargetElapsedTicks(TicksPerSecond / 60)
    {
        if (!QueryPerformanceFrequency(&mQPCFrequency))
            throw std::exception("QueryPerformanceFrequency");

        if (!QueryPerformanceCounter(&mQPCLastTime))
            throw std::exception("QueryPerformanceCounter");

        mQPCMaxDelta = static_cast<uint64_t>(mQPCFrequency.QuadPart / 10);
    }

    uint64_t GetElapsedTicks() const { return mElapsedTicks; }
    double GetElapsedSeconds() const { return TicksToSeconds(mElapsedTicks); }
    uint64_t GetTotalTicks() const { return mTotalTicks; }
    double GetTotalSeconds() const { return TicksToSeconds(mTotalTicks); }
    uint32_t GetFrameCount() const { return mFrameCount; }
    uint32_t GetFramesPerSecond() const { return mFramesPerSecond; }

    void SetFixedTimeStep(bool isFixedTimestep) { mIsFixedTimeStep = isFixedTimestep; }
    void SetTargetElapsedTicks(uint64_t targetElapsed) { mTargetElapsedTicks = targetElapsed; }
    void SetTargetElapsedSeconds(double targetElapsed) { mTargetElapsedTicks = SecondsToTicks(targetElapsed); }

    static const uint64_t TicksPerSecond = 10000000;

    static double TicksToSeconds(uint64_t ticks) { return static_cast<double>(ticks) / TicksPerSecond; }
    static uint64_t SecondsToTicks(double seconds) { return static_cast<uint64_t>(seconds * TicksPerSecond); }

    void ResetElapsedTime()
    {
        if (!QueryPerformanceCounter(&mQPCLastTime))
        {
            throw std::exception("QueryPerformanceCounter");
        }

        mLeftOverTicks = 0;
        mFramesPerSecond = 0;
        mFramesThisSecond = 0;
        mQPCSecondCounter = 0;
    }

    template<typename TUpdate>
    void Run(const TUpdate& update)
    {
        // Query the current time.
        LARGE_INTEGER currentTime;

        if (!QueryPerformanceCounter(&currentTime))
        {
            throw std::exception("QueryPerformanceCounter");
        }

        uint64_t timeDelta = static_cast<uint64_t>(currentTime.QuadPart - mQPCLastTime.QuadPart);

        mQPCLastTime = currentTime;
        mQPCSecondCounter += timeDelta;

        // Clamp excessively large time deltas (e.g. after paused in the debugger).
        if (timeDelta > mQPCMaxDelta)
        {
            timeDelta = mQPCMaxDelta;
        }

        // Convert QPC units into a canonical tick format. This cannot overflow due to the previous clamp.
        timeDelta *= TicksPerSecond;
        timeDelta /= static_cast<uint64_t>(mQPCFrequency.QuadPart);

        uint32_t lastFrameCount = mFrameCount;

        if (mIsFixedTimeStep)
        {
            // Fixed timestep update logic

            // If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
            // the clock to exactly match the target value. This prevents tiny and irrelevant errors
            // from accumulating over time. Without this clamping, a game that requested a 60 fps
            // fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
            // accumulate enough tiny errors that it would drop a frame. It is better to just round
            // small deviations down to zero to leave things running smoothly.

            if (static_cast<uint64_t>(std::abs(static_cast<int64_t>(timeDelta - mTargetElapsedTicks))) < TicksPerSecond / 4000)
            {
                timeDelta = mTargetElapsedTicks;
            }

            mLeftOverTicks += timeDelta;

            while (mLeftOverTicks >= mTargetElapsedTicks)
            {
                mElapsedTicks = mTargetElapsedTicks;
                mTotalTicks += mTargetElapsedTicks;
                mLeftOverTicks -= mTargetElapsedTicks;
                mFrameCount++;

                update();
            }
        }
        else
        {
            // Variable timestep update logic.
            mElapsedTicks = timeDelta;
            mTotalTicks += timeDelta;
            mLeftOverTicks = 0;
            mFrameCount++;

            update();
        }

        // Track the current framerate.
        if (mFrameCount != lastFrameCount)
        {
            mFramesThisSecond++;
        }

        if (mQPCSecondCounter >= static_cast<uint64_t>(mQPCFrequency.QuadPart))
        {
            mFramesPerSecond = mFramesThisSecond;
            mFramesThisSecond = 0;
            mQPCSecondCounter %= static_cast<uint64_t>(mQPCFrequency.QuadPart);
        }
    }

private:
    // Source timing data uses QPC units.
    LARGE_INTEGER mQPCFrequency;
    LARGE_INTEGER mQPCLastTime;
    uint64_t mQPCMaxDelta;
    uint64_t mQPCSecondCounter;

    uint64_t mElapsedTicks;
    uint64_t mTotalTicks;
    uint64_t mLeftOverTicks;
    uint32_t mFrameCount;
    uint32_t mFramesPerSecond;
    uint32_t mFramesThisSecond;
    uint64_t mTargetElapsedTicks;
    bool mIsFixedTimeStep;
};

