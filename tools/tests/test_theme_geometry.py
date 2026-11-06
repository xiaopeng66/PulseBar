"""Bounded taskbar theme geometry regressions.

These checks stay source-local and do not launch the application or GUI. The pure helper
also has a native probe under E:/AI/Zcode/tmp/pulsebar-ui-audit/theme-geometry-独占.
"""
from pathlib import Path
import unittest


PULSEBAR = Path(__file__).resolve().parents[2] / "PulseBar"
DRAW = PULSEBAR / "DrawCommon.cpp"
PREVIEW = PULSEBAR / "TaskBarSettingsDlg.cpp"
HELPER = PULSEBAR / "TaskbarThemeGeometry.h"


def method(text: str, signature: str) -> str:
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 0
    in_string = False
    quote = ""
    escaped = False
    line_comment = False
    block_comment = False
    for index in range(opening, len(text)):
        char = text[index]
        pair = text[index:index + 2]
        if line_comment:
            if char == "\n":
                line_comment = False
            continue
        if block_comment:
            if pair == "*/":
                block_comment = False
            continue
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                in_string = False
            continue
        if pair == "//":
            line_comment = True
            continue
        if pair == "/*":
            block_comment = True
            continue
        if char in "\"'":
            in_string = True
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"unterminated method: {signature}")


def resolve_radius(width: int, height: int, requested: int, capsule: bool) -> int:
    max_radius = max(0, min(width, height) // 2)
    if max_radius == 0:
        return 0
    if capsule:
        return max_radius
    return max(0, min(requested, max_radius))


def bottom_bar(width: int, height: int, radius: int, bar_height: int, margin: int):
    if width <= 0 or height <= 0 or bar_height <= 0 or margin < 0:
        return None
    safe_radius = max(0, min(radius, min(width, height) // 2))
    left = safe_radius
    right = width - safe_radius
    bottom = min(height, max(0, height - margin))
    top = max(0, bottom - bar_height)
    if right <= left or bottom <= top:
        return None
    return left, top, right - left, bottom - top


def left_bar(width: int, height: int, inset_x: int, inset_y: int, requested_width: int):
    if width <= 0 or height <= 0 or requested_width <= 0 or inset_x < 0 or inset_y < 0:
        return None
    left = min(inset_x, width)
    top = min(inset_y, height)
    bottom = max(top, height - inset_y)
    right = min(width, left + requested_width)
    if right <= left or bottom <= top:
        return None
    return left, top, right - left, bottom - top


def panel_alpha(percent: int) -> int:
    return max(1, min(percent, 100)) * 255 // 100


def accent_gradient_end_alpha(panel: int) -> int:
    return max(0, min(panel, 255)) * 120 // 255


class ThemeGeometry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.draw = method(DRAW.read_text(encoding="utf-8-sig"), "void CDrawCommon::DrawThemeBackground")
        cls.preview = method(PREVIEW.read_text(encoding="utf-8-sig"), "void CTaskBarSettingsDlg::OnDrawItem")
        cls.helper = HELPER.read_text(encoding="utf-8")

    def test_radius_zero_and_capsule_use_minimum_dimension(self):
        self.assertEqual(resolve_radius(120, 20, 0, False), 0)
        self.assertEqual(resolve_radius(8, 20, 8, False), 4)
        self.assertEqual(resolve_radius(8, 20, 0, True), 4)
        self.assertEqual(resolve_radius(1, 1, 32, False), 0)

    def test_bottom_bar_is_positive_and_inside(self):
        self.assertIsNone(bottom_bar(8, 20, 4, 2, 2))
        bar = bottom_bar(20, 20, 4, 2, 2)
        self.assertEqual(bar, (4, 16, 12, 2))
        left, top, width, height = bar
        self.assertGreater(width, 0)
        self.assertGreater(height, 0)
        self.assertGreaterEqual(left, 0)
        self.assertGreaterEqual(top, 0)
        self.assertLessEqual(left + width, 20)
        self.assertLessEqual(top + height, 20)

    def test_left_bar_is_clipped_or_skipped_when_narrow(self):
        self.assertIsNone(left_bar(4, 20, 4, 4, 3))
        bar = left_bar(8, 20, 4, 4, 3)
        self.assertEqual(bar, (4, 4, 3, 12))
        left, top, width, height = bar
        self.assertGreater(width, 0)
        self.assertGreater(height, 0)
        self.assertGreaterEqual(left, 0)
        self.assertGreaterEqual(top, 0)
        self.assertLessEqual(left + width, 8)
        self.assertLessEqual(top + height, 20)

    def test_production_and_preview_share_geometry_and_alpha_helpers(self):
        for token in (
            "ResolveCornerRadius",
            "PanelAlphaFromPercent",
            "MakeBottomAccentBar",
            "MakeLeftAccentBar",
        ):
            self.assertIn(token, self.draw)
            self.assertIn(token, self.preview)
        self.assertIn("DPI(max(0, radius))", self.preview)
        self.assertNotIn("DPI(radius * 2)", self.preview)
        self.assertIn("const BYTE accent_alpha = alpha;", self.draw)
        self.assertIn("const BYTE accent_alpha = static_cast<BYTE>(alpha);", self.preview)

    def test_accent_alpha_scales_with_panel_opacity(self):
        self.assertEqual(panel_alpha(70), 178)
        self.assertEqual(panel_alpha(1), 2)
        self.assertEqual(panel_alpha(100), 255)
        self.assertEqual(accent_gradient_end_alpha(panel_alpha(70)), 83)

    def test_zero_radius_has_rectangle_fallback_and_no_old_overflow_rules(self):
        for source in (self.draw, self.preview):
            self.assertIn("graphics.FillRectangle(&brush, rc);", source)
            self.assertIn("graphics.DrawRectangle(&pen, rc);", source)
            self.assertNotIn("if (r < 1) r = 1;", source)
        self.assertNotIn("if (radius < 1) radius = 1;", self.draw)
        self.assertNotIn("if (radius * 2 > height) radius = height / 2;", self.draw)

    def test_helper_is_macro_safe_for_windows_headers(self):
        self.assertIn("(std::min)(width, height)", self.helper)
        self.assertIn("(std::max)(0", self.helper)


if __name__ == "__main__":
    unittest.main(verbosity=2)
