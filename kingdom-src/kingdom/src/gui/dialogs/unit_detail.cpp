/* $Id: player_selection.cpp 49598 2011-05-22 17:55:54Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Jörg Hinrichs <joerg.hinrichs@alice-dsl.de>
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

#include "gui/dialogs/unit_detail.hpp"

#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "hero.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/unit_merit.hpp"
#include "menu_events.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_game_load
 *
 * == Load a game ==
 *
 * This shows the dialog to select and load a savegame file.
 *
 * @begin{table}{dialog_widgets}
 *
 * txtFilter & & text & m &
 *         The filter for the listbox items. $
 *
 * savegame_list & & listbox & m &
 *         List of savegames. $
 *
 * -filename & & control & m &
 *         Name of the savegame. $
 *
 * -date & & control & o &
 *         Date the savegame was created. $
 *
 * preview_pane & & widget & m &
 *         Container widget or grid that contains the items for a preview. The
 *         visible status of this container depends on whether or not something
 *         is selected. $
 *
 * -minimap & & minimap & m &
 *         Minimap of the selected savegame. $
 *
 * -imgLeader & & image & m &
 *         The image of the leader in the selected savegame. $
 *
 * -lblScenario & & label & m &
 *         The name of the scenario of the selected savegame. $
 *
 * -lblSummary & & label & m &
 *         Summary of the selected savegame. $
 *
 * @end{table}
 */

REGISTER_DIALOG(unit_detail)

tunit_detail::tunit_detail(events::menu_handler* menu_handler, game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, const unit* interviewee)
	: menu_handler_(menu_handler)
	, gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, interviewee_(interviewee)
{
}

void tunit_detail::refresh_tooltip(twindow& window)
{
	std::stringstream text, activity_str;
	int index;

	const unit& temp = *interviewee_;

	std::stringstream str;
	int loyalty, activity, hero_count = 1;
	// refresh to gui
	tlabel* label = find_widget<tlabel>(&window, "tip_name", false, true);
	str << _("Hero") << ": " << temp.master().name();
	if (!temp.is_artifical()) {
		text << temp.master().loyalty(*teams_[temp.master().side_].leader());
		loyalty = temp.master().loyalty(*teams_[temp.master().side_].leader());
		activity = temp.master().activity_;
		activity_str << (int)temp.master().activity_;
		if (temp.second().valid()) {
			str << "  " << temp.second().name();
			text << ", " << temp.second().loyalty(*teams_[temp.second().side_].leader());
			loyalty += temp.second().loyalty(*teams_[temp.second().side_].leader());
			activity += temp.second().activity_;
			activity_str << ", " << (int)temp.second().activity_;
			hero_count ++;
		}
		if (temp.third().valid()) {
			str << " " << temp.third().name();
			text << ", " << temp.third().loyalty(*teams_[temp.third().side_].leader());
			loyalty += temp.third().loyalty(*teams_[temp.third().side_].leader());
			activity += temp.third().activity_;
			activity_str << ", " << (int)temp.third().activity_;
			hero_count ++;
		}
		label->set_label(str.str());

		str.str("");
		label = find_widget<tlabel>(&window, "tip_loyalty", false, true);
		str << _("Loyalty") << ": " << loyalty / hero_count << "/(" << text.str() << ")";
		label->set_label(str.str());

		str.str("");
		label = find_widget<tlabel>(&window, "tip_activity", false, true);
		str << _("Activity") << ": " << activity / hero_count << "/(" << activity_str.str() << ")";
		label->set_label(str.str());
	} else {
		label->set_label(str.str());
	}

	// ownership
	str.str("");
	str << _("Ownership");
	if (!temp.is_artifical()) {
		artifical* owner_city = units_.city_from_cityno(temp.cityno());
		if (owner_city) {
			str << _("City") << ": " << owner_city->master().name();
		} else {
			str << _("City") << ": --";
		}
		str << " (" << teams_[temp.side() - 1].name() << ")";
	} else if (temp.is_city()) {
		const artifical& city = *const_unit_2_artifical(&temp);
		str << _("Side") << ": " << teams_[city.side() - 1].name();
	} else {
		const artifical& art = *const_unit_2_artifical(&temp);
		artifical* owner_city = units_.city_from_cityno(art.cityno());
		if (owner_city) {
			str << _("City") << ": " << owner_city->master().name();
		} else {
			str << _("City") << ": --";
		}
		str << " (" << teams_[temp.side() - 1].name() << ")";
	}

	label = find_widget<tlabel>(&window, "tip_ownership", false, true);
	label->set_label(str.str());

	// leadership
	label = find_widget<tlabel>(&window, "tip_leadership", false, true);
	label->set_label(lexical_cast<std::string>(temp.leadership_));

	// force
	label = find_widget<tlabel>(&window, "tip_force", false, true);
	label->set_label(lexical_cast<std::string>(temp.force_));

	// intellect
	label = find_widget<tlabel>(&window, "tip_intellect", false, true);
	label->set_label(lexical_cast<std::string>(temp.intellect_));

	// politics
	label = find_widget<tlabel>(&window, "tip_politics", false, true);
	label->set_label(lexical_cast<std::string>(temp.politics_));

	// charm
	label = find_widget<tlabel>(&window, "tip_charm", false, true);
	label->set_label(lexical_cast<std::string>(temp.charm_));
/*
	// hp
	label = find_widget<tlabel>(&window, "tip_hp", false, true);
	label->set_label(lexical_cast<std::string>(temp.max_hitpoints()));

	// xp
	label = find_widget<tlabel>(&window, "tip_xp", false, true);
	label->set_label(lexical_cast<std::string>(temp.max_experience()));

	// movement
	label = find_widget<tlabel>(&window, "tip_movement", false, true);
	label->set_label(lexical_cast<std::string>(temp.total_movement()));

	// arm
	label = find_widget<tlabel>(&window, "tip_arm", false, true);
	str << temp.type_name() << "(Lv" << temp.level() << ")";
	label->set_label(str.str());
*/

	// movement
	if (!temp.is_artifical()) {
		str.str("");
		str << _("Moves") << ": " << temp.movement_left() << "/" << temp.total_movement();
		// no space, reuse movement to display Munition-turns
		str << "    " << dgettext("wesnoth", "Munition turns") << ": " << temp.keep_turns();

		label = find_widget<tlabel>(&window, "tip_movement", false, true);
		label->set_label(str.str());
	}

	// traits
	str.str("");
	str << _("Traits") << ": " << utils::join(temp.trait_names(), ", ");
	label = find_widget<tlabel>(&window, "tip_traits", false, true);
	label->set_label(str.str());

	// abilities
	str.str("");
	std::vector<std::string> abilities_tt;
	abilities_tt = temp.ability_tooltips(true);
	str << _("Abilities") << ": ";
	if (!abilities_tt.empty()) {
		std::vector<t_string> abilities;
		for (std::vector<std::string>::const_iterator a = abilities_tt.begin(); a != abilities_tt.end(); a += 2) {
			abilities.push_back(*a);
		}

		for (std::vector<t_string>::const_iterator a = abilities.begin(); a != abilities.end(); a++) {
			if (a != abilities.begin()) {
				str << ", ";
			}
			str << (*a);
		}
	}
	label = find_widget<tlabel>(&window, "tip_abilities", false, true);
	label->set_label(str.str());

	// feature
	str.str("");
	index = 0;
	str << _("Feature") << ": ";
	for (int i = 0; i < HEROS_MAX_FEATURE; i ++) {
		if (unit_feature_val2(temp, i) == hero_feature_single_result) {
			if (index > 0) {
				str << ", ";
			}
			index ++;
			str << temp.master().feature_str(i);
		}
	}
	label = find_widget<tlabel>(&window, "tip_feature", false, true);
	label->set_label(str.str());

	// adaptability
	str.str("");
	str << _("Adaptability") << ": ";
	str << hero::arms_str(temp.arms()) << "(" << hero::adaptability_str2(ftofxp12(temp.adaptability_[temp.arms()])) << ")";
	label = find_widget<tlabel>(&window, "tip_adaptability", false, true);
	label->set_label(str.str());

	// skill
	str.str("");
	str << _("Skill") << ": ";
	str << hero::skill_str(hero_skill_demolish) << "(" << hero::adaptability_str2(ftofxp12(temp.skill_[hero_skill_demolish])) << ")";
	label = find_widget<tlabel>(&window, "tip_skill", false, true);
	label->set_label(str.str());

	// attack
	str.str("");
	std::vector<attack_type>* attacks_ptr = const_cast<std::vector<attack_type>*>(&temp.attacks());
	for (std::vector<attack_type>::const_iterator at_it = attacks_ptr->begin(); at_it != attacks_ptr->end(); ++at_it) {
		// see generate_report() in generate_report.cpp
		str << at_it->name() << " (" << dgettext("wesnoth", at_it->type().c_str()) << ")\n";

		std::string accuracy = at_it->accuracy_parry_description();
		if(accuracy.empty() == false) {
			accuracy += " ";
		}

		str << "  " << at_it->damage() << "-" << at_it->num_attacks()
			<< " " << accuracy << "- " << dgettext("wesnoth", at_it->range().c_str());

		std::string special = at_it->weapon_specials(true);
		if (!special.empty()) {
			str << "(" << special << ")";
		}
		str << "\n";
	}
	label = find_widget<tlabel>(&window, "tip_attack", false, true);
	label->set_label(str.str());

	// resistance
	int idx_in_resistances;
	std::set<std::string> resistances_table;
	utils::string_map resistances = temp.get_base_resistances();
	bool att_def_diff = false;
	const map_location& loc = temp.get_location();
	idx_in_resistances = 0;
	str.str("");
	for (utils::string_map::iterator resist = resistances.begin(); resist != resistances.end(); ++resist, idx_in_resistances ++) {
		str << dgettext("wesnoth", resist->first.c_str()) << ": ";

		const map_location& loc = temp.get_location();
		if (loc.valid()) {
			// Some units have different resistances when
			// attacking or defending.
			int res_att = 100 - temp.resistance_against(resist->first, true, loc);
			int res_def = 100 - temp.resistance_against(resist->first, false, loc);
			if (res_att == res_def) {
				str << res_def;
			} else {
				str << res_att << "% / " << res_def; // (Att / Def)
				att_def_diff = true;
			}
		} else {
			str << 100 - lexical_cast_default<int>(resist->second.c_str());
		}
		if (idx_in_resistances % 2) {
			str << "%\n";
		} else {
			str << "%  ";
		}
	}
	label = find_widget<tlabel>(&window, "tip_resistance", false, true);
	label->set_label(str.str());
}

void tunit_detail::pre_show(CVideo& /*video*/, twindow& window)
{
	int side_num = interviewee_->side();
	team& current_team = teams_[side_num - 1];
	std::stringstream str;

	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	if (interviewee_->is_artifical()) {
		if (interviewee_->can_reside()) {
			label->set_label(_("City Detail"));
		} else {
			label->set_label(_("Artifical Detail"));
		}
	} else {
		label->set_label(_("Troop Detail"));
	}

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	int cost_exponent = current_team.cost_exponent();

		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = interviewee_->absolute_image() + "~RC(" + interviewee_->team_color() + ">" + team::get_side_color_index(side_num) + ")";
		list_item_item.insert(std::make_pair("icon", list_item));

		// type/name
		str.str("");
		str << interviewee_->type_name() << "(Lv" << interviewee_->level() << ")\n";
		str << interviewee_->name();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("type", list_item));

		// hp
		str.str("");
		str << interviewee_->hitpoints() << "/\n" << interviewee_->max_hitpoints();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("hp", list_item));

		// xp
		str.str("");
		str << interviewee_->experience() << "/\n";
		if (interviewee_->can_advance()) {
			str << interviewee_->max_experience();
		} else {
			str << "-";
		}
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("xp", list_item));

		str.str("");
		str << interviewee_->cost() * cost_exponent / 100 << "/" << interviewee_->cost() * 2 / 3;
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("cost", list_item));

		list->add_row(list_item_item);

	tgrid* grid = list->get_row_grid(list->get_item_count() - 1);
	twidget* widget = grid->find("human", false);
	widget->set_visible(interviewee_->human()? twidget::VISIBLE: twidget::INVISIBLE);

	tbutton* field_troop = find_widget<tbutton>(&window, "field_troop", false, true);
	connect_signal_mouse_left_click(
		*field_troop
		, boost::bind(
			&tunit_detail::field_troop
			, this
			, boost::ref(window)));
	if (interviewee_->is_artifical()) {
		const artifical* city = const_unit_2_artifical(interviewee_);
		if (!(city->field_troops().size())) {
			field_troop->set_active(false);
		}
	} else {
		field_troop->set_visible(twidget::INVISIBLE);
	}

	tbutton* control = find_widget<tbutton>(&window, "field_commoner", false, true);
	connect_signal_mouse_left_click(
		*control
		, boost::bind(
			&tunit_detail::field_commoner
			, this
			, boost::ref(window)));
	if (interviewee_->is_artifical()) {
		const artifical* city = const_unit_2_artifical(interviewee_);
		if (!(city->field_commoners().size())) {
			control->set_active(false);
		}
	} else {
		control->set_visible(twidget::INVISIBLE);
	}

	tbutton* reside_troop = find_widget<tbutton>(&window, "reside_troop", false, true);
	connect_signal_mouse_left_click(
		*reside_troop
		, boost::bind(
			&tunit_detail::reside_troop
			, this
			, boost::ref(window)));
	if (interviewee_->is_artifical()) {
		const artifical* city = const_unit_2_artifical(interviewee_);
		if (!(city->reside_troops().size())) {
			reside_troop->set_active(false);
		}
	} else {
		reside_troop->set_visible(twidget::INVISIBLE);
	}

	control = find_widget<tbutton>(&window, "reside_commoner", false, true);
	connect_signal_mouse_left_click(
		*control
		, boost::bind(
			&tunit_detail::reside_commoner
			, this
			, boost::ref(window)));
	if (interviewee_->is_artifical()) {
		const artifical* city = const_unit_2_artifical(interviewee_);
		if (!(city->reside_commoners().size())) {
			control->set_active(false);
		}
	} else {
		control->set_visible(twidget::INVISIBLE);
	}

	tbutton* hero_list = find_widget<tbutton>(&window, "hero_list", false, true);
	connect_signal_mouse_left_click(
		*hero_list
		, boost::bind(
			&tunit_detail::hero_list
			, this
			, boost::ref(window)));
	if (interviewee_->is_artifical()) {
		const artifical* city = const_unit_2_artifical(interviewee_);
		if (!(city->fresh_heros().size() + city->finish_heros().size() + city->wander_heros().size())) {
			hero_list->set_active(false);
		}
	}

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "merit", false)
		, boost::bind(
			&tunit_detail::merit
			, this
			, boost::ref(window)));

	refresh_tooltip(window);
}

void tunit_detail::merit(twindow& window)
{
	const unit& temp = *interviewee_;
	tunit_merit merit(gui_, temp);
	merit.show(gui_.video());
}

void tunit_detail::post_show(twindow& window)
{
}

void tunit_detail::reside_troop(twindow& window)
{
	menu_handler_->reside_unit_list_in_city(const_unit_2_artifical(interviewee_));
}

void tunit_detail::field_troop(twindow& window)
{
	menu_handler_->field_unit_list_in_city(const_unit_2_artifical(interviewee_));
}

void tunit_detail::reside_commoner(twindow& window)
{
	menu_handler_->reside_unit_list_in_city(const_unit_2_artifical(interviewee_), false, true);
}

void tunit_detail::field_commoner(twindow& window)
{
	menu_handler_->field_unit_list_in_city(const_unit_2_artifical(interviewee_), false, true);
}

void tunit_detail::hero_list(twindow& window)
{
	if (!interviewee_->is_artifical()) {
		std::vector<hero*> v;
		v.push_back(&interviewee_->master());
		if (interviewee_->second().valid()) {
			v.push_back(&interviewee_->second());
		}
		if (interviewee_->third().valid()) {
			v.push_back(&interviewee_->third());
		}
		menu_handler_->hero_list(v);
	} else {
		const artifical* city = const_unit_2_artifical(interviewee_);
		std::vector<hero*> v = city->fresh_heros();
		std::copy(city->finish_heros().begin(), city->finish_heros().end(), std::back_inserter(v));
		std::copy(city->wander_heros().begin(), city->wander_heros().end(), std::back_inserter(v));
		menu_handler_->hero_list(v);
	}
}

} // namespace gui2
