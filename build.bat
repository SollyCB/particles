@echo off

set lib_src="lib_src.dll"
set lib_src_temp="lib_src_temp.dll"

set cl_flags=-FC -GR- -EHa- -nologo -Zi -W4 -WX -wd4201 -wd4100 -wd4098 -DSDL_MAIN_HANDLED -DDEBUG -Fm -Oi -MT -I C:\VulkanSDK\1.3.296.0\Include\
::set cl_flags=-FC -GR- -EHa- -nologo -Zi -W4 -WX -wd4201 -wd4100 -wd4098 -DSDL_MAIN_HANDLED -Fm -Oi -O2 -MT -I C:\VulkanSDK\1.3.296.0\Include\

set link_flags=/nologo /incremental:no /opt:ref C:\VulkanSDK\1.3.296.0\Lib\vulkan-1.lib C:\VulkanSDK\1.3.296.0\Lib\SDL2.lib

pushd build\
cl %cl_flags% ..\lib_src.c -Felib_src_temp -LD /link %link_flags%
cl %cl_flags% ..\exe_src.c /link %link_flags%
popd
