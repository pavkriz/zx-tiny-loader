## Known issues

* Loaded snapshot is started immediatelly regardless of the period between interrupts when it was captured (better we should wait and start it in particular time after last IRQ signal)
* NMI is not supported since it is used by peripherals using they own memories which we are not aware and we do not shadow-emulate them