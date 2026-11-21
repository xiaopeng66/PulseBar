import importlib.util
from pathlib import Path
import unittest


spec = importlib.util.spec_from_file_location(
    "native_support", Path(__file__).with_name("native_test_support.py")
)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


class TestHardwareMaps(unittest.TestCase):
    def test_repeated_disk_names_get_distinct_non_overwriting_keys(self):
        source = native.source("../OpenHardwareMonitorApi/OpenHardwareMonitorImp.cpp")
        method = native.method(source, "void COpenHardwareMonitor::InsertValueToMap(")
        method = method.replace(
            "void COpenHardwareMonitor::InsertValueToMap(",
            "static void InsertValueToMap(",
            1,
        )
        code = r'''#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
using namespace std;
static int _wtoi(const wchar_t* value) { return static_cast<int>(wcstol(value,nullptr,10)); }
''' + method + r'''

int main() {
    map<wstring,float> values;
    InsertValueToMap(values,L"Same Model",10);
    InsertValueToMap(values,L"Same Model",20);
    InsertValueToMap(values,L"Same Model",30);
    return values.size()==3 && values.at(L"Same Model")==10 &&
        values.at(L"Same Model #1")==20 && values.at(L"Same Model #2")==30 ? 0 : 1;
}
'''
        run, env = native.build(code, "hardware-map-keys")
        self.assertTrue(native.execute(run, env, "disk-keys"))


if __name__ == "__main__":
    unittest.main()
