/*
 * UpdateManagerEzFet.h
 *
 * Functionality for updating eZ-FET debugger
 *
 * Copyright (C) 2007 - 2011 Texas Instruments Incorporated - http://www.ti.com/ 
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

#if _MSC_VER > 1000
#pragma once
#endif
#ifndef DLL430_UPDATEMANAGEREZFET_H
#define DLL430_UPDATEMANAGEREZFET_H

#include <UpdateManager.h>
#include <stdint.h>

#include "HalExecCommand.h"
#include "FileFuncImpl.h"

namespace TI
{
	namespace DLL430
	{
		class FetHandleV3;
		class FileFuncImpl;
		class ConfigManagerV3;
		class UpdateManagerEzFet : public UpdateManager
		{
		public:
			UpdateManagerEzFet(FetHandleV3*, ConfigManagerV3*, FetHandleManager*);
			~UpdateManagerEzFet();
			
			VersionInfo getHalVersion() const;

			bool isUpdateRequired() const;

			bool firmWareUpdate(const char* fname, UpdateNotifyCallback callback, bool* coreUpdate);

		private:
			uint16_t checkHilVersion() const;
			uint16_t checkDcdcLayerVersion() const;
			uint16_t checkDcdcSubMcuVersion() const;
			uint16_t checkCoreVersion() const;
			uint16_t checkUartVersion() const;

			bool updateFirmware(const FileFuncImpl &firmware);
			bool programmSubMcu(DeviceHandle * singleDevice);

			FetHandleV3* fetHandle;
			ConfigManagerV3* configManagerV3;
			FetHandleManager* fetHandleManager;			

			bool upInit(unsigned char level);
			bool InitDcdc(unsigned char level);
			bool upErase(const FileFuncImpl& firmware);
			bool upWrite(const FileFuncImpl& firmware);
			bool upRead(const FileFuncImpl& firmware);
			bool upCoreErase();
			bool upCoreWrite();
			bool upCoreRead();
			bool updateCore(FileFuncImpl &firmware);
			uint16_t checkHalVersion() const;
		};

	};
};

#endif /* DLL430_UPDATEMANAGEREZFET_H */
