$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$shots = @(
 @{name='humanoid-raw'; scene='dae/custom/humanoid.dae'; ops='hud-off'; view=@(0.35,0.15,1.05)},
 @{name='humanoid-loop1'; scene='dae/custom/humanoid.dae'; ops='hud-off,loop,smooth'; view=@(0.35,0.15,1.05)},
 @{name='humanoid-loop2'; scene='dae/custom/humanoid.dae'; ops='hud-off,loop,loop,smooth'; view=@(0.35,0.15,1.05)},
 @{name='humanoid-phong'; scene='dae/custom/humanoid.dae'; ops='hud-off,loop,loop,smooth,phong'; view=@(0.35,0.15,1.05)},
 @{name='humanoid-env'; scene='dae/custom/humanoid.dae'; ops='hud-off,loop,loop,smooth,env'; view=@(0.35,0.15,1.05)},
 @{name='humanoid-env-side'; scene='dae/custom/humanoid.dae'; ops='hud-off,loop,loop,smooth,env'; view=@(0.9,0.25,1.05)}
)
New-Item -ItemType Directory -Force (Join-Path $taskRoot 'docs/screenshots') | Out-Null
Push-Location (Join-Path $taskRoot 'code')
try {
 foreach($shot in $shots) {
  $shotFile = "../docs/screenshots/$($shot.name).png"
  $arguments = @($shot.scene,'--screenshot',$shotFile,'--ops',$shot.ops,'--view') + $shot.view
  & ../bin/meshedit.exe @arguments
  if($LASTEXITCODE -ne 0) {throw "Screenshot failed: $($shot.name)"}
 }
} finally {Pop-Location}
$shots | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $PSScriptRoot 'bonus-screenshots.json') -Encoding utf8

