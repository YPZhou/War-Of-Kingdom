/* $Id: editor_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "wesnoth-editor"

#include "editor_display.hpp"
#include "builder.hpp"

namespace editor {

editor_display::editor_display(CVideo& video, const editor_map& map,
		const config& theme_cfg, const config& level)
	: display(video, &map, theme_cfg, level)
	, brush_locations_()
	, toolbar_hint_()
{
	clear_screen();
}

editor_display::~editor_display()
{
	clear_context_menu_buttons();
}

void editor_display::add_brush_loc(const map_location& hex)
{
	brush_locations_.insert(hex);
	invalidate(hex);
}

void editor_display::set_brush_locs(const std::set<map_location>& hexes)
{
	invalidate(brush_locations_);
	brush_locations_ = hexes;
	invalidate(brush_locations_);
}

void editor_display::clear_brush_locs()
{
	invalidate(brush_locations_);
	brush_locations_.clear();
}

void editor_display::remove_brush_loc(const map_location& hex)
{
	brush_locations_.erase(hex);
	invalidate(hex);
}

void editor_display::rebuild_terrain(const map_location &loc) {
	builder_->rebuild_terrain(loc);
}

void editor_display::pre_draw()
{
}

image::TYPE editor_display::get_image_type(const map_location& loc)
{
	if (brush_locations_.find(loc) != brush_locations_.end()) {
		return image::BRIGHTENED;
	} else if (map().in_selection(loc)) {
#if !defined(__APPLE__) || !TARGET_OS_IPHONE
		return image::SEMI_BRIGHTENED;
#else
		return image::BRIGHTENED;
#endif
	}
	return image::TOD_COLORED;
}

void editor_display::draw_hex(const map_location& loc)
{
	int xpos = get_location_x(loc);
	int ypos = get_location_y(loc);
	display::draw_hex(loc);
	if (map().on_board_with_border(loc)) {
		if (map().in_selection(loc)) {
			drawing_buffer_add(LAYER_FOG_SHROUD, loc, xpos, ypos,
				image::get_image("editor/selection-overlay.png", image::TOD_COLORED));
		}

		if (brush_locations_.find(loc) != brush_locations_.end()) {
			static const image::locator brush(game_config::images::editor_brush);
			drawing_buffer_add(LAYER_MOUSEOVER_OVERLAY, loc, xpos, ypos,
					image::get_image(brush, image::SCALED_TO_HEX));
		}
	}
}

const SDL_Rect& editor_display::get_clip_rect()
{
	return map_outside_area();
}

void editor_display::draw_sidebar()
{
	// Fill in the terrain report
	if(get_map().on_board_with_border(mouseoverHex_)) {
		refresh_report(reports::TERRAIN, reports::report(get_map().get_terrain_editor_string(mouseoverHex_)));
		refresh_report(reports::POSITION, reports::report(lexical_cast<std::string>(mouseoverHex_)));
	}
	refresh_report(reports::VILLAGES, reports::report(lexical_cast<std::string>(get_map().villages().size())));
	refresh_report(reports::EDITOR_TOOL_HINT, reports::report(toolbar_hint_));
}

} //end namespace editor
