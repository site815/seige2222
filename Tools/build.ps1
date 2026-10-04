param([switch]$Package, [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'seige2222.uproject'
& node (Join-Path $PSScriptRoot 'validate_rules.mjs')
if ($LASTEXITCODE -ne 0) { throw 'Rule validation failed' }
& "$Engine\Engine\Build\BatchFiles\Build.bat" SeigeEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed' }
if (-not (Test-Path (Join-Path $root 'Content\Maps\Colony.umap'))) {
  & "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $project -run=pythonscript "-script=$PSScriptRoot\create_map.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput
  if ($LASTEXITCODE -ne 0) { throw 'Map generation failed' }
}
if ($Package) {
  & "$Engine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$project" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -iostore -package -archive "-archivedirectory=$root\Builds" -map=/Game/Maps/Colony -unattended -utf8output
  if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
}
