*** Variables ***
${ELF}          ${CURDIR}/../build/freertos/arm/FreeRTOS.elf
${PLATFORM}     ${CURDIR}/../simulation/stm32f103.repl

*** Test Cases ***
FreeRTOS Cortex M3 Benchmark
    Execute Command    mach create
    Execute Command    machine LoadPlatformDescription @${PLATFORM}
    Execute Command    sysbus LoadELF @${ELF}
    ${heartbeat}=       Execute Command    sysbus GetSymbolAddress "freertos_heartbeat"
    ${messages}=        Execute Command    sysbus GetSymbolAddress "freertos_messages"
    ${errors}=          Execute Command    sysbus GetSymbolAddress "freertos_errors"
    ${ticks}=           Execute Command    sysbus GetSymbolAddress "freertos_ticks"
    ${switches}=        Execute Command    sysbus GetSymbolAddress "freertos_switches"
    Execute Command    start
    Sleep              0.5
    Execute Command    pause
    ${hb}=              Execute Command    sysbus ReadDoubleWord ${heartbeat}
    ${msg}=             Execute Command    sysbus ReadDoubleWord ${messages}
    ${err}=             Execute Command    sysbus ReadDoubleWord ${errors}
    ${tick}=            Execute Command    sysbus ReadDoubleWord ${ticks}
    ${sw}=              Execute Command    sysbus ReadDoubleWord ${switches}
    Should Be True      ${hb} > 10
    Should Be True      ${msg} > 10
    Should Be Equal As Integers    ${err}    0
    Should Be True      ${tick} > 100
    Should Be True      ${sw} > 20
