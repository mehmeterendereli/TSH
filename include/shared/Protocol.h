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
