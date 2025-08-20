#include "../../cs2/shared/cs2game.h"
// #include "../../csgo/shared/csgogame.h"
#include "../../apex/shared/apexgame.h"

#include <sys/time.h>
#include <linux/types.h>
#include <linux/input-event-codes.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include <SDL3/SDL.h>
#include <cstdlib>

static SDL_Renderer* renderer;


static int fd;

struct input_event
{
	struct timeval time;
	__u16 type, code;
	__s32 value;
};

static ssize_t
send_input(int dev, __u16 type, __u16 code, __s32 value)
{
	struct input_event start, end;
	ssize_t wfix;

	gettimeofday(&start.time, 0);
	start.type = type;
	start.code = code;
	start.value = value;

	gettimeofday(&end.time, 0);
	end.type = EV_SYN;
	end.code = SYN_REPORT;
	end.value = 0;
	wfix = write(dev, &start, sizeof(start));
	wfix = write(dev, &end, sizeof(end));
	return wfix;
}

static int open_device(const char *name, size_t length)
{
	int fd = -1;
	DIR *d = opendir("/dev/input/by-id/");
	struct dirent *e;
	char p[512];

	while ((e = readdir(d)))
	{
		if (e->d_type != 10)
			continue;
		if (strcmp(strrchr(e->d_name, '\0') - length, name) == 0)
		{
			snprintf(p, sizeof(p), "/dev/input/by-id/%s", e->d_name);
			fd = open(p, O_RDWR);
			break;
		}
	}
	closedir(d);
	return fd;
}

namespace client
{
	void mouse_move(int x, int y)
	{
		send_input(fd, EV_REL, 0, x);
		send_input(fd, EV_REL, 1, y);
	}

	void mouse1_down(void)
	{
		send_input(fd, EV_KEY, 0x110, 1);
	}

	void mouse1_up(void)
	{
		send_input(fd, EV_KEY, 0x110, 0);
	}

	void DrawRect(void *hwnd, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
	{
		SDL_FRect rect{};
		rect.x = (float)x;
		rect.y = (float)y;
		rect.w = (float)w;
		rect.h = (float)h;
		SDL_SetRenderDrawColor(renderer, r, g, b, 50);
		SDL_RenderFillRect(renderer, &rect);

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderRect(renderer, &rect);
	}

	void DrawFillRect(void *hwnd, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b)
	{
		SDL_FRect rect{};
		rect.x = (float)x;
		rect.y = (float)y;
		rect.w = (float)w;
		rect.h = (float)h;
		SDL_SetRenderDrawColor(renderer, r, g, b, 255);
		SDL_RenderFillRect(renderer, &rect);
	}
}

int main(void)
{
	fd = open_device("event-mouse", 11);
	if (fd == -1)
	{
		LOG("failed to open mouse\n");
		return 0;
	}
	LOG("EC started successfully\n");

    // SDL3: SDL_Init returns true on success, false on failure
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG("SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }
    LOG("SDL_Init OK\n");

    const char* video_driver = SDL_GetCurrentVideoDriver();
    LOG("SDL video driver: %s\n", video_driver ? video_driver : "(null)");
    bool is_wayland = (video_driver && strcmp(video_driver, "wayland") == 0);
    
    SDL_Window *temp = SDL_CreateWindow("EC", 1, 1, SDL_WINDOW_MINIMIZED | SDL_WINDOW_OPENGL);
    if (temp == NULL)
    {
        LOG("SDL_CreateWindow (temp) failed: %s\n", SDL_GetError());
        return 0;
    }
    /* SDL_DisplayID id = SDL_GetDisplayForWindow(temp); */
    if (!is_wayland) {
        SDL_SetWindowPosition(temp, 0, 0);
    }

    // Compute window bounds:
    // - On Wayland: use the primary display only (no global coords, no spanning).
    // - Else: prefer primary display; fall back to previous logic if needed.
    SDL_Rect bounds{};
    bool have_bounds = false;

    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (primary != 0 && SDL_GetDisplayBounds(primary, &bounds) == 0) {
        have_bounds = true;
    } else {
        // Fallback: try legacy enumeration to get at least some bounds (non-Wayland)
        if (!is_wayland) {
            int total_displays = 0;
            SDL_DisplayID* display_ids = SDL_GetDisplays(&total_displays);
            if (total_displays > 0 && display_ids != NULL) {
                int display_min_x = INT_MAX;
                int display_min_y = INT_MAX;
                int display_max_x = 0;
                int display_max_y = 0;
                for (int i = 0; i < total_displays && display_ids[i] != 0; i++)
                {
                    SDL_Rect rect;
                    if (SDL_GetDisplayBounds(display_ids[i], &rect) == 0) {
                        display_min_x = SDL_min(display_min_x, rect.x);
                        display_min_y = SDL_min(display_min_y, rect.y);
                        display_max_x = SDL_max(display_max_x, rect.x + rect.w);
                        display_max_y = SDL_max(display_max_y, rect.y + rect.h);
                    }
                }
                if (display_min_x != INT_MAX) {
                    bounds.x = display_min_x;
                    bounds.y = display_min_y;
                    bounds.w = display_max_x - display_min_x;
                    bounds.h = display_max_y - display_min_y;
                    have_bounds = true;
                }
            }
            if (display_ids) SDL_free(display_ids);
        }
    }

    if (!have_bounds) {
        bounds.x = 0;
        bounds.y = 0;
        bounds.w = 640;
        bounds.h = 480;
        LOG("Using fallback window size: %dx%d (GetDisplayBounds unavailable)\n", bounds.w, bounds.h);
    }

    SDL_Window *window = nullptr;
    if (is_wayland) {
        // Wayland: popup windows require a grab and cannot be freely positioned.
        // Use a normal borderless transparent window sized to the primary display.
        window = SDL_CreateWindow("EC", bounds.w, bounds.h, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS);
        if (window == NULL) {
            LOG("SDL_CreateWindow failed (wayland): %s\n", SDL_GetError());
            return 0;
        }
    } else {
        window = SDL_CreatePopupWindow(temp, 0, 0, 640, 480,
            SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_TOOLTIP);
        if (window == NULL) {
            LOG("SDL_CreatePopupWindow failed: %s\n", SDL_GetError());
            // Fallback: create a normal borderless window
            window = SDL_CreateWindow("EC", 640, 480, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS);
            if (window == NULL) {
                LOG("SDL_CreateWindow fallback failed: %s\n", SDL_GetError());
                return 0;
            }
        }
    }

    SDL_SetWindowSize(window, bounds.w, bounds.h);
    if (!is_wayland) {
        SDL_SetWindowPosition(window, bounds.x, bounds.y);
        SDL_SetWindowAlwaysOnTop(window, true);
    }

    renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        LOG("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	bool quit = false;
	while (!quit)
	{
		SDL_Event e;
		while ( SDL_PollEvent( &e ) )
		{
			if (e.type == SDL_EVENT_QUIT)
			{
				quit = true;
				break;
			}
		}

		SDL_RenderClear(renderer);

		// Always draw a small indicator in top-left corner
		client::DrawFillRect(nullptr, 10, 10, 100, 20, 0, 255, 0); // Green bar

		if (cs2::game_handle)
		{
			if (cs2::running())
			{
				cs2::features::run();
			}
			else
			{
				cs2::features::reset();
			}
		}
		/*
		else if (csgo::game_handle)
		{
			if (csgo::running())
			{
				csgo::features::run();
			}
			else
			{
				csgo::features::reset();
			}
		}
		*/
		else if (apex::game_handle)
		{
			if (apex::running())
			{
				apex::features::run();
			}
			else
			{
				apex::features::reset();
			}
		}
		else
		{
			bool is_running = false;
			if (!is_running) is_running = cs2::running();
			// if (!is_running) is_running = csgo::running();
			if (!is_running) is_running = apex::running();
			
			// Show status in top-left
			if (!is_running) {
				client::DrawFillRect(nullptr, 120, 10, 100, 20, 255, 0, 0); // Red bar = no game
			}
		}
		
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderPresent(renderer);
	}
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

