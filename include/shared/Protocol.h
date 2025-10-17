#pragma once

#include <stdint.h>

#define TSH_PROTOCOL_VERSION 1u

#define TSH_DEVICE_SYM_LINK L"\\DosDevices\\TSHDiagnostic"
#define TSH_DEVICE_NAME     L"\\Device\\TSHDiagnostic"

typedef struct _TSH_PING_REQUEST
{
    uint32_t Token;
} TSH_PING_REQUEST;

typedef struct _TSH_PING_RESPONSE
{
    uint32_t Value;
} TSH_PING_RESPONSE;

typedef struct _TSH_REGION_QUERY_REQUEST
{
    uint32_t ProcessId;
    uint32_t Flags;
} TSH_REGION_QUERY_REQUEST;

typedef struct _TSH_MEMORY_REGION
{
    uint64_t BaseAddress;
    uint64_t Size;
    uint32_t Protection;
    uint32_t State;
    uint32_t Type;
    uint32_t Reserved;
} TSH_MEMORY_REGION;

typedef struct _TSH_SCAN_REQUEST
{
    uint32_t ProcessId;
    uint32_t DataType;
    uint64_t CompareValueAddress;
    uint32_t CompareValueSize;
    uint32_t Flags;
} TSH_SCAN_REQUEST;

typedef struct _TSH_POINTER_TRACE_REQUEST
{
    uint32_t ProcessId;
    uint32_t MaxDepth;
    uint64_t BaseAddress;
} TSH_POINTER_TRACE_REQUEST;

typedef struct _TSH_MONITOR_REQUEST
{
    uint32_t ProcessId;
    uint32_t EntryCount;
    uint32_t PollingIntervalMs;
    uint32_t Flags;
} TSH_MONITOR_REQUEST;

typedef struct _TSH_PATCH_REQUEST
{
    uint32_t ProcessId;
    uint32_t Flags;
    uint64_t TargetAddress;
    uint32_t PayloadSize;
    uint32_t Reserved;
} TSH_PATCH_REQUEST;

typedef struct _TSH_READ_MEMORY_REQUEST
{
    uint32_t ProcessId;
    uint32_t Flags;
    uint64_t SourceAddress;
    uint32_t Size;
    uint32_t Reserved;
} TSH_READ_MEMORY_REQUEST;

typedef struct _TSH_POINTER_TRACE_NODE
{
    uint64_t Address;
    uint64_t Value;
} TSH_POINTER_TRACE_NODE;

typedef struct _TSH_MONITOR_ENTRY
{
    uint64_t Address;
    uint32_t Size;
    uint32_t Reserved;
} TSH_MONITOR_ENTRY;

#define TSH_MONITOR_SAMPLE_MAX_BYTES 64u

typedef struct _TSH_MONITOR_SAMPLE
{
    uint64_t Address;
    uint32_t RequestedSize;
    uint32_t CapturedSize;
    uint32_t Status;
    uint32_t Reserved;
    uint8_t  Data[TSH_MONITOR_SAMPLE_MAX_BYTES];
} TSH_MONITOR_SAMPLE;

typedef struct _TSH_PATCH_RESPONSE
{
    uint32_t BytesWritten;
    uint32_t Status;
} TSH_PATCH_RESPONSE;
