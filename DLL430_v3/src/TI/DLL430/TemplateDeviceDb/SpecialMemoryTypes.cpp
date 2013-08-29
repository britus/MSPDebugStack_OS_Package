/*
 * SpecialMemoryTypes.cpp
 *
 * Memory Types whith special handling. 
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                                                                                                                                                                                                                                                                         
 */

#include "SpecialMemoryTypes.h"
#include "DeviceHandleV3.h"
#include "ConfigManagerV3.h"
#include "FetHandleV3.h"
#include "MemoryManagerV3.h"

namespace TI { namespace DLL430 { namespace TemplateDeviceDb {
namespace Memory {
		
	bool InformationFlashAccess::erase() 
	{
		for ( uint32_t i = 0; i < getBanks(); ++i )
		{
			if ( i == getBanks()-1 && isLocked() )
				continue;

			const uint32_t offset = getStart() + i*getSegmentSize();		
			if ( !MainMemoryAccessBase::erase(offset, offset + getSegmentSize()-1) )
				return false;
		}
		return true;
	}



	bool BslMemoryAccessBase::doRead(uint32_t address, uint32_t* buffer, size_t count)
	{
		//Nothing to do, keep vacant memory pattern
		if (isLocked())
		{
			return true;
		}

		using namespace std;

		assert(NULL != buffer);
		bool success = true;

		const uint32_t lockedStartAddress = getLockedStartAddress();
		const size_t unlockedCount  = min<size_t>(
										max<int32_t>(0, lockedStartAddress - address),
										count);
		const size_t lockedCount = count - unlockedCount;

		if (unlockedCount > 0)
		{
			success = memoryAccess->doRead(address, buffer, unlockedCount);
			address += static_cast<uint32_t>(unlockedCount);
			buffer += unlockedCount;
		}

		if (lockedCount > 0 && success)
		{
			physicallyLocked = !doUnlockBslMemory();

			if(!physicallyLocked)
			{
				success = memoryAccess->doRead(address, buffer , lockedCount);
			}
		}
		
		return success;
	}

	bool BslMemoryAccessBase::doWrite(uint32_t address, uint32_t* buffer, size_t count)
	{
		if (isLocked())
		{
			err = MEMORY_LOCKED_ERROR;
			return false;
		}

		physicallyLocked = !doUnlockBslMemory();
		
		if (!physicallyLocked)
		{
			return memoryAccess->doWrite(address, buffer, count);
		}

		err = MEMORY_UNLOCK_ERROR;
		return false;
	}

	bool BslMemoryAccessBase::doWrite (uint32_t address, uint32_t value)
	{
		return doWrite(address, &value, 1);
	}

	//erase segment wise
	bool BslMemoryAccessBase::erase() 
	{
		return erase(getStart(), getEnd());
	}

	bool BslMemoryAccessBase::erase(uint32_t start, uint32_t end)
	{
		if (isLocked())
		{
			err = MEMORY_LOCKED_ERROR;
			return true;
		}

		physicallyLocked = !doUnlockBslMemory();

		if (!physicallyLocked)
		{
			return memoryAccess->erase(start, end);
		}

		err = MEMORY_UNLOCK_ERROR;
		return false;
	}

	bool BslMemoryAccessBase::doUnlockBslMemory()
	{
		bool success = false;
			
		std::vector<uint32_t> bslPe;
		const bool readBslPeSuccess = readBslPe(&bslPe);
		const bool deviceLocked = readBslPeSuccess && isDeviceLocked(bslPe);
		
		// get first the status of the memory area -LOCKED/UNLOCKED
		if(!deviceLocked)
		{
			success = true;
		}
		else
		{
			const unsigned short sysBslDefaultSize = 0x03;
			success = unlockBslPeAndCheck(sysBslDefaultSize);
		}
		return success;
	}

	bool BslMemoryAccessBase::readBslPe(std::vector<uint32_t>* bslPeBuffer) const 
	{
		const unsigned short mySysBslc = 0x182;

		bslPeBuffer->resize(2);
		const bool readBslPeSuccess = mm->read(mySysBslc, &(*bslPeBuffer)[0], 2);
		const bool sendCommandSuccess = readBslPeSuccess && mm->sync();
		
		return sendCommandSuccess;
	}
		
	bool BslMemoryAccessBase::unlockBslPeAndCheck(uint32_t bslSize) 
	{
		std::vector<uint32_t> bslPeBuffer;
		bslPeBuffer.reserve(2);
		bslPeBuffer.push_back(bslSize);
		bslPeBuffer.push_back(0x00);

		const bool writeBslPeSuccess = mm->write(mySysBslc, &bslPeBuffer[0], mySysBslcSize);
		const bool sendCommandSuccess = writeBslPeSuccess && mm->sync();
		const bool readBackSuccess = sendCommandSuccess && readBslPe(&bslPeBuffer);
		const bool correctBslSize = (bslPeBuffer.size() == mySysBslcSize) && bslPeBuffer[0] == bslSize;
		return readBackSuccess && !isDeviceLocked(bslPeBuffer) && correctBslSize;
	}

	bool BslMemoryAccessBase::isDeviceLocked(const std::vector<uint32_t>& bslPeBuffer) const
	{
		if(bslPeBuffer.size() == mySysBslcSize)
		{
			//it's sufficient to check one byte
			const unsigned int sysBslPe = 0x80;
			return bslPeBuffer[1] == sysBslPe;
		}
		return false;
	}

	uint32_t BslMemoryAccessBase::getLockedStartAddress() const
	{	
		const uint32_t maxBslSize = 0x03;
		std::vector<uint32_t> lockStartAddr;

		const bool readBslPeSuccess = readBslPe(&lockStartAddr);
		const bool bslSizeValid = lockStartAddr[0] <= maxBslSize;
		
		if ( !isDeviceLocked(lockStartAddr) )
		{
			return getSize();
		}
		else if ( bslSizeValid && readBslPeSuccess )
		{
			return (maxBslSize - lockStartAddr[0]) * getSegmentSize(); 
		}
		return 0;
	}


	bool BootcodeRomAccess::doRead (uint32_t address, uint32_t* buffer, size_t count)
	{
		bool omitFirst = (address & 0x1);
		if (omitFirst) 
		{
			--address;
			++count;
		}
		bool omitLast = (count & 1);
		if (omitLast) 
		{
			++count;
		}	

		HalExecElement* el = new HalExecElement(this->devHandle->checkHalId(ID_ReadMemQuick));
		el->appendInputData32(this->getStart() + address);
		el->appendInputData32(static_cast<uint32_t>(count/2));
		el->setOutputSize(count);

		ReadElement r(buffer, count, omitFirst, omitLast, 0);
		this->readMap[this->elements.size()] = r;
		this->elements.push_back(el);
		return true;
	}
}//namespace Memory
}//namespace TemplateDeviceDb
}//namespace DLL430
}//namespace TI
