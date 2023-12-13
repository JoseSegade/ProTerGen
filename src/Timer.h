#pragma once

#include <stdint.h>
#include <windows.h>

namespace ProTerGen
{
	const size_t MAX_TIMER_NUM = 32;
	class Timer
	{
	public:
		using TimerEvent = uint8_t;
		enum class Events : TimerEvent
		{
			TOTAL_INIT = 0,
			WINDOW_CREATION,
			DIRECTX_INIT,
			TEXTURE_INIT,
			VIRTUAL_CACHE_INIT,
			PIPELINE_INIT,
			GEOMETRY_INIT,
			MAIN_LOOP,
			UPDATE_LOOP,
			DRAW_LOOP,
			PAGE_GEN,
			INDIRECTION_UPDATE,
			TERRAIN_QT,
			TERRAIN_MESH,
			COUNT
		};

		static void       AskForFrecuency();
		static void       SetEventOnZero(TimerEvent e);
		static TimerEvent StartEvent();
		static void       RestartEvent(TimerEvent e);
		static double     TimeInMiliseconds(TimerEvent e);
		static void       RestartEventMean(TimerEvent e);
		static double     TimeInMilisecondsMean(TimerEvent e);
		static double     GetLastTimeRequestedTime(TimerEvent e);
	protected:
		static LARGE_INTEGER mFrecuency;
		static LARGE_INTEGER mStartTime[MAX_TIMER_NUM];
		static uint32_t      mTimesExecuted[MAX_TIMER_NUM];
		static double        mTimeAcc[MAX_TIMER_NUM];
		static double        mLastTimeRequested[MAX_TIMER_NUM];
		static uint8_t       mEventCount;
	};
}
