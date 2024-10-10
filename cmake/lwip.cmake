
set(LWIP_PATH ${PROJECT_ROOT}/external/LwIP)


set(LWIP_INCLUDES 
    ${LWIP_PATH}/src/include
    ${LWIP_PATH}/src/include/netif/ppp
    ${LWIP_PATH}/src/include/lwip/apps
    ${LWIP_PATH}/src/include/lwip
    ${LWIP_PATH}/src/include/lwip/apps
    ${LWIP_PATH}/src/include/lwip/priv
    ${LWIP_PATH}/src/include/lwip/prot
    ${LWIP_PATH}/src/include/netif
    ${LWIP_PATH}/src/include/compat/posix
    ${LWIP_PATH}/src/include/compat/posix/arpa
    ${LWIP_PATH}/src/include/compat/posix/net
    ${LWIP_PATH}/src/include/compat/posix/sys
    ${LWIP_PATH}/src/include/compat/stdc
    ${LWIP_PATH}/system
    ${LWIP_PATH}/system/arch
)

file(GLOB_RECURSE LWIP_SRC CONFIGURE_DEPENDS
    ${LWIP_PATH}/*.c
)

set(LWIP_OS_SOURCES "${LWIP_PATH}/system/OS/sys_arch.c")

set(LWIP_SOURCES 
    ${LWIP_SRC}
    ${LWIP_OS_SOURCES}
)


add_library(lwip STATIC)
target_include_directories(lwip PUBLIC ${LWIP_INCLUDES})
target_sources(lwip PRIVATE ${LWIP_SOURCES})

target_link_libraries(lwip PRIVATE freertos)
