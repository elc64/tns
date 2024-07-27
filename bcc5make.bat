@echo off
set PATH=C:\Borland\BCC55\Bin;%PATH%
del /f tns.exe
make -f tns.mak
echo\
echo Done.
start tns.exe
ping 127.0.0.1 -n 64 -w 1000 > nul
