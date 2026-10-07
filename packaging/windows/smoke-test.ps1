[CmdletBinding()]
param([Parameter(Mandatory)][string]$Archive, [string]$OutputDir = 'smoke')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$output = (Resolve-Path $OutputDir).Path
$extract = Join-Path $output 'extracted'
if (Test-Path $extract) { Remove-Item $extract -Recurse -Force }
Expand-Archive (Resolve-Path $Archive).Path -DestinationPath $extract
$exe = @(Get-ChildItem $extract -Filter pad.exe -Recurse)
if ($exe.Count -ne 1) { throw 'Expected exactly one pad.exe in archive' }
$bin = $exe[0].DirectoryName
$savedPath = $env:PATH
$savedPlugin = $env:QT_PLUGIN_PATH
$savedPlatform = $env:QT_QPA_PLATFORM_PLUGIN_PATH
$savedPlatformName = $env:QT_QPA_PLATFORM
$process = $null
try {
    # Do not accidentally load Qt/SDL/MSVC files from the CI developer tools.
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = ''
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = ''
    $env:QT_QPA_PLATFORM = 'windows'
    foreach ($arg in @('--version', '--list')) {
        $p = Start-Process $exe[0].FullName -ArgumentList $arg -WorkingDirectory $bin -PassThru `
            -RedirectStandardOutput "$output/$($arg.TrimStart('-')).stdout.txt" `
            -RedirectStandardError "$output/$($arg.TrimStart('-')).stderr.txt"
        if (-not $p.WaitForExit(20000)) { $p.Kill(); throw "Timeout: $arg" }
        if ($p.ExitCode -ne 0) { throw "$arg exited with code $($p.ExitCode)" }
    }
    $process = Start-Process $exe[0].FullName -ArgumentList '--no-tray' -WorkingDirectory $bin -PassThru `
        -RedirectStandardOutput "$output/gui.stdout.txt" -RedirectStandardError "$output/gui.stderr.txt"
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        if ($process.HasExited) { throw "GUI exited during startup: $($process.ExitCode)" }
    } while ($process.MainWindowHandle -eq 0 -and [DateTime]::UtcNow -lt $deadline)
    if ($process.MainWindowHandle -eq 0) { throw 'GUI did not create a visible top-level window' }
    Start-Sleep -Seconds 3
    $process.Refresh()
    if ($process.HasExited) { throw 'GUI exited after showing its window' }
    # Save runner pixels for human inspection; a successful launch alone is not a UI review.
    Add-Type -AssemblyName System.Windows.Forms, System.Drawing
    function Save-Desktop([string]$file) {
        $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
        $bitmap = [System.Drawing.Bitmap]::new($bounds.Width, $bounds.Height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
            $bitmap.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $graphics.Dispose(); $bitmap.Dispose() }
    }
    Save-Desktop "$output/windows-desktop.png"
    Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class PadWindowProbe {
    public delegate bool EnumProc(IntPtr window, IntPtr param);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr param);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder text, int count);
    public static bool HasSettingsWindow(int process) {
        bool found = false;
        EnumWindows((window, param) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            var title = new StringBuilder(512); GetWindowText(window, title, title.Capacity);
            if (owner == process && IsWindowVisible(window) && (title.ToString() == "Settings" || title.ToString() == "Edit Settings")) found = true;
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@
    # Ctrl+S is the QAction shortcut for MainWindow::openMainSettingsDialog.
    if (-not [PadWindowProbe]::SetForegroundWindow($process.MainWindowHandle)) {
        throw 'Could not focus Pad for dialog screenshot'
    }
    [System.Windows.Forms.SendKeys]::SendWait('^s')
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not [PadWindowProbe]::HasSettingsWindow($process.Id)) {
        if ($process.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Settings dialog did not open' }
        Start-Sleep -Milliseconds 250
    }
    Start-Sleep -Seconds 2
    Save-Desktop "$output/windows-settings-dialog.png"
    "PASS: extracted portable zip; --version and --list exit 0; GUI window created and remains running; Settings dialog opened via Ctrl+S and captured. Physical controller and input injection not tested." |
        Set-Content "$output/result.txt"
} catch {
    "FAIL: $_" | Set-Content "$output/result.txt"
    throw
} finally {
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $env:PATH = $savedPath
    $env:QT_PLUGIN_PATH = $savedPlugin
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $savedPlatform
    $env:QT_QPA_PLATFORM = $savedPlatformName
    if (Test-Path $extract) { Remove-Item $extract -Recurse -Force }
}
