#pragma once

#include "CommonHeaders.h"

namespace ProTerGen
{
   class Clock
   {
   public:
		Clock();

		double TotalTime()const;
		double DeltaTime()const;

		void Reset();
		void Start();
		void Stop(); 
		void Tick(); 

	private:
		double mSecondsPerCount;
		double mDeltaTime;

		int64_t mBaseTime;
		int64_t mPausedTime;
		int64_t mStopTime;
		int64_t mPrevTime;
		int64_t mCurrTime;

		bool mStopped;
   };
}