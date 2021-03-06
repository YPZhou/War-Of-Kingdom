/* $Id: listbox.cpp 54521 2012-06-30 17:46:54Z mordante $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI2_EXPERIMENTAL_LISTBOX

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/widgets/listbox.hpp"

#include "gui/auxiliary/layout_exception.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/listbox.hpp"
#include "gui/auxiliary/window_builder/listbox.hpp"
#include "gui/auxiliary/window_builder/horizontal_listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(listbox)

namespace {
// in separate namespace to avoid name classes
REGISTER_WIDGET3(tlistbox_definition, horizontal_listbox, _4)

void callback_list_item_clicked(twidget* caller)
{
	get_parent<tlistbox>(caller)->list_item_clicked(caller);
}

} // namespace

tlistbox::tlistbox(const bool has_minimum, const bool has_maximum,
		const tgenerator_::tplacement placement, const bool select)
	: tscrollbar_container(2) // FIXME magic number
	, generator_(
			tgenerator_::build(has_minimum, has_maximum, placement, select))
	, list_builder_(NULL)
	, callback_value_changed_(NULL)
	, need_layout_(false)
	, best_size_(0, 0)
	, content_size_(0, 0)
{
}

void tlistbox::add_row(const string_map& item, const int index)
{
	assert(generator_);
	generator_->create_item(
			index, list_builder_, item, callback_list_item_clicked);
}

void tlistbox::add_row(
		  const std::map<std::string /* widget id */, string_map>& data
		, const int index)
{
	assert(generator_);
	generator_->create_item(
			index, list_builder_, data, callback_list_item_clicked);
}

void tlistbox::remove_row(const unsigned row, unsigned count)
{
	assert(generator_);

	if(row >= get_item_count()) {
		return;
	}

	if(!count || count > get_item_count()) {
		count = get_item_count();
	}

	int height_reduced = 0;
	for(; count; --count) {
		if(generator_->item(row).get_visible() != INVISIBLE) {
			height_reduced += generator_->item(row).get_height();
		}
		generator_->delete_item(row);
	}

	if(height_reduced != 0) {
		resize_content(0, -height_reduced);
	}
}

void tlistbox::clear()
{
	// Due to the removing from the linked group, don't use
	// generator_->clear() directly.
	remove_row(0, 0);
}

void tlistbox::sort(void* caller, bool (*callback)(void*, tgrid&, tgrid&))
{
	if (!generator_) return;
	generator_->sort(caller, callback);
}

unsigned tlistbox::get_item_count() const
{
	assert(generator_);
	return generator_->get_item_count();
}

void tlistbox::set_row_active(const unsigned row, const bool active)
{
	assert(generator_);
	generator_->item(row).set_active(active);
}

void tlistbox::set_row_shown(const unsigned row, const bool shown)
{
	assert(generator_);

	twindow *window = get_window();
	assert(window);

	const int selected_row = get_selected_row();

	bool resize_needed;
	{
		twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*window);

		generator_->set_item_shown(row, shown);
		generator_->place(generator_->get_origin()
				, generator_->calculate_best_size());
		resize_needed = !content_resize_request();
	}

	if(resize_needed) {
		window->invalidate_layout();
	} else {
		content_grid_->set_visible_area(content_visible_area());
		set_dirty();
	}

	if(selected_row != get_selected_row() && callback_value_changed_) {
		callback_value_changed_(this);
	}
}

void tlistbox::set_row_shown(const std::vector<bool>& shown)
{
	assert(generator_);
	assert(shown.size() == get_item_count());

	twindow *window = get_window();
	assert(window);

	const int selected_row = get_selected_row();

	bool resize_needed;
	{
		twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*window);

		for(size_t i = 0; i < shown.size(); ++i) {
			generator_->set_item_shown(i, shown[i]);
		}
		generator_->place(generator_->get_origin()
				, generator_->calculate_best_size());
		resize_needed = !content_resize_request();
	}

	if(resize_needed) {
		window->invalidate_layout();
	} else {
		content_grid_->set_visible_area(content_visible_area());
		set_dirty();
	}

	if(selected_row != get_selected_row() && callback_value_changed_) {
		callback_value_changed_(this);
	}
}

const tgrid* tlistbox::get_row_grid(const unsigned row) const
{
	assert(generator_);
	// rename this function and can we return a reference??
	return &generator_->item(row);
}

tgrid* tlistbox::get_row_grid(const unsigned row)
{
	assert(generator_);
	return &generator_->item(row);
}

bool tlistbox::select_row(const unsigned row, const bool select)
{
	assert(generator_);

	generator_->select_item(row, select);

	return true; // FIXME test what result should have been!!!
}

int tlistbox::get_selected_row() const
{
	assert(generator_);

	return generator_->get_selected_item();
}

void tlistbox::list_item_clicked(twidget* caller)
{
	assert(caller);
	assert(generator_);

	/** @todo Hack to capture the keyboard focus. */
	get_window()->keyboard_capture(this);

	for(size_t i = 0; i < generator_->get_item_count(); ++i) {

		if(generator_->item(i).has_widget(caller)) {
			generator_->toggle_item(i);
			if(callback_value_changed_) {
				callback_value_changed_(this);
			}
			return;
		}
	}
	assert(false);
}

bool tlistbox::update_content_size()
{
	if(get_visible() == twidget::INVISIBLE) {
		return true;
	}

	if(get_size() == tpoint(0, 0)) {
		return false;
	}

	if(content_resize_request(true)) {
		content_grid_->set_visible_area(content_visible_area());
		set_dirty();
		return true;
	}

	return false;
}

void tlistbox::place(const tpoint& origin, const tpoint& size)
{
	// Inherited.
	tscrollbar_container::place(origin, size);

	/**
	 * @todo Work-around to set the selected item visible again.
	 *
	 * At the moment the listboxes and dialogs in general are resized a lot as
	 * work-around for sizing. So this function makes the selected item in view
	 * again. It doesn't work great in all cases but the proper fix is to avoid
	 * resizing dialogs a lot. Need more work later on.
	 */
	const int selected_item = generator_->get_selected_item();
	if(selected_item != -1) {
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(selected_item).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		// show_content_rect(rect);
		scrollbar_moved();
	}
}

tpoint tlistbox::adjust_content_size(const tpoint& size)
{
	unsigned items = generator_->get_item_count();
	if (!items) {
		return size;
	}
	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	// by this time, hasn't called place(), cannot use get_size().
	int header_height = header->get_best_size().y;
	int height = generator_->item(0).get_best_size().y;
	if (header_height + height > size.y) {
		return size;
	}
	int list_height = size.y - header_height;
	list_height = list_height / height * height;

	// reduce hight if necessary.
	height = generator_->get_best_size().y;
	if (list_height > height) {
		list_height = height;
	}
	return tpoint(size.x, header_height + list_height);
}

void tlistbox::adjust_offset(int& x_offset, int& y_offset)
{
	unsigned items = generator_->get_item_count();
	if (!items || !y_offset) {
		return;
	}
	int height = generator_->item(0).get_size().y;
	if (y_offset % height) {
		y_offset = y_offset / height * height + height;
		// y_offset = y_offset / height * height;
	}
}

void tlistbox::set_content_grid_origin(const tpoint& origin, const tpoint& content_origin)
{
	// Here need call twidget::set_visible_area in conent_grid_ only.
	content_grid()->twidget::set_origin(content_origin);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();

	header->set_origin(tpoint(content_origin.x, origin.y));
	generator_->set_origin(tpoint(content_origin.x, content_origin.y + size.y));
}

void tlistbox::set_content_grid_visible_area(const SDL_Rect& area)
{
	// Here need call twidget::set_visible_area in conent_gri_ only.
	content_grid()->twidget::set_visible_area(area);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();
	SDL_Rect header_area = intersect_rects(area, header->get_rect());
	header->set_visible_area(header_area);
	
	SDL_Rect list_area = area;
	list_area.y = area.y + size.y;
	list_area.h = area.h - size.y;
	generator_->set_visible_area(list_area);
}

void tlistbox::resize_content(
		  const int width_modification
		, const int height_modification)
{
	DBG_GUI_L << LOG_HEADER << " current size " << content_grid()->get_size()
			<< " width_modification " << width_modification
			<< " height_modification " << height_modification
			<< ".\n";

	if(content_resize_request(width_modification, height_modification)) {

		// Calculate new size.
		tpoint size = content_grid()->get_size();
		size.x += width_modification;
		size.y += height_modification;

		// Set new size.
		content_grid()->set_size(size);

		// Set status.
		need_layout_ = true;
		// If the content grows assume it "overwrites" the old content.
		if(width_modification < 0 || height_modification < 0) {
			set_dirty();
		}

		content_size_ = tpoint(0, 0);

		DBG_GUI_L << LOG_HEADER << " succeeded.\n";
	} else {
		DBG_GUI_L << LOG_HEADER << " failed.\n";
	}
}

void tlistbox::layout_children()
{
	layout_children(false);
}

void tlistbox::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tscrollbar_container::child_populate_dirty_list(caller, call_stack);

/*
	tscrollbar_container::child_populate_dirty_list has called content_grid_->populate_dirty_list,
	it includs generator_->populate_dirty_list.

	assert(generator_);
	std::vector<twidget*> child_call_stack = call_stack;
	generator_->populate_dirty_list(caller, child_call_stack);
*/
}

void tlistbox::handle_key_up_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_up_arrow(modifier, handled);

	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(
				generator_->get_selected_item()).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		// Inherited.
		tscrollbar_container::handle_key_up_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_down_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_down_arrow(modifier, handled);

	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(
				generator_->get_selected_item()).get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		// Inherited.
		tscrollbar_container::handle_key_up_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_left_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_left_arrow(modifier, handled);

	// Inherited.
	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// vertical scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(
				generator_->get_selected_item()).get_rect();

		rect.y = visible.y;
		rect.h = visible.h;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		tscrollbar_container::handle_key_left_arrow(modifier, handled);
	}
}

void tlistbox::handle_key_right_arrow(SDLMod modifier, bool& handled)
{
	assert(generator_);

	generator_->handle_key_right_arrow(modifier, handled);

	// Inherited.
	if(handled) {
		// When scrolling make sure the new items is visible but leave the
		// vertical scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = generator_->item(
				generator_->get_selected_item()).get_rect();

		rect.y = visible.y;
		rect.h = visible.h;

		show_content_rect(rect);

		if(callback_value_changed_) {
			callback_value_changed_(this);
		}
	} else {
		tscrollbar_container::handle_key_left_arrow(modifier, handled);
	}
}

namespace {

/**
 * Swaps an item in a grid for another one.*/
void swap_grid(tgrid* grid,
		tgrid* content_grid, twidget* widget, const std::string& id)
{
	assert(content_grid);
	assert(widget);

	// Make sure the new child has same id.
	widget->set_id(id);

	// Get the container containing the wanted widget.
	tgrid* parent_grid = NULL;
	if(grid) {
		parent_grid = find_widget<tgrid>(grid, id, false, false);
	}
	if(!parent_grid) {
		parent_grid = find_widget<tgrid>(content_grid, id, true, false);
	}
	parent_grid = dynamic_cast<tgrid*>(parent_grid->parent());
	assert(parent_grid);

	// Replace the child.
	widget = parent_grid->swap_child(id, widget, false);
	assert(widget);

	delete widget;
}

} // namespace

void tlistbox::finalize(
		tbuilder_grid_const_ptr header,
		tbuilder_grid_const_ptr footer,
		const std::vector<string_map>& list_data)
{
	// "Inherited."
	tscrollbar_container::finalize_setup();

	assert(generator_);

	if(header) {
		swap_grid(&grid(), content_grid(), header->build(), "_header_grid");
	}

	if(footer) {
		swap_grid(&grid(), content_grid(), footer->build(), "_footer_grid");
	}

	generator_->create_items(
			-1, list_builder_, list_data, callback_list_item_clicked);
	swap_grid(NULL, content_grid(), generator_, "_list_grid");

}

void tlistbox::set_content_size(const tpoint& origin, const tpoint& size)
{
	/** @todo This function needs more testing. */
	assert(content_grid());

	const int best_height = content_grid()->get_best_size().y;
	const tpoint s(size.x, size.y < best_height ? size.y : best_height);

	content_grid()->place(origin, s);
}

void tlistbox::layout_children(const bool force)
{
	assert(content_grid());

	if (need_layout_ || force) {
		place(get_origin(), get_size());
		// set_content_grid_visible_area(content_visible_area_);

		need_layout_ = false;
		set_dirty();
	}
}

void tlistbox::set_list_builder(tbuilder_grid_ptr list_builder)
{ 
	if (!list_builder_) {
		list_builder_ = list_builder; 
		return;
	}

	// generator_->clear();
	unsigned count = get_item_count();

	int height_reduced = 0;
	for (; count; --count) {
		if (generator_->item(0).get_visible() != INVISIBLE) {
			height_reduced += generator_->item(0).get_height();
		}
		generator_->delete_item(0);
	}

	need_layout_ = true;
	set_dirty();

	// in order to decide visiable/invisiable scrollbar, reset content_size_
	content_size_ = tpoint(0, 0);

	list_builder_ = list_builder; 
}

void tlistbox::set_best_size(const tpoint& best_size) 
{ 
	best_size_ = best_size; 
}

void tlistbox::layout_init(const bool full_initialization)
{
	// in order to decide visiable/invisiable scrollbar, reset content_size_
	content_size_ = tpoint(0, 0);

	// Inherited.
	tcontainer_::layout_init(full_initialization);

	if (full_initialization && content_size_ == tpoint(0, 0)) {

		assert(vertical_scrollbar_grid_);
		switch(vertical_scrollbar_mode_) {
			case always_visible :
				vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);
				break;

			case auto_visible :
				vertical_scrollbar_grid_->set_visible(twidget::HIDDEN);
				break;

			default :
				vertical_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}

		assert(horizontal_scrollbar_grid_);
		switch(horizontal_scrollbar_mode_) {
			case always_visible :
				horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);
				break;

			case auto_visible :
				horizontal_scrollbar_grid_->set_visible(twidget::HIDDEN);
				break;

			default :
				horizontal_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}
	}

	assert(content_grid_);
	if (content_size_ == tpoint(0, 0)) {
		content_grid_->layout_init(full_initialization);
	}
}

void tlistbox::request_reduce_height(
		const unsigned maximum_height)
{
	DBG_GUI_L << LOG_HEADER
			<< " requested height " << maximum_height
			<< ".\n";


	VALIDATE(content_size_.x && content_size_.y, "content_size is zero.");
	if (best_size_ != tpoint(0, 0)) {
		// fixed size case, do nothing
		return;
	}
	/*
	 * First ask the content to reduce it's height. This seems to work for now,
	 * but maybe some sizing hints will be required later.
	 */
	/** @todo Evaluate whether sizing hints are required. */
	assert(content_grid_);
/*	const unsigned offset = horizontal_scrollbar_grid_
			&& horizontal_scrollbar_grid_->get_visible() != twidget::INVISIBLE
				?  horizontal_scrollbar_grid_->get_best_size().y
				: 0;
	content_grid_->request_reduce_height(maximum_height - offset);
*/
	// Did we manage to achieve the wanted size?
	tpoint size = get_best_size();
	if (static_cast<unsigned>(size.y) <= maximum_height) {
		DBG_GUI_L << LOG_HEADER
				<< " child honoured request, height " << size.y
				<< ".\n";
		return;
	}

	if (vertical_scrollbar_mode_ == always_invisible) {
		DBG_GUI_L << LOG_HEADER << " request failed due to scrollbar mode.\n";
		return;
	}

	assert(vertical_scrollbar_grid_);
	const bool resized =
		vertical_scrollbar_grid_->get_visible() == twidget::INVISIBLE;

	// Always set the bar visible, is a nop is already visible.
	vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);

	const tpoint scrollbar_size = vertical_scrollbar_grid_->get_best_size();

	// If showing the scrollbar increased the height, hide and abort.
	if (resized && scrollbar_size.y > size.y) {
		vertical_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		DBG_GUI_L << LOG_HEADER
				<< " request failed, showing the scrollbar"
			    << " increased the height to " << scrollbar_size.y
				<< ".\n";
		return;
	}

	if (maximum_height > static_cast<unsigned>(scrollbar_size.y)) {
		size.y = maximum_height;
	} else {
		size.y = scrollbar_size.y;
	}

	// FIXME adjust for the step size of the scrollbar

	set_layout_size(size);
	DBG_GUI_L << LOG_HEADER
			<< " resize resulted in " << size.y
			<< ".\n";

	if (resized) {
		DBG_GUI_L << LOG_HEADER
				<< " resize modified the width, throw notification.\n";

		// posix_print("id: %s, layout_size_: (%i, %i), width of scrollbar: %i\n", id().c_str(), layout_size().x, layout_size().y, scrollbar_size.x);
		throw tlayout_exception_width_modified();
	}
}

void tlistbox::request_reduce_width(const unsigned maximum_width)
{
	DBG_GUI_L << LOG_HEADER
			<< " requested width " << maximum_width
			<< ".\n";

	VALIDATE(content_size_.x && content_size_.y, "content_size is zero.");
	if (best_size_ != tpoint(0, 0)) {
		// fixed size case, do nothing
		return;
	}

	// First ask our content, it might be able to wrap which looks better as
	// a scrollbar.
	assert(content_grid_);
/*
	const unsigned offset = vertical_scrollbar_grid_
			&& vertical_scrollbar_grid_->get_visible() != twidget::INVISIBLE
				?  vertical_scrollbar_grid_->get_best_size().x
				: 0;

	content_grid_->request_reduce_width(maximum_width - offset);
*/
	// Did we manage to achieve the wanted size?
	tpoint size = get_best_size();
	if(static_cast<unsigned>(size.x) <= maximum_width) {
		DBG_GUI_L << LOG_HEADER
				<< " child honoured request, width " << size.x
				<< ".\n";
		return;
	}

	if(horizontal_scrollbar_mode_ == always_invisible) {
		DBG_GUI_L << LOG_HEADER << " request failed due to scrollbar mode.\n";
		return;
	}

	// Always set the bar visible, is a nop when it's already visible.
	assert(horizontal_scrollbar_grid_);
	horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);
	size = get_best_size();

	const tpoint scrollbar_size = horizontal_scrollbar_grid_->get_best_size();

	// If showing the scrollbar increased the width, hide and abort.
	if(horizontal_scrollbar_mode_ == auto_visible_first_run
			&& scrollbar_size.x > size.x) {

		horizontal_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		DBG_GUI_L << LOG_HEADER
				<< " request failed, showing the scrollbar"
			    << " increased the width to " << scrollbar_size.x
				<< ".\n";
		return;
	}

	if(maximum_width > static_cast<unsigned>(scrollbar_size.x)) {
		size.x = maximum_width;
	} else {
		size.x = scrollbar_size.x;
	}

	// FIXME adjust for the step size of the scrollbar

	set_layout_size(size);
	DBG_GUI_L << LOG_HEADER
			<< " resize resulted in " << size.x
			<< ".\n";
}

tpoint tlistbox::calculate_best_size() const
{
	bool from_best_size = (best_size_ != tpoint(0, 0))? true: false;

	if (from_best_size && content_size_ != tpoint(0, 0)) {
		return best_size_;
	}
	
	/***** get vertical scrollbar size *****/
	const tpoint vertical_scrollbar =
			vertical_scrollbar_grid_->get_visible() == twidget::INVISIBLE
			? tpoint(0, 0)
			: vertical_scrollbar_grid_->get_best_size();

	/***** get horizontal scrollbar size *****/
	const tpoint horizontal_scrollbar =
			horizontal_scrollbar_grid_->get_visible() == twidget::INVISIBLE
			? tpoint(0, 0)
			: horizontal_scrollbar_grid_->get_best_size();

	/***** get content size *****/
	assert(content_grid_);
	if (content_size_ == tpoint(0, 0)) {
		content_size_ = content_grid_->get_best_size();
	}

	if (from_best_size) {
		bool visiable_horizontal_bar = false;
		if (best_size_.x < content_size_.x) {
			VALIDATE(horizontal_scrollbar_mode_ != always_invisible, "request failed due to scrollbar mode.");
			horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);
			visiable_horizontal_bar = true;
		} else {
			horizontal_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}
		if ((best_size_.y - (visiable_horizontal_bar? horizontal_scrollbar_grid_->get_best_size().y: 0)) < content_size_.y) {
			VALIDATE(vertical_scrollbar_mode_ != always_invisible, "request failed due to scrollbar mode.");
			vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);
		} else {
			vertical_scrollbar_grid_->set_visible(twidget::INVISIBLE);
		}
	}

	if (from_best_size) {
		return best_size_;
	} else {
		const tpoint result(
			vertical_scrollbar.x +
				std::max(horizontal_scrollbar.x, content_size_.x),
			horizontal_scrollbar.y +
				std::max(vertical_scrollbar.y,  content_size_.y));
		return result;
	}
}

const std::string& tlistbox::get_control_type() const
{
	static const std::string type = "listbox";
	return type;
}
} // namespace gui2

#endif
