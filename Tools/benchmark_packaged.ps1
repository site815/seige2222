param([ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name='release', [switch]$NaniteBaseline, [switch]$EpicReference, [int]$Width=1600, [int]$Height=900)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$versionLine=Get-Content (Join-Path $projectRoot 'Config/DefaultGame.ini') | Where-Object {$_ -match '^ProjectVersion='} | Select-Object -First 1
$version=$versionLine.Substring('ProjectVersion='.Length)
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid project version'}
$gameDirectory=Join-Path $projectRoot "Builds/v$version/Windows/seige2222/Binaries/Win64"
$gameExe=Join-Path $gameDirectory 'Seige-Win64-Shipping.exe'
if($Width -lt 960 -or $Width -gt 7680 -or $Height -lt 540 -or $Height -gt 4320){throw 'Invalid benchmark dimensions'}
$arguments="-GraphicsBenchmark -BenchmarkName=$Name -RenderOffscreen -windowed -ForceRes -ResX=$Width -ResY=$Height -unattended -nosplash -NoSound"
if($NaniteBaseline){$arguments+=' -BenchmarkNaniteBaseline'}
if($EpicReference){$arguments+=' -BenchmarkV05Epic'}
$started=Get-Date
$process=Start-Process -FilePath $gameExe -WorkingDirectory $gameDirectory -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
if($process.ExitCode -ne 0){throw "Graphics benchmark exited with code $($process.ExitCode)"}
$reportPath=Join-Path $env:LOCALAPPDATA "seige2222/Saved/GraphicsBenchmark-$Name.json"
if(!(Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started){throw 'Missing or stale benchmark report'}
$report=Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if($report.views.Count -ne 5 -or $report.width -ne $Width -or $report.height -ne $Height){throw 'Incomplete graphics benchmark'}
$shadowQuality=if($EpicReference){3}else{2}
if($report.quality.runtime_resolution_quality -ne 100 -or $report.quality.'sg.ShadowQuality' -ne $shadowQuality){throw 'Unexpected benchmark quality settings'}
Copy-Item -LiteralPath $reportPath -Destination (Join-Path $projectRoot "Saved/GraphicsBenchmark-$Name.json")
$report | ConvertTo-Json -Depth 8
