setlocal
call clean.bat
call ..\Tools\SetVsEnv x86

stampinf -f qemufwcfg.inf -d "*" -v "2.1.0.100"
inf2cat /uselocaltime /driver:. /os:8_X86,8_X64,Server8_X64,6_3_X86,6_3_X64,Server6_3_X64,10_X86,10_X64,Server10_X64

mkdir Install
copy qemufwcfg.* .\Install\

endlocal
