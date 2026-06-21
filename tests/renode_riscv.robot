*** Variables ***
${ELF}          ${CURDIR}/../build/riscv/AIXOS.elf
${PLATFORM}     ${CURDIR}/../simulation/riscv32_virt.repl

*** Test Cases ***
RISC V Kernel Should Tick And Run Tasks
    Execute Command    mach create
    Execute Command    machine LoadPlatformDescription @${PLATFORM}
    Execute Command    sysbus LoadELF @${ELF}
    ${heartbeat}=       Execute Command    sysbus GetSymbolAddress "test_heartbeat"
    ${ticks}=           Execute Command    sysbus GetSymbolAddress "total_ticks"
    ${timer_irqs}=      Execute Command    sysbus GetSymbolAddress "aixos_riscv_timer_interrupts"
    ${soft_irqs}=       Execute Command    sysbus GetSymbolAddress "aixos_riscv_software_interrupts"
    ${bad_cause}=       Execute Command    sysbus GetSymbolAddress "aixos_riscv_unhandled_mcause"
    ${reg_errors}=      Execute Command    sysbus GetSymbolAddress "test_riscv_register_errors"
    ${user_hb}=         Execute Command    sysbus GetSymbolAddress "test_user_heartbeat"
    ${user_errors}=     Execute Command    sysbus GetSymbolAddress "test_user_syscall_errors"
    Execute Command    start
    Sleep              0.2
    Execute Command    pause
    ${hb1}=             Execute Command    sysbus ReadDoubleWord ${heartbeat}
    ${tick1}=           Execute Command    sysbus ReadDoubleWord ${ticks}
    ${timer1}=          Execute Command    sysbus ReadDoubleWord ${timer_irqs}
    ${soft1}=           Execute Command    sysbus ReadDoubleWord ${soft_irqs}
    ${cause1}=          Execute Command    sysbus ReadDoubleWord ${bad_cause}
    ${regerr1}=         Execute Command    sysbus ReadDoubleWord ${reg_errors}
    ${uhb1}=            Execute Command    sysbus ReadDoubleWord ${user_hb}
    ${uerr1}=           Execute Command    sysbus ReadDoubleWord ${user_errors}
    ${hb1}=             Convert To Integer    ${hb1}
    ${tick1}=           Convert To Integer    ${tick1}
    ${timer1}=          Convert To Integer    ${timer1}
    ${soft1}=           Convert To Integer    ${soft1}
    ${cause1}=          Convert To Integer    ${cause1}
    ${regerr1}=         Convert To Integer    ${regerr1}
    ${uhb1}=            Convert To Integer    ${uhb1}
    ${uerr1}=           Convert To Integer    ${uerr1}
    Should Be True      ${hb1} > 0
    Should Be True      ${tick1} > 0
    Should Be True      ${timer1} > 0
    Should Be True      ${soft1} > 0
    Should Be Equal As Integers    ${cause1}    0
    Should Be Equal As Integers    ${regerr1}    0
    Should Be True      ${uhb1} > 0
    Should Be Equal As Integers    ${uerr1}    0

    Execute Command    start
    Sleep              0.2
    Execute Command    pause
    ${hb2}=             Execute Command    sysbus ReadDoubleWord ${heartbeat}
    ${tick2}=           Execute Command    sysbus ReadDoubleWord ${ticks}
    ${timer2}=          Execute Command    sysbus ReadDoubleWord ${timer_irqs}
    ${cause2}=          Execute Command    sysbus ReadDoubleWord ${bad_cause}
    ${regerr2}=         Execute Command    sysbus ReadDoubleWord ${reg_errors}
    ${uhb2}=            Execute Command    sysbus ReadDoubleWord ${user_hb}
    ${uerr2}=           Execute Command    sysbus ReadDoubleWord ${user_errors}
    ${hb2}=             Convert To Integer    ${hb2}
    ${tick2}=           Convert To Integer    ${tick2}
    ${timer2}=          Convert To Integer    ${timer2}
    ${cause2}=          Convert To Integer    ${cause2}
    ${regerr2}=         Convert To Integer    ${regerr2}
    ${uhb2}=            Convert To Integer    ${uhb2}
    ${uerr2}=           Convert To Integer    ${uerr2}
    Should Be True      ${hb2} > ${hb1}
    Should Be True      ${tick2} > ${tick1}
    Should Be True      ${timer2} > ${timer1}
    Should Be Equal As Integers    ${cause2}    0
    Should Be Equal As Integers    ${regerr2}    0
    Should Be True      ${uhb2} > ${uhb1}
    Should Be Equal As Integers    ${uerr2}    0
