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
    $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bitmap = [System.Drawing.Bitmap]::new($bounds.Width, $bounds.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
        $bitmap.Save("$output/windows-desktop.png", [System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
    "PASS: extracted portable zip; --version and --list exit 0; GUI window created and remains running. Physical controller and input injection not tested." |
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
