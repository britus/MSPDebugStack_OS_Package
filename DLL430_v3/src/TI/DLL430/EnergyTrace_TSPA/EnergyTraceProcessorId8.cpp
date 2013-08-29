/*
 * EnergyTraceProcessorID8.cpp
 *
 * Process incoming serial data into EnergyTrace records
 *
 * Copyright (c) 2007 - 2013 Texas Instruments Incorporated - http://www.ti.com/
 *
 * All rights reserved not granted herein.
 * Limited License.
 *
 * Texas Instruments Incorporated grants a world-wide, royalty-free,
 * non-exclusive license under copyrights and patents it now or hereafter
 * owns or controls to make, have made, use, import, offer to sell and sell ("Utilize")
 * this software subject to the terms herein.  With respect to the foregoing patent
 * license, such license is granted  solely to the extent that any such patent is necessary
 * to Utilize the software alone.  The patent license shall not apply to any combinations which
 * include this software, other than combinations with devices manufactured by or for TI (�TI Devices�).
 * No hardware patent is licensed hereunder.
 *
 * Redistributions must preserve existing copyright notices and reproduce this license (including the
 * above copyright notice and the disclaimer and (if applicable) source code license limitations below)
 * in the documentation and/or other materials provided with the distribution
 *
 * Redistribution and use in binary form, without modification, are permitted provided that the following
 * conditions are met:
 *
 *	* No reverse engineering, decompilation, or disassembly of this software is permitted with respect to any
 *     software provided in binary form.
 *	* any redistribution and use are licensed by TI for use only with TI Devices.
 *	* Nothing shall obligate TI to provide you with source code for the software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution of the source code are permitted
 * provided that the following conditions are met:
 *
 *   * any redistribution and use of the source code, including any resulting derivative works, are licensed by
 *     TI for use only with TI Devices.
 *   * any redistribution and use of any object code compiled from the source code and any resulting derivative
 *     works, are licensed by TI for use only with TI Devices.
 *
 * Neither the name of Texas Instruments Incorporated nor the names of its suppliers may be used to endorse or
 * promote products derived from this software without specific prior written permission.
 *
 * DISCLAIMER.
 *
 * THIS SOFTWARE IS PROVIDED BY TI AND TI�S LICENSORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TI AND TI�S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <EnergyTraceProcessorId8.h>
#include "EnergyTraceLowPassFilter.h"
#include "EnergyTraceRunningAverageFilter.h"


using namespace TI::DLL430;
using namespace std;

EnergyTraceProcessorId8::EnergyTraceProcessorId8(size_t dataSize)
	: mBuffer(dataSize)
	,mPrevCurrent(0)
	,mEnergyMicroWsec(0)
{
	Reset();
}

EnergyTraceProcessorId8::~EnergyTraceProcessorId8()
{}

void EnergyTraceProcessorId8::Reset(void)
{
    mBuffer.Reset();
    mFilter->Reset();
	mVoutFilter->Reset();
	mAccumulatedT = 0;
	mAccumulatedN = 0;
	mCurrent = 0;
	mCurrentValid = false;
	mEnergyMicroWsec = 0;
	mEnergyMicroWsecAdder = 0;
	mPrevTimeTag = 0;
	mAccumulatedNDiv =1;

	mSkip = SKIP_COUNTER;
}

bool EnergyTraceProcessorId8::AddData(void *data, size_t size)
{
	bool retVal = false;
	// Process the stream and store it in the buffer
	EnergyTraceRecordEt8_t *pRecord = (EnergyTraceRecordEt8_t *)data;

	uint32_t numEntries = size/sizeof(*pRecord);
	while (numEntries--)
	{
		// ----------------------------------------------------
		// Discard first samples
		if (mSkip > 0)
		{
			--mSkip;
			mPrevCurrentTick = pRecord->currentTicks;
			mPrevTimeTag = pRecord->TimeStamp;
			mVoutFilter->Reset();
			mTimeTag_us = 0;
			mCurrent = 0;
			if (mSkip == 0)
			{
				mCurrentValid = true;
			}
			mAccumulatedT = 0;
			mAccumulatedN = 0;
			mEnergyMicroWsec = 0;
			mEnergyMicroWsecAdder = 0;
			mAccumulatedNDiv = 1;
			return false;
		}
		EnergyRecordEt8 newRecord;

		// ----------------------------------------------------------------------------------
		// Calculate timestamp in usec and passed time between this and last sample
		double deltaT = (double)(pRecord->TimeStamp - mPrevTimeTag) * TIME_BASE_ns / 1000.00; // Convert to micro-seconds

		uint32_t deltaN = pRecord->currentTicks - mPrevCurrentTick;

		if(pRecord->TimeStamp  < mPrevTimeTag)
		{
			return false;
		}

		if(pRecord->currentTicks  < mPrevCurrentTick)
		{
			return false;
		}
		//------- Voltage measurement
		uint32_t VOut = pRecord->voltage;

		// ----------------------------------------------------------------------------------
		// Average voltage
		if(mFilterEnable)
		{
			mVoutFilter->AddData(&VOut, sizeof(VOut));
			newRecord.voltage = *((uint32_t *)mVoutFilter->GetReadBufferPtr());
		}
		else
		{
			newRecord.voltage = VOut;
		}

		//------- Current calculation-----------------------------------------------------------
		// Setup integrators
		double mAccumulatedTMin = 1000;
		double mAccumulatedNMin = (double)tickThreshold/mAccumulatedNDiv;

		// Add up samples-------------------------------------------------------------------------------
		mAccumulatedT += deltaT;
		mAccumulatedN += deltaN;

		if (mCurrentValid || ((mAccumulatedT > mAccumulatedTMin) && (mAccumulatedN > mAccumulatedNMin))) // Calculate sample every 1000usec, need to have at least 1 tick
		{
			double normalizedN = (double)mAccumulatedN / mAccumulatedT * 1000.00; // Normalized ticks per 1msec
			if (normalizedN < mCalibrationValues[0].threshold)
			{
				mCurrent = 0; // Clamp negative values to 0uA -- avoid undershoot
			}
			else
			{
				mCurrent = (uint32_t)((normalizedN - mCalibrationValues[0].offset) / mCalibrationValues[0].gradient);
			}
			//------- Current calculation-----------------------------------------------------------
			if ((double)deltaN/deltaT*1000 > mCalibrationValues[1].threshold/5) // High current (larger than 1.6mA/5) -- average less
			{
					if (mAccumulatedNDiv < ACCUMULATED_N_DIV_MAX)
					{
						mAccumulatedNDiv += ACCUMULATED_N_DIV_STEP_SIZE;
					}
			}
			else // Low current -- average more
			{
					if (mAccumulatedNDiv > ACCUMULATED_N_DIV_MIN)
					{
						mAccumulatedNDiv -= ACCUMULATED_N_DIV_STEP_SIZE;
					}
			}
			if (mAccumulatedNMin<1)
			{
				mAccumulatedNMin = 1;
			}

			mAccumulatedT = 0;
			mAccumulatedN = 0;
			mCurrentValid = false;
		}

		// ----------------------------------------------------------------------------------
		// Round current to next 100nA
		mCurrent = ((mCurrent + 50) / 100) * 100;

		// ----------------------------------------------------------------------------------
		// Assemble header
		mTimeTag_us += deltaT; // Time tag is in micro-seconds
		newRecord.header = ((uint64_t)mTimeTag_us << 8) + ((uint64_t)pRecord->eventID & 0xFF);

		// ----------------------------------------------------------------------------------
		// Calculate total energy in 0.1uWsec
		mEnergyMicroWsecAdder += (double)deltaN - ((double)mCalibrationValues[0].threshold*(deltaT/1000));
		if (mEnergyMicroWsecAdder > 0) // Only increase energy
		{
			mEnergyMicroWsec += mEnergyMicroWsecAdder * oneTickinMicroWsec * 10;
			mEnergyMicroWsecAdder = 0;
		}
		newRecord.energy = (uint32_t)mEnergyMicroWsec;

		newRecord.current = mCurrent;
		retVal |= mBuffer.AddData(newRecord); // Add the current record to the double buffer

		// Store the current state
		mPrevTimeTag = pRecord->TimeStamp;
		mPrevCurrentTick = pRecord->currentTicks;
		pRecord++;
	}
	return retVal;
}


//---------------------------------
void* EnergyTraceProcessorId8::GetReadBufferPtr(void)
{
	return mBuffer.GetReadBufferPtr();
}

//---------------------------------
size_t EnergyTraceProcessorId8::GetReadBufferSize(void)
{
	return mBuffer.GetBufferSize() * sizeof(*mBuffer.GetReadBufferPtr());
}
