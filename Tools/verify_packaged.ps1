param(
  [ValidateSet('UiSmoke','PrototypeSmoke','NextUpdateSmoke')][string]$Mode='UiSmoke',
  [ValidateRange(800,7680)][int]$Width=1600,
  [ValidateRange(600,4320)][int]$Height=900
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$versionLine=Get-Content (Join-Path $projectRoot 'Config/DefaultGame.ini') | Where-Object {$_ -match '^ProjectVersion='} | Select-Object -First 1
$version=$versionLine.Substring('ProjectVersion='.Length)
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid project version'}
$packageRoot=Join-Path $projectRoot "Builds/v$version/Windows"
$gameExe=Join-Path $packageRoot 'Seige.exe'
$binaryDirectory=Join-Path $packageRoot 'seige2222/Binaries/Win64'
$shippingExe=Join-Path $binaryDirectory 'Seige-Win64-Shipping.exe'
$runtimeDirectories=@('Rules','AIFILES','Interface','Graphics')
function Get-FileIdentity([string]$Path) {
  $item=Get-Item -LiteralPath $Path -ErrorAction Stop
  [pscustomobject]@{path=$item.FullName.Substring($projectRoot.Length+1).Replace('\','/');bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash}
}
function Get-SourceInputs {
  $extensions=@('.cpp','.h','.cs','.ini','.uasset','.umap','.json')
  $files=@(foreach($directory in @('Source','Config','Content')+$runtimeDirectories){
    Get-ChildItem -LiteralPath (Join-Path $projectRoot $directory) -Recurse -File | Where-Object {$_.Extension.ToLowerInvariant() -in $extensions}
  })
  $files+=Get-Item -LiteralPath (Join-Path $projectRoot 'seige2222.uproject')
  foreach($file in ($files | Sort-Object FullName)){Get-FileIdentity $file.FullName}
}
function Get-PackagedInputs {
  Get-FileIdentity $shippingExe
  $files=@(Get-ChildItem -LiteralPath (Join-Path $packageRoot 'seige2222/Content/Paks') -File | Where-Object {$_.Extension.ToLowerInvariant() -in @('.pak','.ucas','.utoc')} | Sort-Object FullName)
  if($files.Count -eq 0){throw 'No cooked package containers found'}
  foreach($file in $files){Get-FileIdentity $file.FullName}
}
function Get-RuntimeFiles {
  $source=@{}; $staged=@{}
  foreach($directory in $runtimeDirectories){
    foreach($file in (Get-ChildItem -LiteralPath (Join-Path $projectRoot $directory) -Recurse -Filter '*.json' -File)){
      $source[$file.FullName.Substring($projectRoot.Length+1).Replace('\','/')]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
    foreach($file in (Get-ChildItem -LiteralPath (Join-Path $binaryDirectory $directory) -Recurse -Filter '*.json' -File)){
      $staged[$file.FullName.Substring($binaryDirectory.Length+1).Replace('\','/')]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
  }
  foreach($path in (@($source.Keys)+@($staged.Keys) | Sort-Object -Unique)){
    [pscustomobject]@{path=$path;source_sha256=$source[$path];staged_sha256=$staged[$path];match=[bool]($source.ContainsKey($path) -and $staged.ContainsKey($path) -and $source[$path] -ceq $staged[$path])}
  }
}
# Capture the actual Shipping child and cooked assets, not just the bootstrap.
# A later record can then reject evidence from a previous executable or data set.
$sourceInputs=@(Get-SourceInputs)
$packagedInputs=@(Get-PackagedInputs)
$runtimeFiles=@(Get-RuntimeFiles)
if($runtimeFiles.Count -eq 0 -or @($runtimeFiles | Where-Object {-not $_.match}).Count -gt 0){throw 'Source and staged runtime JSON differ'}
$saveRoot=Join-Path $env:LOCALAPPDATA 'seige2222/Saved'
$reportName=if($Mode -eq 'UiSmoke'){'PresentationSmoke.json'}elseif($Mode -eq 'NextUpdateSmoke'){'NextUpdateSmoke.json'}else{'SmokeReport.json'}
$timeoutSeconds=if($Mode -eq 'NextUpdateSmoke'){120}else{3600}
$audioArgument=if($Mode -eq 'NextUpdateSmoke'){''}else{'-NoSound'}
$started=Get-Date
$launch=Start-Process -FilePath $gameExe -ArgumentList "-$Mode -RenderOffscreen -windowed -ForceRes -ResX=$Width -ResY=$Height -unattended -nosplash -NoSaveDisplay $audioArgument" -WindowStyle Hidden -PassThru
$samples=[System.Collections.Generic.List[object]]::new()
do {
  Start-Sleep -Seconds 2
  $gameProcesses=@(Get-Process -Name 'Seige','Seige-Win64-Shipping' -ErrorAction SilentlyContinue | Where-Object {$_.Path -and $_.Path.StartsWith($packageRoot,[StringComparison]::OrdinalIgnoreCase)})
  if($gameProcesses.Count -gt 0){
    $gameIds=@($gameProcesses.Id)
    # A failed OS query is not evidence of zero endpoints. Keep this check
    # fail-closed while the launch/child processes are actually alive.
    $tcp=@(Get-NetTCPConnection -ErrorAction Stop | Where-Object {$_.OwningProcess -in $gameIds})
    $udp=@(Get-NetUDPEndpoint -ErrorAction Stop | Where-Object {$_.OwningProcess -in $gameIds})
    $samples.Add([pscustomobject]@{seconds=[math]::Round(((Get-Date)-$started).TotalSeconds,2);process_ids=$gameIds;tcp=$tcp.Count;udp=$udp.Count})
  }
  $launch.Refresh()
} while(($gameProcesses.Count -gt 0 -or -not $launch.HasExited) -and ((Get-Date)-$started).TotalSeconds -lt $timeoutSeconds)
if($gameProcesses.Count -gt 0){throw 'Packaged smoke did not finish before the verification timeout'}
$reportPath=Join-Path $saveRoot $reportName
if(-not (Test-Path -LiteralPath $reportPath)){throw "Missing $reportName"}
if((Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started){throw 'Smoke report is stale'}
$result=Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$sourceAfter=@(Get-SourceInputs)
$packageAfter=@(Get-PackagedInputs)
$runtimeAfter=@(Get-RuntimeFiles)
$inputsUnchanged=($sourceInputs | ConvertTo-Json -Depth 6 -Compress) -ceq ($sourceAfter | ConvertTo-Json -Depth 6 -Compress)
$inputsUnchanged=$inputsUnchanged -and (($packagedInputs | ConvertTo-Json -Depth 6 -Compress) -ceq ($packageAfter | ConvertTo-Json -Depth 6 -Compress))
$inputsUnchanged=$inputsUnchanged -and (($runtimeFiles | ConvertTo-Json -Depth 6 -Compress) -ceq ($runtimeAfter | ConvertTo-Json -Depth 6 -Compress))
$record=[pscustomobject]@{schema_version=2;version=$version;mode=$Mode;resolution=@($Width,$Height);started=$started.ToString('o');exit_code=$launch.ExitCode;report=$result;socket_samples=$samples;inputs_unchanged=$inputsUnchanged;packaged_inputs=$packagedInputs;source_inputs=$sourceInputs;runtime_files=$runtimeFiles}
$resolutionSuffix=if($Width -eq 1600 -and $Height -eq 900){''}else{"-${Width}x${Height}"}
$recordPath=Join-Path $projectRoot "Saved/packaged-v$version-$Mode$resolutionSuffix-verification.json"
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recordPath -Encoding UTF8
# Keep console logs small; the complete observations and hashes live in recordPath.
[pscustomobject]@{record=$recordPath;exit_code=$launch.ExitCode;report=$result;socket_samples=$samples.Count;source_files=$sourceInputs.Count;runtime_files=$runtimeFiles.Count;inputs_unchanged=$inputsUnchanged} | ConvertTo-Json -Depth 5
if(-not $inputsUnchanged){throw 'Source, cooked assets, executable or runtime JSON changed during verification'}
if($launch.ExitCode -ne 0){throw "Packaged game exited with code $($launch.ExitCode)"}
if($Mode -eq 'NextUpdateSmoke'){
  if(-not $result.passed -or $result.failures -ne 0 -or $result.completed_stages -ne 25){throw 'Bounded packaged check assertions failed'}
}elseif(-not $result.ready -or ($Mode -eq 'UiSmoke' -and ($result.failures -ne 0 -or $result.completed_stages -ne 117))){throw 'Packaged smoke assertions failed'}
if($samples.Count -eq 0){throw 'No live process observed'}
if(@($samples | Where-Object {$_.tcp -gt 0 -or $_.udp -gt 0}).Count -gt 0){throw 'Game opened a network endpoint'}
