@echo off

REM compile shaders

if not exist Assets\Shaders mkdir Assets\Shaders

REM simple shader
Temp\shaderc.exe ^
-f Source/Shaders/test/test_v.sc -o Assets/Shaders/test_v.bin ^
--platform windows --type vertex --verbose -i ./ -p s_5_0

Temp\shaderc.exe ^
-f Source/Shaders/test/test_f.sc -o Assets/Shaders/test_f.bin ^
--platform windows --type fragment --verbose -i ./ -p s_5_0

if not exist Assets\Textures mkdir Assets\Textures

Temp\texturec.exe ^
-f Source/Assets/Textures/fieldstone-rgba.tga ^
-o Assets/Textures/fieldstone-rgba.dds

Temp\texturec.exe -n ^
-f Source/Assets/Textures/fieldstone-n.tga ^
-o Assets/Textures/fieldstone-n.dds

echo.
echo Press any key to exit . . .
pause >nul
