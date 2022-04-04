call vcvars32
call copy-files
rmdir /s /q Release
msbuild keymap2cpp.vcxproj /p:Configuration=Release
Release\keymap2cpp
rmdir /s /q Release
msbuild skin2cpp.vcxproj /p:Configuration=Release
Release\skin2cpp
rmdir /s /q Release
msbuild Plus42Binary.vcxproj /p:Configuration=Release
move Release\Plus42Binary.exe .
rmdir /s /q Release
msbuild Plus42Decimal.vcxproj /p:Configuration=Release
move Release\Plus42Decimal.exe .
rmdir /s /q Release
