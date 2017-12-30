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
                } DUMMYSTRUCTNAME;
                struct
                {
                        HANDLE hMainMutex;
                        HANDLE hUnloadEvent;
                } u;
                HANDLE handles[2];
        };
} ContextHandles;
#pragma pack(pop)

VOID CALLBACK ServiceNotifyCallback(PSERVICE_NOTIFYW pNotifyBuffer);
DWORD WINAPI StartAddress(LPVOID pParam);
