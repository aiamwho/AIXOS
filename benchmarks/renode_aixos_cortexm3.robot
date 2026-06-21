*** Variables ***
${ELF}          ${CURDIR}/../build/benchmarks/aixos/arm/AIXOS.elf
${PLATFORM}     ${CURDIR}/../simulation/stm32f103.repl

*** Test Cases ***
AIXOS Cortex M3 Benchmark
    Execute Command    mach create
    Execute Command    machine LoadPlatformDescription @${PLATFORM}
    Execute Command    sysbus LoadELF @${ELF}
    ${heartbeat}=       Execute Command    sysbus GetSymbolAddress "aixos_bench_heartbeat"
    ${messages}=        Execute Command    sysbus GetSymbolAddress "aixos_bench_messages"
    ${errors}=          Execute Command    sysbus GetSymbolAddress "aixos_bench_errors"
    ${ticks}=           Execute Command    sysbus GetSymbolAddress "total_ticks"
    ${switches}=        Execute Command    sysbus GetSymbolAddress "switch_count"
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
