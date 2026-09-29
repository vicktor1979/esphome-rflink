#!/usr/bin/env python3
"""Run the actual Alecto display lambda with host JSON/sensor doubles."""
from pathlib import Path
import importlib.util
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("package_host", ROOT / "tests/test_v013.py")
host = importlib.util.module_from_spec(spec)
spec.loader.exec_module(host)
config = host.load(ROOT / "packages/rflink-alecto-006c.yaml")
source = host.STUBS
# Fixtures contain flat, string-valued protocol JSON only. Avoid a json-c
# dependency: this mock provides just the JsonObject API used by the lambda.
# It deliberately does not claim to validate ArduinoJson or arbitrary JSON.
source = source.replace("#include <json-c/json.h>", "#include <map>\n#include <regex>")
begin = source.index("struct JsonVariant{")
end = source.index("void assert_near", begin)
source = source[:begin] + r'''
struct JsonVariant {
  const std::string *value = nullptr;
  bool isNull() const { return value == nullptr; }
  const char *operator|(const char *fallback) const { return value ? value->c_str() : fallback; }
  template<typename T> bool is() const { return value && std::is_same_v<T, const char *>; }
  template<typename T> T as() const {
    if constexpr (std::is_same_v<T, const char *>) return value ? value->c_str() : nullptr;
    else return T{};
  }
};
struct JsonObject {
  const std::map<std::string, std::string> *values;
  JsonVariant operator[](const char *key) const {
    auto found = values->find(key);
    return {found == values->end() ? nullptr : &found->second};
  }
};
namespace json {
template<typename Callback> bool parse_json(const std::string &message, Callback callback) {
  std::map<std::string, std::string> values;
  static const std::regex field(R"rx("([^"]+)"\s*:\s*"([^"]*)")rx");
  for (std::sregex_iterator it(message.begin(), message.end(), field), end; it != end; ++it)
    values[(*it)[1].str()] = (*it)[2].str();
  return callback(JsonObject{&values});
}
}
''' + source[end:]
for group, kind in (("sensor", "Sensor"), ("text_sensor", "TextSensor"),
                    ("binary_sensor", "BinarySensor")):
    for item in config[group]:
        source += kind + " " + item["id"] + ";\n"
source += "void route(const std::string &message) {\n"
source += host.translate(config["script"][0]["then"][0]["lambda"], {}, ())
source += "\n}\n"
source += r'''
int main() {
  route(R"({"NAME":"Alecto V1","ID":"00A4","RAIN":"0010"})");
  assert(!alecto_006c_temp.has_state());
  route(R"({"NAME":"Alecto V1","ID":"0074","TEMP":"00e1","BAT":"LOW"})");
  assert_near(alecto_006c_temp.state, 22.5f);
  assert(alecto_006c_bat.state && alecto_006c_current_id.state == "0074");
  assert(alecto_1_channel.state == "Nincs adat");
  route(R"({"NAME":"Alecto V1","ID":"0020","TEMP":"8032","BAT":"OK"})");
  assert_near(alecto_2_temp.state, -5.0f);
  assert(!alecto_2_bat.state && alecto_2_current_id.state == "0020");
  route(R"({"NAME":"Alecto V1","ID":"0074","TEMP":"0191","BAT":"OK"})");
  assert_near(alecto_006c_temp.state, 40.1f);
  assert(!alecto_006c_bat.state);
  route(R"({"NAME":"Alecto V1","ID":"0084","TEMP":"00f1"})");
  assert_near(alecto_3_temp.state, 24.1f);
  assert(alecto_3_current_id.state == "0084");
  route(R"({"NAME":"Alecto V1","ID":"0094","TEMP":"0011"})");
  assert(alecto_3_current_id.state == "0084");
  route(R"({"NAME":"Alecto V1","ID":"0074","BAT":"LOW"})");
  assert_near(alecto_006c_temp.state, 40.1f);
  assert(alecto_006c_bat.state);
  route(R"({"NAME":"EV1527","ID":"0074","TEMP":"0001"})");
  assert_near(alecto_006c_temp.state, 40.1f);
  std::cout << "PASS actual display lambda: first measurement, immediate updates, raw IDs, three independent slots, retained values\n";
}
'''
with tempfile.TemporaryDirectory(prefix="rflink-alecto-display-") as directory:
    work = Path(directory)
    (work / "test.cpp").write_text(source)
    subprocess.run(["g++", "-std=c++17", "-O1", "-g", "-fno-pie", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I" + str(ROOT / "components/rflink"), str(work / "test.cpp"),
                    "-o", str(work / "test")], check=True)
    subprocess.run([str(work / "test")], check=True,
                   env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"))
print("LIMIT: host JSON/sensor doubles; no ESPHome code generation or hardware test.")
