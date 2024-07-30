@echo OFF
set MSVC_DIR="C:/Program Files/Microsoft Visual Studio/2022/Community"
set CMAKE_EXE=%MSVC_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set CMAKE_EXE=^"%CMAKE_EXE:"=%^"

set VCPKG_PATH=e:/build/vcpkg
set BUILD_PATH=e:/build/AdaptiveCpp.win
set VCPKG_PATH=%VCPKG_PATH_BACK:\=/%

set CUDA_TOOLKIT_ROOT_DIR="%CUDA_PATH:\=/%"
IF NOT EXIST %CUDA_TOOLKIT_ROOT_DIR% (
	@ECHO "Installing CUDA"
	@rem See https://developer.nvidia.com/cuda-toolkit-archive
	curl -o cuda_installer.exe https://developer.download.nvidia.com/compute/cuda/12.6.2/local_installers/cuda_12.6.2_560.94_windows.exe
	echo call cuda_installer.exe -s nvcc_12.6 cudart_12.6 cuxxfilt_12.6 opencl_12.6 Display.Driver
	exit /B 1
)

@REM wget -O rocm.exe https://download.amd.com/developer/eula/rocm-hub/AMD-Software-PRO-Edition-23.Q4-Win10-Win11-For-HIP.exe
@REM rocm.exe -install

@REM This builds LLVM first, so the cmake script below can use it
set SOURCE_PATH=%CD%
pushd %VCPKG_PATH_BACK% & vcpkg.exe install --triplet x64-windows-release --overlay-triplets=%SOURCE_PATH%/cmake/vcpkg_triplets --x-install-root=%BUILD_PATH%/release-win/vcpkg_installed llvm[clang,lld,enable-rtti,export-symbols,target-x86,target-nvptx] & popd
@REM __%VCPKG_PATH_BACK%\vcpkg.exe install --triplet x64-windows-release --overlay-triplets=%SOURCE_PATH%/cmake/vcpkg_triplets

%CMAKE_EXE% --preset release-win
