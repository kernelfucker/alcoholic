/* See LICENSE file for license details */
/* alcoholic - eyes that follow the mouse in x */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <xcb/xcb.h>

#define version "0.1"

typedef struct{
	int x;
	int y;
	int w;
	int h;
} State;

typedef struct{
	xcb_connection_t *c;
	xcb_screen_t *s;
	xcb_window_t w;
	xcb_gcontext_t g;
	xcb_atom_t wm_d;
	xcb_atom_t wm_p;
	State left, right;
	int m_x;
	int m_y;
	int v_flag;
	int h_flag;
} Last;

void draw_eye(Last *l, State *s){
	xcb_translate_coordinates_cookie_t t = xcb_translate_coordinates(l->c, l->w, l->s->root, 0, 0);
	xcb_translate_coordinates_reply_t *r = xcb_translate_coordinates_reply(l->c, t, NULL);
	int r_x = r ? r->dst_x : 0;
	int r_y = r ? r->dst_y : 0;
	if(r) free(r);
	int cx_w = s->x + s->w/2;
	int cy_w = s->y + s->h/2;
	int cx_r = r_x + cx_w;
	int cy_r = r_y + cy_w;
	double dx = (double)l->m_x - (double)cx_r;
	double dy = (double)l->m_y - (double)cy_r;
	int rx = s->w/2 - 6;
	int ry = s->h/2 - 6;
	double maxd = (rx < ry ? rx : ry) * 0.65;
	double d = sqrt(dx*dx + dy*dy);
	if(d > maxd && d > 0.0001){
		dx *= maxd/d;
		dy *= maxd/d;
	}

	int sx = (int)(cx_w + dx) - 5;
	int sy = (int)(cy_w + dy) - 5;
	uint32_t wh = l->s->white_pixel;
	uint32_t bl = 0x000000;
	xcb_change_gc(l->c, l->g, XCB_GC_FOREGROUND, &wh);
	xcb_poly_fill_rectangle(l->c, l->w, l->g, 1, (xcb_rectangle_t[]){{s->x, s->y, s->w, s->h}});
	xcb_change_gc(l->c, l->g, XCB_GC_FOREGROUND, &bl);
	xcb_poly_arc(l->c, l->w, l->g, 1, (xcb_arc_t[]){{s->x, s->y, s->w, s->h, 0, 360<<6}});
	xcb_poly_fill_arc(l->c, l->w, l->g, 1, (xcb_arc_t[]){{sx, sy, 10, 10, 0, 360<<6}});
}

void handle(Last *l, xcb_generic_event_t *e){
	switch(e->response_type & ~0x80){
		case XCB_MOTION_NOTIFY:{
			xcb_motion_notify_event_t *m = (void *)e;
			l->m_x = m->root_x;
			l->m_y = m->root_y;
			xcb_clear_area(l->c, 0, l->w, 0, 0, 0, 0);
			draw_eye(l, &l->left);
			draw_eye(l, &l->right);
			xcb_flush(l->c);
			break;
		}

		case XCB_CONFIGURE_NOTIFY:{
			xcb_configure_notify_event_t *cf = (void *)e;
			l->left = (State){cf->width/4 - 50, cf->height/2 - 50, 100, 100};
			l->right = (State){3 * cf->width/4 - 50, cf->height/2 - 50, 100, 100};
			break;
		}

		case XCB_EXPOSE:{
			draw_eye(l, &l->left);
			draw_eye(l, &l->right);
			xcb_flush(l->c);
			break;
		}

		case XCB_CLIENT_MESSAGE:
			if(((xcb_client_message_event_t *)e)->data.data32[0] == l->wm_d) exit(0);
			break;
	}
}

void show_version(){
	printf("alcoholic-%s\n", version);
}

void help(){
	printf("usage: alcoholic [options]..\n");
	printf("options:\n");
	printf("  -v	show version information\n");
	printf("  -h	display this\n");
}

void init(Last *l, int argc, char **argv){
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "-v") == 0){
			l->v_flag = 1;
		} else if(strcmp(argv[i], "-h") == 0){
			l->h_flag = 1;
		}
	}

	if(l->v_flag){
		show_version();
		exit(0);
	}

	if(l->h_flag){
		help();
		exit(0);
	}

	l->c = xcb_connect(NULL, NULL);
	if(xcb_connection_has_error(l->c)){
		fprintf(stderr, "unable to connect x server\n");
		exit(0);
	}

	l->s = xcb_setup_roots_iterator(xcb_get_setup(l->c)).data;
	l->w = xcb_generate_id(l->c);
	uint32_t mask = XCB_CW_BACK_PIXEL|XCB_CW_EVENT_MASK;
	uint32_t vl[] = {l->s->white_pixel, XCB_EVENT_MASK_POINTER_MOTION|XCB_EVENT_MASK_KEY_PRESS|XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_EXPOSURE};
	xcb_create_window(l->c, XCB_COPY_FROM_PARENT, l->w, l->s->root, 100, 100, 220, 110, 2, XCB_WINDOW_CLASS_INPUT_OUTPUT, l->s->root_visual, mask, vl);
	const char *tl = "alcoholic";
	xcb_change_property(l->c, XCB_PROP_MODE_REPLACE, l->w, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(tl), tl);
	xcb_intern_atom_cookie_t pk = xcb_intern_atom(l->c, 0, 12, "WM_PROTOCOLS");
	xcb_intern_atom_reply_t *pl = xcb_intern_atom_reply(l->c, pk, NULL);
	l->wm_p = pl->atom;
	free(pl);
	xcb_change_property(l->c, XCB_PROP_MODE_REPLACE, l->w, l->wm_p, 4, 32, 1, &l->wm_d);
	uint32_t r_mask = XCB_EVENT_MASK_POINTER_MOTION;
	xcb_change_window_attributes(l->c, l->s->root, XCB_CW_EVENT_MASK, &r_mask);
	l->g = xcb_generate_id(l->c);
	xcb_create_gc(l->c, l->g, l->w, 0, NULL);
	l->left = (State){100, 50, 100, 100};
	l->right = (State){200, 50, 100, 100};
	l->m_x = 200;
	l->m_y = 100;
	xcb_map_window(l->c, l->w);
	xcb_flush(l->c);
}

int main(int argc, char **argv){
	Last l = {0};
	init(&l, argc, argv);
	if(l.v_flag || l.h_flag){
		return 0;
	}

	xcb_generic_event_t *e;
	while((e = xcb_wait_for_event(l.c))){
		handle(&l, e);
		free(e);
	}

	xcb_disconnect(l.c);
	return 0;
}
