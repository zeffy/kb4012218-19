#pragma once

#pragma pack(push, 1)
typedef struct
{
        HANDLE hAuxiliaryMutex;
        union
        {
                struct
                {
                        HANDLE hMainMutex;
                        HANDLE hUnloadEvent;
                };
                HANDLE handles[2];
        };
} ContextHandles;
#pragma pack(pop)

VOID CALLBACK ServiceNotifyCallback(PSERVICE_NOTIFYW pNotifyBuffer);
DWORD WINAPI StartAddress(LPVOID pParam);
