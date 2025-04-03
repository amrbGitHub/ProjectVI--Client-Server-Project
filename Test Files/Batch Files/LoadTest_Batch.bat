@echo off
SET /A "index = 1"
SET /A "count = 50"
SET "IP=10.144.10.43"
SET "PORT=12345"
SET "FILE=Telem_2023_3_12_14_56_40.txt"

:while
if %index% leq %count% (
     START /MIN Client.exe %IP% %PORT% %FILE%
     SET /A index = %index% + 1
     @echo Starting client %index%
     goto :while
)

