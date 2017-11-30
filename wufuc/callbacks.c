#include "stdafx.h"
#include "callbacks.h"
#include "helpers.h"
#include <Psapi.h>

VOID CALLBACK ServiceNotifyCallback(PSERVICE_NOTIFYW pNotifyBuffer)
{
        HMODULE hModule;

        switch ( pNotifyBuffer->dwNotificationStatus ) {
        case ERROR_SUCCESS:
                if ( inject_self_into_process(pNotifyBuffer->ServiceStatus.dwProcessId, &hModule) ) {

                }
                break;
        case ERROR_SERVICE_MARKED_FOR_DELETE:
                SetEvent((HANDLE)pNotifyBuffer->pContext);
                break;
        }
        if ( pNotifyBuffer->pszServiceNames )
                LocalFree((HLOCAL)pNotifyBuffer->pszServiceNames);
}
