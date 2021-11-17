#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#define DEVICE_NAME						\
	_T("KeyLogger")

#define DEVICE_PATH						\
	_T("\\\\.\\") DEVICE_NAME

INT _tmain(VOID)
{
	

	return EXIT_SUCCESS;
}