param([ValidateSet('UiSmoke','PrototypeSmoke')][string]$Mode='UiSmoke')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$versionLine=Get-Content (Join-Path $projectRoot 'Config/DefaultGame.ini') | Where-Object {$_ -match '^ProjectVersion='} | Select-Object -First 1
$version=$versionLine.Substring('ProjectVersion='.Length)
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid project version'}
$packageRoot=Join-Path $projectRoot "Builds/v$version/Windows"
$gameExe=Join-Path $packageRoot 'Seige.exe'
$saveRoot=Join-Path $env:LOCALAPPDATA 'seige2222/Saved'
$reportName=if($Mode -eq 'UiSmoke'){'PresentationSmoke.json'}else{'SmokeReport.json'}
$started=Get-Date
$launch=Start-Process -FilePath $gameExe -ArgumentList "-$Mode -RenderOffscreen -windowed -ForceRes -ResX=1600 -ResY=900 -unattended -nosplash -NoSound" -WindowStyle Hidden -PassThru
$samples=[System.Collections.Generic.List[object]]::new()
do {
  Start-Sleep -Seconds 2
  $gameProcesses=@(Get-Process -Name 'Seige','Seige-Win64-Shipping' -ErrorAction SilentlyContinue | Where-Object {$_.Path -and $_.Path.StartsWith($packageRoot,[StringComparison]::OrdinalIgnoreCase)})
  if($gameProcesses.Count -gt 0){
    $gameIds=@($gameProcesses.Id)
    $tcp=@(Get-NetTCPConnection -ErrorAction SilentlyContinue | Where-Object {$_.OwningProcess -in $gameIds})
    $udp=@(Get-NetUDPEndpoint -ErrorAction SilentlyContinue | Where-Object {$_.OwningProcess -in $gameIds})
    $samples.Add([pscustomobject]@{seconds=[math]::Round(((Get-Date)-$started).TotalSeconds,2);process_ids=$gameIds;tcp=$tcp.Count;udp=$udp.Count})
  }
  $launch.Refresh()
} while(($gameProcesses.Count -gt 0 -or -not $launch.HasExited) -and ((Get-Date)-$started).TotalSeconds -lt 360)
if($gameProcesses.Count -gt 0){throw 'Packaged smoke did not finish before the verification timeout'}
$reportPath=Join-Path $saveRoot $reportName
if(-not (Test-Path -LiteralPath $reportPath)){throw "Missing $reportName"}
if((Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started){throw 'Smoke report is stale'}
$result=Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$record=[pscustomobject]@{mode=$Mode;started=$started.ToString('o');exit_code=$launch.ExitCode;report=$result;socket_samples=$samples}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $projectRoot "Saved/packaged-v$version-$Mode-verification.json")
$record | ConvertTo-Json -Depth 8
if($launch.ExitCode -ne 0){throw "Packaged game exited with code $($launch.ExitCode)"}
if(-not $result.ready -or ($Mode -eq 'UiSmoke' -and ($result.failures -ne 0 -or $result.completed_stages -ne 79))){throw 'Packaged smoke assertions failed'}
if($samples.Count -eq 0){throw 'No live process observed'}
if(@($samples | Where-Object {$_.tcp -gt 0 -or $_.udp -gt 0}).Count -gt 0){throw 'Game opened a network endpoint'}
