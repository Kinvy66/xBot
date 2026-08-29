param(
  [switch]$Flash,
  [ValidateSet("StLink", "JLink")]
  [string]$Probe = "StLink",
  [string]$BuildType = "Debug",
  [string]$CltRoot = "",
  [string]$JLinkExe = "",
  [string]$JLinkDevice = "STM32F103C8",
  [int]$JLinkSpeed = 4000
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Invoke-AppBuild {
  param([string]$Type)

  $preset = $Type
  $dir = Join-Path $Root "build\$preset"
  $elfPath = Join-Path $dir "xBot.elf"

  if (-not (Test-Path (Join-Path $dir "build.ninja"))) {
    Write-Host "Configuring CMake ($preset)..."
    # Out-Host: keep cmake stdout off the success stream so callers get only $elfPath
    cmake --preset $preset | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
  }

  Write-Host "Building ($preset)..."
  cmake --build --preset $preset -j | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }

  if (-not (Test-Path $elfPath)) {
    throw "ELF not found: $elfPath"
  }
  Write-Host "Built: $elfPath"
  $hex = Join-Path $dir "xBot.hex"
  $bin = Join-Path $dir "xBot.bin"
  if (Test-Path $hex) { Write-Host "Hex:   $hex" }
  if (Test-Path $bin) { Write-Host "Bin:   $bin" }
  return $elfPath
}

function Test-CubeCltRoot {
  param([string]$Path)
  if (-not $Path) { return $false }
  return (Test-Path (Join-Path $Path "CMake\bin\cmake.exe")) -and
         (Test-Path (Join-Path $Path "Ninja\bin\ninja.exe")) -and
         (Test-Path (Join-Path $Path "GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe"))
}

function Resolve-CubeCltRoot {
  param([string]$Explicit)

  if ($Explicit) {
    if (-not (Test-CubeCltRoot $Explicit)) {
      throw "Invalid CubeCLT root (missing cmake/ninja/gcc): $Explicit"
    }
    return (Resolve-Path $Explicit).Path
  }

  foreach ($envName in @("STM32CUBECLT_PATH", "STM32_CUBE_CLT_PATH", "CUBECLT_PATH")) {
    $fromEnv = [Environment]::GetEnvironmentVariable($envName)
    if ($fromEnv -and (Test-CubeCltRoot $fromEnv)) {
      return (Resolve-Path $fromEnv).Path
    }
  }

  # Prefer tools already on PATH (may come from CLion/user env).
  $cmake = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
  if ($cmake) {
    # ...\STM32CubeCLT_x.y.z\CMake\bin\cmake.exe -> CltRoot
    $maybe = Split-Path (Split-Path (Split-Path $cmake.Source -Parent) -Parent) -Parent
    if (Test-CubeCltRoot $maybe) { return $maybe }
  }

  $searchRoots = @("C:\ST", "C:\Program Files\ST", "C:\Program Files (x86)\ST")
  $found = @()
  foreach ($base in $searchRoots) {
    if (-not (Test-Path $base)) { continue }
    $found += Get-ChildItem $base -Directory -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -like "STM32CubeCLT*" -and (Test-CubeCltRoot $_.FullName) }
  }

  if ($found.Count -eq 0) {
    throw @"
STM32CubeCLT not found.
Install STM32CubeCLT, set STM32CUBECLT_PATH, or pass -CltRoot <path>.
Expected layout: <root>\CMake\bin\cmake.exe
"@
  }

  # Newest install by folder name (e.g. STM32CubeCLT_1.21.0 > 1.19.0).
  $best = $found | Sort-Object Name -Descending | Select-Object -First 1
  return $best.FullName
}

function Resolve-JLinkExe {
  param([string]$Explicit)

  if ($Explicit) {
    if (-not (Test-Path $Explicit)) {
      throw "JLinkExe not found: $Explicit"
    }
    return (Resolve-Path $Explicit).Path
  }

  foreach ($envName in @("JLINK_PATH", "SEGGER_JLINK_PATH")) {
    $fromEnv = [Environment]::GetEnvironmentVariable($envName)
    if (-not $fromEnv) { continue }
    if (Test-Path $fromEnv -PathType Leaf) { return (Resolve-Path $fromEnv).Path }
    $exe = Join-Path $fromEnv "JLink.exe"
    if (Test-Path $exe) { return (Resolve-Path $exe).Path }
  }

  $fromPath = Get-Command "JLink.exe" -ErrorAction SilentlyContinue
  if ($fromPath) { return $fromPath.Source }

  $searchRoots = @(
    "C:\Program Files\SEGGER",
    "C:\Program Files (x86)\SEGGER"
  )
  $found = @()
  foreach ($base in $searchRoots) {
    if (-not (Test-Path $base)) { continue }
    $found += Get-ChildItem $base -Directory -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -like "JLink*" } |
      ForEach-Object { Join-Path $_.FullName "JLink.exe" } |
      Where-Object { Test-Path $_ }
  }

  if ($found.Count -eq 0) {
    throw "JLink.exe not found. Install SEGGER J-Link, set JLINK_PATH, or pass -JLinkExe <path>."
  }

  # Prefer versioned dirs (JLink_V944) newest-first; fall back to plain JLink.
  # @()：仅 1 个匹配时 Sort-Object 返回字符串，[0] 会取到路径首字符 'C'
  $ranked = @($found | Sort-Object {
    $dir = Split-Path $_ -Parent | Split-Path -Leaf
    if ($dir -match 'V(\d+)') { [int]$Matches[1] } else { 0 }
  } -Descending)

  return $ranked[0]
}

function Invoke-StLinkFlash {
  param([string]$ElfPath, [string]$Clt)

  $Prog = Join-Path $Clt "STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
  if (-not (Test-Path $Prog)) {
    throw "STM32_Programmer_CLI not found: $Prog"
  }
  Write-Host "Flashing via ST-Link (SWD)..."
  Write-Host "Using: $Prog"
  & $Prog -c port=SWD mode=UR -w $ElfPath -v -rst
  if ($LASTEXITCODE -ne 0) { throw "ST-Link flash failed ($LASTEXITCODE)" }
}

function Resolve-JLinkGdbServer {
  param([string]$JLinkExePath)

  $dir = Split-Path -Parent $JLinkExePath
  $cl = Join-Path $dir "JLinkGDBServerCL.exe"
  if (-not (Test-Path $cl)) {
    throw "JLinkGDBServerCL.exe not found next to JLink.exe: $dir"
  }
  return $cl
}

function Invoke-JLinkFlash {
  param(
    [string]$ElfPath,
    [string]$Device,
    [string]$Exe,
    [string]$Clt,
    [int]$Speed
  )

  # Prefer the same GDB Server path as CLion when only J-Link is available.
  $jlink = Resolve-JLinkExe -Explicit $Exe
  $gdbServer = Resolve-JLinkGdbServer -JLinkExePath $jlink
  $gdb = Join-Path $Clt "GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe"
  if (-not (Test-Path $gdb)) {
    throw "arm-none-eabi-gdb.exe not found: $gdb"
  }

  $port = 2331
  $freq = [Math]::Max(100, [Math]::Min($Speed, 4000))
  Write-Host "Flashing via J-Link GDB Server (device=$Device, speed=$freq)"
  Write-Host "GDBServer: $gdbServer"
  Write-Host "GDB: $gdb"
  Write-Host "Hint: stop CLion Debug first if the probe is busy."

  # Fast port probe (avoid Get-NetTCPConnection — it is often multi-second per call on Windows).
  function Test-LocalPortOpen([int]$Port) {
    try {
      $tcp = New-Object System.Net.Sockets.TcpClient
      $iar = $tcp.BeginConnect("127.0.0.1", $Port, $null, $null)
      $ok = $iar.AsyncWaitHandle.WaitOne(200, $false)
      if ($ok -and $tcp.Connected) {
        $tcp.EndConnect($iar)
        $tcp.Close()
        return $true
      }
      try { $tcp.Close() } catch { }
      return $false
    } catch {
      return $false
    }
  }

  Write-Host "Waiting for port $port to be free..."
  for ($i = 0; $i -lt 50; ++$i) {
    if (-not (Test-LocalPortOpen $port)) { break }
    Start-Sleep -Milliseconds 100
  }

  # No -singlerun: script owns server lifetime (finally Stop-Process).
  # TcpClient probe is safe without -singlerun (server keeps listening after Close).
  $gdbArgs = @(
    "-device", $Device,
    "-if", "SWD",
    "-speed", "$freq",
    "-port", "$port",
    "-swoport", "2332",
    "-telnetport", "2333",
    "-halt",
    "-nogui",
    "-silent"
  )

  Write-Host "Starting JLinkGDBServerCL..."
  $serverProc = Start-Process -FilePath $gdbServer -ArgumentList $gdbArgs -PassThru -WindowStyle Hidden
  try {
    Write-Host "Waiting for GDB port $port..."
    $ready = $false
    for ($i = 0; $i -lt 50; ++$i) {
      Start-Sleep -Milliseconds 100
      if ($serverProc.HasExited) {
        throw "JLinkGDBServerCL exited early (code=$($serverProc.ExitCode)). Is another debug session using the probe?"
      }
      if (Test-LocalPortOpen $port) {
        $ready = $true
        break
      }
    }
    if (-not $ready) {
      throw "JLinkGDBServerCL did not open port $port in time"
    }

    $gdbScript = Join-Path $env:TEMP ("xbot_flash_{0}.gdb" -f [guid]::NewGuid().ToString("N"))
    @"
set confirm off
set pagination off
target remote localhost:$port
monitor reset
monitor halt
load
monitor reset
monitor go
disconnect
quit
"@ | Set-Content -Path $gdbScript -Encoding ASCII

    Write-Host "Running gdb load..."
    try {
      $elfUnix = ($ElfPath -replace '\\', '/')
      # 用文件重定向捕获 stdout/stderr，避免 PowerShell 把 gdb 的 stderr 当成终止错误
      $outLog = Join-Path $env:TEMP ("xbot_gdb_{0}.out.txt" -f [guid]::NewGuid().ToString("N"))
      $errLog = Join-Path $env:TEMP ("xbot_gdb_{0}.err.txt" -f [guid]::NewGuid().ToString("N"))
      $gdbProc = Start-Process -FilePath $gdb `
        -ArgumentList @("--batch", "-x", $gdbScript, $elfUnix) `
        -Wait -PassThru -NoNewWindow `
        -RedirectStandardOutput $outLog `
        -RedirectStandardError $errLog
      $gdbExit = $gdbProc.ExitCode
      $outLines = @()
      if (Test-Path $outLog) {
        $outLines += Get-Content -Path $outLog -ErrorAction SilentlyContinue
      }
      if (Test-Path $errLog) {
        $outLines += Get-Content -Path $errLog -ErrorAction SilentlyContinue
      }
      Remove-Item -Path $outLog, $errLog -ErrorAction SilentlyContinue
      # gdb/JLink 可能夹带 ANSI 颜色码；原样 Write-Host 会把终端前景色“粘住”
      $ansi = [regex]'(\x1B\[[0-9;]*[A-Za-z]|\x1B\][^\x07]*\x07)'
      $outLines = $outLines | ForEach-Object { $ansi.Replace("$_", "") }
      $outLines | ForEach-Object { Write-Host $_ }
      try { [Console]::ResetColor() } catch { }
      Write-Host ("{0}[0m" -f [char]27) -NoNewline
      $text = ($outLines -join "`n")
      if ($gdbExit -ne 0) {
        throw "arm-none-eabi-gdb flash failed (exit=$gdbExit)"
      }
      if ($text -match "Remote communication error|Connection timed out|No connection|failed to load|Error erasing|Memory write failed") {
        throw "J-Link GDB flash failed (see gdb output above)"
      }
      if ($text -notmatch "Loading section|Transfer rate|Start address") {
        Write-Host "Warning: gdb output missing typical load markers; check target serial log."
      }
    }
    finally {
      Remove-Item -Path $gdbScript -ErrorAction SilentlyContinue
    }
  }
  finally {
    if ($serverProc -and -not $serverProc.HasExited) {
      Stop-Process -Id $serverProc.Id -Force -ErrorAction SilentlyContinue
      try { $serverProc.WaitForExit(3000) | Out-Null } catch { }
    }
  }
}

$CltRoot = Resolve-CubeCltRoot -Explicit $CltRoot
Write-Host "CubeCLT: $CltRoot"
$env:PATH = "$CltRoot\CMake\bin;$CltRoot\Ninja\bin;$CltRoot\GNU-tools-for-STM32\bin;$env:PATH"

foreach ($tool in @("cmake.exe", "ninja.exe", "arm-none-eabi-gcc.exe")) {
  if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
    throw "Required tool not found in PATH: $tool (CubeCLT: $CltRoot)"
  }
}

# Invoke-AppBuild may still emit host text; take the last string as the ELF path.
$Elf = @(Invoke-AppBuild -Type $BuildType) | Select-Object -Last 1
if (-not $Elf -or -not (Test-Path -LiteralPath $Elf)) {
  throw "Build did not return a valid ELF path (got: $Elf)"
}

if ($Flash) {
  switch ($Probe) {
    "StLink" {
      Invoke-StLinkFlash -ElfPath $Elf -Clt $CltRoot
    }
    "JLink" {
      Invoke-JLinkFlash -ElfPath $Elf -Device $JLinkDevice -Exe $JLinkExe -Clt $CltRoot -Speed $JLinkSpeed
    }
  }
  Write-Host "Flash done."
}
