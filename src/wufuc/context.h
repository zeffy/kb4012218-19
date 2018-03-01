#pragma once

#pragma pack(push, 1)
typedef struct
{
        CRITICAL_SECTION cs;
        unsigned count;
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
                        unsigned mutex_tag;
                        unsigned uevent_tag;
                };
                unsigned tags[MAXIMUM_WAIT_OBJECTS];
        };
} context;
#pragma pack(pop)

bool ctx_create(context *ctx,
        const wchar_t *MutexName,
        unsigned MutexTag,
        const wchar_t *EventName,
        const wchar_t *EventStringSecurityDescriptor,
        unsigned EventTag);

bool ctx_delete(context *ctx);

unsigned ctx_add_handle(context *ctx, HANDLE Handle, unsigned Tag);

bool ctx_get_tag(context *ctx, HANDLE Handle, unsigned *pTag);

unsigned ctx_add_new_mutex(context *ctx,
        bool InitialOwner,
        const wchar_t *Name,
        unsigned Tag,
        HANDLE *pMutexHandle);

unsigned ctx_add_new_mutex_fmt(context *ctx,
        bool InitialOwner,
        unsigned Tag,
        HANDLE *pMutexHandle,
        const wchar_t *const NameFormat,
        ...);

unsigned ctx_add_event(context *ctx,
        const wchar_t *Name,
        const wchar_t *StringSecurityDescriptor,
        unsigned Tag,
        HANDLE *pEventHandle);

bool ctx_wait(context *ctx, bool WaitAll, bool Alertable, DWORD *pResult);

bool ctx_close_and_remove_handle(context *ctx,
        HANDLE Handle);

bool ctx_duplicate_context(const context *pSrc,
        HANDLE hProcess,
        context *pDst,
        HANDLE Handle,
        DWORD DesiredAccess,
        unsigned Tag);
