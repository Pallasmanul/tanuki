cmake_minimum_required(VERSION 3.12)

set(DEVICE "stm32f103c8t6")
add_definitions(-DSTM32F1)

find_program(CMAKE cmake)
if (NOT CMAKE)
  message(FATAL_ERROR "cmake is used to compile component which build with cmake, please install it.")
endif ()
find_program(MAKE make)
if (NOT MAKE)
  message(FATAL_ERROR "make is used to compile component which build with make, please install it.")
endif ()
find_program(OPENOCD openocd)
if (NOT OPENOCD)
  message (FATAL_ERROR "openocd is used to flash program, please install it.")
endif ()
find_program(PYTHON python)
if (NOT PYTHON)
  message(FATAL_ERROR "python is required to generate the linker script, please install it.")
endif ()
find_program(COMPILER arm-none-eabi-gcc)
if (NOT COMPILER)
  message(FATIAL_ERROR "arm-none-eabi-gcc is cross compile toolchain, please install it.")
endif()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(OBJCOPY arm-none-eabi-objcopy)
set(OBJDUMP arm-none-eabi-objdump)
set(OBJSIZE arm-none-eabi-size)
set(OOCD openocd)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_C_FLAGS "-Wextra -Wshadow -Wimplicit-function-declaration -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes  -fno-common -ffunction-sections -fdata-sections ")
set(CMAKE_CXX_FLAGS "-Wextra -Wshadow -Wredundant-decls  -Weffc++ -fno-common -ffunction-sections -fdata-sections")

option(DEBUG "project debug" OFF)

if(DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
else()
  set(CMAKE_BUILD_TYPE Release)
endif(DEBUG)

enable_language(C CXX)
project(tanuki LANGUAGES C CXX)

list(APPEND TANUKI_SOURCE
  main.c
  board/board.c
  )

list(TRANSFORM TANUKI_SOURCE PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/)


# libopencm3

# Generate linker information for device, based on libopencm3/mk/genlink-config.mk
if (NOT DEVICE)
  message(FATAL_ERROR "No DEVICE specified for linker script generator")
endif ()

find_file(LIBOPENCM3_DIR "libopencm3" "${CMAKE_CURRENT_SOURCE_DIR}" PATH_SUFFXIES "lib/")
if (LIBOPENCM3_DIR STREQUAL "LIBOPENCM3_DIR-NOTFOUND")
  message(FATAL_ERROR "Could not locate libopencm3 directory , Please update submodule")
endif ()

set(GENLINK_SCRIPT "${LIBOPENCM3_DIR}/scripts/genlink.py")
set(DEVICES_DATA "${LIBOPENCM3_DIR}/ld/devices.data")

execute_process(
  COMMAND "${PYTHON}" "${GENLINK_SCRIPT}" "${DEVICES_DATA}" "${DEVICE}" "FAMILY"
  OUTPUT_VARIABLE GENLINK_FAMILY
  )
execute_process(
  COMMAND "${PYTHON}" "${GENLINK_SCRIPT}" "${DEVICES_DATA}" "${DEVICE}" "USBFAMILY"
  OUTPUT_VARIABLE GENLINK_SUBFAMILY
  )
execute_process(
  COMMAND "${PYTHON}" "${GENLINK_SCRIPT}" "${DEVICES_DATA}" "${DEVICE}" "CPU"
  OUTPUT_VARIABLE GENLINK_CPU
  )
execute_process(
  COMMAND "${PYTHON}" "${GENLINK_SCRIPT}" "${DEVICES_DATA}" "${DEVICE}" "FPU"
  OUTPUT_VARIABLE GENLINK_FPU
  )
execute_process(
  COMMAND "${PYTHON}" "${GENLINK_SCRIPT}" "${DEVICES_DATA}" "${DEVICE}" "CPPFLAGS"
  OUTPUT_VARIABLE GENLINK_CPPFLAGS
  )
execute_process(
  COMMAND "${PYTHON}" "${GENLINK_SCRIPT}" "${DEVICES_DATA}" "${DEVICE}" "DEFS"
  OUTPUT_VARIABLE GENLINK_DEFS
  )

set(ARCH_FLAGS "-mcpu=${GENLINK_CPU}")
set(CORTEX_CPU cortex-m0 cortex-m0plus cortex-m3 cortex-m4 cortex-m7)

list(FILTER CORTEX_CPU INCLUDE REGEX ${GENLINK_CPU})
if (GENLINK_CPU STREQUAL CORTEX_CPU)
  list(APPEND ARCH_FLAGS "-mthumb -mthumb-interwork")
endif ()

# Check FPU
if (GENLINK_FPU STREQUAL "soft")
  list(APPEND ARCH_FLAGS "-msoft-float")
elseif (GENLINK_FPU STREQUAL "hard-fpv4-sp-d16")
  list(APPEND ARCH_FLAGS "-mfloat-abi=hard -mfpu=fpv4-sp-d16")
elseif (GENLINK_FPU STREQUAL "hard-fpv5-sp-d16")
  list(APPEND ARCH_FLAGS "-mfloat-abi=hard -mfpu=fpv5-sp-d16")
else ()
  message(WARNING "No match for the FPU flags")
endif ()

# Check family
if (NOT GENLINK_FAMILY)
  message(WARNING "${DEVICE} not found in ${DEVICES_DATA}")
endif ()

# Linker stuff
set(LINKER_SCRIPT "generated.${DEVICE}.ld")

if (EXISTS "${LIBOPENCM3_DIR}/lib/libopencm3_${GENLINK_FAMILY}.a")
  set(LDLIBS "${LDLIBS} -lopencm3_${GENLINK_FAMILY}")
else ()
  if (EXISTS "${LIBOPENCM3_DIR}/lib/libopencm3_${GENLINK_SUBFAMILY}.a")
    set(LDLIBS "${LDLIBS} -lopencm3_${GENLINK_SUBFAMILY}")
  else ()
    message(WARNING "${LIBOPENCM3_DIR}/lib/libopencm3_${GENLINK_FAMILY}. A library variant for the selected device does not exist.")
  endif ()
endif ()



function(JOIN VALUES GLUE OUTPUT)
  string(REGEX REPLACE "([^\\]|^);" "\\1${GLUE}" _TMP_STR "${VALUES}")
  string(REGEX REPLACE "[\\](.)" "\\1" _TMP_STR "${_TMP_STR}") #fixes escaping
  set(${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()
# ARCH_FLAGS and GENLINK_DEFS has to be passed as list here
string(REPLACE " " ";" GENLINK_DEFS ${GENLINK_DEFS})
# Get rid of any spaces and turn the thing into a list
JOIN("${ARCH_FLAGS}" " " ARCH_FLAGS)
string(REPLACE " " ";" ARCH_FLAGS ${ARCH_FLAGS})
# ------------------
execute_process(
  COMMAND ${CMAKE_CXX_COMPILER} ${ARCH_FLAGS} ${GENLINK_DEFS} "-P" "-E" "${LIBOPENCM3_DIR}/ld/linker.ld.S"
  OUTPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT}"
)


# ARCH_FLAGS has to be passed as a string here
JOIN("${ARCH_FLAGS}" " " ARCH_FLAGS)
# Set linker flags
set(LINKER_FLAGS "${LINKER_FLAGS} ${LDLIBS} -T${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT} ${ARCH_FLAGS}")

# Compiler flags
set(TARGET_SPECIFIC_FLAGS "${GENLINK_CPPFLAGS} ${ARCH_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_SPECIFIC_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TARGET_SPECIFIC_FLAGS}")

add_custom_target(
  libopencm3 make
  WORKING_DIRECTORY ${LIBOPENCM3_DIR}
)


link_directories("${LIBOPENCM3_DIR}/lib/")
include_directories(SYSTEM ${LIBOPENCM3_DIR}/include/)


# Additional Flags
set(ADDITIONAL_C_FLAGS "-fmessage-length=0 -fno-strict-aliasing -ffunction-sections -fdata-sections -fsigned-char")
set(ADDITIONAL_CXX_FLAGS "-fno-exceptions -fno-rtti")
set(ADDITIONAL_LINKER_FLAGS "-Wl,-Map,${CMAKE_PROJECT_NAME}_${CMAKE_BUILD_TYPE}.map,--cref,--gc-sections")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra ${TARGET_FLAGS} ${ADDITIONAL_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra ${TARGET_FLAGS} ${ADDITIONAL_CXX_FLAGS}")
set(LINKER_FLAGS "${LINKER_FLAGS} -nostartfiles -lc -lnosys --specs=rdimon.specs ${ADDITIONAL_LINKER_FLAGS}")


# add tanuki target compile
add_executable(${PROJECT_NAME}.elf ${TANUKI_SOURCE})
target_link_libraries(${PROJECT_NAME}.elf "opencm3_${GENLINK_FAMILY}")
set_target_properties(${PROJECT_NAME}.elf PROPERTIES OUTPUT_NAME "${PROJECT_NAME}_${CMAKE_BUILD_TYPE}.elf")
set_target_properties(${PROJECT_NAME}.elf PROPERTIES LINK_FLAGS ${LINKER_FLAGS})

message(STATUS "Generated Linkder File : ${LINKER_SCRIPT}")
message(STATUS "Cmake Current Source Dir : ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Linker Flags : ${LINKER_FLAGS}")
message(STATUS "Library : ${GENLINK_FAMILY}")
message(STATUS "LibOPENCM3 DIR : ${LIBOPENCM3_DIR}")
message(STATUS "LibOPENCM3 LIB : libopencm3_${GENLINK_FAMILY}.a")

