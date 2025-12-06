#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#define ERR(...) { \
	fprintf(stderr, __VA_ARGS__); \
	fflush(stderr); \
}

#define OUT(...) { \
	fprintf(stdout, __VA_ARGS__); \
	fflush(stdout); \
}

enum mouse_state_update {
	MOUSE_UPDATE_UNCHANGED,
	MOUSE_UPDATE_LEFT_DOWN,
	MOUSE_UPDATE_LEFT_UP,
	MOUSE_UPDATE_MIDDLE_DOWN,
	MOUSE_UPDATE_MIDDLE_UP,
	MOUSE_UPDATE_RIGHT_DOWN,
	MOUSE_UPDATE_RIGHT_UP
};

struct debug_line{
	int written;
	char debug_text[1024];
};

#define WIDTH 1280
#define HEIGHT 800

struct debug_line debug_lines[800 / 12] = {0};
int cur_line = 0;

void render_debug_lines(SDL_Renderer *renderer){
	int offset = 0;
	for(int i = 0;i < sizeof(debug_lines) / sizeof(struct debug_line);i++){
		int line_number = cur_line - 1 - i;
		if (line_number < 0){
			line_number = sizeof(debug_lines) / sizeof(struct debug_line) + line_number;
		}
		if (!debug_lines[line_number].written){
			continue;
		}
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
		SDL_SetRenderScale(renderer, 1, 1);
		SDL_RenderDebugText(renderer, 0, offset * 12, debug_lines[line_number].debug_text);
		offset++;
	}

}

struct pointer_state{
	struct pointer_state *next;
	struct pointer_state *prev;
	int mouse;
	SDL_MouseID mouse_id;
	SDL_TouchID touch_id;
	SDL_FingerID finger_id;
	float x;
	float y;
	float pressure;
	bool left_down;
	bool middle_down;
	bool right_down;
};

struct pointer_state *search_state(struct pointer_state **list, int mouse, SDL_MouseID mouse_id, SDL_TouchID touch_id, SDL_FingerID finger_id){
	struct pointer_state *cur = *list;
	while(cur != NULL){
		if (mouse){
			if (cur->mouse && cur->mouse_id == mouse_id){
				return cur;
			}
		}else{
			if (!cur->mouse && cur->finger_id == finger_id && cur->touch_id == touch_id){
				return cur;
			}
		}
		cur = cur->next;
	}
	return NULL;	
}

struct pointer_state *search_mouse_state(struct pointer_state **list, SDL_MouseID mouse_id){
	return search_state(list, 1, mouse_id, 0, 0);
}

struct pointer_state *search_finger_state(struct pointer_state **list, SDL_TouchID touch_id, SDL_FingerID finger_id){
	return search_state(list, 0, 0, touch_id, finger_id);
}

void insert_state(struct pointer_state **list, int mouse, SDL_MouseID mouse_id, SDL_TouchID touch_id, SDL_FingerID finger_id, float x, float y, float pressure, enum mouse_state_update mouse_update){
	#define MOUSE_UPDATE() { \
		switch(mouse_update){ \
			case MOUSE_UPDATE_LEFT_DOWN: \
				state->left_down = true; \
				break; \
			case MOUSE_UPDATE_LEFT_UP: \
				state->left_down = false; \
				break; \
			case MOUSE_UPDATE_MIDDLE_DOWN: \
				state->middle_down = true; \
				break; \
			case MOUSE_UPDATE_MIDDLE_UP: \
				state->middle_down = false; \
				break; \
			case MOUSE_UPDATE_RIGHT_DOWN: \
				state->right_down = true; \
				break; \
			case MOUSE_UPDATE_RIGHT_UP: \
				state->right_down = false; \
				break; \
		} \
	}

	if (*list == NULL){
		struct pointer_state *state = malloc(sizeof(struct pointer_state));
		if (state == NULL){
			ERR("out of memory\n");
			exit(1);
		}
		memset(state, 0, sizeof(struct pointer_state));
		state->mouse = mouse;
		state->x = x;
		state->y = y;
		state->pressure = pressure;
		state->mouse_id = mouse_id;
		state->touch_id = touch_id;
		state->finger_id = finger_id;
		MOUSE_UPDATE();
		state->prev = NULL;
		state->next = NULL;
		list[0] = state;
		return;
	}

	struct pointer_state *state = search_state(list, mouse, mouse_id, touch_id, finger_id);
	if (state != NULL){
		state->x = x;
		state->y = y;
		state->pressure = pressure;
		MOUSE_UPDATE();
		return;
	}

	state = malloc(sizeof(struct pointer_state));
	if (state == NULL){
		ERR("out of memory\n");
		exit(1);
	}
	memset(state, 0, sizeof(struct pointer_state));
	state->mouse = mouse;
	state->x = x;
	state->y = y;
	state->pressure = pressure;
	state->mouse_id = mouse_id;
	state->touch_id = touch_id;
	state->finger_id = finger_id;
	MOUSE_UPDATE();
	state->prev = NULL;
	state->next = *list;
	list[0]->prev = state;
	list[0] = state;
}

void insert_mouse_state(struct pointer_state **list, SDL_MouseID mouse_id, float x, float y, enum mouse_state_update mouse_update){
	insert_state(list, 1, mouse_id, 0, 0, x, y, 0, mouse_update);
}

void insert_finger_state(struct pointer_state **list, SDL_TouchID touch_id, SDL_FingerID finger_id, float x, float y, float pressure){
	insert_state(list, 0, 0, touch_id, finger_id, x, y, pressure, MOUSE_UPDATE_UNCHANGED);
}

void remove_state(struct pointer_state **list, int mouse, SDL_MouseID mouse_id, SDL_TouchID touch_id, SDL_FingerID finger_id){
	struct pointer_state *state = search_state(list, mouse, mouse_id, touch_id, finger_id);
	if (state == NULL){
		ERR("please debug this, removing untracked state\n");
		ERR("mouse %d mouse_id %u touch_id %u finger_id %u\n", mouse, mouse_id, touch_id, finger_id);
		return;
	}

	struct pointer_state *prev_node = state->prev;
	struct pointer_state *next_node = state->next;
	free(state);
	if (prev_node != NULL){
		prev_node->next = next_node;
	}
	if (next_node != NULL){
		next_node->prev = prev_node;
	}
	if (state == *list){
		*list = next_node;
	}
}

void remove_mouse_state(struct pointer_state **list, SDL_MouseID mouse_id){
	remove_state(list, 1, mouse_id, 0, 0);
}

void remove_finger_state(struct pointer_state **list, SDL_TouchID touch_id, SDL_FingerID finger_id){
	remove_state(list, 0, 0, touch_id, finger_id);
}

void render_state(SDL_Renderer *renderer, SDL_Window *window, struct pointer_state **list){
	static int times = 0;
	struct pointer_state *cur = *list;
	if (times == 0){
		ERR("---\n");
	}
	while(cur != NULL){
		char text_buf[1024] = {0};
		int width = 0;
		int height = 0;
		SDL_GetWindowSize(window, &width, &height);
		float x = 0;
		float y = 0;
		if (cur->mouse){
			x = WIDTH / 2 * (cur->x / width);
			y = HEIGHT / 2 * (cur->y / height);
			sprintf(text_buf, "[] mouse %u %f %f %d %d %d", cur->mouse_id, cur->x, cur->y, cur->left_down, cur->middle_down, cur->right_down);
		}else{
			x = WIDTH / 2 * cur->x;
			y = HEIGHT / 2 * cur->y;
			sprintf(text_buf, "[] finger %u %u %f %f %f", cur->touch_id, cur->finger_id, cur->x, cur->y, cur->pressure);
		}
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
		SDL_SetRenderScale(renderer, 2, 2);
		SDL_RenderDebugText(renderer, x, y, text_buf);
		if (times == 0){
			ERR("mouse %d mouse_id %u touch_id %d finger_id %d x %f y %f pressure %f\n", cur->mouse, cur->mouse_id, cur->touch_id, cur->finger_id, cur->x, cur->y, cur->pressure)
		}
		cur = cur->next;
	}
	times ++;
	times = times % 60;
}

int main(){
	int initialized = SDL_Init(SDL_INIT_VIDEO);
	if (!initialized){
		ERR("failed initializing sdl\n");
		exit(1);
	}
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	int created = SDL_CreateWindowAndRenderer("input_test", WIDTH, HEIGHT, SDL_WINDOW_FULLSCREEN, &window, &renderer);
	if (!created){
		ERR("failed creating window and renderer\n");
		exit(1);
	}
	OUT("window created\n");

	SDL_SetRenderLogicalPresentation(renderer, WIDTH, HEIGHT, SDL_LOGICAL_PRESENTATION_STRETCH);
	SDL_SetRenderVSync(renderer, 1);
	OUT("renderer ready\n");

	struct pointer_state *pointer_states = NULL;

	SDL_SetWindowMouseGrab(window, true);

	while(1){
		int updated = 0;
		#define write_debug_line(...){ \
			debug_lines[cur_line].written = 1; \
			sprintf(debug_lines[cur_line].debug_text, __VA_ARGS__); \
			cur_line++; \
			cur_line = cur_line % (sizeof(debug_lines) / sizeof(struct debug_line)); \
			updated = 1; \
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);

		while(1){
			SDL_Event event;
			SDL_PollEvent(&event);

			switch(event.type){
				case SDL_EVENT_QUIT:
					OUT("terminated by the user\n");
					exit(0);
					break;
				case SDL_EVENT_TERMINATING:
					OUT("terminated by the OS\n");
					exit(0);
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:{
					SDL_MouseButtonEvent *mouse_event = (void *)&event;
					enum mouse_state_update mouse_update = MOUSE_UPDATE_UNCHANGED;
					switch(mouse_event->button){
						case 1:
							mouse_update = MOUSE_UPDATE_LEFT_DOWN;
							break;
						case 2:
							mouse_update = MOUSE_UPDATE_MIDDLE_DOWN;
							break;
						case 3:
							mouse_update = MOUSE_UPDATE_RIGHT_DOWN;
							break;
					}
					write_debug_line("mouse down event %u %f %f %d\n", mouse_event->which, mouse_event->x, mouse_event->y, mouse_event->button);
					ERR("mouse down %u %d\n", mouse_event->which, mouse_event->button);
					insert_mouse_state(&pointer_states, mouse_event->which, mouse_event->x, mouse_event->y, mouse_update);
					break;
				}
				case SDL_EVENT_MOUSE_MOTION:{
					SDL_MouseMotionEvent *mouse_event = (void *)&event;
					insert_mouse_state(&pointer_states, mouse_event->which, mouse_event->x, mouse_event->y, MOUSE_UPDATE_UNCHANGED);
					write_debug_line("mouse motion event %u %f %f\n", mouse_event->which, mouse_event->x, mouse_event->y);
					break;
				}
				case SDL_EVENT_MOUSE_BUTTON_UP:{
					SDL_MouseButtonEvent *mouse_event = (void *)&event;
					enum mouse_state_update mouse_update = MOUSE_UPDATE_UNCHANGED;
					switch(mouse_event->button){
						case 1:
							mouse_update = MOUSE_UPDATE_LEFT_UP;
							break;
						case 2:
							mouse_update = MOUSE_UPDATE_MIDDLE_UP;
							break;
						case 3:
							mouse_update = MOUSE_UPDATE_RIGHT_UP;
							break;
					}
					write_debug_line("mouse up event %u %f %f\n", mouse_event->which, mouse_event->x, mouse_event->y);
					ERR("mouse up %u\n", mouse_event->which);
					insert_mouse_state(&pointer_states, mouse_event->which, mouse_event->x, mouse_event->y, mouse_update);
					break;
				}
				case SDL_EVENT_MOUSE_REMOVED:{
					SDL_MouseDeviceEvent *mouse_event = (void *)&event;
					ERR("mouse removed %u\n", mouse_event->which);
					remove_mouse_state(&pointer_states, mouse_event->which);
					break;
				}
				case SDL_EVENT_FINGER_DOWN:{
					SDL_TouchFingerEvent *finger_event = (void *)&event;
					write_debug_line("finger down event %u %u %f %f %f\n", finger_event->touchID, finger_event->fingerID, finger_event->x, finger_event->y, finger_event->pressure);
					ERR("finger down %u %u\n", finger_event->touchID, finger_event->fingerID);
					insert_finger_state(&pointer_states, finger_event->touchID, finger_event->fingerID, finger_event->x, finger_event->y, finger_event->pressure);
					break;
				}
				case SDL_EVENT_FINGER_UP:{
					SDL_TouchFingerEvent *finger_event = (void *)&event;
					write_debug_line("finger up event %u %u %f %f %f\n", finger_event->touchID, finger_event->fingerID, finger_event->x, finger_event->y, finger_event->pressure);
					ERR("finger up %u %u\n", finger_event->touchID, finger_event->fingerID);
					remove_finger_state(&pointer_states, finger_event->touchID, finger_event->fingerID);
					break;
				}
				case SDL_EVENT_FINGER_CANCELED:{
					SDL_TouchFingerEvent *finger_event = (void *)&event;
					ERR("finger cancel %u %u\n", finger_event->touchID, finger_event->fingerID);
					remove_finger_state(&pointer_states, finger_event->touchID, finger_event->fingerID);
					break;
				}
				case SDL_EVENT_FINGER_MOTION:{
					SDL_TouchFingerEvent *finger_event = (void *)&event;
					write_debug_line("finger motion event %u %u %f %f %f\n", finger_event->touchID, finger_event->fingerID, finger_event->x, finger_event->y, finger_event->pressure);
					struct pointer_state *finger_state = search_finger_state(&pointer_states, finger_event->touchID, finger_event->fingerID);
					if (finger_state != NULL){
						finger_state->x = finger_event->x;
						finger_state->y = finger_event->y;
						finger_state->pressure = finger_event->pressure;
					}
					break;
				}
			}
			if (event.type == SDL_EVENT_POLL_SENTINEL){
				break;
			}
		}

		//render_debug_lines(renderer);
		render_state(renderer, window, &pointer_states);
		SDL_RenderPresent(renderer);
	}
}
