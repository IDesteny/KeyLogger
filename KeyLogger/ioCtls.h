#pragma once

#ifdef _CONSOLE
#include <windows.h>
#else
#include <ntddk.h>
#endif

#define IOCTL(CODE)	\
	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800 + CODE, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define GET_KEY IOCTL(1)