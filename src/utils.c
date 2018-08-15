#include "osu.h"
#include "pattern.h"

#include <time.h>
#include <stdio.h>

#ifdef ON_LINUX
  Display *display;
#endif /* ON_LINUX */

#ifdef ON_WINDOWS
  #include <windows.h>
  #include <tlhelp32.h>

  HWND game_window;
  HANDLE game_proc;

  struct handle_data {
	HWND window_handle;
	unsigned long process_id;
  };

  __stdcall int enum_windows_callback(HWND handle, void *param);

  /**
   * Returns a handle to the main window of the process with the given ID.
   */
  HWND find_window(unsigned long process_id);
#endif /* ON_WINDOWS */

void *time_address;
pid_t game_proc_id;

void send_keypress(char key, int down)
{
#ifdef ON_LINUX
	int keycode = XKeysymToKeycode(display, key);

	XTestFakeKeyEvent(display, keycode, down, CurrentTime);

	XFlush(display);
#endif /* ON_LINUX */

#ifdef ON_WINDOWS
	INPUT in;

	in.type = INPUT_KEYBOARD;

	in.ki.time = 0;
	in.ki.wScan = 0;
	in.ki.dwExtraInfo = 0;
	in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
	in.ki.wVk = VkKeyScanEx(key, GetKeyboardLayout(0)) & 0xFF;

	SendInput(1, &in, sizeof(INPUT));
#endif /* ON_WINDOWS */
}

void do_setup()
{
#ifdef ON_LINUX
	if (!(display = XOpenDisplay(NULL))) {
		printf("failed to open X display");

		return;
	} else debug("opened X display (%#x)", display);
#endif /* ON_LINUX */

#ifdef ON_WINDOWS
	if (!(game_proc = OpenProcess(PROCESS_VM_READ, 0, game_proc_id))) {
		printf("failed to get handle to game process\n");
		return;
	} else debug("got handle to game process with ID %d", game_proc_id);

	if (!(game_window = find_window(game_proc_id))) {
		printf("failed to find game window\n");
		return;
	} else debug("found game window");;
#endif /* ON_WINDOWS */
}

void *get_time_address()
{
#ifdef ON_WINDOWS
	void *time_address = NULL;
	void *time_ptr = find_pattern((unsigned char *)SIGNATURE,
		sizeof(SIGNATURE) - 1);

	if (!ReadProcessMemory(game_proc, (void *)time_ptr, &time_address,
		sizeof(DWORD), NULL))
	{
		return NULL;
	}

	return time_address;
#endif

#ifdef ON_LINUX
	return (void *)LINUX_TIME_ADDRESS;
#endif
}

int get_window_title(char **title)
{
#ifdef ON_WINDOWS
	const int title_len = 128;
	*title = malloc(title_len);
	return GetWindowText(game_window, *title, title_len);
#endif /* ON_WINDOWS */

	return 0;
}

// I hate having to do this but I can't think of a cleaner solution. (TODO?)
#ifdef ON_WINDOWS
HWND find_window(unsigned long process_id)
{
	struct handle_data data = { 0, process_id };
	EnumWindows((WNDENUMPROC)enum_windows_callback, (LPARAM)&data);

	return data.window_handle;
}

__stdcall int enum_windows_callback(HWND handle, void *param)
{
	struct handle_data *data = (struct handle_data *)param;

	unsigned long process_id = 0;
	GetWindowThreadProcessId(handle, &process_id);

	if (process_id != data->process_id)
		return 1;

	data->window_handle = handle;
	return 0;
}
#endif /* ON_WINDOWS */

// TODO: I'm certain there's a more elegant way to go about this.
int partial_match(char *base, char *partial)
{
	int i = 0;
	int m = 0;
	while (*base) {
		char c = partial[i];
		if (c == '.') {
			i++;
			continue;
		}

		if (*base++ == c) {
			i++;
			m++;
		}
	}

	return m;
}

void path_get_last(char *path, char **last)
{
	int i, l = 0;
	*last = path;
	for (i = 0; *path; path++, i++)
		if (*path == SEPERATOR)
			l = i + 1;

	*last += l;
}

void debug(char *fmt, ...)
{
#ifdef DEBUG
	static clock_t old_clock = 0;
	clock_t cur_clock = clock();

	va_list args;
	va_start(args, fmt);

	char message[256];
	sprintf(message, "[debug] [t:%-6ld s:%-.8x]   ", cur_clock - old_clock,
		(int)(uintptr_t)__builtin_return_address(1));
	strcat(message, fmt);

	int len = strlen(message);
	message[len] = '\n';
	message[len + 1] = '\0';

	vprintf(message, args);

	va_end(args);

	old_clock = clock();
#endif /* DEBUG */
}
