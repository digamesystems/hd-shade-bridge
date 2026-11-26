# SPDX-License-Identifier: GPL-2.0-or-later
# Closes any PlatformIO serial monitor (`pio device monitor`) so the board's
# serial port is free, e.g. for an upload. Touches nothing else: only processes
# whose command line is `pio[.exe] device monitor` (pio.exe and the python
# processes it starts). Prints what it closed.
$procs = Get-CimInstance Win32_Process |
    Where-Object { $_.CommandLine -match 'pio(\.exe)?"?\s+device\s+monitor' }
if (-not $procs) {
    'No serial monitor running.'
    exit 0
}
foreach ($p in $procs) {
    try {
        Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop
        "Closed serial monitor process $($p.ProcessId) ($($p.Name))"
    } catch {
        # A child often exits with its parent before we reach it.
    }
}
Start-Sleep -Milliseconds 500  # let Windows release the port
