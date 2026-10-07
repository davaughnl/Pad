[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDir,[Parameter(Mandatory)][string]$QtDir,[Parameter(Mandatory)][string]$SdlDir,[string]$OutputDir='editor-evidence')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$exe=(Resolve-Path "$BuildDir/pad-editor-tests.exe").Path
$bin=Split-Path $exe
New-Item -ItemType Directory $OutputDir -Force | Out-Null
$output=(Resolve-Path $OutputDir).Path
& "$QtDir/bin/windeployqt.exe" --release --no-compiler-runtime $exe
if($LASTEXITCODE -ne 0){throw 'Editor fixture runtime deployment failed'}
Copy-Item "$QtDir/bin/Qt5Test.dll" $bin
Copy-Item "$SdlDir/lib/x64/SDL2.dll" $bin
$env:QT_QPA_PLATFORM='windows'
foreach($case in @('button','keyboard-mouse','advanced','stick','mouse','sensor-accel','sensor-gyro','about')){
    $process=Start-Process $exe -ArgumentList $case,$output -WorkingDirectory $bin -PassThru -RedirectStandardOutput "$output/$case.stdout.txt" -RedirectStandardError "$output/$case.stderr.txt"
    if(-not $process.WaitForExit(45000)){$process.Kill();throw "Editor case timed out: $case"}
    Get-Content "$output/$case.stdout.txt"
    if($process.ExitCode -ne 0){Get-Content "$output/$case.stderr.txt";throw "Editor case failed: $case ($($process.ExitCode))"}
}
'PASS: eight native editor/model cases. Screenshot existence is not visual verification; inspect pixels.' | Set-Content "$output/result.txt"
