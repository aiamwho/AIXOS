*** Variables ***
${ELF}          ${CURDIR}/../build/arm/AIXOS.elf
${PLATFORM}     ${CURDIR}/../simulation/stm32f103.repl
*** Test Cases ***
Cortex M3 Kernel Should Tick And Run Tasks
    Execute Command    mach create
    Execute Command    machine LoadPlatformDescription @${PLATFORM}
    Execute Command    sysbus LoadELF @${ELF}
    ${heartbeat}=       Execute Command    sysbus GetSymbolAddress "test_heartbeat"
    ${ticks}=           Execute Command    sysbus GetSymbolAddress "total_ticks"
    ${user_hb}=         Execute Command    sysbus GetSymbolAddress "test_user_heartbeat"
    ${user_errors}=     Execute Command    sysbus GetSymbolAddress "test_user_syscall_errors"
    Execute Command    start
    Sleep              0.2
    Execute Command    pause
    ${hb1}=             Execute Command    sysbus ReadDoubleWord ${heartbeat}
    ${tick1}=           Execute Command    sysbus ReadDoubleWord ${ticks}
    ${uhb1}=            Execute Command    sysbus ReadDoubleWord ${user_hb}
    ${uerr1}=           Execute Command    sysbus ReadDoubleWord ${user_errors}
    ${hb1}=             Convert To Integer    ${hb1}
    ${tick1}=           Convert To Integer    ${tick1}
    ${uhb1}=            Convert To Integer    ${uhb1}
    ${uerr1}=           Convert To Integer    ${uerr1}
    Should Be True      ${hb1} > 0
    Should Be True      ${tick1} > 0
    Should Be True      ${uhb1} > 0
    Should Be Equal As Integers    ${uerr1}    0

    Execute Command    start
    Sleep              0.2
    Execute Command    pause
    ${hb2}=             Execute Command    sysbus ReadDoubleWord ${heartbeat}
    ${tick2}=           Execute Command    sysbus ReadDoubleWord ${ticks}
    ${uhb2}=            Execute Command    sysbus ReadDoubleWord ${user_hb}
    ${uerr2}=           Execute Command    sysbus ReadDoubleWord ${user_errors}
    ${hb2}=             Convert To Integer    ${hb2}
    ${tick2}=           Convert To Integer    ${tick2}
    ${uhb2}=            Convert To Integer    ${uhb2}
    ${uerr2}=           Convert To Integer    ${uerr2}
    Should Be True      ${hb2} > ${hb1}
    Should Be True      ${tick2} > ${tick1}
    Should Be True      ${uhb2} > ${uhb1}
    Should Be Equal As Integers    ${uerr2}    0
