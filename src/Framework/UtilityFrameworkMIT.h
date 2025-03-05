// This file is part of "NppCppMSVS: Visual Studio Project Template for a Notepad++ C++ Plugin"
// Copyright 2025 by Randall Joseph Fellmy <software@coises.com>, <http://www.coises.com/software/>

// The source code contained in this file is independent of Notepad++ code.
// It is released under the MIT (Expat) license:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and 
// associated documentation files (the "Software"), to deal in the Software without restriction, 
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial 
// portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT 
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


// Inline utility / helper routines -- for convenience; none are required by Notepad++ or Scintilla

#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>


// Convert between Windows utf-16 strings and Scintilla's ANSI or utf-8 strings, even if they are very long.

inline std::string fromWide(std::wstring_view s, unsigned int codepage) {
    std::string r;
    size_t inputLength = s.length();
    if (!inputLength) return r;
    constexpr unsigned int safeSize = std::numeric_limits<int>::max() / 8;
    size_t workingPoint = 0;
    while (inputLength - workingPoint > safeSize) {
        int ss = safeSize;
        if (s[ss - 1] >= 0xD800 && s[ss - 1] <= 0xDBFF) --ss;  // proposed block ends with high surrogate: leave it for the next block
        int segmentLength = WideCharToMultiByte(codepage, 0, s.data() + workingPoint, ss, 0, 0, 0, 0);
        size_t outputPoint = r.length();
        r.resize(outputPoint + segmentLength);
        WideCharToMultiByte(codepage, 0, s.data() + workingPoint, ss, r.data() + outputPoint, segmentLength, 0, 0);
        workingPoint += ss;
    }
    int segmentLength = WideCharToMultiByte(codepage, 0, s.data() + workingPoint, static_cast<int>(inputLength - workingPoint), 0, 0, 0, 0);
    size_t outputPoint = r.length();
    r.resize(outputPoint + segmentLength);
    WideCharToMultiByte(codepage, 0, s.data() + workingPoint, static_cast<int>(inputLength - workingPoint), r.data() + outputPoint, segmentLength, 0, 0);
    return r;
}

inline std::wstring toWide(std::string_view s, unsigned int codepage) {
    std::wstring r;
    size_t inputLength = s.length();
    if (!inputLength) return r;
    constexpr unsigned int safeSize = std::numeric_limits<int>::max() / 2;
    size_t workingPoint = 0;
    while (inputLength - workingPoint > safeSize) {
        int ss = safeSize;
        if (codepage == CP_UTF8 && ((s[ss] & 0xC0) == 0x80)) while ((s[--ss] & 0xC0) == 0x80);  // find a first byte to start the next block
        int segmentLength = MultiByteToWideChar(codepage, 0, s.data() + workingPoint, ss, 0, 0);
        size_t outputPoint = r.length();
        r.resize(outputPoint + segmentLength);
        MultiByteToWideChar(codepage, 0, s.data() + workingPoint, ss, r.data() + outputPoint, segmentLength);
        workingPoint += ss;
    }
    int segmentLength = MultiByteToWideChar(codepage, 0, s.data() + workingPoint, static_cast<int>(inputLength - workingPoint), 0, 0);
    size_t outputPoint = r.length();
    r.resize(outputPoint + segmentLength);
    MultiByteToWideChar(codepage, 0, s.data() + workingPoint, static_cast<int>(inputLength - workingPoint), r.data() + outputPoint, segmentLength);
    return r;
}


// Get the text from a Window or a from a control in a dialog as a std::wstring

inline std::wstring GetWindowString(HWND hWnd) {
    std::wstring s(GetWindowTextLength(hWnd), 0);
    if (!s.empty()) s.resize(GetWindowText(hWnd, s.data(), static_cast<int>(s.length() + 1)));
    return s;
}

inline std::wstring GetDlgItemString(HWND hwndDlg, int item) {
    return GetWindowString(GetDlgItem(hwndDlg, item));
}


// Show a balloon tip on an edit control, or on the edit control associated with a combobox or a spin control, in a dialog.

inline bool ShowBalloonTip(HWND hwndDlg, int item, const std::wstring& text) {
    HWND hControl = GetDlgItem(hwndDlg, item);
    std::wstring classname(127, 0);
    GetClassName(hControl, classname.data(), static_cast<int>(classname.length() + 1));
    classname.resize(std::min(classname.find(L'\0'), classname.length()));
    if (classname == UPDOWN_CLASS) {
        hControl = reinterpret_cast<HWND>(SendMessage(hControl, UDM_GETBUDDY, 0, 0));
        if (!hControl) return false;
    }
    else if (classname == WC_COMBOBOX || classname == WC_COMBOBOXEX) {
        COMBOBOXINFO cbi;
        cbi.cbSize = sizeof(COMBOBOXINFO);
        if (!GetComboBoxInfo(hControl, &cbi)) return false;
        hControl = cbi.hwndItem;
    }
    EDITBALLOONTIP ebt;
    ebt.cbStruct = sizeof(EDITBALLOONTIP);
    ebt.pszTitle = L"";
    ebt.ttiIcon = TTI_NONE;
    ebt.pszText = text.data();
    if (!SendMessage(hControl, EM_SHOWBALLOONTIP, 0, reinterpret_cast<LPARAM>(&ebt))) return false;
    SendMessage(hwndDlg, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(hControl), TRUE);
    return true;
}


// A struct for managing resizable dialogs

struct DialogStretch {

    HWND dialog = 0;
    RECT original = { 0, 0, 0, 0 };
    std::map<HWND, RECT> controls;

    DialogStretch() {}

    void setup(HWND hwndDlg) {
        dialog = hwndDlg;
        GetWindowRect(dialog, &original);
        EnumChildWindows(dialog, [](HWND h, LPARAM p) -> BOOL {
            (*reinterpret_cast<std::map<HWND, RECT>*>(p)).emplace(h, RECT());
            return TRUE;
        }, reinterpret_cast<LPARAM>(&controls));
        for (auto& c : controls) {
            GetWindowRect(c.first, &c.second);
            MapWindowPoints(0, dialog, reinterpret_cast<LPPOINT>(&c.second), 2);
        }
    }

    int originalWidth () const { return original.right - original.left; }
    int originalHeight() const { return original.bottom - original.top; }

    class Stretched {
        friend struct DialogStretch;
        HWND dialog;
        const std::map<HWND, RECT>& controls;
        int addWidth = 0;
        int addHeight = 0;
        Stretched(HWND dialog, const RECT& original, const std::map<HWND, RECT>& controls) : dialog(dialog), controls(controls) {
            RECT currentRectangle;
            GetWindowRect(dialog, &currentRectangle);
            addWidth = currentRectangle.right - currentRectangle.left - original.right + original.left;
            addHeight = currentRectangle.bottom - currentRectangle.top - original.bottom + original.top;
        }
    public:
        Stretched& adjust(HWND h, double xStretch, double yStretch = 0, double xMove = 0, double yMove = 0) {
            if (controls.contains(h)) {
                const RECT& cRef = controls.at(h);
                SetWindowPos(h, 0, std::lround(cRef.left + xMove * addWidth),
                    std::lround(cRef.top + yMove * addHeight),
                    std::lround(cRef.right - cRef.left + xStretch * addWidth),
                    std::lround(cRef.bottom - cRef.top + yStretch * addHeight),
                    SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
                InvalidateRect(h, 0, TRUE);
            }
            return *this;
        }
        Stretched& adjust(int control, double xStretch, double yStretch = 0, double xMove = 0, double yMove = 0) {
            return adjust(GetDlgItem(dialog, control), xStretch, yStretch, xMove, yMove);
        }
    };

    Stretched adjust(HWND h, double xStretch, double yStretch = 0, double xMove = 0, double yMove = 0) {
        Stretched stretched(dialog, original, controls);
        return stretched.adjust(h, xStretch, yStretch, xMove, yMove);
    }

    Stretched adjust(int control, double xStretch, double yStretch = 0, double xMove = 0, double yMove = 0) {
        Stretched stretched(dialog, original, controls);
        return stretched.adjust(control, xStretch, yStretch, xMove, yMove);
    }

};
