# Usage Guide

This guide shows how to use the TSH Native Diagnostics toolkit from both the command-line shell and the optional Dear ImGui interface. The walkthrough targets a practical scenario: locating and patching the word `mehmet` inside Notepad.

## Prerequisites
- Launch Notepad and type a sentence containing the word `mehmet`. Leave Notepad running.
- Start `tsh_user.exe` from an elevated command prompt (Administrator) for driver access.
- Ensure the driver service is loaded if you plan to exercise kernel features.

## Command-Line Workflow

1. **Attach to Notepad**
   ```text
   list
   attach <pid-of-notepad>
   driver
   ```
   Confirm the driver status is `connected` for privileged operations.

2. **Initial Scan** – Search for the UTF-16 text `mehmet` (Notepad stores text as UTF-16):
   ```text
   scan utf16 mehmet
   results 10
   ```
   Note the reported addresses. If there are many hits, edit the text in Notepad (e.g., change the word) and run a refine step with the new value.

3. **Refine Scan** – After editing the word to `mehmet1` in Notepad:
   ```text
   refine mehmet1
   results 10
   ```
   The list should narrow to the actual buffer holding the text.

4. **Monitor Bytes** – Observe the candidate address:
   ```text
   monitor 0x<address>:12
   ```
   The shell prints the byte snapshot, letting you confirm the UTF-16 encoding.

5. **Patch Text** – Replace the buffer with another word (e.g., `mehmet` › `MEHMET`):
   ```text
   patch 0x<address> 4D 00 45 00 48 00 45 00 54 00
   ```
   The patch uses UTF-16 bytes; Notepad updates instantly. The shell reports the driver’s status and bytes written.

6. **Pointer Trace (optional)** – If you want the owning heap chain:
   ```text
   pointer 0x<address> 0x0 0x10
   ```
   Interpret the returned chain to map the structure layout.

7. **Exit** – Type `quit` when done.

## Dear ImGui Interface

Launch with:
```text
tsh_user.exe --imgui
```

Use the panels:
- **Processes**: Refresh, select Notepad, and press **Attach**.
- **Pointer Trace**: Enter a base address and offsets, then press **Trace Pointer** to populate the table.
- **Monitor**: Enter comma-separated entries like `0xADDRESS:12`, click **Snapshot**, and inspect the table/graph. Hover the plot to see live byte magnitudes.
- **Patch**: Provide address and byte list (hex, space separated) and click **Apply Patch**. The status panel reflects the driver response.

> **Renderer Reminder**: The included ImGui shell supplies model logic only. Hook it up with a platform/renderer backend (e.g., Win32 + DirectX11) for an interactive window. The stub closes after one frame until a backend loop is wired in.

## Tips
- Driver unavailable? Commands fall back to Win32 reads; kernel features (pointer trace/patch) require elevated privileges.
- Adjust monitor sizes to multiples of the target data type for clearer visualization.
- Use `results` frequently to validate that patching hasn’t shifted memory regions.

Happy debugging!
