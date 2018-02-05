#pragma once

#ifdef UNICODE
#define RUNDLL32_Start       RUNDLL32_StartW
#define RUNDLL32_Unload      RUNDLL32_UnloadW
#define RUNDLL32_DeleteFile  RUNDLL32_DeleteFileW
#else
#define RUNDLL32_Start       RUNDLL32_StartA
#define RUNDLL32_Unload      RUNDLL32_UnloadA
#define RUNDLL32_DeleteFile  RUNDLL32_DeleteFileA
#endif // !UNICODE
