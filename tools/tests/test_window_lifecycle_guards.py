"""Source contracts for actual HWND lifecycle routes (not GUI acceptance)."""
from pathlib import Path
import importlib.util
import unittest

spec = importlib.util.spec_from_file_location('lifecycle_native', Path(__file__).with_name('native_test_support.py'))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


class WindowLifecycleGuards(unittest.TestCase):
    def test_apply_revalidates_window_after_create_can_fail(self):
        body = native.method(native.source('PulseBarDlg.cpp'), 'void CPulseBarDlg::ApplySettings(')
        after = body[body.index('OpenTaskBarWnd();'):]
        self.assertIn('if (IsTaskbarWndValid())', after[:after.index('m_tBarDlg->ApplyWindowTransparentColor()')])

    def test_queued_close_matches_current_object_generation(self):
        close = native.method(native.source('TaskBarDlg.cpp'), 'void CTaskBarDlg::OnClose()')
        host = native.method(native.source('PulseBarDlg.cpp'), 'afx_msg LRESULT CPulseBarDlg::OnTaskbarWndClosed(')
        self.assertIn('GetInstanceToken()', close)
        self.assertIn('wParam != m_tBarDlg->GetInstanceToken()', host)
        self.assertLess(host.index('wParam != m_tBarDlg->GetInstanceToken()'), host.index('m_show_task_bar_wnd = false'))

    def test_theme_transition_checks_backend_and_window_style(self):
        theme = native.method(native.source('PulseBarDlg.cpp'), 'void CPulseBarDlg::ApplyTaskbarTheme()')
        self.assertIn('NeedsWindowRecreation()', theme)
        self.assertIn('CloseTaskBarWnd()', theme)
        self.assertIn('OpenTaskBarWnd()', theme)
        taskbar = native.source('TaskBarDlg.cpp')
        self.assertIn('bool CTaskBarDlg::NeedsWindowRecreation() const', taskbar)
        method = native.method(taskbar, 'bool CTaskBarDlg::NeedsWindowRecreation() const')
        self.assertIn('WS_EX_NOREDIRECTIONBITMAP', method)
        self.assertIn('CSupportedRenderEnums desired', method)

    def test_menu_commands_do_not_recreate_owner_inside_tracking_loop(self):
        method = native.method(native.source('TaskBarDlg.cpp'), 'void CTaskBarDlg::OnRButtonUp(')
        self.assertIn('TPM_RETURNCMD', method)
        self.assertIn('::PostMessage(', method)
        self.assertNotIn('CDialogEx::OnRButtonUp(', method)
        self.assertIn('if (!::IsWindow(source_hwnd)) return;', method)

    def test_failed_notify_rectangle_forces_fallback(self):
        method = native.method(native.source('Win11TaskbarDlg.cpp'), 'void CWin11TaskbarDlg::AdjustTaskbarWndPos(')
        self.assertIn('if (!notify_rect_valid)', method)
        self.assertIn('m_rcNotify.SetRectEmpty()', method)
        self.assertIn('if (force_adjust || !notify_rect_valid', method)

    def test_pinned_dpi_update_refreshes_before_and_after_move(self):
        method = native.method(native.source('TaskBarDlg.cpp'), 'bool CTaskBarDlg::AdjustWindowPos(')
        self.assertIn('OnDpiChanged(GetDPI(), taskbar_rect)', method)
        self.assertGreater(method.index('OnDpiChanged(GetDPI(), taskbar_rect)'), method.index('AdjustTaskbarWndPos(force_adjust)'))
        method = native.method(native.source('TaskBarDlg.cpp'), 'void CTaskBarDlg::SetDPI(')
        self.assertIn('OnDpiChanged(dpi, taskbar_rect)', method)
        self.assertLess(method.index('m_taskbar_dpi = dpi'), method.index('OnDpiChanged(dpi, taskbar_rect)'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
