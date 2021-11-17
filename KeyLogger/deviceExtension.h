#pragma once
#include <wdm.h>

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pTargetDeviceObject;
	UNICODE_STRING symbolicLinkName;
	KGUARDED_MUTEX mutex;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;