// SYNTHETIC FORMATTER TEST, not real RF measurements. Requires original repository test stubs.

#include "rflink_engine.cpp"
#include <cassert>
#include <iostream>
uint32_t test_millis = 1000;
int main(){
 using namespace rflink_legacy;
 reset(); display_Header(); display_Name(PSTR("SYNTHETIC")); display_IDn(7,4);
 display_SWITCH(8); display_CMD(false,CMD_On); display_SET_LEVEL(15);
 display_TEMP(0x00ea); display_HUM(0x43,HUM_BCD); display_BARO(0x03f5);
 display_HSTATUS(1); display_BFORECAST(4); display_UV(10); display_LUX(1000);
 display_BAT(false); display_RAIN(141); display_RAINRATE(5);
 display_WINSP(123); display_AWINSP(100); display_WINGS(128); display_WINDIR(4);
 display_WINCHL(0x8037); display_WINTMP(200); display_CHIME(2);
 display_SMOKEALERT(true); display_PIR(false); display_CO2(800); display_SOUND(42);
 display_KWATT(1000); display_WATT(250); display_CURRENT(123); display_DIST(456);
 display_METER(1000); display_VOLT(230); display_RGBW(0xa5fe); display_Footer();
 assert(finished && !overflow); std::cout<<output;
}
