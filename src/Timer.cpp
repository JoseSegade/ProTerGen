#include "Timer.h"
#include <stdio.h>

LARGE_INTEGER ProTerGen::Timer::mFrecuency;
LARGE_INTEGER ProTerGen::Timer::mStartTime[ProTerGen::MAX_TIMER_NUM];
uint32_t      ProTerGen::Timer::mTimesExecuted[ProTerGen::MAX_TIMER_NUM];
double        ProTerGen::Timer::mLastTimeRequested[ProTerGen::MAX_TIMER_NUM];
double        ProTerGen::Timer::mTimeAcc[ProTerGen::MAX_TIMER_NUM];
uint8_t       ProTerGen::Timer::mEventCount = (Timer::TimerEvent)Timer::Events::COUNT;

void ProTerGen::Timer::AskForFrecuency()
{
	QueryPerformanceFrequency(&mFrecuency);
}

void ProTerGen::Timer::SetEventOnZero(TimerEvent e)
{
	mTimesExecuted[(size_t)mEventCount]     = 0;
	mLastTimeRequested[(size_t)mEventCount] = 0;
	mTimeAcc[(size_t)mEventCount]           = 0;
}

ProTerGen::Timer::TimerEvent ProTerGen::Timer::StartEvent()
{
	if (((size_t)mEventCount + 1) > MAX_TIMER_NUM)
	{
		printf("Already reached the maximun num of timer events.\n");
		throw - 1;
	}
	mTimesExecuted[(size_t)mEventCount]     = 0;
	mLastTimeRequested[(size_t)mEventCount] = 0;
	mTimeAcc[(size_t)mEventCount]           = 0;
	QueryPerformanceFrequency(&mFrecuency);
	QueryPerformanceCounter(&mStartTime[mEventCount]);
	return mEventCount++;
}

void ProTerGen::Timer::RestartEvent(TimerEvent e)
{
	mTimesExecuted[(size_t)e]     = 0;
	mLastTimeRequested[(size_t)e] = 0;
	mTimeAcc[(size_t)e]           = 0;
	QueryPerformanceCounter(&mStartTime[(size_t)e]);
}

double ProTerGen::Timer::TimeInMiliseconds(TimerEvent e)
{
	LARGE_INTEGER end{};
	QueryPerformanceCounter(&end);
	mLastTimeRequested[(size_t)e] = (1000.0 * (double)(end.QuadPart - mStartTime[(size_t)e].QuadPart)) / (double)mFrecuency.QuadPart;
	return mLastTimeRequested[(size_t)e];
}

void ProTerGen::Timer::RestartEventMean(TimerEvent e)
{
	QueryPerformanceCounter(&mStartTime[(size_t)e]);
}

double ProTerGen::Timer::TimeInMilisecondsMean(TimerEvent e)
{
	mTimeAcc[(size_t)e]           += TimeInMiliseconds(e);
	const double n                 = (double)++mTimesExecuted[(size_t)e];
	mLastTimeRequested[(size_t)e]  = mTimeAcc[(size_t)e] / n;
	return mLastTimeRequested[(size_t)e];
}

double ProTerGen::Timer::GetLastTimeRequestedTime(TimerEvent e)
{
	return mLastTimeRequested[(size_t)e];
}
