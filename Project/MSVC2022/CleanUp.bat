@echo off

rem MS Visual Studio specific ---
if exist .vs rmdir .vs /Q /S
for %%D in (ARM64 Win32 x64) do if exist %%D rmdir %%D /Q /S
for %%P in (Library RegressionTest Example Fuzz) do call :clean %%P
goto :eof

:clean
pushd %1
for %%D in (ARM64 Win32 x64) do if exist %%D rmdir %%D /Q /S
if exist *.user del *.user
popd
goto :eof
