/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "ui_local.h"

#include "viewcontrollers/MainViewController.h"

#include "client.h"

extern cl_static_t cls;

static WindowController *windowController;
static ViewController *viewController;

/**
 * @brief Dispatch events to the user interface. Filter most common event types for
 * performance consideration.
 */
void Ui_HandleEvent(const SDL_Event *event) {

	if (cls.key_state.dest != KEY_UI) {

		switch (event->type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
			case SDL_MOUSEMOTION:
			case SDL_TEXTINPUT:
			case SDL_TEXTEDITING:
				return;
		}
	}

	$(windowController, respondToEvent, event);
}

/**
 * @brief Renders the user interface to a texture in a reserved OpenGL context, then
 * blits it back to the screen in the default context. A separate OpenGL context is
 * used to avoid OpenGL state pollution.
 */
void Ui_Draw(void) {
	extern void R_BindDefaultArray(GLenum type);

	if (cls.key_state.dest != KEY_UI) {
		return;
	}

	GLuint texnum;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *) &texnum);

	glDisable(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0, r_context.window_width, r_context.window_height, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	$(windowController, render);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	glBindTexture(GL_TEXTURE_2D, texnum);

	glOrtho(0, r_context.width, r_context.height, 0, -1, 1);
}

/**
 * @brief Initializes the user interface.
 */
void Ui_Init(void) {

	windowController = $(alloc(WindowController), initWithWindow, r_context.window);

	viewController = $((ViewController *) alloc(MainViewController), init);

	$(windowController, setViewController, viewController);
}

/**
 * @brief Shuts down the user interface.
 */
void Ui_Shutdown(void) {

	release(viewController);
	release(windowController);

	Mem_FreeTag(MEM_TAG_UI);
}
