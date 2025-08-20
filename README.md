# JAWBONE
Just Another WSPR Beacon Of Noisy Electronics

Custom PCB and software for a RP2040 based WSPR Beacon intended for tracking high altitude superpressure balloons (aka Pico Balloons).

This is an evolution of my  [pico-WSPRer](https://github.com/EngineerGuy314/pico-WSPRer) project. It is very similar except JAWBONE uses the MS5351 clock generator chip instead of abusing the RP2040's internal PLL oscillator. Also, this project is only intended to be used with a custom PCB, there is no longer an option to assemble it from a RaspberryPi Pico and external components (it is probably still possible, but not tested or documented). For now please continue to reference the [pico-WSPRer Wiki](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) because most of the information there is still relevant to JAWBONE.

I owe great thanks and acknowledgement to Roman Piksaykin (https://github.com/RPiks/) for the original program that was the starting point for pico-WSPRer. Much of his code still lives on inside JAWBONE and I have tried to maintain his copyright information in all relevant files.






random notes:
try 18Mhz w/set_sys_clock_khz() dont *need* to use PLL_SYS_MHZ macro
wuz:
void InitPicoClock(int PLL_SYS_MHZ)
{
    const uint32_t clkhz = PLL_SYS_MHZ * 1000000L;
	 printf("\n ABOUT TO SET SYSTEM KLOCK TO %dMhz\n", PLL_SYS_MHZ); 
	
    set_sys_clock_khz(clkhz / kHz, true);

    clock_configure(clk_peri, 0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    PLL_SYS_MHZ * MHZ,
                    PLL_SYS_MHZ * MHZ);
