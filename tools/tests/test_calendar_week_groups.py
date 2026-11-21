"""Year-week numbers must use the same weekday offset as calendar rows."""
from pathlib import Path
import importlib.util

spec = importlib.util.spec_from_file_location("native_support", Path(__file__).with_name("native_test_support.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    week_method = native.method(native.source("CommonData.cpp"), "int Date::week() const")
    calendar = native.source("CalendarHelper.cpp")
    weekday = native.method(calendar, "int CCalendarHelper::CaculateWeekDay(int y, int m, int d)")
    days_in_month = native.method(calendar, "int CCalendarHelper::DaysInMonth(int year, int month)")
    leap = native.method(calendar, "bool CCalendarHelper::IsLeapYear(int year)")
    code = r'''#include <string>
struct Config {bool m_sunday_first=true;}; struct App {Config m_cfg_data;} theApp;
struct Date {int year{},month{},day{}; int week()const;};
class CCalendarHelper {public: static bool IsLeapYear(int);static int CaculateWeekDay(int,int,int);static int DaysInMonth(int,int);};
'''+leap+days_in_month+weekday+week_method+r'''
int wmain(int argc,wchar_t** argv) {
    if(argc!=3||std::wstring(argv[2]).find(PULSEBAR_TEST_ROOT)!=0)return 2;
    const auto week=[](int day,bool sunday_first){theApp.m_cfg_data.m_sunday_first=sunday_first;return Date{2023,1,day}.week();};
    if(week(1,true)!=1||week(2,true)!=1||week(8,true)!=2)return 1;
    if(week(1,false)!=1||week(2,false)!=2||week(8,false)!=2||week(9,false)!=3)return 1;
    return 0;
}
'''
    run, env = native.build(code, "calendar-week-groups")
    result = native.execute(run, env, "calendar")
    print(f"ARTIFACTS {run}")
    return int(not result)


if __name__ == "__main__":
    raise SystemExit(main())
