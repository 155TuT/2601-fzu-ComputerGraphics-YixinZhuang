$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$shots = @(
 @{name='bezier-curve'; scene='bzc/curve1.bzc'; ops='CEEE'; view=@(0,0,1)},
 @{name='bezier-curve2'; scene='bzc/curve2.bzc'; ops='CEEEE'; view=@(0,0,1)},
 @{name='bezier-teapot'; scene='bez/teapot.bez'; ops='hud-off,smooth,phong'; view=@(0.4,0.3,1.5)},
 @{name='teapot-flat'; scene='dae/teapot.dae'; ops='hud-off,phong'; view=@(0.4,0.3,1.5)},
 @{name='teapot-smooth'; scene='dae/teapot.dae'; ops='hud-off,smooth,phong'; view=@(0.4,0.3,1.5)},
 @{name='flip-before'; scene='dae/teapot.dae'; ops='hud-off,focus:100,select:100'; view=@(0,0,1)},
 @{name='flip-after'; scene='dae/teapot.dae'; ops='hud-off,focus:100,flip:100'; view=@(0,0,1)},
 @{name='split-before'; scene='dae/teapot.dae'; ops='hud-off,focus:100,select:100'; view=@(0,0,1)},
 @{name='split-after'; scene='dae/teapot.dae'; ops='hud-off,focus:100,split:100'; view=@(0,0,1)},
 @{name='mix-before'; scene='dae/teapot.dae'; ops='hud-off,focus:100,select:100'; view=@(0,0,1)},
 @{name='mix-after'; scene='dae/teapot.dae'; ops='hud-off,focus:100,split:100,flip:100'; view=@(0,0,1)},
 @{name='boundary-before'; scene='dae/open-square.dae'; ops='hud-off,select:1'; view=@(0,1.2,1.3)},
 @{name='boundary-after'; scene='dae/open-square.dae'; ops='hud-off,split:1'; view=@(0,1.2,1.3)},
 @{name='boundary-loop1'; scene='dae/open-square.dae'; ops='hud-off,loop'; view=@(0,1.2,1.3)},
 @{name='boundary-loop3'; scene='dae/open-square.dae'; ops='hud-off,loop,loop,loop'; view=@(0,1.2,1.3)},
 @{name='cube-raw'; scene='dae/cube.dae'; ops='hud-off'; view=@(0.7,0.4,1.15)},
 @{name='cube-loop1'; scene='dae/cube.dae'; ops='hud-off,loop'; view=@(0.7,0.4,1.15)},
 @{name='cube-loop2'; scene='dae/cube.dae'; ops='hud-off,loop,loop,smooth'; view=@(0.7,0.4,1.15)},
 @{name='cube-loop3'; scene='dae/cube.dae'; ops='hud-off,loop,loop,loop,smooth'; view=@(0.7,0.4,1.15)},
 @{name='cube-preprocessed'; scene='dae/cube.dae'; ops='hud-off,symmetric'; view=@(0.7,0.4,1.15)},
 @{name='cube-symmetric-loop2'; scene='dae/cube.dae'; ops='hud-off,symmetric,loop,loop,smooth'; view=@(0.7,0.4,1.15)},
 @{name='cube-symmetric-loop3'; scene='dae/cube.dae'; ops='hud-off,symmetric,loop,loop,loop,smooth'; view=@(0.7,0.4,1.15)},
 @{name='teapot-loop1'; scene='dae/teapot.dae'; ops='hud-off,loop,smooth,phong'; view=@(0.4,0.3,1.5)}
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
$shots | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $PSScriptRoot 'screenshots.json') -Encoding utf8
