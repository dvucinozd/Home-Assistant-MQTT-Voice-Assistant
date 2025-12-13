<#
.SYNOPSIS
Kills lingering ESP-IDF monitor processes that often keep COM ports locked on Windows.

.EXAMPLE
.\tools\kill_monitor.ps1 -Port COM13

.EXAMPLE
.\tools\kill_monitor.ps1 -All

.EXAMPLE
.\tools\kill_monitor.ps1 -Port COM13 -ListOnly
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory = $false)]
  [string]$Port = $env:ESP32_COM_PORT,

  [Parameter(Mandatory = $false)]
  [switch]$All,

  [Parameter(Mandatory = $false)]
  [switch]$ListOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $All) {
  if ([string]::IsNullOrWhiteSpace($Port)) {
    $Port = "COM13"
  }
}

$monitorRegex = '(?i)(idf_monitor\.py|esp_idf_monitor|\\bidf\\.py(\\.exe)?\\b.*\\bmonitor\\b)'

function Get-MonitorProcesses {
  $procs = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -and ($_.CommandLine -match $monitorRegex) }
  if ($All) {
    return $procs
  }

  $portUpper = $Port.ToUpperInvariant()
  $portDevice = "\\.\" + $portUpper
  return $procs | Where-Object {
    $cmd = $_.CommandLine
    if (-not $cmd) { return $false }
    $u = $cmd.ToUpperInvariant()
    return ($u.Contains($portUpper) -or $u.Contains($portDevice.ToUpperInvariant()))
  }
}

$targets = @(Get-MonitorProcesses)

if ($targets.Count -eq 0) {
  if ($All) {
    Write-Host "No ESP-IDF monitor processes found."
  } else {
    Write-Host "No ESP-IDF monitor processes found for $Port."
  }
  exit 0
}

Write-Host "Found $($targets.Count) monitor-related process(es):"
$targets |
  Sort-Object ProcessId |
  Select-Object ProcessId, ParentProcessId, Name,
    @{ Name = "CommandLine"; Expression = { ($_.CommandLine -replace "\\s+", " ").Substring(0, [Math]::Min(180, ($_.CommandLine -replace "\\s+", " ").Length)) } } |
  Format-Table -AutoSize

if ($ListOnly) {
  exit 0
}

# Kill children first (rough heuristic: higher PID tends to be newer/child process).
foreach ($p in ($targets | Sort-Object ProcessId -Descending)) {
  try {
    Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop
    Write-Host "Killed PID $($p.ProcessId) ($($p.Name))"
  } catch {
    Write-Host "Failed to kill PID $($p.ProcessId) ($($p.Name)): $($_.Exception.Message)"
  }
}

exit 0

