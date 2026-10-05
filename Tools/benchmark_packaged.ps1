param([ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name='release', [switch]$NaniteBaseline)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$versionLine=Get-Content (Join-Path $projectRoot 'Config/DefaultGame.ini') | Where-Object {$_ -match '^ProjectVersion='} | Select-Object -First 1
$version=$versionLine.Substring('ProjectVersion='.Length)
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid project version'}
$gameDirectory=Join-Path $projectRoot "Builds/v$version/Windows/seige2222/Binaries/Win64"
$gameExe=Join-Path $gameDirectory 'Seige-Win64-Shipping.exe'
$arguments="-GraphicsBenchmark -BenchmarkName=$Name -RenderOffscreen -windowed -ForceRes -ResX=1600 -ResY=900 -unattended -nosplash -NoSound"
if($NaniteBaseline){$arguments+=' -BenchmarkNaniteBaseline'}
$started=Get-Date
$process=Start-Process -FilePath $gameExe -WorkingDirectory $gameDirectory -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
if($process.ExitCode -ne 0){throw "Graphics benchmark exited with code $($process.ExitCode)"}
$reportPath=Join-Path $env:LOCALAPPDATA "seige2222/Saved/GraphicsBenchmark-$Name.json"
if(!(Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started){throw 'Missing or stale benchmark report'}
$report=Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if($report.views.Count -ne 5 -or $report.width -ne 1600 -or $report.height -ne 900){throw 'Incomplete graphics benchmark'}
if($report.quality.runtime_resolution_quality -ne 100 -or $report.quality.'sg.ShadowQuality' -ne 3){throw 'Unexpected benchmark quality settings'}
Copy-Item -LiteralPath $reportPath -Destination (Join-Path $projectRoot "Saved/GraphicsBenchmark-$Name.json")
$report | ConvertTo-Json -Depth 8
