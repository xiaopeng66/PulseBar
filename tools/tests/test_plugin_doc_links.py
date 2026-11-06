"""Plugin links and menu localization must point to actual project content.

Static/local only; GitHub availability is verified separately at publication.
"""
from pathlib import Path
import configparser
import importlib.util
import unittest

REPO=Path(__file__).resolve().parents[2]
SOURCE=REPO/'PulseBar'
spec=importlib.util.spec_from_file_location('plugin_link_support',Path(__file__).with_name('native_test_support.py'))
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)


class PluginLinks(unittest.TestCase):
    def test_plugin_manager_links_point_to_documents_that_exist(self):
        init=native.method(native.source('PluginManagerDlg.cpp'),'BOOL CPluginManagerDlg::OnInitDialog()')
        for file in ('downloads.md','development.md'):
            self.assertTrue((REPO/'docs/plugins'/file).is_file(),file)
            self.assertIn('https://github.com/xiaopeng66/PulseBar/blob/main/docs/plugins/'+file,init)
        self.assertNotIn('PulseBarPlugins',init)
        self.assertNotIn('/wiki/',init)
        self.assertIn('m_open_plugin_dir_lnk.SetLinkIsURL(false)',init)

    def test_open_directory_passes_folder_as_shell_target_not_command_arguments(self):
        body=native.method(native.source('PluginManagerDlg.cpp'),'afx_msg LRESULT CPluginManagerDlg::OnLinkClicked(')
        self.assertIn('ShellExecuteW(GetSafeHwnd(), L"open", plugin_dir.c_str(), nullptr',body)
        self.assertNotIn('_T("explorer")',body)

    def test_plugin_base_menu_labels_use_menu_not_text_string_table(self):
        builder=native.method(native.source('PulseBar.cpp'),'void CPulseBarApp::UpdateTrayPluginMenu()')
        for key in ('TXT_PLUGIN_DETAIL','TXT_PLUGIN_OPTIONS','TXT_PLUGIN_DISABLE'):
            self.assertIn('m_str_table.LoadMenuText(L"'+key+'")',builder,key)
            self.assertNotIn('CCommon::LoadText(L"'+key+'")',builder,key)

    def test_all_eighteen_languages_supply_requested_plugin_menu_keys(self):
        files=list((SOURCE/'language').glob('*.ini'))
        self.assertEqual(len(files),18)
        for path in files:
            parser=configparser.ConfigParser(interpolation=None,strict=False)
            parser.optionxform=str
            parser.read_string(path.read_text(encoding='utf-8-sig'))
            for key in ('TXT_PLUGIN_DETAIL','TXT_PLUGIN_OPTIONS','TXT_PLUGIN_DISABLE'):
                with self.subTest(language=path.name,key=key):
                    self.assertTrue(parser.get('menu',key).strip(' "'))


if __name__=='__main__':unittest.main(verbosity=2)
