#include "Clock.h"

ProTerGen::Clock::Clock()
	: mSecondsPerCount(0.0), mDeltaTime(-1.0), mStopTime(0), mBaseTime(0),
	mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
	int64_t countsPerSec = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

double ProTerGen::Clock::TotalTime() const
{
	const int64_t time = mStopped ? mStopTime : mCurrTime;
	return ((time - mPausedTime) - mBaseTime) * mSecondsPerCount;
}

double ProTerGen::Clock::DeltaTime()const
{
	return mDeltaTime;
}

void ProTerGen::Clock::Reset()
{
	int64_t currTime = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void ProTerGen::Clock::Start()
{
	int64_t startTime = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
	if (mStopped)
	{
		mPausedTime += (startTime - mStopTime);

		mPrevTime = startTime;
		mStopTime = 0;
		mStopped = false;
	}
}

void ProTerGen::Clock::Stop()
{
	if (!mStopped)
	{
		int64_t currTime = 0;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped = true;
	}
}

void ProTerGen::Clock::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}

	int64_t currTime = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	mDeltaTime = static_cast<double>((mCurrTime - mPrevTime) * mSecondsPerCount);

	mPrevTime = mCurrTime;

	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}
