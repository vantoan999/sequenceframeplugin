@echo off

set a=0

setlocal EnableDelayedExpansion

for %%n in (*.png) do (

if !a! lss 10 (
    ren "%%n" "frame_00000!a!.png"
) else if !a! lss 100 (
    ren "%%n" "frame_0000!a!.png"
) else (
    ren "%%n" "frame_000!a!.png"
)
echo !a!

set /A a+=1

)