## Known issues

* Only 48k is fully supported and tested (128k memory layout not emulated yet)
* Loaded snapshot is started immediatelly regardless of the period between interrupts when it was captured (better we should wait and start it in particular time after last IRQ signal)
* NMI may not be fully supported since it is used by peripherals using they own memories which we are not aware and we do not shadow-emulate them (it could be more easy to emulate them and not used the real such peripherals)
* R register is not emulated well yet
* IM0 and IM2 modes have not been tester, I register emulation have not been tested