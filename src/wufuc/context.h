#pragma once

#pragma pack(push, 1)
typedef struct
{
        CRITICAL_SECTION cs;
        DWORD count;
        union
        {
                struct
                {
                        HANDLE mutex;
                        HANDLE uevent;
                };
                HANDLE handles[MAXIMUM_WAIT_OBJECTS];
        };
        union
        {
                struct
                {
                        DWORD mutex_tag;
                        DWORD uevent_tag;
                };
                DWORD tags[MAXIMUM_WAIT_OBJECTS];
        };
} context;
#pragma pack(pop)

bool ctx_create(context *ctx,
        const wchar_t *MutexName,
        DWORD MutexTag,
        const wchar_t *EventName,
        const wchar_t *EventStringSecurityDescriptor,
        DWORD EventTag);
bool ctx_delete(context *ctx);
DWORD ctx_add_handle(context *ctx, HANDLE Handle, DWORD Tag);
bool ctx_get_tag(context *ctx, HANDLE Handle, DWORD *pTag);
DWORD ctx_add_new_mutex(context *ctx,
        bool InitialOwner,
        const wchar_t *Name,
        DWORD Tag,
        HANDLE *pMutexHandle);
DWORD ctx_add_new_mutex_fmt(context *ctx,
        bool InitialOwner,
        DWORD Tag,
        HANDLE *pMutexHandle,
        const wchar_t *const NameFormat,
        ...);
DWORD ctx_add_event(context *ctx,
        const wchar_t *Name,
        const wchar_t *StringSecurityDescriptor,
        DWORD Tag,
        HANDLE *pEventHandle);
bool ctx_wait_all(context *ctx,
        bool Alertable,
        DWORD *pResult);
DWORD ctx_wait_any(context *ctx,
        bool Alertable,
        DWORD *pResult,
        HANDLE *pHandle,
        DWORD *pTag);
bool ctx_close_and_remove_handle(context *ctx, HANDLE Handle);
bool ctx_duplicate_context(const context *pSrc,
        HANDLE hProcess,
        context *pDst,
        HANDLE Handle,
        DWORD DesiredAccess,
        DWORD Tag);
