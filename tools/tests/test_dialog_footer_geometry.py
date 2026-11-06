"""Footer geometry regressions for every reported overlapping dialog.

Checks source geometry with real resource coordinates, including CONTROL
statements split over two resource lines. No program execution, user
configuration or UI manipulation. Compiled resources are scanned separately.
"""
from pathlib import Path
import argparse
import csv
import importlib.util
import re
import unittest

SOURCE = Path(__file__).resolve().parents[2] / 'PulseBar'


def controls(resource):
    text = (SOURCE / 'PulseBar.rc').read_text(encoding='utf-16')
    match = re.search(r'^' + re.escape(resource) + r' DIALOGEX[^\n]*\n[\s\S]*?\nBEGIN\n([\s\S]*?)\nEND', text, re.M)
    if not match:
        raise AssertionError('dialog missing: ' + resource)
    result = {}
    body = re.sub(r',\s*\n\s*"', ',"', match[1])
    for line in body.splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) != 2:
            continue
        kind, rest = parts
        fields = next(csv.reader([rest], skipinitialspace=True))
        if kind == 'CONTROL':
            rid, coords = fields[1], fields[4:8]
        elif kind in {'DEFPUSHBUTTON', 'PUSHBUTTON', 'LTEXT', 'CTEXT', 'RTEXT', 'GROUPBOX'}:
            rid, coords = fields[1], fields[2:6]
        elif kind in {'EDITTEXT', 'LISTBOX', 'COMBOBOX'}:
            rid, coords = fields[0], fields[1:5]
        else:
            continue
        try:
            rect = tuple(int(s) for s in coords)
        except ValueError:
            continue
        if len(rect) == 4:
            result[rid] = rect
    return result


def intersects(a, b):
    return max(a[0], b[0]) < min(a[0]+a[2], b[0]+b[2]) and max(a[1], b[1]) < min(a[1]+a[3], b[1]+b[3])


class DialogFooterGeometry(unittest.TestCase):
    def test_about_links_are_above_button_and_footer_separator(self):
        c = controls('IDD_ABOUTBOX')
        for name in ('IDC_OPENHARDWAREMONITOR_LINK', 'IDC_TINYXML2_LINK'):
            self.assertFalse(intersects(c[name], c['IDOK']), name)
            # Separator is drawn 6 physical pixels above the button: allow 4 DLU.
            self.assertLessEqual(c[name][1]+c[name][3]+4, c['IDOK'][1])

    def test_message_text_does_not_cross_button_row(self):
        c = controls('IDD_MESSAGE_DIALOG')
        self.assertLessEqual(c['IDC_HELP_EDIT'][1]+c['IDC_HELP_EDIT'][3]+4, c['IDOK'][1])

    def test_auto_adapt_checkbox_clears_buttons(self):
        c = controls('IDD_ATUO_ADAPT_SETTING_DIALOG')
        for name in ('IDOK', 'IDCANCEL'):
            self.assertFalse(intersects(c['IDC_AUTO_SAVE_TO_PRESET_CHECK'], c[name]))
            self.assertLessEqual(c['IDC_AUTO_SAVE_TO_PRESET_CHECK'][1]+c['IDC_AUTO_SAVE_TO_PRESET_CHECK'][3]+4, c[name][1])

    def test_taskbar_auto_background_checkbox_does_not_touch_auto_adapt_button(self):
        c = controls('IDD_TASKBAR_SETTINGS_DIALOG')
        self.assertFalse(intersects(c['IDC_AUTO_SET_BACK_COLOR_CHECK'], c['IDC_AUTO_ADAPT_SETTINGS_BUTTON']))
        table = (SOURCE / 'TaskBarSettingsDlg.cpp').read_text(encoding='utf-8-sig')
        rect = c['IDC_AUTO_SET_BACK_COLOR_CHECK']
        needle = '{ ' + ', '.join(map(str, rect)) + ', true, 1 }'
        self.assertIn(needle, table, 'runtime section table must match resource coordinates')

    def test_history_confirm_is_compact_and_aligned_with_content(self):
        c = controls('IDD_HISTORY_TRAFFIC_DIALOG')
        self.assertEqual(c['IDOK'][2:], (50, 14))
        self.assertEqual(c['IDOK'][0]+c['IDOK'][2], c['IDC_TAB1'][0]+c['IDC_TAB1'][2])
        self.assertLessEqual(c['IDC_TAB1'][1]+c['IDC_TAB1'][3]+7, c['IDOK'][1])

    def test_plugin_links_have_own_tall_row_above_compact_buttons(self):
        c = controls('IDD_PLUGIN_MANAGER_DIALOG')
        buttons = ('IDC_PLUGIN_INFO_BUTTON', 'IDC_OPTINS_BUTTON', 'IDCANCEL')
        for name in buttons:
            self.assertEqual(c[name][3], 14)
        for name in ('IDC_PLUGIN_DOWNLOAD_STATIC', 'IDC_PLUGIN_DEV_GUID_STATIC', 'IDC_OPEN_PLUGIN_DIR_STATIC'):
            self.assertGreaterEqual(c[name][3], 12)
            self.assertLessEqual(c[name][1]+c[name][3]+6, min(c[b][1] for b in buttons))
        spans = sorted((c[name][0], c[name][0]+c[name][2]) for name in
                       ('IDC_PLUGIN_DOWNLOAD_STATIC', 'IDC_PLUGIN_DEV_GUID_STATIC', 'IDC_OPEN_PLUGIN_DIR_STATIC'))
        self.assertTrue(all(spans[i][1]+3 <= spans[i+1][0] for i in range(len(spans)-1)))

    def test_other_list_footers_reserve_four_dlu_gap(self):
        for resource in ('IDD_TASKBAR_COLOR_DIALOG', 'IDD_SELECT_CONNECTIONS_DIALOG'):
            with self.subTest(resource=resource):
                c = controls(resource)
                for name in ('IDOK', 'IDCANCEL'):
                    self.assertLessEqual(c['IDC_LIST1'][1]+c['IDC_LIST1'][3]+4, c[name][1])

    def test_network_buttons_keep_compact_height_and_clear_list(self):
        c = controls('IDD_NETWORK_INFO_DIALOG')
        for name in ('IDOK', 'IDC_PREVIOUS_BUTTON', 'IDC_NEXT_BUTTON'):
            self.assertEqual(c[name][3], 14)
            self.assertLessEqual(c['IDC_INFO_LIST1'][1]+c['IDC_INFO_LIST1'][3]+4, c[name][1])


if __name__ == '__main__':
    unittest.main(verbosity=2)
