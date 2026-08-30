param(
  [string]$HostName = "192.168.1.14",
  [string]$User = "orangepi",
  [string]$RemoteWs = "~/xbot_ws"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$SrcDir = Join-Path $RepoRoot "software\src"

if (-not (Test-Path (Join-Path $SrcDir "xbot_bringup"))) {
  throw "Source not found: $SrcDir"
}

$target = "${User}@${HostName}"
Write-Host "Sync $SrcDir -> ${target}:${RemoteWs}/src"

ssh $target "mkdir -p $RemoteWs/src"
if ($LASTEXITCODE -ne 0) { throw "ssh mkdir failed ($LASTEXITCODE)" }

scp -r "$SrcDir\*" "${target}:${RemoteWs}/src/"
if ($LASTEXITCODE -ne 0) { throw "scp failed ($LASTEXITCODE)" }

Write-Host "Done. On the board:"
Write-Host "  source /opt/ros/humble/setup.bash"
Write-Host "  cd $RemoteWs"
Write-Host "  colcon build --symlink-install --parallel-workers 1"
Write-Host "  source install/setup.bash"
Write-Host "  ros2 run xbot_bringup bringup_node"
