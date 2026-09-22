// Include engine in this test TU only to inspect its private JSON formatter.
// This does not add any test hook to the firmware.
#include "rflink_engine.cpp"
#include <cassert>
#include <iostream>
uint32_t test_millis = 1000;
int main() {
  using namespace rflink_legacy;
  reset();
  char id[] = "ab\"cd\\ef\n";
  display_Header();
  display_Name(PSTR("EV1527\"\\\n\x01"));
  display_IDc(id);
  display_SWITCHc(PSTR("08"));
  display_Footer();
  assert(!overflow && finished);
  assert(output == "{\"PARAM\":\"20;00\",\"NAME\":\"EV1527\\\"\\\\\\u000a\\u0001\","
                   "\"ID\":\"ab\\\"cd\\\\ef\\u000a\",\"SWITCH\":\"08\"}");
  display_Header();
  display_Name(nullptr);
  display_IDc(PSTR(""));
  display_SWITCHc(nullptr);
  display_Footer();
  assert(!overflow && finished);
  assert(output == "{\"PARAM\":\"20;01\",\"NAME\":\"\",\"ID\":\"\",\"SWITCH\":\"\"}");
  display_Header();
  display_Name("RAM name");
  display_IDc(PSTR("00ff"));
  display_Footer();
  assert(!overflow && finished);
  assert(output.find("\"NAME\":\"RAM name\"") != std::string::npos);
  assert(output.find("\"ID\":\"00ff\"") != std::string::npos);
  // UTF-8 bytes must survive, whether the source is RAM or flash.
  display_Header(); display_Name(PSTR("Árvíztűrő")); display_Footer();
  assert(!overflow && output.find("Árvíztűrő") != std::string::npos);
  // Excessive fields must still trigger the existing bounded-output guard.
  std::string too_long(2048, 'x');
  display_Header(); display_Name(too_long.c_str()); display_Footer();
  assert(overflow && output.size() <= MAX_JSON_SIZE);
#ifdef RFLINK_TEST_STRICT_PROGMEM
  assert(progmem_guard::storage().reads > 0);
#endif
  std::cout << "PASS: formatter RAM/PSTR strings, escaping, UTF-8, null/empty, size limit.\n";
}
