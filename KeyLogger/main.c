#include <initguid.h>
#include <wdm.h>
#include <ntddkbd.h>
#include "info.h"
#include "ioCtls.h"

#pragma warning(disable: 5045) //Disable Spectre

#define DEVICE_PATH \
	_T("\\Device\\") DEVICE_NAME

#define SYMBOLIC_LINK_NAME \
	_T("\\DosDevices\\") DEVICE_NAME

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pTargetDeviceObject;
	UNICODE_STRING symbolicLinkName;
	PIRP pIrpKey;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

VOID
UnloadRoutine(
	PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_EXTENSION pDeviceExtension = pDriverObject->DeviceObject->DeviceExtension;

	IoDeleteSymbolicLink(&pDeviceExtension->symbolicLinkName);
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
HookCompletionRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp,
	PVOID pContext)
{
	UNREFERENCED_PARAMETER(pContext);
	ObfDereferenceObject(pDeviceObject);

	PDEVICE_EXTENSION pDeviceExtension = pDeviceObject->DeviceExtension;
	if (NT_SUCCESS(pIrp->IoStatus.Status) && pDeviceExtension->pIrpKey)
	{
		PKEYBOARD_INPUT_DATA keyInfo = pIrp->AssociatedIrp.SystemBuffer;
		if (!keyInfo->Flags)
		{
			PUSHORT pSysBuff = pDeviceExtension->pIrpKey->AssociatedIrp.SystemBuffer;
			*pSysBuff = keyInfo->MakeCode;
			CompleteIrp(pDeviceExtension->pIrpKey, STATUS_SUCCESS, sizeof(USHORT));
			pDeviceExtension->pIrpKey = NULL;
		}
	}

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
DeviceControlRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG controlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;

	if (controlCode == GET_KEY)
	{
		IoMarkIrpPending(pIrp);
		PDEVICE_EXTENSION pDeviceExtension = pDeviceObject->DeviceExtension;
		pDeviceExtension->pIrpKey = pIrp;
		return STATUS_PENDING;
	}

	return CompleteIrp(pIrp, STATUS_INVALID_PARAMETER, 0);
}

NTSTATUS
CreateFileRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);
	return CompleteIrp(pIrp, STATUS_SUCCESS, 0);
}

NTSTATUS
CloseFileRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);
	return CompleteIrp(pIrp, STATUS_SUCCESS, 0);
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
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateFileRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateFileRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControlRoutine;
	pDriverObject->DriverUnload = UnloadRoutine;

	NTSTATUS returnedStatus;
	PDEVICE_OBJECT pDeviceObject;

	UNICODE_STRING deviceName;
	RtlInitUnicodeString(&deviceName, DEVICE_PATH);

	returnedStatus = IoCreateDevice(
		pDriverObject,
		sizeof(DEVICE_EXTENSION),
		&deviceName,
		FILE_DEVICE_KEYBOARD,
		0, FALSE,
		&pDeviceObject);
	
	if (!NT_SUCCESS(returnedStatus))
		return returnedStatus;

	pDeviceObject->Flags |= DO_BUFFERED_IO;
	pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	PZZWSTR symLinks;
	returnedStatus = IoGetDeviceInterfaces(
		&GUID_CLASS_KEYBOARD,
		NULL, 0, &symLinks);

	if (!NT_SUCCESS(returnedStatus))
	{
		IoDeleteDevice(pDeviceObject);
		return returnedStatus;
	}
	
	PDEVICE_EXTENSION pDeviceExtension = pDeviceObject->DeviceExtension;
	RtlInitUnicodeString(&pDeviceExtension->symbolicLinkName, SYMBOLIC_LINK_NAME);

	returnedStatus = IoCreateSymbolicLink(
		&pDeviceExtension->symbolicLinkName,
		&deviceName);

	if (!NT_SUCCESS(returnedStatus))
	{
		IoDeleteDevice(pDeviceObject);
		return returnedStatus;
	}

	UNICODE_STRING targetDeviceName;
	RtlInitUnicodeString(&targetDeviceName, symLinks);
	ExFreePool(symLinks);

	returnedStatus = IoAttachDevice(
		pDeviceObject,
		&targetDeviceName,
		&pDeviceExtension->pTargetDeviceObject);

	if (!NT_SUCCESS(returnedStatus))
	{
		IoDeleteSymbolicLink(&pDeviceExtension->symbolicLinkName);
		IoDeleteDevice(pDeviceObject);
		return returnedStatus;
	}

	return returnedStatus;
}