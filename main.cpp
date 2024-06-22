#include <iostream>
#include <fstream>
#include <filesystem>
#include <richedit.h>
#include "aviutl.hpp"
#include "exedit.hpp"

constexpr auto plugin_name = "メモ帳";
constexpr auto plugin_information = "メモ帳 r1 by 蛇色";

inline static constexpr struct CommandID {
	inline static constexpr auto c_cut = 1;
	inline static constexpr auto c_copy = 2;
	inline static constexpr auto c_paste = 3;
	inline static constexpr auto c_erase = 4;
	inline static constexpr auto c_select_all = 5;
	inline static constexpr auto c_undo = 6;
	inline static constexpr auto c_redo = 7;
	inline static constexpr auto c_use_project = 1000;
} c_command_id;

struct ProjectData {
	WCHAR text[1];
};

HWND editbox = nullptr;

std::filesystem::path get_module_file_name(HINSTANCE instance)
{
	WCHAR file_name[MAX_PATH] = {};
	::GetModuleFileNameW(instance, file_name, MAX_PATH);
	return file_name;
}

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
{
	switch (message)
	{
	case AviUtl::FilterPlugin::WindowMessage::Init:
		{
			// メニューを作成します。
			auto menu = ::CreateMenu();
			auto config_menu = ::CreatePopupMenu();
			::AppendMenuW(config_menu, MF_STRING, c_command_id.c_use_project, L"プロジェクトと連動");
			::AppendMenuW(menu, MF_POPUP, (UINT_PTR)config_menu, L"設定");
			::SetMenu(hwnd, menu);

			// エディットボックスを作成します。
			::LoadLibraryW(L"riched20.dll");
			editbox = ::CreateWindowExW(
				WS_EX_CLIENTEDGE,
				RICHEDIT_CLASSW,
				nullptr,
				WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL |
				WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
				ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
				0, 0, 0, 0,
				hwnd, nullptr, nullptr, nullptr);
			if (!editbox)
			{
				::MessageBoxW(hwnd, L"エディットボックスの作成に失敗しました", L"メモ帳", MB_OK);

				break;
			}

			// エディットボックスのフォントを設定します。
			AviUtl::SysInfo si = {};
			fp->exfunc->get_sys_info(nullptr, &si);
			::SendMessage(editbox, WM_SETFONT, (WPARAM)si.hfont, FALSE);

			// エディットボックスのイベントマスクを設定します。
			auto event_mask = ::SendMessage(editbox, EM_GETEVENTMASK, 0, 0);
			event_mask |= ENM_MOUSEEVENTS;
			::SendMessage(editbox, EM_SETEVENTMASK, 0, event_mask);

			// エディットボックスの言語オプションを設定します。
			auto lang_options = ::SendMessage(editbox, EM_GETLANGOPTIONS, 0, 0); 
//			lang_options &= ~IMF_DUALFONT;
//			lang_options &= ~IMF_AUTOFONT;
//			lang_options &= ~IMF_AUTOFONTSIZEADJUST;
			lang_options |= IMF_UIFONTS;
			::SendMessage(editbox, EM_SETLANGOPTIONS, 0, lang_options);

			// エディットボックスの余白を設定します。
			RECT margin = { 6, 6, 6, 6 };
			::SendMessage(editbox, EM_SETRECT, TRUE, (LPARAM)&margin);
//			::SendMessage(editbox, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));

			// ファイルにアクセスする準備をします。
			auto module_file_name = get_module_file_name(fp->dll_hinst);
			auto ini_file_name = module_file_name.wstring() + L".ini";
			auto txt_file_name = module_file_name.wstring() + L".txt";

			// iniファイルから設定を読み込みます。
			auto use_project = ::GetPrivateProfileIntW(L"config", L"use_project", TRUE, ini_file_name.c_str());
			if (use_project) ::CheckMenuItem(config_menu, c_command_id.c_use_project, MF_CHECKED);

			try
			{
				// txtファイルから文字列を読み込みます。
				std::ifstream ifs(txt_file_name, std::ios_base::binary);
				auto file_size = (size_t)std::filesystem::file_size(txt_file_name);
				std::vector<char> buffer(file_size + sizeof(WCHAR), '\0');
				ifs.read(buffer.data(), buffer.size());
				::SetWindowText(editbox, (LPCWSTR)buffer.data());
			}
			catch (...)
			{
			}

			break;
		}
	case AviUtl::FilterPlugin::WindowMessage::Exit:
		{
			// ファイルにアクセスする準備をします。
			auto module_file_name = get_module_file_name(fp->dll_hinst);
			auto ini_file_name = module_file_name.wstring() + L".ini";
			auto txt_file_name = module_file_name.wstring() + L".txt";

			// iniファイルに設定を書き込みます。
			auto menu = ::GetMenu(hwnd);
			auto use_project = ::GetMenuState(menu, c_command_id.c_use_project, MF_BYCOMMAND);
			::WritePrivateProfileStringW(L"config", L"use_project",
				std::to_wstring(!!(use_project & MF_CHECKED)).c_str(), ini_file_name.c_str());

			try
			{
				// txtファイルに文字列を書き込みます。
				std::ofstream ofs(txt_file_name, std::ios_base::binary);
				auto text_length = ::GetWindowTextLengthW(editbox);
				std::vector<WCHAR> buffer(text_length + 1, L'\0');
				::GetWindowTextW(editbox, buffer.data(), buffer.size());
				ofs.write((const char*)buffer.data(), text_length * sizeof(WCHAR));
			}
			catch (...)
			{
			}

			break;
		}
	case WM_SIZE:
		{
			// エディットボックスをリサイズします。
			RECT rc; ::GetClientRect(hwnd, &rc);
			auto x = rc.left;
			auto y = rc.top;
			auto w = rc.right - rc.left;
			auto h = rc.bottom - rc.top;
			::SetWindowPos(editbox, nullptr, x, y, w, h, SWP_NOZORDER);

			break;
		}
	case WM_COMMAND:
		{
			auto code = HIWORD(wParam);
			auto id = LOWORD(wParam);
			auto control = (HWND)lParam;

			switch (id)
			{
			case c_command_id.c_cut: return ::SendMessage(editbox, WM_CUT, 0, 0);
			case c_command_id.c_copy: return ::SendMessage(editbox, WM_COPY, 0, 0);
			case c_command_id.c_paste: return ::SendMessage(editbox, WM_PASTE, 0, 0);
			case c_command_id.c_erase: return ::SendMessage(editbox, WM_KEYDOWN, VK_DELETE, 0);
			case c_command_id.c_select_all: return ::SendMessage(editbox, EM_SETSEL, 0, -1);
			case c_command_id.c_undo: return ::SendMessage(editbox, EM_UNDO, 0, 0);
			case c_command_id.c_redo: return ::SendMessage(editbox, EM_REDO, 0, 0);
			case c_command_id.c_use_project:
				{
					auto menu = ::GetMenu(hwnd);
					auto use_project = ::GetMenuState(menu, c_command_id.c_use_project, MF_BYCOMMAND);
					use_project ^= MF_CHECKED;
					::CheckMenuItem(menu, c_command_id.c_use_project, use_project);

					break;
				}
			}

			break;
		}
	case WM_NOTIFY:
		{
			auto header = (NMHDR*)lParam;

			switch (header->code)
			{
			case EN_MSGFILTER:
				{
					auto msg_filter = (MSGFILTER*)lParam;

					switch (msg_filter->msg)
					{
					case WM_RBUTTONDOWN:
						{
							// エディットボックスのコンテキストメニューを表示します。
							auto menu = ::CreatePopupMenu();
							AppendMenuW(menu, MF_STRING, c_command_id.c_undo, L"元に戻す(&U)");
							AppendMenuW(menu, MF_STRING, c_command_id.c_redo, L"やり直し(&R)");
							AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
							AppendMenuW(menu, MF_STRING, c_command_id.c_cut, L"切り取り(&T)");
							AppendMenuW(menu, MF_STRING, c_command_id.c_copy, L"コピー(&C)");
							AppendMenuW(menu, MF_STRING, c_command_id.c_paste, L"貼り付け(&P)");
							AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
							AppendMenuW(menu, MF_STRING, c_command_id.c_erase, L"削除(&D)");
							AppendMenuW(menu, MF_STRING, c_command_id.c_select_all, L"すべて選択(&A)");
							if (!::SendMessage(editbox, EM_CANUNDO, 0, 0)) ::EnableMenuItem(menu, c_command_id.c_undo, MF_DISABLED | MF_GRAYED);
							if (!::SendMessage(editbox, EM_CANREDO, 0, 0)) ::EnableMenuItem(menu, c_command_id.c_redo, MF_DISABLED | MF_GRAYED);
							auto pt = POINT { LOWORD(msg_filter->lParam), HIWORD(msg_filter->lParam) };
							::ClientToScreen(msg_filter->nmhdr.hwndFrom, &pt);
							::TrackPopupMenuEx(menu, 0, pt.x, pt.y, hwnd, nullptr);
							::DestroyMenu(menu);

							break;
						}
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

BOOL func_project_load(AviUtl::FilterPlugin* fp, AviUtl::EditHandle* editp, void* data, int32_t size)
{
	auto menu = ::GetMenu(fp->hwnd);
	auto use_project = ::GetMenuState(menu, c_command_id.c_use_project, MF_BYCOMMAND);
	if (!(use_project & MF_CHECKED)) return FALSE;

	if (!data) return FALSE;
	if (size < sizeof(ProjectData)) return FALSE;

	auto project_data = (ProjectData*)data;
	::SetWindowText(editbox, project_data->text);

	return TRUE;
}

BOOL func_project_save(AviUtl::FilterPlugin* fp, AviUtl::EditHandle* editp, void* data, int32_t* size)
{
	auto menu = ::GetMenu(fp->hwnd);
	auto use_project = ::GetMenuState(menu, c_command_id.c_use_project, MF_BYCOMMAND);
	if (!(use_project & MF_CHECKED)) return FALSE;

	if (!data)
	{
		auto text_length = ::GetWindowTextLengthW(editbox);
		*size = sizeof(ProjectData) + text_length * sizeof(WCHAR);

		return TRUE;
	}

	auto project_data = (ProjectData*)data;
	auto text_length = ::GetWindowTextLengthW(editbox);
	::GetWindowTextW(editbox, project_data->text, text_length + 1);

	return TRUE;
}

EXTERN_C AviUtl::FilterPluginDLL* WINAPI GetFilterTable()
{
	static AviUtl::FilterPluginDLL filter =
	{
		.flag =
			AviUtl::FilterPluginDLL::Flag::AlwaysActive |
			AviUtl::FilterPluginDLL::Flag::DispFilter |
			AviUtl::FilterPluginDLL::Flag::WindowThickFrame |
			AviUtl::FilterPluginDLL::Flag::WindowSize |
			AviUtl::FilterPluginDLL::Flag::ExInformation,
		.x = 800,
		.y = 600,
		.name = plugin_name,
		.func_WndProc = func_WndProc,
		.information = plugin_information,
		.func_project_load = func_project_load,
		.func_project_save = func_project_save,
	};

	return &filter;
}
