// OptionsDlg.h - 插件设置对话框（纯Win32，无MFC依赖）
#pragma once
#include "../pch.h"
#include "../resource.h"

// 对话框资源ID（在resource.h中定义）
// 使用独立的 Win32 对话框，通过 DialogBoxParam 显示

class COptionsDlg {
public:
    // 显示对话框，返回 true 表示用户点击了确定
    static bool Show(HWND hParent);

private:
    static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    static void OnInitDialog(HWND hDlg);
    static bool OnOK(HWND hDlg);
    static void UpdateUI(HWND hDlg);
};
