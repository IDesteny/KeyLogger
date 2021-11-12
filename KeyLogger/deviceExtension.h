#pragma once
#include <wdm.h>

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pTargetDeviceObject;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;