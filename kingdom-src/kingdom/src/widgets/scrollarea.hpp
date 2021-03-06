/* $Id: scrollarea.hpp 40489 2010-01-01 13:16:49Z mordante $ */
/*
   Copyright (C) 2004 - 2010 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file widgets/scrollarea.hpp */

#ifndef SCROLLAREA_HPP_INCLUDED
#define SCROLLAREA_HPP_INCLUDED

#include "SDL.h"
#include "../sdl_utils.hpp"
#include "scrollbar.hpp"
#include "widget.hpp"

namespace gui {

class scrollarea : public widget
{
public:
	/**
	 * Create a zone with automatic handling of scrollbar.
	 * @todo FIXME: parameterlist ??
	 */
	//- \param d the display object
	//- \param pane the widget where wheel events take place
	scrollarea(CVideo &video, bool auto_join=true);

	virtual void hide(bool value = true);

protected:
	virtual handler_vector handler_members();
	virtual void update_location(SDL_Rect const &rect);
	virtual void handle_event(const SDL_Event& event);
	virtual void process_event();
	virtual void scroll(unsigned int pos) = 0;
	virtual void set_inner_location(SDL_Rect const &rect) = 0;

	SDL_Rect inner_location() const;

	unsigned scrollbar_width() const;
	unsigned get_position() const;
	unsigned get_max_position() const;
	void set_position(unsigned pos);
	void adjust_position(unsigned pos);
	void move_position(int dep);
	void set_shown_size(unsigned h);
	void set_full_size(unsigned h);
	void set_scroll_rate(unsigned r);
	bool has_scrollbar() const;

	unsigned hscrollbar_height() const;
	unsigned get_hposition() const;
	unsigned get_max_hposition() const;
	void set_hposition(unsigned pos);
	void adjust_hposition(unsigned pos);
	void move_hposition(int dep);
	void set_hshown_size(unsigned w);
	void set_hfull_size(unsigned w);
	void set_hscroll_rate(unsigned r);
	bool has_hscrollbar() const;

private:
	scrollbar vscrollbar_;
	scrollbar hscrollbar_;
	int old_vposition_;
	int old_hposition_;
	bool recursive_;
	bool shown_vscrollbar_;
	bool shown_hscrollbar_;
	unsigned vshown_size_;
	unsigned hshown_size_;
	unsigned vfull_size_;
	unsigned hfull_size_;

	void test_vscrollbar();
};

} // end namespace gui

#endif
