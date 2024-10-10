set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)


set(TOOLCHAIN_PATH "/opt/gcc-arm-none-eabi-10.3-2021.10/bin")
set(TOOLCHAIN_PREFIX "${TOOLCHAIN_PATH}/arm-none-eabi-")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CPU_PARAMETERS "-mthumb -mcpu=${CFG_SELECTED_MCU} -mfpu=auto -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT "${CPU_PARAMETERS}" CACHE INTERNAL "")
set(CMAKE_CXX_FLAGS_INIT "${CPU_PARAMETERS}" CACHE INTERNAL "")
set(CMAKE_ASM_FLAGS_INIT "${CPU_PARAMETERS}" CACHE INTERNAL "")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-T${CFG_LINKER_FILE} ${CPU_PARAMETERS} -Wl,-Map=memory.map --specs=nosys.specs -u _printf_float -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group -Wl,--print-memory-usage" CACHE INTERNAL "")

set(COMPILE_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused-parameter")

# Options for DEBUG build
# -Og   Enables optimizations that do not interfere with debugging.
# -g    Produce debugging information in the operating system’s native format.
set(CMAKE_C_FLAGS_DEBUG "-Og -g ${COMPILE_FLAGS}" CACHE INTERNAL "C Compiler options for debug build type")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g ${COMPILE_FLAGS}" CACHE INTERNAL "C++ Compiler options for debug build type")
set(CMAKE_ASM_FLAGS_DEBUG "-g ${COMPILE_FLAGS}" CACHE INTERNAL "ASM Compiler options for debug build type")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "" CACHE INTERNAL "Linker options for debug build type")

# Options for RELEASE build
# -Os   Optimize for size. -Os enables all -O2 optimizations.
# -flto Runs the standard link-time optimizer.
set(CMAKE_C_FLAGS_RELEASE "-Os -flto" CACHE INTERNAL "C Compiler options for release build type")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -flto" CACHE INTERNAL "C++ Compiler options for release build type")
set(CMAKE_ASM_FLAGS_RELEASE "" CACHE INTERNAL "ASM Compiler options for release build type")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto" CACHE INTERNAL "Linker options for release build type")

#---------------------------------------------------------------------------------------
# Set compilers
#---------------------------------------------------------------------------------------
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc CACHE INTERNAL "C Compiler")
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++ CACHE INTERNAL "C++ Compiler")
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc CACHE INTERNAL "ASM Compiler")

set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE_UTIL ${TOOLCHAIN_PREFIX}size)

set(CMAKE_FIND_ROOT_PATH ${BINUTILS_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
