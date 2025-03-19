# Hardware Alpha bringup Notes
## Errors/Fixes
1. Place 100u Electrolytic THT cap on J13 3V3 BYPASS for regulator stability
2. Place 100u electrolytic THT cap on J3 5V BYPASS
3. Scrape solder mask near D7 and add 4.7u ceramic 0603 or 0402 cap
4. C1, C46 wrong DKPN, 3.3uH inductors instead of 47uF elec caps
5. Y1 load caps should be 12pF not 12uF

## Tested
- PoE
- USB-C Power
- Power ORing
- Power regulation
- MCU turn-on, programming, and clocks

## To be tested
- Ethernet
- WiFi
- SD Card
- RS-232
- New IIS3DWB breakout
- USB-C data/programming

*Note:* USB-DFU seemed to not be working, firmware was uploaded over JTAG
