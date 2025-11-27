/*
 * Kernel-Level Memory Reader Driver
 * Windows Driver Framework (WDF) Implementation
 * 
 * Based on Microsoft Windows Driver Kit (WDK) documentation:
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/
 * 
 * WARNING: This driver is for educational and research purposes only.
 * Unauthorized use may violate terms of service and applicable laws.
 */

#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>

// Microsoft WDF version requirement
#if !defined(_WIN64)
#error "This driver requires x64 architecture"
#endif

#define DEVICE_NAME L"\\Device\\GameMemoryReader"
#define SYMBOLIC_LINK L"\\DosDevices\\GameMemoryReader"
#define DRIVER_TAG 'GMRD'

// IOCTL Codes
#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCAN_PATTERN CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RESOLVE_POINTER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MAP_PHYSICAL_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_PROCESS_BASE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Structures
typedef struct _MEMORY_READ_REQUEST {
    ULONG ProcessId;
    ULONG_PTR Address;
    ULONG Size;
    PVOID Buffer;
} MEMORY_READ_REQUEST, *PMEMORY_READ_REQUEST;

typedef struct _MEMORY_WRITE_REQUEST {
    ULONG ProcessId;
    ULONG_PTR Address;
    ULONG Size;
    PVOID Buffer;
} MEMORY_WRITE_REQUEST, *PMEMORY_WRITE_REQUEST;

typedef struct _PATTERN_SCAN_REQUEST {
    ULONG ProcessId;
    ULONG_PTR StartAddress;
    ULONG_PTR EndAddress;
    UCHAR Pattern[256];
    UCHAR Mask[256];
    ULONG PatternSize;
    ULONG_PTR Results[1024];
    ULONG ResultCount;
} PATTERN_SCAN_REQUEST, *PPATTERN_SCAN_REQUEST;

typedef struct _POINTER_RESOLVE_REQUEST {
    ULONG ProcessId;
    ULONG_PTR BaseAddress;
    ULONG OffsetCount;
    ULONG Offsets[16];
    ULONG_PTR Result;
} POINTER_RESOLVE_REQUEST, *PPOINTER_RESOLVE_REQUEST;

typedef struct _PHYSICAL_MEMORY_MAP {
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Size;
    PVOID VirtualAddress;
} PHYSICAL_MEMORY_MAP, *PPHYSICAL_MEMORY_MAP;

// Forward declarations
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD GameMemoryReaderEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL GameMemoryReaderEvtIoDeviceControl;
EVT_WDF_DEVICE_FILE_CREATE GameMemoryReaderEvtDeviceFileCreate;
EVT_WDF_FILE_CLOSE GameMemoryReaderEvtFileClose;

// Helper functions
NTSTATUS ReadProcessMemory(ULONG ProcessId, ULONG_PTR Address, PVOID Buffer, ULONG Size);
NTSTATUS WriteProcessMemory(ULONG ProcessId, ULONG_PTR Address, PVOID Buffer, ULONG Size);
NTSTATUS ScanMemoryPattern(ULONG ProcessId, ULONG_PTR StartAddress, ULONG_PTR EndAddress, 
                           PUCHAR Pattern, PUCHAR Mask, ULONG PatternSize, PULONG_PTR Results, PULONG ResultCount);
NTSTATUS ResolveMultiLevelPointer(ULONG ProcessId, ULONG_PTR BaseAddress, PULONG Offsets, ULONG OffsetCount, PULONG_PTR Result);
NTSTATUS GetProcessBaseAddress(ULONG ProcessId, PULONG_PTR BaseAddress);
NTSTATUS MapPhysicalMemory(PHYSICAL_ADDRESS PhysicalAddress, ULONG Size, PVOID* VirtualAddress);
PEPROCESS GetProcessByPid(ULONG ProcessId);

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    KdPrint(("GameMemoryReader: DriverEntry called\n"));

    WDF_DRIVER_CONFIG_INIT(&config, GameMemoryReaderEvtDeviceAdd);
    config.EvtDriverUnload = NULL; // Keep driver loaded

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        KdPrint(("GameMemoryReader: WdfDriverCreate failed with status 0x%08X\n", status));
        return status;
    }

    KdPrint(("GameMemoryReader: Driver loaded successfully\n"));
    return STATUS_SUCCESS;
}

NTSTATUS GameMemoryReaderEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_FILEOBJECT_CONFIG fileConfig;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLink;

    UNREFERENCED_PARAMETER(Driver);

    KdPrint(("GameMemoryReader: EvtDeviceAdd called\n"));

    // Set exclusive access
    WdfDeviceInitSetExclusive(DeviceInit, TRUE);

    // Set file object configuration
    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, GameMemoryReaderEvtDeviceFileCreate, GameMemoryReaderEvtFileClose, NULL);
    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, WDF_NO_OBJECT_ATTRIBUTES);

    // Create device
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.Tag = DRIVER_TAG;
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("GameMemoryReader: WdfDeviceCreate failed with status 0x%08X\n", status));
        return status;
    }

    // Create device interface (Microsoft recommended approach)
    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    status = WdfDeviceCreateSymbolicLink(device, &deviceName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("GameMemoryReader: WdfDeviceCreateSymbolicLink failed with status 0x%08X\n", status));
        return status;
    }

    // Set I/O type to buffered (Microsoft best practice for security)
    WdfDeviceSetIoType(device, WdfDeviceIoBuffered);

    // Configure IO queue
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = GameMemoryReaderEvtIoDeviceControl;
    queueConfig.PowerManaged = WdfFalse;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        KdPrint(("GameMemoryReader: WdfIoQueueCreate failed with status 0x%08X\n", status));
        return status;
    }

    KdPrint(("GameMemoryReader: Device created successfully\n"));
    return STATUS_SUCCESS;
}

VOID GameMemoryReaderEvtDeviceFileCreate(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);

    KdPrint(("GameMemoryReader: File opened\n"));
    WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID GameMemoryReaderEvtFileClose(
    _In_ WDFFILEOBJECT FileObject
)
{
    UNREFERENCED_PARAMETER(FileObject);
    KdPrint(("GameMemoryReader: File closed\n"));
}

NTSTATUS GameMemoryReaderEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID inputBuffer = NULL;
    PVOID outputBuffer = NULL;
    size_t bytesReturned = 0;

    UNREFERENCED_PARAMETER(Queue);

    // Get input buffer (Microsoft WDF best practice)
    if (InputBufferLength > 0) {
        status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &inputBuffer, NULL);
        if (!NT_SUCCESS(status)) {
            KdPrint(("GameMemoryReader: Failed to retrieve input buffer, status 0x%08X\n", status));
            WdfRequestComplete(Request, status);
            return status;
        }
        
        // Validate input buffer is not NULL (Microsoft security requirement)
        if (inputBuffer == NULL) {
            WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
            return STATUS_INVALID_PARAMETER;
        }
    }

    // Get output buffer (Microsoft WDF best practice)
    if (OutputBufferLength > 0) {
        status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &outputBuffer, NULL, NULL);
        if (!NT_SUCCESS(status)) {
            KdPrint(("GameMemoryReader: Failed to retrieve output buffer, status 0x%08X\n", status));
            WdfRequestComplete(Request, status);
            return status;
        }
        
        // Validate output buffer is not NULL (Microsoft security requirement)
        if (outputBuffer == NULL) {
            WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
            return STATUS_INVALID_PARAMETER;
        }
    }

    // Process IOCTL
    switch (IoControlCode) {
    case IOCTL_READ_MEMORY: {
        PMEMORY_READ_REQUEST readReq = (PMEMORY_READ_REQUEST)inputBuffer;
        if (InputBufferLength < sizeof(MEMORY_READ_REQUEST) || OutputBufferLength < readReq->Size) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = ReadProcessMemory(readReq->ProcessId, readReq->Address, outputBuffer, readReq->Size);
        if (NT_SUCCESS(status)) {
            bytesReturned = readReq->Size;
        }
        break;
    }

    case IOCTL_WRITE_MEMORY: {
        PMEMORY_WRITE_REQUEST writeReq = (PMEMORY_WRITE_REQUEST)inputBuffer;
        if (InputBufferLength < sizeof(MEMORY_WRITE_REQUEST) + writeReq->Size) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = WriteProcessMemory(writeReq->ProcessId, writeReq->Address, 
                                   (PUCHAR)inputBuffer + sizeof(MEMORY_WRITE_REQUEST), writeReq->Size);
        break;
    }

    case IOCTL_SCAN_PATTERN: {
        PPATTERN_SCAN_REQUEST scanReq = (PPATTERN_SCAN_REQUEST)inputBuffer;
        if (InputBufferLength < sizeof(PATTERN_SCAN_REQUEST)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = ScanMemoryPattern(scanReq->ProcessId, scanReq->StartAddress, scanReq->EndAddress,
                                  scanReq->Pattern, scanReq->Mask, scanReq->PatternSize,
                                  scanReq->Results, &scanReq->ResultCount);
        if (NT_SUCCESS(status)) {
            RtlCopyMemory(outputBuffer, scanReq, sizeof(PATTERN_SCAN_REQUEST));
            bytesReturned = sizeof(PATTERN_SCAN_REQUEST);
        }
        break;
    }

    case IOCTL_RESOLVE_POINTER: {
        PPOINTER_RESOLVE_REQUEST ptrReq = (PPOINTER_RESOLVE_REQUEST)inputBuffer;
        if (InputBufferLength < sizeof(POINTER_RESOLVE_REQUEST)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = ResolveMultiLevelPointer(ptrReq->ProcessId, ptrReq->BaseAddress,
                                         ptrReq->Offsets, ptrReq->OffsetCount, &ptrReq->Result);
        if (NT_SUCCESS(status)) {
            RtlCopyMemory(outputBuffer, ptrReq, sizeof(POINTER_RESOLVE_REQUEST));
            bytesReturned = sizeof(POINTER_RESOLVE_REQUEST);
        }
        break;
    }

    case IOCTL_GET_PROCESS_BASE: {
        ULONG processId = *(PULONG)inputBuffer;
        ULONG_PTR baseAddress = 0;
        if (InputBufferLength < sizeof(ULONG)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = GetProcessBaseAddress(processId, &baseAddress);
        if (NT_SUCCESS(status)) {
            *(PULONG_PTR)outputBuffer = baseAddress;
            bytesReturned = sizeof(ULONG_PTR);
        }
        break;
    }

    case IOCTL_MAP_PHYSICAL_MEMORY: {
        PPHYSICAL_MEMORY_MAP mapReq = (PPHYSICAL_MEMORY_MAP)inputBuffer;
        if (InputBufferLength < sizeof(PHYSICAL_MEMORY_MAP)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = MapPhysicalMemory(mapReq->PhysicalAddress, mapReq->Size, &mapReq->VirtualAddress);
        if (NT_SUCCESS(status)) {
            RtlCopyMemory(outputBuffer, mapReq, sizeof(PHYSICAL_MEMORY_MAP));
            bytesReturned = sizeof(PHYSICAL_MEMORY_MAP);
        }
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
    return status;
}

NTSTATUS ReadProcessMemory(ULONG ProcessId, ULONG_PTR Address, PVOID Buffer, ULONG Size)
{
    PEPROCESS process = NULL;
    KAPC_STATE apcState;
    NTSTATUS status = STATUS_SUCCESS;
    SIZE_T bytesRead = 0;
    KIRQL oldIrql;

    // Validate parameters (Microsoft security best practice)
    if (Buffer == NULL || Size == 0 || Size > 0x100000) { // Max 1MB per read
        return STATUS_INVALID_PARAMETER;
    }

    // Check IRQL level (Microsoft requirement)
    oldIrql = KeGetCurrentIrql();
    if (oldIrql > APC_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    __try {
        process = GetProcessByPid(ProcessId);
        if (!process) {
            status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        // Attach to process context (Microsoft documented method)
        KeStackAttachProcess(process, &apcState);
        __try {
            // Probe memory accessibility (Microsoft security requirement)
            ProbeForRead((PVOID)Address, Size, 1);
            
            // Validate address range
            if (Address == 0 || Address + Size < Address) {
                status = STATUS_INVALID_ADDRESS;
                __leave;
            }
            
            RtlCopyMemory(Buffer, (PVOID)Address, Size);
            bytesRead = Size;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
            if (status == STATUS_ACCESS_VIOLATION) {
                status = STATUS_INVALID_ADDRESS;
            }
        }
        KeUnstackDetachProcess(&apcState);

        ObDereferenceObject(process);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        if (process) {
            ObDereferenceObject(process);
        }
        status = GetExceptionCode();
    }

    return status;
}

NTSTATUS WriteProcessMemory(ULONG ProcessId, ULONG_PTR Address, PVOID Buffer, ULONG Size)
{
    PEPROCESS process = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    PMDL mdl = NULL;
    PVOID mappedAddress = NULL;
    KIRQL oldIrql;

    // Validate parameters (Microsoft security best practice)
    if (Buffer == NULL || Size == 0 || Size > 0x100000) { // Max 1MB per write
        return STATUS_INVALID_PARAMETER;
    }

    // Check IRQL level (Microsoft requirement)
    oldIrql = KeGetCurrentIrql();
    if (oldIrql > DISPATCH_LEVEL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    __try {
        process = GetProcessByPid(ProcessId);
        if (!process) {
            status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        // Validate address range
        if (Address == 0 || Address + Size < Address) {
            status = STATUS_INVALID_ADDRESS;
            __leave;
        }

        // Create MDL for memory write (Microsoft documented method)
        mdl = IoAllocateMdl((PVOID)Address, Size, FALSE, FALSE, NULL);
        if (!mdl) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        __try {
            // Probe and lock pages (Microsoft security requirement)
            MmProbeAndLockPages(mdl, KernelMode, IoModifyAccess);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            IoFreeMdl(mdl);
            status = GetExceptionCode();
            __leave;
        }

        // Map locked pages (Microsoft documented API)
        mappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority);
        if (!mappedAddress) {
            MmUnlockPages(mdl);
            IoFreeMdl(mdl);
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        // Perform memory copy
        RtlCopyMemory(mappedAddress, Buffer, Size);
        
        // Cleanup MDL (Microsoft requirement)
        MmUnmapLockedPages(mappedAddress, mdl);
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
        mdl = NULL;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        if (mdl) {
            if (mappedAddress) {
                MmUnmapLockedPages(mappedAddress, mdl);
            }
            MmUnlockPages(mdl);
            IoFreeMdl(mdl);
        }
        if (process) {
            ObDereferenceObject(process);
        }
        status = GetExceptionCode();
    }

    return status;
}

NTSTATUS ScanMemoryPattern(ULONG ProcessId, ULONG_PTR StartAddress, ULONG_PTR EndAddress,
                           PUCHAR Pattern, PUCHAR Mask, ULONG PatternSize, PULONG_PTR Results, PULONG ResultCount)
{
    PEPROCESS process = NULL;
    KAPC_STATE apcState;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR currentAddress = StartAddress;
    ULONG foundCount = 0;
    ULONG i;

    __try {
        process = GetProcessByPid(ProcessId);
        if (!process) {
            status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        KeStackAttachProcess(process, &apcState);
        __try {
            while (currentAddress < EndAddress && foundCount < 1024) {
                BOOLEAN match = TRUE;
                __try {
                    ProbeForRead((PVOID)currentAddress, PatternSize, 1);
                    for (i = 0; i < PatternSize; i++) {
                        if (Mask[i] == 0xFF) {
                            if (((PUCHAR)currentAddress)[i] != Pattern[i]) {
                                match = FALSE;
                                break;
                            }
                        }
                    }
                    if (match) {
                        Results[foundCount++] = currentAddress;
                    }
                }
                __except(EXCEPTION_EXECUTE_HANDLER) {
                    // Skip inaccessible memory
                }
                currentAddress += 1;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
        }
        KeUnstackDetachProcess(&apcState);

        ObDereferenceObject(process);
        *ResultCount = foundCount;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        if (process) {
            ObDereferenceObject(process);
        }
        status = GetExceptionCode();
    }

    return status;
}

NTSTATUS ResolveMultiLevelPointer(ULONG ProcessId, ULONG_PTR BaseAddress, PULONG Offsets, ULONG OffsetCount, PULONG_PTR Result)
{
    PEPROCESS process = NULL;
    KAPC_STATE apcState;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR currentAddress = BaseAddress;
    ULONG i;

    __try {
        process = GetProcessByPid(ProcessId);
        if (!process) {
            status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        KeStackAttachProcess(process, &apcState);
        __try {
            for (i = 0; i < OffsetCount; i++) {
                ProbeForRead((PVOID)currentAddress, sizeof(ULONG_PTR), 1);
                currentAddress = *(PULONG_PTR)currentAddress;
                if (currentAddress == 0) {
                    status = STATUS_INVALID_ADDRESS;
                    __leave;
                }
                currentAddress += Offsets[i];
            }
            *Result = currentAddress;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            status = GetExceptionCode();
        }
        KeUnstackDetachProcess(&apcState);

        ObDereferenceObject(process);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        if (process) {
            ObDereferenceObject(process);
        }
        status = GetExceptionCode();
    }

    return status;
}

NTSTATUS GetProcessBaseAddress(ULONG ProcessId, PULONG_PTR BaseAddress)
{
    PEPROCESS process = NULL;
    NTSTATUS status = STATUS_SUCCESS;

    __try {
        process = GetProcessByPid(ProcessId);
        if (!process) {
            status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        *BaseAddress = (ULONG_PTR)PsGetProcessSectionBaseAddress(process);
        ObDereferenceObject(process);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        if (process) {
            ObDereferenceObject(process);
        }
        status = GetExceptionCode();
    }

    return status;
}

NTSTATUS MapPhysicalMemory(PHYSICAL_ADDRESS PhysicalAddress, ULONG Size, PVOID* VirtualAddress)
{
    PHYSICAL_ADDRESS lowAddress = PhysicalAddress;
    PHYSICAL_ADDRESS highAddress;
    highAddress.QuadPart = PhysicalAddress.QuadPart + Size;

    *VirtualAddress = MmMapIoSpace(lowAddress, Size, MmNonCached);
    if (*VirtualAddress == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

PEPROCESS GetProcessByPid(ULONG ProcessId)
{
    PEPROCESS process = NULL;
    NTSTATUS status;

    status = PsLookupProcessByProcessId((HANDLE)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return NULL;
    }

    return process;
}
