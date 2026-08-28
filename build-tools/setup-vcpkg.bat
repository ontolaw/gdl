@echo off
set VCPKG_ROOT = "e:\tools\vcpkg"

:: update VCPKG by pulling in latest "PortFile" changes from the GIT repository
pushd "%VCPKG_ROOT%"
git pull
popd

:: disable metrics tracking by creating "vcpkg.disable-metrics" in the VCPKG ROOT folder
IF NOT EXIST "%VCPKG_ROOT%\vcpkg.disable-metrics" type nul>"%VCPKG_ROOT%\vcpkg.disable-metrics"

call %VCPKG_ROOT%\bootstrap-vcpkg.bat -disableMetrics

set "VCPKG_DEFAULT_TRIPLET=x64-windows-static"
call %VCPKG_ROOT%\vcpkg install --feature-flags=manifests
call %VCPKG_ROOT%\vcpkg update --feature-flags=manifests

call %VCPKG_ROOT%\vcpkg integrate install

pause
