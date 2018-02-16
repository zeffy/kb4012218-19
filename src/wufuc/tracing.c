#include "stdafx.h"
#include "tracing.h"
#include <Shlobj.h>

void itrace_(const wchar_t *const format, ...)
{
        va_list argptr;
        int count;
        wchar_t *buffer;

        va_start(argptr, format);

        count = _vscwprintf(format, argptr) + 1;
        buffer = calloc(count, sizeof *buffer);
        vswprintf_s(buffer, count, format, argptr);

        va_end(argptr);
        OutputDebugStringW(buffer);
        free(buffer);
}

HANDLE g_hTracingMutex;
wchar_t path[MAX_PATH];
wchar_t exepath[MAX_PATH];

DWORD WINAPI tracing_thread(LPVOID pParam)
{
        wchar_t *folder;
        HANDLE file;
        DWORD dwProcessId;
        wchar_t *exename;
        int count;

        if ( !*path ) {
                SHGetKnownFolderPath(&FOLDERID_ProgramData, 0, NULL, &folder);
                wcscpy_s(path, _countof(path), folder);
                CoTaskMemFree(folder);
                PathAppendW(path, L"wufuc");
                CreateDirectoryW(path, NULL);
                PathAppendW(path, L"wufuc.log");
        }
        if ( !*exepath )
                GetModuleFileNameW(NULL, exepath, _countof(exepath));

        file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        itrace(L"CreateFileW=%p", file);
        dwProcessId = GetCurrentProcessId();
        exename = PathFindFileNameW(exepath);

        va_start(argptr, format);
        count = _vscwprintf(format, argptr) + 1;
        buf1 = calloc(count, sizeof *buf1);
        vswprintf_s(buf1, count, format, argptr);
        va_end(argptr);

        count = _scwprintf(fmt, datebuf, timebuf, dwProcessId, exename, buf1);
        buf2 = calloc(count + 1, sizeof *buf2);
        swprintf_s(buf2, count + 1, fmt, datebuf, timebuf, dwProcessId, exename, buf1);
        free(buf1);
        itrace(L"WriteFile=%d", WriteFile(file, buf2, count * (sizeof *buf2), &written, NULL));
        free(buf2);
        itrace(L"FlushFileBuffers=%d", FlushFileBuffers(file));
        itrace(L"CloseHandle=%d", CloseHandle(file));
        itrace(L"ReleaseMutex=%d", ReleaseMutex(g_hTracingMutex));


        HANDLE hHeap = GetProcessHeap();
        TCHAR* pchRequest = (TCHAR*)HeapAlloc(hHeap, 0, 512 * sizeof(TCHAR));

        DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
        BOOL fSuccess = FALSE;
        HANDLE hPipe = NULL;

        // Do some extra error checking since the app will keep running even if this
        // thread fails.

        if ( pParam == NULL ) {
                itrace(L"\nERROR - Pipe Server Failure:");
                itrace(L"   InstanceThread got an unexpected NULL value in lpvParam.");
                itrace(L"   InstanceThread exitting.");
                if ( pchRequest != NULL ) HeapFree(hHeap, 0, pchRequest);
                return -1;
        }

        // Print verbose messages. In production code, this should be for debugging only.
        printf("InstanceThread created, receiving and processing messages.\n");

        // The thread's parameter is a handle to a pipe object instance. 

        hPipe = (HANDLE)pParam;

        // Loop until done reading
        while ( true ) {
                // Read client requests from the pipe. This simplistic code only allows messages
                // up to BUFSIZE characters in length.
                fSuccess = ReadFile(
                        hPipe,        // handle to pipe 
                        pchRequest,    // buffer to receive data 
                        512 * sizeof(TCHAR), // size of buffer 
                        &cbBytesRead, // number of bytes read 
                        NULL);        // not overlapped I/O 

                if ( !fSuccess || cbBytesRead == 0 ) {
                        if ( GetLastError() == ERROR_BROKEN_PIPE ) {
                                itrace(L"InstanceThread: client disconnected.");
                        } else {
                                itrace(L"InstanceThread ReadFile failed, GLE=%d.", GetLastError());
                        }
                        break;
                }

                _wstrdate_s(datebuf, _countof(datebuf));
                _wstrtime_s(timebuf, _countof(timebuf));

                if ( !fSuccess ) {
                        itrace(L"InstanceThread WriteFile failed, GLE=%d.", GetLastError());
                        break;
                }
        }

        // Flush the pipe to allow the client to read the pipe's contents 
        // before disconnecting. Then disconnect the pipe, and close the 
        // handle to this pipe instance. 

        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);

        HeapFree(hHeap, 0, pchRequest);

        printf("InstanceThread exitting.\n");
        return 1;
}

bool start_tracing(const wchar_t *pipename)
{
        HANDLE hPipe;
        HANDLE hPipe = INVALID_HANDLE_VALUE;
        HANDLE hThread = NULL;

        while ( true ) {
                hPipe = CreateNamedPipeW(pipename,
                        PIPE_ACCESS_INBOUND, 0, PIPE_UNLIMITED_INSTANCES, 512, 0, 512, NULL);
                if ( hPipe == INVALID_HANDLE_VALUE ) {
                        itrace(L"CreateNamedPipe failed, GLE=%d.\n"), GetLastError();
                        return false;
                }

                if ( ConnectNamedPipe(hPipe, NULL) ||
                        GetLastError() == ERROR_PIPE_CONNECTED ) {
                        itrace(L"Client connected, creating a processing thread.");

                        // Create a thread for this client. 
                        hThread = CreateThread(
                                NULL,             // no security attribute 
                                0,                // default stack size 
                                tracing_thread,   // thread proc
                                (LPVOID)hPipe,    // thread parameter 
                                0,                // not suspended 
                                NULL);            // returns thread ID 

                        if ( hThread == NULL ) {
                                itrace(L"CreateThread failed, GLE=%d.\n"), GetLastError();
                                return false;
                        } else {
                                CloseHandle(hThread);
                        }
                } else {
                        // The client could not connect, so close the pipe. 
                        CloseHandle(hPipe);
                }
        }
        return true;
}

void trace_(const wchar_t *const format, ...)
{
        va_list argptr;
        wchar_t *folder;
        wchar_t datebuf[9];
        wchar_t timebuf[9];
        HANDLE file;
        DWORD dwProcessId;
        wchar_t *exename;
        int count;
        DWORD written;
        const wchar_t fmt[] = L"%ls %ls [%lu] Exe(%ls) %ls";
        wchar_t *buf1;
        wchar_t *buf2;

        va_start(argptr, format);
        itrace_(format);
        va_end(argptr);

        if ( !*path ) {
                SHGetKnownFolderPath(&FOLDERID_ProgramData, 0, NULL, &folder);
                wcscpy_s(path, _countof(path), folder);
                CoTaskMemFree(folder);
                PathAppendW(path, L"wufuc");
                CreateDirectoryW(path, NULL);
                PathAppendW(path, L"wufuc.log");
        }
        if ( !*exepath )
                GetModuleFileNameW(NULL, exepath, _countof(exepath));

        if ( !g_hTracingMutex )
                g_hTracingMutex = CreateMutexW(NULL, FALSE, L"Global\\6b2f5740-7435-47f7-865c-dbd825292f32");

        itrace(L"WaitForSingleObject=%lu", WaitForSingleObject(g_hTracingMutex, INFINITE));

        _wstrdate_s(datebuf, _countof(datebuf));
        _wstrtime_s(timebuf, _countof(timebuf));

        file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        itrace(L"CreateFileW=%p", file);
        dwProcessId = GetCurrentProcessId();
        exename = PathFindFileNameW(exepath);

        va_start(argptr, format);
        count = _vscwprintf(format, argptr) + 1;
        buf1 = calloc(count, sizeof *buf1);
        vswprintf_s(buf1, count, format, argptr);
        va_end(argptr);

        count = _scwprintf(fmt, datebuf, timebuf, dwProcessId, exename, buf1);
        buf2 = calloc(count + 1, sizeof *buf2);
        swprintf_s(buf2, count + 1, fmt, datebuf, timebuf, dwProcessId, exename, buf1);
        free(buf1);
        itrace(L"WriteFile=%d", WriteFile(file, buf2, count * (sizeof *buf2), &written, NULL));
        free(buf2);
        itrace(L"FlushFileBuffers=%d", FlushFileBuffers(file));
        itrace(L"CloseHandle=%d", CloseHandle(file));
        itrace(L"ReleaseMutex=%d", ReleaseMutex(g_hTracingMutex));
}
