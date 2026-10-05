<#
Native moving-camera example:
  ./Tools/benchmark_packaged.ps1 -Name baseline-orbit -NativeResolution -Orbit
  ./Tools/benchmark_packaged.ps1 -Name current-orbit -NativeResolution -Orbit -BaselineReport Saved/GraphicsBenchmark-baseline-orbit.json
Omit -Orbit to retain the original static-camera test. Diagnostics change only
the launched process, not player preferences. -SimpleTerrain isolates terrain
shading; -HideSward isolates dense meadow geometry while retaining other foliage.
Use -View ground (or colony/meadow/hills/boundary) for one sampled camera and
its screenshot. Omit -View to retain the complete five-view route.

Each view waits for complete scenery (120s watchdog), settles for four seconds,
then samples five seconds. Schema-2 baselines instead began warmup immediately
after synchronous setup; comparison metadata preserves that distinction.
Orbit samples retain live camera-driven streaming cost and report pending cells;
static samples must remain complete.
The orbit makes one
full yaw turn during sampling, with modest tilt variation. p99/max are sensitive
to individual hitches in this short run; repeat comparisons and inspect the CPU
and GPU channels before attributing a difference to a specific subsystem.
Counters are the latest completed engine measurements, not a synchronized trace.
For deeper thread/frame attribution use Unreal Insights or a GPU capture:
https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-performance-profiling-and-configuration-in-unreal-engine
#>
param(
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name='release',
    [switch]$NaniteBaseline, [switch]$EpicReference,
    [switch]$Orbit, [switch]$SimpleTerrain, [switch]$HideSward, [switch]$Clearing,
    [ValidateSet('colony','meadow','ground','hills','boundary')][string]$View,
    [switch]$NativeResolution,
    [int]$Width=1600, [int]$Height=900,
    [string]$ExecutablePath,
    [string]$BaselineReport
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if($NativeResolution){
    if($PSBoundParameters.ContainsKey('Width') -or $PSBoundParameters.ContainsKey('Height')){throw 'Choose NativeResolution or explicit Width/Height, not both'}
    # EnumDisplaySettings returns physical primary-display pixels even when the
    # calling PowerShell process is DPI-unaware at 200% Windows scaling.
    if(-not ('SeigeBenchmarkDisplay' -as [type])){
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SeigeBenchmarkDisplay {
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    public struct Mode {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string DeviceName;
        public ushort SpecVersion, DriverVersion, Size, DriverExtra;
        public uint Fields;
        public int PositionX, PositionY;
        public uint DisplayOrientation, DisplayFixedOutput;
        public short Color, Duplex, YResolution, TTOption, Collate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string FormName;
        public ushort LogPixels;
        public uint BitsPerPel, PelsWidth, PelsHeight, DisplayFlags, DisplayFrequency;
        public uint ICMMethod, ICMIntent, MediaType, DitherType, Reserved1, Reserved2, PanningWidth, PanningHeight;
    }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    static extern bool EnumDisplaySettings(string deviceName, int modeNumber, ref Mode mode);
    public static int[] PrimaryPixels() {
        var mode = new Mode(); mode.Size = (ushort)Marshal.SizeOf(typeof(Mode));
        if(!EnumDisplaySettings(null, -1, ref mode)) throw new InvalidOperationException("Cannot query physical primary display mode");
        return new int[]{(int)mode.PelsWidth, (int)mode.PelsHeight};
    }
}
'@
    }
    $physicalPixels=[SeigeBenchmarkDisplay]::PrimaryPixels()
    $Width=$physicalPixels[0]; $Height=$physicalPixels[1]
}
$versionLine=Get-Content (Join-Path $projectRoot 'Config/DefaultGame.ini') | Where-Object {$_ -match '^ProjectVersion='} | Select-Object -First 1
$version=$versionLine.Substring('ProjectVersion='.Length)
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid project version'}
$gameDirectory=Join-Path $projectRoot "Builds/v$version/Windows/seige2222/Binaries/Win64"
$gameExe=Join-Path $gameDirectory 'Seige-Win64-Shipping.exe'
if($ExecutablePath){$gameExe=(Resolve-Path -LiteralPath $ExecutablePath).Path; $gameDirectory=Split-Path $gameExe -Parent}
if(!(Test-Path -LiteralPath $gameExe)){throw "Missing package: $gameExe"}
if($Width -lt 960 -or $Width -gt 7680 -or $Height -lt 540 -or $Height -gt 4320){throw 'Invalid benchmark dimensions'}
$arguments="-GraphicsBenchmark -BenchmarkName=$Name -RenderOffscreen -windowed -ForceRes -ResX=$Width -ResY=$Height -unattended -nosplash -NoSound -NoSaveDisplay"
if($NaniteBaseline){$arguments+=' -BenchmarkNaniteBaseline'}
if($EpicReference){$arguments+=' -BenchmarkV05Epic'}
if($Orbit){$arguments+=' -BenchmarkOrbit'}
if($SimpleTerrain){$arguments+=' -BenchmarkSimpleTerrain'}
if($HideSward){$arguments+=' -BenchmarkHideSward'}
if($Clearing){if($View -ne 'colony'){throw 'Clearing diagnostic requires -View colony'}; $arguments+=' -BenchmarkClearing'}
if($View){$arguments+=" -BenchmarkView=$($View.ToLowerInvariant())"}
$started=Get-Date
$process=Start-Process -FilePath $gameExe -WorkingDirectory $gameDirectory -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
if($process.ExitCode -ne 0){throw "Graphics benchmark exited with code $($process.ExitCode)"}
$reportPath=Join-Path $env:LOCALAPPDATA "seige2222/Saved/GraphicsBenchmark-$Name.json"
if(!(Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started){throw 'Missing or stale benchmark report'}
$report=Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$expectedViews=if($View){@($View.ToLowerInvariant())}else{@('colony','meadow','ground','hills','boundary')}
$reportedViews=@($report.views | ForEach-Object {$_.view})
if($reportedViews.Count -ne $expectedViews.Count -or @($expectedViews | Where-Object {$_ -notin $reportedViews}).Count -gt 0 -or $report.width -ne $Width -or $report.height -ne $Height){throw 'Incomplete graphics benchmark or incorrect requested views'}
if($View -and $report.selected_view -ne $View){throw 'Package does not support the requested single-view benchmark'}
if(!$Orbit -and $report.schema_version -ge 3 -and @($report.views | Where-Object {$_.pending_scenery_cells_at_sample_end -ne 0}).Count -gt 0){throw 'Static benchmark sampled incomplete scenery'}
$mode=if($Orbit){'orbit'}else{'static'}
$reportedMode=if($report.camera_mode){$report.camera_mode}else{'static'}
if($reportedMode -ne $mode){throw 'Package does not support the requested camera benchmark mode'}
if($SimpleTerrain -and (!$report.diagnostic_simple_terrain -or @($report.views | Where-Object {$_.simple_terrain_components -le 0}).Count -gt 0)){throw 'Simple terrain diagnostic was not applied'}
if($HideSward -and !$report.diagnostic_sward_hidden){throw 'Hidden sward diagnostic was not applied'}
$shadowQuality=if($EpicReference){3}else{2}
if($report.quality.runtime_resolution_quality -ne 100 -or $report.quality.'sg.ShadowQuality' -ne $shadowQuality){throw 'Unexpected benchmark quality settings'}
Copy-Item -LiteralPath $reportPath -Destination (Join-Path $projectRoot "Saved/GraphicsBenchmark-$Name.json")
if($BaselineReport){
    $baseline=Get-Content -LiteralPath $BaselineReport -Raw | ConvertFrom-Json
    $baselineMode=if($baseline.camera_mode){$baseline.camera_mode}else{'static'}
    $baselineSpeed=if($null -ne $baseline.simulation_speed){$baseline.simulation_speed}else{0}
    $currentSpeed=if($null -ne $report.simulation_speed){$report.simulation_speed}else{0}
    if($baselineSpeed -ne $currentSpeed){throw 'Baseline simulation workload differs: compare benchmarks at the same simulation speed'}
    if($baselineMode -ne $reportedMode -or $baseline.width -ne $report.width -or $baseline.height -ne $report.height){throw 'Baseline must have the same camera mode and output dimensions'}
    if($Orbit -and ($baseline.camera_path_version -ne $report.camera_path_version -or $baseline.orbit_degrees_per_second -ne $report.orbit_degrees_per_second -or $baseline.orbit_pitch_amplitude_degrees -ne $report.orbit_pitch_amplitude_degrees)){throw 'Orbit trajectories differ'}
    if($baseline.quality.runtime_resolution_quality -ne $report.quality.runtime_resolution_quality){throw 'Baseline render resolution differs'}
    $comparisons=@()
    foreach($viewResult in $report.views){
        $before=@($baseline.views | Where-Object {$_.view -eq $viewResult.view})
        if($before.Count -ne 1){throw "Missing/duplicate baseline view: $($viewResult.view)"}
        $before=$before[0]
        $metrics=@()
        foreach($channel in @('wall','game','render','rhi','gpu')){
            foreach($metric in @('mean_ms','p95_ms','p99_ms','max_ms')){
                $oldValue=$null; $newValue=$null
                if($before.timings -and $before.timings.$channel.available){$oldValue=$before.timings.$channel.$metric}
                if($viewResult.timings -and $viewResult.timings.$channel.available){$newValue=$viewResult.timings.$channel.$metric}
                # Legacy static reports provide wall mean/p95 only.
                if($channel -eq 'wall'){
                    $legacy=@{mean_ms='mean_frame_ms';p95_ms='p95_frame_ms';p99_ms='p99_frame_ms';max_ms='max_frame_ms'}[$metric]
                    if($null -eq $oldValue){$oldValue=$before.$legacy}
                    if($null -eq $newValue){$newValue=$viewResult.$legacy}
                }
                if($null -ne $oldValue -and $null -ne $newValue -and $oldValue -gt 0){
                    $metrics+= [ordered]@{channel=$channel;metric=$metric;baseline_ms=$oldValue;current_ms=$newValue;delta_ms=$newValue-$oldValue;change_percent=100*($newValue/$oldValue-1)}
                }
            }
        }
        $comparisons+= [ordered]@{view=$viewResult.view;baseline_mean_fps=$before.mean_fps;current_mean_fps=$viewResult.mean_fps;metrics=$metrics}
    }
    $comparison=[ordered]@{
        baseline=$baseline.name;current=$report.name;camera_mode=$reportedMode;width=$report.width;height=$report.height
        method='Single-run comparison; negative milliseconds/percent indicate improvement. Match builds, assets, driver and background load or document differences. Repeat runs before treating small differences as reliable.'
        baseline_profile=$baseline.profile;current_profile=$report.profile
        baseline_warmup_policy=$(if($baseline.warmup_policy){$baseline.warmup_policy}else{'4s-after-synchronous-setup'})
        current_warmup_policy=$(if($report.warmup_policy){$report.warmup_policy}else{'4s-after-synchronous-setup'})
        baseline_simple_terrain=[bool]$baseline.diagnostic_simple_terrain;current_simple_terrain=[bool]$report.diagnostic_simple_terrain
        baseline_sward_hidden=[bool]$baseline.diagnostic_sward_hidden;current_sward_hidden=[bool]$report.diagnostic_sward_hidden
        views=$comparisons
    }
    $comparison | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $projectRoot "Saved/GraphicsComparison-$Name.json") -Encoding UTF8
}
$report | ConvertTo-Json -Depth 8
