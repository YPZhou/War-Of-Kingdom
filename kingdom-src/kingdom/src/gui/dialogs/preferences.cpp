/* $Id: campaign_selection.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/preferences.hpp"

#include "foreach.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/widgets/image.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/window.hpp"
#include "serialization/string_utils.hpp"
#include "gettext.hpp"
#include <preferences.hpp>
#include "game_preferences.hpp"
#include "preferences_display.hpp"
#include "display.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_selection
 *
 * == Campaign selection ==
 *
 * This shows the dialog which allows the user to choose which campaign to
 * play.
 *
 * @begin{table}{dialog_widgets}
 *
 * campaign_list & & listbox & m &
 *         A listbox that contains all available campaigns. $
 *
 * -icon & & image & o &
 *         The icon for the campaign. $
 *
 * -name & & control & o &
 *         The name of the campaign. $
 *
 * -victory & & image & o &
 *         The icon to show when the user finished the campaign. The engine
 *         determines whether or not the user has finished the campaign and
 *         sets the visible flag for the widget accordingly. $
 *
 * campaign_details & & multi_page & m &
 *         A multi page widget that shows more details for the selected
 *         campaign. $
 *
 * -image & & image & o &
 *         The image for the campaign. $
 *
 * -description & & control & o &
 *         The description of the campaign. $
 *
 * @end{table}
 */

REGISTER_DIALOG(preferences)

struct video_mode_change_exception 
{
	enum TYPE { CHANGE_RESOLUTION, MAKE_FULLSCREEN, MAKE_WINDOWED };

	video_mode_change_exception(TYPE type) : type(type)
	{}

	TYPE type;
};

void show_preferences_dialog(display& disp)
{
	int start_page = tpreferences::GENERAL_PAGE;
	for (;;) {
		try {
			tpreferences dlg(disp, start_page);
			dlg.show(disp.video());
			return;
		} catch (video_mode_change_exception& e) {
			switch(e.type) {
			case video_mode_change_exception::CHANGE_RESOLUTION:
				preferences::show_video_mode_dialog(disp);
				break;
			case video_mode_change_exception::MAKE_FULLSCREEN:
				preferences::set_fullscreen(true);
				break;
			case video_mode_change_exception::MAKE_WINDOWED:
				preferences::set_fullscreen(false);
				break;
			}
			start_page = tpreferences::DISPLAY_PAGE;

		} catch(twml_exception& e) {
			e.show(disp);
			return;
		}
	}
}

tpreferences::tpreferences(display& disp, int start_page)
	: disp_(disp)
	, start_page_(start_page)
	, page_(-1)
	, options_grid_(NULL)
	, zoom_(NULL)
{
}

void tpreferences::page_selected(twindow& window)
{
	page_ = find_widget<tlistbox>(&window, "campaign_list", false).get_selected_row();

	swap_page(window, page_ + MIN_PAGE, true);
}

void tpreferences::pre_show(CVideo& /*video*/, twindow& window)
{
	options_grid_ = find_widget<tgrid>(&window, "options", false, true);

	swap_page(window, start_page_, false);

	// Setup campaign list.
	tlistbox& list =
			find_widget<tlistbox>(&window, "campaign_list", false);

	list.set_callback_value_change(dialog_callback
			<tpreferences
			, &tpreferences::page_selected>);

	
	for (int index = MIN_PAGE; index <= MAX_PAGE; index ++) {
		// Add list item
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		if (index == GENERAL_PAGE) { 
			list_item["label"] = "icons/icon-general.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = sgettext("Prefs section^General");
			list_item_item.insert(std::make_pair("name", list_item));

		} else if (index == DISPLAY_PAGE) {
			list_item["label"] = "icons/icon-display.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = sgettext("Prefs section^Display");
			list_item_item.insert(std::make_pair("name", list_item));

		} else if (index == MUSIC_PAGE) {
			list_item["label"] = "icons/icon-music.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = sgettext("Prefs section^Sound");
			list_item_item.insert(std::make_pair("name", list_item));

		} else if (index == MULTIPLAYER_PAGE) {
			list_item["label"] = "icons/icon-multiplayer.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = sgettext("Prefs section^Multiplayer");
			list_item_item.insert(std::make_pair("name", list_item));

		} else if (index == ADVANCED_PAGE) {
			list_item["label"] = "icons/icon-advanced.png";
			list_item_item.insert(std::make_pair("icon", list_item));
			list_item["label"] = sgettext("Advanced section^Advanced");
			list_item_item.insert(std::make_pair("name", list_item));
		} 

		list.add_row(list_item_item);

		tgrid* grid = list.get_row_grid(list.get_item_count() - 1);
	}
}

void tpreferences::post_show(twindow& window)
{
}

//
// general
//
void tpreferences::scroll_changed(tslider* widget, int value)
{
	preferences::set_scroll_speed(value);
}

void tpreferences::turbo_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_turbo(toggle->get_value());
}

void tpreferences::turbo_changed(tslider* widget, int value)
{
	// 0.25 0.5 1 2 4 8 16
	std::vector< double > turbo_items;
	turbo_items.push_back(0.25);
	turbo_items.push_back(0.5);
	turbo_items.push_back(0.75);
	turbo_items.push_back(1);
	turbo_items.push_back(1.5);
	turbo_items.push_back(2);
	turbo_items.push_back(4);
	turbo_items.push_back(8);
	turbo_items.push_back(16);

	preferences::set_turbo_speed(turbo_items[value]);
}

void tpreferences::eng_file_name_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_eng_file_name(!toggle->get_value());
}

void tpreferences::turn_dialog_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_turn_dialog(toggle->get_value());
}

void tpreferences::save_replays_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_save_replays(toggle->get_value());
}

void tpreferences::delete_saves_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_delete_saves(toggle->get_value());
}

void tpreferences::autosavemax_changed(tslider* widget, int value)
{
	preferences::set_autosavemax(value);
}

//
// display
//
void tpreferences::fullscreen_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	throw video_mode_change_exception(toggle->get_value()
			? video_mode_change_exception::MAKE_FULLSCREEN
			: video_mode_change_exception::MAKE_WINDOWED);
}

void tpreferences::flip_time_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_flip_time(toggle->get_value());
}

void tpreferences::show_floating_labels_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_show_floating_labels(toggle->get_value());
}

void tpreferences::show_team_colors_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_show_side_colors(toggle->get_value());
}

void tpreferences::default_move_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_default_move(toggle->get_value());
}

void tpreferences::idle_anim_changed(tslider* widget, int value)
{
	preferences::set_idle_anim_rate(value);
}

void tpreferences::video_mode_button(twindow& window)
{
	throw video_mode_change_exception(video_mode_change_exception::CHANGE_RESOLUTION);
}

//
// music
//
void tpreferences::sound_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_sound(toggle->get_value());
}

void tpreferences::sound_changed(tslider* widget, int value)
{
	preferences::set_sound_volume(value);
}

void tpreferences::music_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_music(toggle->get_value());
}

void tpreferences::music_changed(tslider* widget, int value)
{
	preferences::set_music_volume(value);
}

void tpreferences::turn_bell_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_turn_bell(toggle->get_value());
}

void tpreferences::turn_bell_changed(tslider* widget, int value)
{
	preferences::set_bell_volume(value);
}

void tpreferences::UI_sound_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_UI_sound(toggle->get_value());
}

void tpreferences::UI_sound_changed(tslider* widget, int value)
{
	preferences::set_UI_volume(value);
}

//
// multiplayer
//
void tpreferences::chat_timestamp_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_chat_timestamping(toggle->get_value());
}

//
// advanced
//
void tpreferences::scroll_when_mouse_outside_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set("scroll_when_mouse_outside", toggle->get_value());
}

void tpreferences::whiteboard_on_start_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_enable_whiteboard_mode_on_start(toggle->get_value());
}

void tpreferences::interrupt_when_ally_sighted_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_interrupt_when_ally_sighted(toggle->get_value());
}

void tpreferences::scroll_to_action_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_scroll_to_action(toggle->get_value());
}

void tpreferences::show_color_cursors_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_color_cursors(toggle->get_value());
}

void tpreferences::zoom_button(twindow& window)
{
	std::vector<std::string> zooms;
	zooms.push_back("72 x 72");
	zooms.push_back("64 x 64");
	zooms.push_back("56 x 56");
	zooms.push_back("48 x 48");

	int zoom_index;
	if (display::default_zoom_ == display::ZOOM_72) {
		zoom_index = 0;
	} else if (display::default_zoom_ == display::ZOOM_64) {
		zoom_index = 1;
	} else if (display::default_zoom_ == display::ZOOM_56) {
		zoom_index = 2;
	} else if (display::default_zoom_ == display::ZOOM_48) {
		zoom_index = 3;
	} else {
		VALIDATE(false, _("Invalid display::default_zoom_"));
	}

	gui2::tcombo_box dlg(zooms, zoom_index);
	dlg.show(disp_.video());

	int new_index = dlg.selected_index();
	if (new_index != zoom_index) {
		if (new_index == 0) {
			display::default_zoom_ = display::ZOOM_72;
		} else if (new_index == 1) {
			display::default_zoom_ = display::ZOOM_64;
		} else if (new_index == 2) {
			display::default_zoom_ = display::ZOOM_56;
		} else {
			display::default_zoom_ = display::ZOOM_48;
		}
		preferences::_set_zoom(display::default_zoom_);

		std::stringstream str;
		str << _("Grid Size") << ": " << display::default_zoom_ << " x " << display::default_zoom_;
		zoom_->set_label(str.str());

		image::set_zoom(display::default_zoom_);
		unit_map::set_zoom();
	}
}

void tpreferences::show_grid_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	preferences::set_grid(toggle->get_value());
}

int turbo_slider_get_value()
{
	// 0.25 0.5 1 2 4 8 16
	std::vector< double > turbo_items;
	turbo_items.push_back(0.25);
	turbo_items.push_back(0.5);
	turbo_items.push_back(0.75);
	turbo_items.push_back(1);
	turbo_items.push_back(1.5);
	turbo_items.push_back(2);
	turbo_items.push_back(4);
	turbo_items.push_back(8);
	turbo_items.push_back(16);

	std::vector<double>::iterator itor = std::find(turbo_items.begin(), turbo_items.end(), preferences::turbo_speed());
	if (itor != turbo_items.end()) {
		return itor - turbo_items.begin();
	} else {
		return 1;
	}
}

void tpreferences::swap_page(twindow& window, int page, bool swap)
{
	if (page < MIN_PAGE || page > MAX_PAGE) {
		return;
	}
	int index = page - MIN_PAGE;

	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}
	
	window.alternate_uh(options_grid_, index);

	if (swap) {
		window.alternate_bh(options_grid_, index);
	}

	if (index == GENERAL_PAGE) {
		tslider* slider = dynamic_cast<tslider*>(options_grid_->find("scroll_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::scroll_changed, this, _1, _2));
		slider->set_value(preferences::scroll_speed());

		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("turbo_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::turbo_toggled, this, _1));
		toggle->set_value(preferences::turbo());

		slider = dynamic_cast<tslider*>(options_grid_->find("turbo_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::turbo_changed, this, _1, _2));
		slider->set_value(turbo_slider_get_value());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("eng_file_name_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::eng_file_name_toggled, this, _1));
		toggle->set_value(!preferences::eng_file_name());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("turn_dialog_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::turn_dialog_toggled, this, _1));
		toggle->set_value(preferences::turn_dialog());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("save_replays_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::save_replays_toggled, this, _1));
		toggle->set_value(preferences::save_replays());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("delete_saves_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::delete_saves_toggled, this, _1));
		toggle->set_value(preferences::delete_saves());

		slider = dynamic_cast<tslider*>(options_grid_->find("autosavemax_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::autosavemax_changed, this, _1, _2));
		slider->set_value(preferences::autosavemax());
	
	} else if (index == DISPLAY_PAGE) {
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("flip_time_button", false));
		toggle->set_value(preferences::flip_time());
		toggle->set_callback_state_change(boost::bind(&tpreferences::flip_time_toggled, this, _1));

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("show_floating_labels_button", false));
		toggle->set_value(preferences::show_floating_labels());
		toggle->set_callback_state_change(boost::bind(&tpreferences::show_floating_labels_toggled, this, _1));
		
		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("show_team_colors_button", false));
		toggle->set_value(preferences::show_side_colors());
		toggle->set_callback_state_change(boost::bind(&tpreferences::show_team_colors_toggled, this, _1));
		
		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("default_move_button", false));
		toggle->set_value(preferences::default_move());
		toggle->set_callback_state_change(boost::bind(&tpreferences::default_move_toggled, this, _1));

		tslider* slider = dynamic_cast<tslider*>(options_grid_->find("idle_anim_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::idle_anim_changed, this, _1, _2));
		slider->set_value(preferences::idle_anim_rate());
		
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
		options_grid_->find("fullscreen_button", false)->set_visible(twidget::INVISIBLE);
		find_widget<tbutton>(&window, "video_mode_button", false).set_visible(twidget::INVISIBLE);
#else 
		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("fullscreen_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::fullscreen_toggled, this, _1));
		toggle->set_value(preferences::fullscreen());

		connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "video_mode_button", false)
			, boost::bind(
				&tpreferences::video_mode_button
				, this
				, boost::ref(window)));
#endif
	} else if (index == MUSIC_PAGE) {
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("sound_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::sound_toggled, this, _1));
		toggle->set_value(preferences::sound_on());

		tslider* slider = dynamic_cast<tslider*>(options_grid_->find("sound_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::sound_changed, this, _1, _2));
		slider->set_value(preferences::sound_volume());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("music_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::music_toggled, this, _1));
		toggle->set_value(preferences::music_on());

		slider = dynamic_cast<tslider*>(options_grid_->find("music_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::music_changed, this, _1, _2));
		slider->set_value(preferences::music_volume());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("turn_bell_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::turn_bell_toggled, this, _1));
		toggle->set_value(preferences::turn_bell());

		slider = dynamic_cast<tslider*>(options_grid_->find("bell_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::turn_bell_changed, this, _1, _2));
		slider->set_value(preferences::bell_volume());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("UI_sound_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::UI_sound_toggled, this, _1));
		toggle->set_value(preferences::UI_sound_on());
		toggle->set_active(false);

		slider = dynamic_cast<tslider*>(options_grid_->find("UI_sound_slider", false));
		slider->set_callback_value_change(boost::bind(&tpreferences::UI_sound_changed, this, _1, _2));
		slider->set_value(preferences::UI_volume());
		slider->set_active(false);

	} else if (index == MULTIPLAYER_PAGE) {
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("chat_timestamp_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::chat_timestamp_toggled, this, _1));
		toggle->set_value(preferences::chat_timestamping());

	} else if (index == ADVANCED_PAGE) {
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("scroll_when_mouse_outside", true));
		toggle->set_callback_state_change(boost::bind(&tpreferences::scroll_when_mouse_outside_toggled, this, _1));
		toggle->set_value(preferences::get("scroll_when_mouse_outside", true));

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("whiteboard_on_start_button", false));
#if 0
		toggle->set_callback_state_change(boost::bind(&tpreferences::whiteboard_on_start_toggled, this, _1));
		toggle->set_value(preferences::enable_whiteboard_mode_on_start());
#else
		toggle->set_visible(twidget::INVISIBLE);
#endif

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("interrupt_when_ally_sighted_button", false));
		toggle->set_callback_state_change(boost::bind(&tpreferences::interrupt_when_ally_sighted_toggled, this, _1));
		toggle->set_value(preferences::interrupt_when_ally_sighted());

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("scroll_to_action_button", false));
		toggle->set_value(preferences::scroll_to_action());
		toggle->set_callback_state_change(boost::bind(&tpreferences::scroll_to_action_toggled, this, _1));

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("show_color_cursors_button", false));
// #if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
#if 1
		toggle->set_visible(twidget::INVISIBLE);
#else
		toggle->set_value(preferences::use_color_cursors());
		toggle->set_callback_state_change(boost::bind(&tpreferences::show_color_cursors_toggled, this, _1));
#endif

		zoom_ = dynamic_cast<tbutton*>(options_grid_->find("zoom_button", false));
		if (disp_.in_game()) {
			zoom_->set_visible(twidget::INVISIBLE);
		} else {
			connect_signal_mouse_left_click(
				*zoom_
				, boost::bind(
					&tpreferences::zoom_button
					, this
					, boost::ref(window)));
			std::stringstream str;
			str << _("Grid Size") << ": " << display::default_zoom_ << " x " << display::default_zoom_;
			zoom_->set_label(str.str());
		}

		toggle = dynamic_cast<ttoggle_button*>(options_grid_->find("show_grid_button", false));
		toggle->set_value(preferences::grid());
		toggle->set_callback_state_change(boost::bind(&tpreferences::show_grid_toggled, this, _1));
	}
	
}

} // namespace gui2

