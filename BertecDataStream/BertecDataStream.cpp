#include "pch.h"
#include <iostream>
#include <time.h>
#include <windows.h>
#include <string.h>
#include "bertecif.h"
#ifdef _WIN32
#include <tchar.h>
#endif

bool dropHeader = true;
bool includesyncaux = true;
int limitChannels = 0;

// Portable version of Windows Sleep function
void WaitForXmilliseconds(int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, nullptr);
#endif
}

void CALLBACK dataCallback(bertec_Handle handle, bertec_DataFrame* dataFrame, void* userData) {
    // If device count is zero (meaning the device somehow disconnected or something else bad happened) fail quietly
    if (dataFrame->deviceCount <= 0) {
        return;
    }

    // Log the header
    if (dropHeader) {
        printf("\nTimestamp");

        if (includesyncaux) {
            printf(",Sync,Aux");
        }

        // Get the channel names from the device's eprom, null terminated
        char channelNames[BERTEC_MAX_CHANNELS][BERTEC_MAX_NAME_LENGTH + 1];
        int channelCount = bertec_GetDeviceChannels(handle, 0, &channelNames[0][0], sizeof(channelNames));

        if (limitChannels > 0 && limitChannels < channelCount) {
            channelCount = limitChannels;
        }

        for (int col = 0; col < channelCount; ++col) {
            printf(",%s", channelNames[col]);
        }

        printf("\n");

        // Prevent logging the header on future callbacks
        dropHeader = false;
    }

    // Get the first device that was detected
    bertec_DeviceData& data = dataFrame->device[0];
    int channelCount = data.channelData.count;

    if (limitChannels > 0 && limitChannels < channelCount) {
        channelCount = limitChannels;
    }

    if (channelCount > 0) {
        // The timestamps that come in are expressed in 8ths of a millisecond even if there is no external clock signal
        printf("%.3f", (double)data.additionalData.timestamp / 8.0);

        if (includesyncaux) {
            printf(",%d,%d", data.additionalData.syncData, data.additionalData.auxData);
        }

        for (int col = 0; col < channelCount; ++col) {
            printf(",%f", data.channelData.data[col]);
        }

        printf("\n");
    }
}

void CALLBACK statusCallback(bertec_Handle handle, int status, void* userData) {
    if (bertec_GetStatus(handle) != BERTEC_DEVICES_READY) {
        return;
    }

    char buffer[256] = "";
    bertec_GetDeviceSerialNumber(handle, 0, buffer, sizeof(buffer));
    printf("Plate (Serial: %s) is ready.\n", buffer);

    bertec_SetExternalClockMode(handle, 0, bertec_ClockSourceFlags::CLOCK_SOURCE_INTERNAL);
    bertec_SetEnableAutozero(handle, true);
    bertec_ZeroNow(handle);
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: ./BertecDataStream.exe <time (s)>");
        return 1;
    }

    bool useCallbacks = true;
    bool usePolling = false;
    double runTimeSeconds = atof(argv[1]);
    int useAutozeroing = 1;
    int timestampStepping = 1000;

    printf("Running Bertec Library version %d.\n", bertec_LibraryVersion());
    printf("Initializing virtual handle...\n");
    bertec_Handle handle = bertec_Init();

    printf("Autozeroing is %s\n", useAutozeroing ? "enabled" : "disabled");
    bertec_SetEnableAutozero(handle, useAutozeroing);

    printf("Registering data collection callback function...\n");
    bertec_RegisterDataCallback(handle, dataCallback, nullptr);

    printf("Registering device status callback function...\n");
    bertec_RegisterStatusCallback(handle, statusCallback, nullptr);

    bertec_Start(handle);
    printf("Searching for connected devices...\n");

    WaitForXmilliseconds(1000 * runTimeSeconds);

    bertec_Stop(handle);
    bertec_Close(handle);

    printf("\nData successfully gathered. Terminating session.\n");
    return 0;
}

