@echo OFF
set VCPKG_PATH=E:/build/vcpkg
set MSVC_DIR="C:/Program Files/Microsoft Visual Studio/2022/Community"
set CMAKE_EXE=%MSVC_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set CMAKE_EXE=^"%CMAKE_EXE:"=%^"

@rem set LLVM_PREFIX_DIR=%MSVC_DIR%/VC/Tools/Llvm/x64
@rem set LLVM_PREFIX_DIR=^"%LLVM_PREFIX_DIR:"=%^"
@rem set LLVM_DIR=%LLVM_PREFIX_DIR%/lib/cmake/llvm
@rem set LLVM_DIR=^"%LLVM_DIR:"=%^"
@rem set LLVM_TOOLS_BINARY_DIR=%LLVM_PREFIX_DIR%/bin
@rem set LLVM_TOOLS_BINARY_DIR=^"%LLVM_TOOLS_BINARY_DIR:"=%^"

@rem set CLANG_EXE=%LLVM_PREFIX_DIR%\bin\clang.exe
@rem IF NOT EXIST %CLANG_EXE% (
	@rem @ECHO "Installing clang"
	@rem @ECHO set MSVC_SETUP="C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe"
	@rem @ECHO %MSVC_SETUP% --add Microsoft.VisualStudio.Component.VC.CMake.Project --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset
	@rem exit /B 1
@rem )

set CUDA_TOOLKIT_ROOT_DIR="%CUDA_PATH:\=/%"
IF NOT EXIST %CUDA_TOOLKIT_ROOT_DIR% (
	@ECHO "Installing CUDA"
	@rem See https://developer.nvidia.com/cuda-toolkit-archive
	echo wget -O cuda_installer.exe https://developer.download.nvidia.com/compute/cuda/12.4.1/local_installers/cuda_12.4.1_551.78_windows.exe
	echo cuda_installer.exe -s nvcc_12.4 cudart_12.4 cuxxfilt_12.4 opencl_12.4 Display.Driver
	exit /B 1
)

@rem wget -O rocm.exe https://download.amd.com/developer/eula/rocm-hub/AMD-Software-PRO-Edition-23.Q4-Win10-Win11-For-HIP.exe
@rem rocm.exe -install

@rem The debug build produces more symbols than COFF can handle, so build only release:
@rem https://github.com/llvm/llvm-project/issues/87865#issuecomment-2041148045
@set VCPKG_OVERLAY_TRIPLETS="%~dp0cmake/vcpkg_triplets"
@set VCPKG_OVERLAY_TRIPLETS="%VCPKG_OVERLAY_TRIPLETS:\=/%"
@set BUILD_DIR=E:/build/AdaptiveCpp.win
@set LLVM_LIBRARY_DIRS=%BUILD_DIR%/vcpkg_installed/x64-windows-release
@set LLVM_TOOLS_BINARY_DIR=%LLVM_LIBRARY_DIRS%/tools/llvm


@rem Use the system python:
@rem set CONDA_EXE=C:\ProgramData\Miniconda3\condabin\conda.bat
@rem call %CONDA_EXE% activate
@rem set PYTHON_ROOT=C:/ProgramData/Miniconda3
@rem set OVERLAY_DIR="%~dp0cmake/overlays/python3"
@rem set OVERLAY_DIR=%OVERLAY_DIR:\=/%

@rem -DLLVM_PREFIX_DIR=%LLVM_PREFIX_DIR% -DLLVM_DIR=%LLVM_DIR% -DLLVM_TOOLS_BINARY_DIR=%LLVM_TOOLS_BINARY_DIR%
@rem -G "Visual Studio 17 2022" -T ClangCL
@rem -DVCPKG_OVERLAY_PORTS=%OVERLAY_DIR% -DPython3_ROOT_DIR=%PYTHON_ROOT%
@echo ON
%CMAKE_EXE% -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=%VCPKG_PATH%\scripts\buildsystems\vcpkg.cmake -DCMAKE_TOOLCHAIN_FILE=%VCPKG_PATH%\scripts\buildsystems\vcpkg.cmake -DVCPKG_OVERLAY_TRIPLETS=%VCPKG_OVERLAY_TRIPLETS% -DCUDA_TOOLKIT_ROOT_DIR=%CUDA_TOOLKIT_ROOT_DIR% -DWITH_CUDA_BACKEND=ON -DACPP_COMPILER_FEATURE_PROFILE=full-DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="ZIP;NSIS" -Bbuild.win -S.
