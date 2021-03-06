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

#include "cg_local.h"

#include "CvarTextView.h"

#define _Class _CvarTextView

#pragma mark - View

/**
 * @see View::awakeWithDictionary(View *, const Dictionary *)
 */
static void awakeWithDictionary(View *self, const Dictionary *dictionary) {

	super(View, self, awakeWithDictionary, dictionary);

	CvarTextView *this = (CvarTextView *) self;

	const Inlet inlets[] = MakeInlets(
		MakeInlet("var", InletTypeApplicationDefined, &this->var, Cg_BindCvar)
	);

	$(self, bind, inlets, dictionary);
}

/**
 * @see View::init(View *)
 */
static View *init(View *self) {
	return (View *) $((CvarTextView *) self, initWithVariable, NULL);
}

/**
 * @see View::updateBindings(View *)
 */
static void updateBindings(View *self) {

	super(View, self, updateBindings);

	CvarTextView *this = (CvarTextView *) self;
	if (this->var) {
		$((TextView *) self, setDefaultText, this->var->string);
	} else {
		$((TextView *) self, setDefaultText, NULL);
	}
}

#pragma mark - Control

/**
 * @see Control::stateDidChange(Control *)
 */
static void stateDidChange(Control *self) {

	super(Control, self, stateDidChange);

	const CvarTextView *this = (CvarTextView *) self;
	if (this->var) {
		if (!$(self, isFocused)) {
			const String *string = (String *) this->textView.attributedText;
			cgi.SetCvarString(this->var->name, string->chars);
		}
	}
}

#pragma mark - CvarTextView

/**
 * @fn CvarTextView *CvarTextView::initWithVariable(CvarTextView *self, cvar_t *var)
 *
 * @memberof CvarTextView
 */
static CvarTextView *initWithVariable(CvarTextView *self, cvar_t *var) {

	self = (CvarTextView *) super(TextView, self, initWithFrame, NULL);
	if (self) {
		self->var = var;
	}

	return self;
}

#pragma mark - Class lifecycle

/**
 * @see Class::initialize(Class *)
 */
static void initialize(Class *clazz) {

	((ViewInterface *) clazz->interface)->awakeWithDictionary = awakeWithDictionary;
	((ViewInterface *) clazz->interface)->init = init;
	((ViewInterface *) clazz->interface)->updateBindings = updateBindings;

	((ControlInterface *) clazz->interface)->stateDidChange = stateDidChange;

	((CvarTextViewInterface *) clazz->interface)->initWithVariable = initWithVariable;
}

/**
 * @fn Class *CvarTextView::_CvarTextView(void)
 * @memberof CvarTextView
 */
Class *_CvarTextView(void) {
	static Class *clazz;
	static Once once;

	do_once(&once, {
		clazz = _initialize(&(const ClassDef) {
			.name = "CvarTextView",
			.superclass = _TextView(),
			.instanceSize = sizeof(CvarTextView),
			.interfaceOffset = offsetof(CvarTextView, interface),
			.interfaceSize = sizeof(CvarTextViewInterface),
			.initialize = initialize,
		});
	});

	return clazz;
}

#undef _Class

