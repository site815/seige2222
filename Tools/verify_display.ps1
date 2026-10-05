param()
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$versionLine=Get-Content (Join-Path $projectRoot 'Config/DefaultGame.ini') | Where-Object {$_ -match '^ProjectVersion='} | Select-Object -First 1
$version=$versionLine.Substring('ProjectVersion='.Length)
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid project version'}
$gameDirectory=Join-Path $projectRoot "Builds/v$version/Windows/seige2222/Binaries/Win64"
$gameExe=Join-Path $gameDirectory 'Seige-Win64-Shipping.exe'
$started=Get-Date
# Deliberately no RenderOffscreen/ForceRes: validate the actual monitor and
# Windows viewport, while keeping player preference writes disabled.
$process=Start-Process -FilePath $gameExe -WorkingDirectory $gameDirectory -ArgumentList '-DisplaySmoke -NoSaveDisplay -unattended -nosplash -NoSound' -WindowStyle Hidden -PassThru
if(-not $process.WaitForExit(120000)){throw 'Display verification did not finish within two minutes'}
if($process.ExitCode -ne 0){throw "Display verification exited with code $($process.ExitCode)"}
$reportPath=Join-Path $env:LOCALAPPDATA 'seige2222/Saved/DisplaySmoke.json'
if(!(Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started){throw 'Missing or stale display report'}
$report=Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if($report.failures -ne 0 -or $report.states.Count -ne 4 -or @($report.states | Where-Object {-not $_.valid}).Count -gt 0){throw 'Actual viewport settings assertions failed'}
Copy-Item -LiteralPath $reportPath -Destination (Join-Path $projectRoot "Saved/DisplaySmoke-v$version.json")
$report | ConvertTo-Json -Depth 5
