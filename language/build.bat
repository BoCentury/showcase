@echo off

rem -wd4201 nameless struct/union
rem  wd4100 unreferenced procedure parameter
rem -wd4189 local variable is initialized but not referenced
rem -wd4505 unreferenced local function has been removed 
rem -wd4477 printf wrong type, disabled for modified stb_sprintf using %S for string instead of wchar_t *
rem -w44061 enum member is not explicitly handled by a case label
rem -w44062 enum member is not handled

rem CL/LINK env variables are prepended to compiler/linker options automatically
rem _CL_/_LINK_ is the same but appended

set CL= -Od -Oi -MTd -Z7 -Gm- -GR-  -nologo -W4 -WX -wd4201 -wd4100 -wd4189 -wd4505 -wd4477 -FC -fp:fast -fp:except- -std:c++latest -diagnostics:caret -D_INTERNAL_BUILD=1 -Zc:strictStrings-
set LINK= -incremental:no -opt:ref  dbghelp.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

cl -Fe:compile ..\code\win32.cpp

popd