# Little Kernel bootloader for Quectel EG25 Series modem

This is the source code for Quectel's EG25-G modem bootloader (aboot)

This bootloader should also be compatible with (at least) the following models:
* Quectel
  * EC20
  * EC21
  * EC25
* Simcom
  * SIM7600M (Work in progress)

## You're playing with bootloaders here. One error and you might brick your modem. You're using this at your own risk, so please be careful.

This bootloader has been tuned for the Pinephone's modem, and has some changes done to make the modem more confortable to work with in this environment:
* Current functionality
  * Boot: OK
  * Flash: OK
  * Debugging: Via debug UART
  * Forced fastboot mode via GPIO pins
	* On reset, the bootloader enters into fastboot mode automatically for 2 seconds, and boots normally unless instructed to stay.
	* Run _fastboot oem stay_ to stay in fastboot mode
  * Jump to...
   * Fastboot mode: OK (fastboot reboot-bootloader)
   * DLOAD Mode: WIP (fastboot oem reboot-edl) (currently correct values are stored in what should be the correct memory area but the sbl ignores them, under investigation)
   * Recovery mode: OK (fastboot oem reboot-recovery)

If you want to build this yourself, check out the Pinephone Modem SDK for tooling and instructions (https://github.com/biktorgj/pinephone_modem_sdk)