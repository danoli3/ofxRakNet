/*
 *  Original work: Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  RakNet License.txt file in the licenses directory of this source tree. An additional grant
 *  of patent rights can be found in the RakNet Patents.txt file in the same directory.
 *
 *
 *  Modified work: Copyright (c) 2016-2019, SLikeSoft UG (haftungsbeschränkt)
 *
 *  This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
 *  license found in the license.txt file in the root directory of this source tree.
 */

 /// \file
 ///



#include <slikenet/statistics.h>
#include <inttypes.h>
#include <stdio.h> // sprintf
#include <slikenet/GetTime.h>
#include <slikenet/string.h>
#include <slikenet/crtstring_adapter.h>

using namespace SLNet;

// Verbosity level currently supports 0 (low), 1 (medium), 2 (high)
// Buffer must be hold enough to hold the output string.  See the source to get an idea of how many bytes will be output
void RAK_DLL_EXPORT SLNet::StatisticsToString(RakNetStatistics *s, char *buffer, int verbosityLevel)
{
	if(s == 0)
	{
#pragma warning(push)
#pragma warning(disable:4996)
		sprintf(buffer, "stats is a NULL pointer in statsToString\n");
#pragma warning(pop)
		return;
	}

	if(verbosityLevel == 0)
	{
#pragma warning(push)
#pragma warning(disable:4996)
		sprintf(buffer,
			"Bytes per second sent     %" PRIu64 "\n"
			"Bytes per second received %" PRIu64 "\n"
			"Current packetloss        %.1f%%\n",
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			s->packetlossLastSecond * 100.0f
		);
#pragma warning(pop)
	}
	else if(verbosityLevel == 1)
	{
#pragma warning(push)
#pragma warning(disable:4996)
		sprintf(buffer,
			"Actual bytes per second sent       %" PRIu64 "\n"
			"Actual bytes per second received   %" PRIu64 "\n"
			"Message bytes per second pushed    %" PRIu64 "\n"
			"Total actual bytes sent            %" PRIu64 "\n"
			"Total actual bytes received        %" PRIu64 "\n"
			"Total message bytes pushed         %" PRIu64 "\n"
			"Current packetloss                 %.1f%%\n"
			"Average packetloss                 %.1f%%\n"
			"Elapsed connection time in seconds %" PRIu64 "\n",
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_PUSHED],
			(uint64_t) s->runningTotal[ACTUAL_BYTES_SENT],
			(uint64_t) s->runningTotal[ACTUAL_BYTES_RECEIVED],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_PUSHED],
			s->packetlossLastSecond * 100.0f,
			s->packetlossTotal * 100.0f,
			(uint64_t)((SLNet::GetTimeUS() - s->connectionStartTime) / 1000000)
		);
#pragma warning(pop)

		if(s->BPSLimitByCongestionControl != 0)
		{
			char buff2[128];
			sprintf_s(buff2,
				"Send capacity                    %" PRIu64 " bytes per second (%.0f%%)\n",
				(uint64_t) s->BPSLimitByCongestionControl,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByCongestionControl
			);
#pragma warning(push)
#pragma warning(disable:4996)
			strcat(buffer, buff2);
#pragma warning(pop)
		}
		if(s->BPSLimitByOutgoingBandwidthLimit != 0)
		{
			char buff2[128];
			sprintf_s(buff2,
				"Send limit                       %" PRIu64 " (%.0f%%)\n",
				(uint64_t) s->BPSLimitByOutgoingBandwidthLimit,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByOutgoingBandwidthLimit
			);
#pragma warning(push)
#pragma warning(disable:4996)
			strcat(buffer, buff2);
#pragma warning(pop)
		}
	}
	else
	{
#pragma warning(push)
#pragma warning(disable:4996)
		sprintf(buffer,
			"Actual bytes per second sent         %" PRIu64 "\n"
			"Actual bytes per second received     %" PRIu64 "\n"
			"Message bytes per second sent        %" PRIu64 "\n"
			"Message bytes per second resent      %" PRIu64 "\n"
			"Message bytes per second pushed      %" PRIu64 "\n"
			"Message bytes per second returned	  %" PRIu64 "\n"
			"Message bytes per second ignored     %" PRIu64 "\n"
			"Total bytes sent                     %" PRIu64 "\n"
			"Total bytes received                 %" PRIu64 "\n"
			"Total message bytes sent             %" PRIu64 "\n"
			"Total message bytes resent           %" PRIu64 "\n"
			"Total message bytes pushed           %" PRIu64 "\n"
			"Total message bytes returned		  %" PRIu64 "\n"
			"Total message bytes ignored          %" PRIu64 "\n"
			"Messages in send buffer, by priority %i,%i,%i,%i\n"
			"Bytes in send buffer, by priority    %i,%i,%i,%i\n"
			"Messages in resend buffer            %i\n"
			"Bytes in resend buffer               %" PRIu64 "\n"
			"Current packetloss                   %.1f%%\n"
			"Average packetloss                   %.1f%%\n"
			"Elapsed connection time in seconds   %" PRIu64 "\n",
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_SENT],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_RESENT],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_PUSHED],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_RECEIVED_PROCESSED],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_RECEIVED_IGNORED],
			(uint64_t) s->runningTotal[ACTUAL_BYTES_SENT],
			(uint64_t) s->runningTotal[ACTUAL_BYTES_RECEIVED],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_SENT],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_RESENT],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_PUSHED],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_RECEIVED_PROCESSED],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_RECEIVED_IGNORED],
			s->messageInSendBuffer[IMMEDIATE_PRIORITY], s->messageInSendBuffer[HIGH_PRIORITY], s->messageInSendBuffer[MEDIUM_PRIORITY], s->messageInSendBuffer[LOW_PRIORITY],
			(unsigned int)s->bytesInSendBuffer[IMMEDIATE_PRIORITY], (unsigned int)s->bytesInSendBuffer[HIGH_PRIORITY], (unsigned int)s->bytesInSendBuffer[MEDIUM_PRIORITY], (unsigned int)s->bytesInSendBuffer[LOW_PRIORITY],
			s->messagesInResendBuffer,
			(uint64_t) s->bytesInResendBuffer,
			s->packetlossLastSecond * 100.0f,
			s->packetlossTotal * 100.0f,
			(uint64_t)((SLNet::GetTimeUS() - s->connectionStartTime) / 1000000)
		);
#pragma warning(pop)

		if(s->BPSLimitByCongestionControl != 0)
		{
			char buff2[128];
			sprintf_s(buff2,
				"Send capacity                    %" PRIu64 " bytes per second (%.0f%%)\n",
				(uint64_t) s->BPSLimitByCongestionControl,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByCongestionControl
			);
#pragma warning(push)
#pragma warning(disable:4996)
			strcat(buffer, buff2);
#pragma warning(pop)
		}
		if(s->BPSLimitByOutgoingBandwidthLimit != 0)
		{
			char buff2[128];
			sprintf_s(buff2,
				"Send limit                       %" PRIu64 " (%.0f%%)\n",
				(uint64_t) s->BPSLimitByOutgoingBandwidthLimit,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByOutgoingBandwidthLimit
			);
#pragma warning(push)
#pragma warning(disable:4996)
			strcat(buffer, buff2);
#pragma warning(pop)
		}
	}
}
void RAK_DLL_EXPORT SLNet::StatisticsToString(RakNetStatistics *s, char *buffer, size_t bufferLength, int verbosityLevel)
{
	if(s == 0)
	{
		sprintf_s(buffer, bufferLength, "stats is a NULL pointer in statsToString\n");
		return;
	}

	if(verbosityLevel == 0)
	{
		sprintf_s(buffer, bufferLength,
			"Bytes per second sent     %" PRIu64 "\n"
			"Bytes per second received %" PRIu64 "\n"
			"Current packetloss        %.1f%%\n",
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			s->packetlossLastSecond * 100.0f
		);
	}
	else if(verbosityLevel == 1)
	{
		sprintf_s(buffer, bufferLength,
			"Actual bytes per second sent       %" PRIu64 "\n"
			"Actual bytes per second received   %" PRIu64 "\n"
			"Message bytes per second pushed    %" PRIu64 "\n"
			"Total actual bytes sent            %" PRIu64 "\n"
			"Total actual bytes received        %" PRIu64 "\n"
			"Total message bytes pushed         %" PRIu64 "\n"
			"Current packetloss                 %.1f%%\n"
			"Average packetloss                 %.1f%%\n"
			"Elapsed connection time in seconds %" PRIu64 "\n",
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_PUSHED],
			(uint64_t) s->runningTotal[ACTUAL_BYTES_SENT],
			(uint64_t) s->runningTotal[ACTUAL_BYTES_RECEIVED],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_PUSHED],
			s->packetlossLastSecond * 100.0f,
			s->packetlossTotal * 100.0f,
			(uint64_t)((SLNet::GetTimeUS() - s->connectionStartTime) / 1000000)
		);

		if(s->BPSLimitByCongestionControl != 0)
		{
			char buff2[128];
			sprintf_s(buff2,
				"Send capacity                    %" PRIu64 " bytes per second (%.0f%%)\n",
				(uint64_t) s->BPSLimitByCongestionControl,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByCongestionControl
			);
			strcat_s(buffer, bufferLength, buff2);
		}
		if(s->BPSLimitByOutgoingBandwidthLimit != 0)
		{
			char buff2[128];
			sprintf_s(buff2,
				"Send limit                       %" PRIu64 " (%.0f%%)\n",
				(uint64_t) s->BPSLimitByOutgoingBandwidthLimit,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByOutgoingBandwidthLimit
			);
			strcat_s(buffer, bufferLength, buff2);
		}
	}
	else
	{
		sprintf_s(buffer, bufferLength,
			"Actual bytes per second sent         %" PRIu64 "\n"
			"Actual bytes per second received     %" PRIu64 "\n"
			"Message bytes per second sent        %" PRIu64 "\n"
			"Message bytes per second resent      %" PRIu64 "\n"
			"Message bytes per second pushed      %" PRIu64 "\n"
			"Message bytes per second returned	  %" PRIu64 "\n"
			"Message bytes per second ignored     %" PRIu64 "\n"
			"Total bytes sent                     %" PRIu64 "\n"
			"Total bytes received                 %" PRIu64 "\n"
			"Total message bytes sent             %" PRIu64 "\n"
			"Total message bytes resent           %" PRIu64 "\n"
			"Total message bytes pushed           %" PRIu64 "\n"
			"Total message bytes returned		  %" PRIu64 "\n"
			"Total message bytes ignored          %" PRIu64 "\n"
			"Messages in send buffer, by priority %i,%i,%i,%i\n"
			"Bytes in send buffer, by priority    %i,%i,%i,%i\n"
			"Messages in resend buffer            %i\n"
			"Bytes in resend buffer               %" PRIu64 "\n"
			"Current packetloss                   %.1f%%\n"
			"Average packetloss                   %.1f%%\n"
			"Elapsed connection time in seconds   %" PRIu64 "\n",
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(uint64_t) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_SENT],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_RESENT],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_PUSHED],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_RECEIVED_PROCESSED],
			(uint64_t) s->valueOverLastSecond[USER_MESSAGE_BYTES_RECEIVED_IGNORED],
			(uint64_t) s->runningTotal[ACTUAL_BYTES_SENT],
			(uint64_t) s->runningTotal[ACTUAL_BYTES_RECEIVED],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_SENT],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_RESENT],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_PUSHED],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_RECEIVED_PROCESSED],
			(uint64_t) s->runningTotal[USER_MESSAGE_BYTES_RECEIVED_IGNORED],
			s->messageInSendBuffer[IMMEDIATE_PRIORITY], s->messageInSendBuffer[HIGH_PRIORITY], s->messageInSendBuffer[MEDIUM_PRIORITY], s->messageInSendBuffer[LOW_PRIORITY],
			(unsigned int)s->bytesInSendBuffer[IMMEDIATE_PRIORITY], (unsigned int)s->bytesInSendBuffer[HIGH_PRIORITY], (unsigned int)s->bytesInSendBuffer[MEDIUM_PRIORITY], (unsigned int)s->bytesInSendBuffer[LOW_PRIORITY],
			s->messagesInResendBuffer,
			(uint64_t) s->bytesInResendBuffer,
			s->packetlossLastSecond * 100.0f,
			s->packetlossTotal * 100.0f,
			(uint64_t)((SLNet::GetTimeUS() - s->connectionStartTime) / 1000000)
		);

		if(s->BPSLimitByCongestionControl != 0)
		{
			char buff2[128];
			sprintf_s(buff2,
				"Send capacity                    %" PRIu64 " bytes per second (%.0f%%)\n",
				(uint64_t) s->BPSLimitByCongestionControl,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByCongestionControl
			);
			strcat_s(buffer, bufferLength, buff2);
		}
		if(s->BPSLimitByOutgoingBandwidthLimit != 0)
		{
			char buff2[128];
			sprintf_s(buff2,
				"Send limit                       %" PRIu64 " (%.0f%%)\n",
				(uint64_t) s->BPSLimitByOutgoingBandwidthLimit,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByOutgoingBandwidthLimit
			);
			strcat_s(buffer, bufferLength, buff2);
		}
	}
}
