#include <windows.h>
#include <tchar.h>
#include <stdio.h>

//99 проблем

#define DEVICE_NAME						\
	_T("KeyLogger")

#define DEVICE_PATH						\
	_T("\\\\.\\") DEVICE_NAME

HANDLE KL_OpenDriver(PCTSTR symbolicLink)
{
	HANDLE hDriver = CreateFile(
		DEVICE_PATH,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	return hDriver;
}

BOOL KL_CloseDriver(HANDLE driver)
{
	return !CloseHandle(driver);
}

BOOL KL_ReactionToPush(HANDLE driver, VOID(*callback)(UINT))
{
	if (!callback)
		return EXIT_FAILURE;


}

VOID Reaction(USHORT key)
{
	printf("Key: %hu", key);
}

INT _tmain(VOID)
{
	//HANDLE kl = OpenDriver("KeyLogger");
	//ReactionToPush(kl, Reaction);
	//CloseDriver(kl);
}