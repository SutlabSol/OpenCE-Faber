/*
WIN32_WINDOW.C

Keep borderless OpenGL windows on the desktop. NVIDIA's driver treats
SDL's WS_POPUP fullscreen window as exclusive even without a display mode,
which blanks the monitor when the player switches to another application.
*/

#include <windows.h>
#include <SDL3/SDL.h>

void win32_window_borderless_apply(SDL_Window *window)
{
	HWND handle;
	LONG_PTR style;

	if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) || SDL_GetWindowFullscreenMode(window))
		return;
	handle = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	if (!handle)
		return;
	style = GetWindowLongPtr(handle, GWL_STYLE);
	if (!(style & WS_POPUP) && !(GetWindowLongPtr(handle, GWL_EXSTYLE) & WS_EX_TOPMOST))
		return;
	SetWindowLongPtr(handle, GWL_STYLE, style & ~WS_POPUP);
	SetWindowPos(handle, HWND_NOTOPMOST, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

int win32_window_open_web_overlay(SDL_Window *window, const char *url)
{
	static const char *const edge_paths[] = {
		"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
		"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe"
	};
	int win_x = 120, win_y = 80, win_w = 1280, win_h = 720;
	int ov_w = 1024, ov_h = 680, ov_x, ov_y;
	char cmdline[1024];
	int i;

	if (!url || !url[0])
		return 0;
	if (window)
	{
		SDL_GetWindowPosition(window, &win_x, &win_y);
		SDL_GetWindowSize(window, &win_w, &win_h);
	}
	if (win_w > 320 && win_h > 240)
	{
		ov_w = (win_w * 85) / 100;
		ov_h = (win_h * 85) / 100;
		if (ov_w < 800) ov_w = 800;
		if (ov_h < 560) ov_h = 560;
	}
	ov_x = win_x + (win_w - ov_w) / 2;
	ov_y = win_y + (win_h - ov_h) / 2;

	for (i = 0; i < (int)(sizeof(edge_paths) / sizeof(edge_paths[0])); i++)
	{
		DWORD attrs = GetFileAttributesA(edge_paths[i]);
		if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
		{
			STARTUPINFOA si;
			PROCESS_INFORMATION pi;

			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			ZeroMemory(&pi, sizeof(pi));
			SDL_snprintf(
				cmdline,
				sizeof(cmdline),
				"\"%s\" --app=\"%s\" --window-size=%d,%d --window-position=%d,%d",
				edge_paths[i],
				url,
				ov_w,
				ov_h,
				ov_x,
				ov_y);
			if (CreateProcessA(edge_paths[i], cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
			{
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				return 1;
			}
		}
	}
	return 0;
}
