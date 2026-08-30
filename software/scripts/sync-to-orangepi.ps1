param(
  [string]$HostName = "192.168.1.14",
  [string]$User = "orangepi",
  [string]$RemoteWs = "~/xbot_ws",
  [switch]$ScriptsOnly
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$SrcDir = Join-Path $RepoRoot "software\src"
$ScriptsDir = Join-Path $RepoRoot "software\scripts"

if (-not $ScriptsOnly -and -not (Test-Path (Join-Path $SrcDir "xbot_bringup"))) {
  throw "Source not found: $SrcDir"
}

$target = "${User}@${HostName}"

if (-not $ScriptsOnly) {
  Write-Host "Sync $SrcDir -> ${target}:${RemoteWs}/src"
  ssh $target "mkdir -p $RemoteWs/src"
  if ($LASTEXITCODE -ne 0) { throw "ssh mkdir failed ($LASTEXITCODE)" }
  scp -r "$SrcDir\*" "${target}:${RemoteWs}/src/"
  if ($LASTEXITCODE -ne 0) { throw "scp src failed ($LASTEXITCODE)" }
}

Write-Host "Sync $ScriptsDir -> ${target}:${RemoteWs}/scripts"
ssh $target "mkdir -p $RemoteWs/scripts"
if ($LASTEXITCODE -ne 0) { throw "ssh mkdir scripts failed ($LASTEXITCODE)" }
scp -r "$ScriptsDir\*" "${target}:${RemoteWs}/scripts/"
if ($LASTEXITCODE -ne 0) { throw "scp scripts failed ($LASTEXITCODE)" }

Write-Host "Done. On the board:"
Write-Host "  chmod +x $RemoteWs/scripts/*.sh"
Write-Host "  # Audio route (HP=earphone, SPK=robot speaker) + USB mic default:"
Write-Host "  bash $RemoteWs/scripts/orangepi_audio_setup.sh HP"
Write-Host "  bash $RemoteWs/scripts/install_orangepi_audio_service.sh HP"
Write-Host "  bash $RemoteWs/scripts/orangepi_mic_test.sh"
Write-Host "  # LCD / STM32 / (see docs/orangepi_system_setup.md)"
Write-Host "  source /opt/ros/humble/setup.bash"
Write-Host "  cd $RemoteWs"
Write-Host "  colcon build --symlink-install --parallel-workers 1"
Write-Host "  source install/setup.bash"
Write-Host "  ros2 run xbot_bringup bringup_node"
