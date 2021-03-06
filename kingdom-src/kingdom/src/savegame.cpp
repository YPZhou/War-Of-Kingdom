/* $Id: savegame.cpp 40674 2010-01-11 22:11:12Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by J?rg Hinrichs, refactored from various
   places formerly created by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "savegame.hpp"

#include "dialogs.hpp" //FIXME: get rid of this as soon as the two remaining dialogs are moved to gui2
#include "foreach.hpp"
#include "game_display.hpp"
#include "game_end_exceptions.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/game_load.hpp"
#include "gui/dialogs/game_save.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "replay.hpp"
#include "resources.hpp"
#include "serialization/binary_or_text.hpp"
#include "serialization/parser.hpp"
#include "statistics.hpp"
//#include "unit.hpp"
#include "unit_id.hpp"
#include "version.hpp"

#include "posix.h"

static lg::log_domain log_engine("engine");
#define LOG_SAVE LOG_STREAM(info, log_engine)
#define ERR_SAVE LOG_STREAM(err, log_engine)

#ifdef _WIN32
	#ifdef INADDR_ANY
		#undef INADDR_ANY
	#endif
	#ifdef INADDR_BROADCAST
		#undef INADDR_BROADCAST
	#endif
	#ifdef INADDR_NONE
		#undef INADDR_NONE
	#endif

	#include <windows.h>

	void replace_underbar2space(std::string &name) {
		LOG_SAVE << "conv(A2U)-from:[" << name << "]" << std::endl;
		conv_ansi_utf8(name, true);
		LOG_SAVE << "conv(A2U)-to:[" << name << "]" << std::endl;
		LOG_SAVE << "replace_underbar2space-from:[" << name << "]" << std::endl;
		std::replace(name.begin(), name.end(), '_', ' ');
		LOG_SAVE << "replace_underbar2space-to:[" << name << "]" << std::endl;
	}

	void replace_space2underbar(std::string &name) {
		LOG_SAVE << "conv(U2A)-from:[" << name << "]" << std::endl;
		conv_ansi_utf8(name, false);
		LOG_SAVE << "conv(U2A)-to:[" << name << "]" << std::endl;
		LOG_SAVE << "replace_underbar2space-from:[" << name << "]" << std::endl;
		std::replace(name.begin(), name.end(), ' ', '_');
		LOG_SAVE << "replace_underbar2space-to:[" << name << "]" << std::endl;
	}
#else /* ! _WIN32 */
	void replace_underbar2space(std::string &name) {
		std::replace(name.begin(),name.end(),'_',' ');
	}
	void replace_space2underbar(std::string &name) {
		std::replace(name.begin(),name.end(),' ','_');
	}
#endif /* _WIN32 */

namespace savegame {

config dummy_snapshot;

const std::string save_info::format_time_local() const{
	char time_buf[256] = {0};
	tm* tm_l = localtime(&time_modified);
	if (tm_l) {
		const size_t res = strftime(time_buf,sizeof(time_buf),_("%a %b %d %H:%M %Y"),tm_l);
		if(res == 0) {
			time_buf[0] = 0;
		}
	} else {
		LOG_SAVE << "localtime() returned null for time " << time_modified << ", save " << name;
	}

	return time_buf;
}

const std::string save_info::format_time_summary() const
{
	time_t t = time_modified;
	time_t curtime = time(NULL);
	const struct tm* timeptr = localtime(&curtime);
	if(timeptr == NULL) {
		return "";
	}

	const struct tm current_time = *timeptr;

	timeptr = localtime(&t);
	if(timeptr == NULL) {
		return "";
	}

	const struct tm save_time = *timeptr;

	const char* format_string = _("%b %d %y");

	if(current_time.tm_year == save_time.tm_year) {
		const int days_apart = current_time.tm_yday - save_time.tm_yday;
		if(days_apart == 0) {
			// save is from today
			format_string = _("%H:%M");
		} else if(days_apart > 0 && days_apart <= current_time.tm_wday) {
			// save is from this week
			format_string = _("%A, %H:%M");
		} else {
			// save is from current year
			format_string = _("%b %d");
		}
	} else {
		// save is from a different year
		format_string = _("%b %d %y");
	}

	char buf[40];
	const size_t res = strftime(buf,sizeof(buf),format_string,&save_time);
	if(res == 0) {
		buf[0] = 0;
	}

	return buf;
}

/**
 * A structure for comparing to save_info objects based on their modified time.
 * If the times are equal, will order based on the name.
 */
struct save_info_less_time {
	bool operator()(const save_info& a, const save_info& b) const {
		if (a.time_modified > b.time_modified) {
		        return true;
		} else if (a.time_modified < b.time_modified) {
			return false;
		// Special funky case; for files created in the same second,
		// a replay file sorts less than a non-replay file.  Prevents
		// a timing-dependent bug where it may look like, at the end
		// of a scenario, the replay and the autosave for the next
		// scenario are displayed in the wrong order.
		} else if (a.name.find(_(" replay"))==std::string::npos && b.name.find(_(" replay"))!=std::string::npos) {
			return true;
		} else if (a.name.find(_(" replay"))!=std::string::npos && b.name.find(_(" replay"))==std::string::npos) {
			return false;
		} else {
			return  a.name > b.name;
		}
	}
};

// @name: short file name. uft8 codeset.
// @start_heros: NULL, don't read start hero data
// @replay_data: NULL, don't read replay data
// @heros: NULL, don't read hero data
void manager::read_save_file(const std::string& name, config* summary_cfg, config* cfg, hero_map* start_heros, command_pool* replay_data, hero_map* heros, std::string* error_log)
{
	std::string modified_name = name;
	// replace_space2underbar(modified_name);

	try {
		std::string str = get_saves_dir() + "/" + modified_name;
		posix_file_t fp = INVALID_FILE;
#ifdef _WIN32
		// utf8 ---> utf16
		int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
		WCHAR *wc = new WCHAR[wlen];
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wc, wlen);
			
		fp = CreateFileW(wc, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL);
		// posix_fopen(str.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
		delete [] wc;
#else
		posix_fopen(str.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
#endif
		if (fp == INVALID_FILE) {
			throw game::load_game_failed();
		}
		uint32_t summary_size, scenario_size, start_scenario_size, start_hero_size, player_data_size, replay_size, side_size, bytertd, fsizelow, fsizehigh, should_least_size;
		uint8_t *data;

		bool has_side_data = true;
		//
		// read size
		//
		posix_fsize(fp, fsizelow, fsizehigh);
		posix_fseek(fp, 0, 0);

		// check misc size 
		should_least_size = 0;
		should_least_size += sizeof(summary_size);
		should_least_size += sizeof(scenario_size);
		should_least_size += sizeof(side_size);
		should_least_size += sizeof(start_scenario_size);
		should_least_size += sizeof(start_hero_size);
		should_least_size += sizeof(player_data_size);
		should_least_size += sizeof(replay_size);
		if (fsizelow < should_least_size) {
			posix_fclose(fp);
			throw game::load_game_failed();
		}
		posix_fread(fp, &summary_size, sizeof(summary_size), bytertd);
		posix_fread(fp, &scenario_size, sizeof(scenario_size), bytertd);
		if (has_side_data) {
			posix_fread(fp, &side_size, sizeof(side_size), bytertd);
		} else {
			side_size = 0;
		}
		posix_fread(fp, &start_scenario_size, sizeof(start_scenario_size), bytertd);
		posix_fread(fp, &start_hero_size, sizeof(start_hero_size), bytertd);
		posix_fread(fp, &player_data_size, sizeof(player_data_size), bytertd);
		posix_fread(fp, &replay_size, sizeof(replay_size), bytertd);
		// check should least size 
		should_least_size += summary_size + scenario_size + side_size + start_scenario_size + start_hero_size + player_data_size + replay_size;
		if (fsizelow < should_least_size) {
			posix_fclose(fp);
			throw game::load_game_failed();
		}

		// reset should_least_size
		if (has_side_data) {
			should_least_size = sizeof(summary_size) + sizeof(scenario_size) + sizeof(side_size) + sizeof(start_scenario_size) + sizeof(start_hero_size) + sizeof(player_data_size) + sizeof(replay_size);
		} else {
			should_least_size = sizeof(summary_size) + sizeof(scenario_size) + sizeof(start_scenario_size) + sizeof(start_hero_size) + sizeof(player_data_size) + sizeof(replay_size);
		}
		//
		// read data
		//
		size_t max_size = std::max<size_t>(summary_size + 1, scenario_size + 1);
		max_size = std::max<size_t>(max_size, start_scenario_size + 1);
		max_size = std::max<size_t>(max_size, start_hero_size);
		max_size = std::max<size_t>(max_size, player_data_size);
		max_size = std::max<size_t>(max_size, replay_size);
		data = (uint8_t*)malloc(max_size);
		// read summary data
		should_least_size += summary_size;
		if (summary_cfg) {
			// summary_cfg->clear();
			posix_fread(fp, data, summary_size, bytertd);
			data[summary_size] = '\0';
			read(*summary_cfg, std::string((char*)data));
		} else {
			posix_fseek(fp, should_least_size, 0);
		}
		// read scenario data
		should_least_size += scenario_size + side_size + start_scenario_size;
		if (cfg) {
			cfg->clear();
			posix_fread(fp, data, scenario_size, bytertd);
			data[scenario_size] = '\0';
			read(*cfg, std::string((char*)data));

			// read side data
			if (side_size) {
				posix_fread(fp, unit::savegame_cache_, side_size, bytertd);
			} else {
				memset(unit::savegame_cache_, 0, sizeof(unit_segment2));
			}

			// read start scenario data
			posix_fread(fp, data, start_scenario_size, bytertd);
			data[start_scenario_size] = '\0';
			config& replay_start_cfg = cfg->add_child("replay_start");
			read(replay_start_cfg, std::string((char*)data));
		} else {
			posix_fseek(fp, should_least_size, 0);
		}
		// read start hero data
		if (start_heros) {
			start_heros->map_from_file_fp(fp, should_least_size, start_hero_size);
		}
		should_least_size += start_hero_size;
		posix_fseek(fp, should_least_size, 0);

		// read player data
		posix_fread(fp, data, player_data_size, bytertd);
		should_least_size += player_data_size;
		int rpg_number;
		size_t tmp;
		std::string rpg_name, rpg_surname;
		memcpy(&rpg_number, data, 4);
		tmp = 4;
		memcpy(&bytertd, data + tmp, 4);
		tmp += 4;
		rpg_name.assign((const char*)data + tmp, bytertd);
		tmp += bytertd;
		memcpy(&bytertd, data + tmp, 4);
		tmp += 4;
		rpg_surname.assign((const char*)data + tmp, bytertd);
		tmp += bytertd;

		// read replay data
		should_least_size += replay_size;
		if (replay_data) {
			if (replay_size) {
				posix_fread(fp, data, replay_size, bytertd);
				replay_data->read(data);
			} else {
				replay_data->clear();
			}
		}
		free(data);
		// read hero data
		if (heros) {
			// need read hero data
			heros->map_from_file_fp(fp, should_least_size);
		}
		posix_fclose(fp);

		if (start_heros) {
			(*start_heros)[rpg_number].set_name(rpg_name);
			(*start_heros)[rpg_number].set_surname(rpg_surname);
		}
		if (heros) {
			(*heros)[rpg_number].set_name(rpg_name);
			(*heros)[rpg_number].set_surname(rpg_surname);
		}
		// detect_format_and_read(cfg, *file_stream, error_log);
	} catch (config::error &err) {
		LOG_SAVE << err.message;
		throw game::load_game_failed();
	}
}

void manager::load_summary(const std::string& name, config& cfg_summary, std::string* error_log)
{
	read_save_file(name, &cfg_summary, NULL, NULL, NULL, NULL, error_log);
}

bool manager::save_game_exists(const std::string& name)
{
	// std::string fname = name;
	// replace_space2underbar(fname);
	std::string fullname = get_saves_dir() + "/" + name + ".sav";
#ifdef _WIN32
	conv_ansi_utf8(fullname, false);
#endif

	return file_exists(fullname);
}

std::vector<save_info> manager::get_saves_list(const std::string *dir, const std::string* filter)
{
	// Don't use a reference, it seems to break on arklinux with GCC-4.3.
	std::string saves_dir = (dir) ? *dir : get_saves_dir();
#ifdef _WIN32
	conv_ansi_utf8(saves_dir, false);
#endif

	std::vector<std::string> saves;
	get_files_in_dir(saves_dir,&saves);

	std::vector<save_info> res;
	for(std::vector<std::string>::iterator i = saves.begin(); i != saves.end(); ++i) {
		if(filter && std::search(i->begin(), i->end(), filter->begin(), filter->end()) == i->end()) {
			continue;
		}

		const time_t modified = file_create_time(saves_dir + "/" + *i);

		// replace_underbar2space(*i);
#ifdef _WIN32
		res.push_back(save_info(conv_ansi_utf8_2(*i, true), modified));
#else
		res.push_back(save_info(*i, modified));
#endif
	}

	std::sort(res.begin(),res.end(),save_info_less_time());

	return res;
}

void manager::clean_saves(const std::string &label)
{
	std::vector<save_info> games = get_saves_list();
	std::string prefix = label + "-" + _("Auto-Save");
	std::cerr << "Cleaning saves with prefix '" << prefix << "'\n";
	for (std::vector<save_info>::iterator i = games.begin(); i != games.end(); ++i) {
		if (i->name.compare(0, prefix.length(), prefix) == 0) {
			std::cerr << "Deleting savegame '" << i->name << "'\n";
			delete_game(i->name);
		}
	}
}

void manager::remove_old_auto_saves(const int autosavemax, const int infinite_auto_saves)
{
	std::string auto_save = _("Auto-Save");
	int countdown = autosavemax;
	if (countdown == infinite_auto_saves)
		return;

#ifdef _WIN32
	conv_ansi_utf8(auto_save, false);
#endif
	std::vector<save_info> games = get_saves_list(NULL, &auto_save);
	for (std::vector<save_info>::iterator i = games.begin(); i != games.end(); ++i) {
		if (countdown-- <= 0) {
			LOG_SAVE << "Deleting savegame '" << i->name << "'\n";
			delete_game(i->name);
		}
	}
}

void manager::delete_game(const std::string& name)
{
	// std::string modified_name = name;
	// replace_space2underbar(modified_name);

	std::string fullname = get_saves_dir() + "/" + name;
#ifdef _WIN32
	conv_ansi_utf8(fullname, false);
#endif
	remove(fullname.c_str());

	// remove((get_saves_dir() + "/" + modified_name).c_str());
}

loadgame::loadgame(display& gui, const config& game_config, game_state& gamestate)
	: game_config_(game_config)
	, gui_(gui)
	, gamestate_(gamestate)
	, filename_()
	, load_config_()
	, show_replay_(false)
	, cancel_orders_(false)
{}

void loadgame::show_dialog(bool show_replay, bool cancel_orders)
{
	//FIXME: Integrate the load_game dialog into this class
	//something to watch for the curious, but not yet ready to go
	gui2::tgame_load load_dialog(game_config_);
	load_dialog.show(gui_.video());

	if (load_dialog.get_retval() == gui2::twindow::OK){
		filename_ = load_dialog.filename();
		show_replay_ = cancel_orders_ = false;
		// show_replay_ = load_dialog.show_replay();
		// cancel_orders_ = load_dialog.cancel_orders();
	}
}

void loadgame::load_game()
{
	show_dialog(false, false);

	if(filename_ != "")
		throw game::load_game_exception(filename_, show_replay_, cancel_orders_);
}

void loadgame::load_game(std::string& filename, bool show_replay, bool cancel_orders, hero_map& heros, hero_map* heros_start)
{
	filename_ = filename;

	if (filename_.empty()){
		show_dialog(show_replay, cancel_orders);
	}
	else{
		show_replay_ = show_replay;
		cancel_orders_ = cancel_orders;
	}

	if (filename_.empty())
		throw load_game_cancelled_exception();

	std::string error_log;
	manager::read_save_file(filename_, NULL, &load_config_, heros_start, &replay_data_, &heros, &error_log);

	if(!error_log.empty()) {
        try {
		    gui2::show_error_message(gui_.video(),
				    _("Warning: The file you have tried to load is corrupt. Loading anyway.\n") +
				    error_log);
        } catch (utils::invalid_utf8_exception&) {
		    gui2::show_error_message(gui_.video(),
				    _("Warning: The file you have tried to load is corrupt. Loading anyway.\n") +
                    std::string("(UTF-8 ERROR)"));
        }
	}

	gamestate_.classification().difficulty = load_config_["difficulty"].str();
	gamestate_.classification().campaign_define = load_config_["campaign_define"].str();

	gamestate_.classification().campaign = load_config_["campaign"].str();
	gamestate_.classification().campaign_type = load_config_["campaign_type"].str();

	gamestate_.classification().campaign_xtra_defines = utils::split(load_config_["campaign_extra_defines"]);
	gamestate_.classification().version = load_config_["version"].str();

	check_version_compatibility();

}

void loadgame::check_version_compatibility()
{
	if (gamestate_.classification().version == game_config::version) {
		return;
	}

	const version_info save_version = gamestate_.classification().version;
	const version_info &wesnoth_version = game_config::wesnoth_version;
	// Even minor version numbers indicate stable releases which are
	// compatible with each other.

	// Do not load if too old. If either the savegame or the current
	// game has the version 'test', load. This 'test' version is never
	// supposed to occur, except when Soliton is testing MP servers.
	{
		gui2::show_message(gui_.video(), "", _("This save is from a version too old to be loaded."));
		throw load_game_cancelled_exception();
	}

	const int res = gui2::show_message(gui_.video(), "", _("This save is from a different version of the game. Do you want to try to load it?"),
		gui2::tmessage::yes_no_buttons);

	if (res == gui2::twindow::CANCEL) {
		throw load_game_cancelled_exception();
	}
}

void loadgame::set_gamestate()
{
	gamestate_ = game_state(load_config_, replay_data_, show_replay_);

	// Get the status of the random in the snapshot.
	// For a replay we need to restore the start only, the replaying gets at
	// proper location.
	// For normal loading also restore the call count.
	int seed = load_config_["random_seed"].to_int(42);
	unsigned calls = show_replay_ ? 0 : gamestate_.snapshot["random_calls"].to_int();
	gamestate_.rng().seed_random(seed, calls);
}

const std::pair<int, unsigned> loadgame::recorder_generator() const
{
	const int seed = lexical_cast_default<int>(load_config_["recorder_random_seed"], 42);
	const unsigned calls = show_replay_? 0 : lexical_cast_default<unsigned>(load_config_["recorder_random_calls"]);
	return std::make_pair<int, unsigned>(seed, calls);
}

void loadgame::load_multiplayer_game()
{
	show_dialog(false, false);

	if (filename_.empty())
		throw load_game_cancelled_exception();

	std::string error_log;
	{
		cursor::setter cur(cursor::WAIT);
		log_scope("load_game");

		manager::read_save_file(filename_, NULL, &load_config_, NULL, &replay_data_, NULL, &error_log);
		copy_era(load_config_);

		gamestate_ = game_state(load_config_, replay_data_);
	}

	if(!error_log.empty()) {
		gui2::show_error_message(gui_.video(),
				_("The file you have tried to load is corrupt: '") +
				error_log);
		throw load_game_cancelled_exception();
	}

	if(gamestate_.classification().campaign_type != "multiplayer") {
		gui2::show_message(gui_.video(), "", _("This is not a multiplayer save"));
		throw load_game_cancelled_exception();
	}

	check_version_compatibility();
}

void loadgame::fill_mplevel_config(config& level){
	gamestate_.mp_settings().saved_game = true;

	// If we have a start of scenario MP campaign scenario the snapshot
	// is empty the starting position contains the wanted info.
	const config& start_data = !gamestate_.snapshot.empty() ? gamestate_.snapshot : gamestate_.starting_pos;
	level["map_data"] = start_data["map_data"];
	level["id"] = start_data["id"];
	level["name"] = start_data["name"];
	level["completion"] = start_data["completion"];
	level["next_underlying_unit_id"] = start_data["next_underlying_unit_id"];
	// Probably not needed.
	level["turn"] = start_data["turn_at"];
	level["turn_at"] = start_data["turn_at"];

	level.add_child("multiplayer", gamestate_.mp_settings().to_config());

	//Start-of-scenario save
	if(gamestate_.snapshot.empty()){
		//For a start-of-scenario-save, write the data to the starting_pos and not the snapshot, since
		//there should only be snapshots for midgame reloads
		if (config &c = level.child("replay_start")) {
			c.merge_with(start_data);
		} else {
			level.add_child("replay_start") = start_data;
		}
		level.add_child("snapshot") = config();
	} else {
		level.add_child("snapshot") = start_data;
		level.add_child("replay_start") = gamestate_.starting_pos;
	}
	level["random_seed"] = start_data["random_seed"];
	level["random_calls"] = start_data["random_calls"];

	// Adds the replay data, and the replay start, to the level,
	// so clients can receive it.
	// level.add_child("statistics") = statistics::write_stats();
}

void loadgame::copy_era(config &cfg)
{
	const config &replay_start = cfg.child("replay_start");
	if (!replay_start) return;

	const config &era = replay_start.child("era");
	if (!era) return;

	config &snapshot = cfg.child("snapshot");
	if (!snapshot) return;

	snapshot.add_child("era", era);
}

savegame::savegame(hero_map& heros, game_state& gamestate, config& snapshot, const std::string& title)
	: heros_(heros)
	, gamestate_(gamestate)
	, snapshot_(snapshot)
	, filename_()
	, title_(title)
	, error_message_(_("The game could not be saved: "))
	, show_confirmation_(false)
{}

bool savegame::save_game_automatic(CVideo& video, bool ask_for_overwrite, const std::string& filename)
{
	bool overwrite = true;

	if (filename == "")
		create_filename();
	else
		filename_ = filename;

	if (ask_for_overwrite){
		overwrite = check_overwrite(video);

		if (!overwrite)
			return save_game_interactive(video, "", gui::OK_CANCEL);
	}

	return save_game(&video);
}

bool savegame::save_game_interactive(CVideo& video, const std::string& message,
									 gui::DIALOG_TYPE dialog_type)
{
	show_confirmation_ = true;
	create_filename();

	int res = gui2::twindow::OK;
	bool exit = true;

	do{
		try{
			res = show_save_dialog(video, message, dialog_type);
			exit = true;

			if (res == gui2::twindow::OK){
				exit = check_overwrite(video);
			}
		}
		catch (illegal_filename_exception){
			exit = false;
		}
	}
	while (!exit);

	if (res == 2) //Quit game
		throw end_level_exception(QUIT);

	if (res != gui2::twindow::OK)
		return false;

	return save_game(&video);
}

int savegame::show_save_dialog(CVideo& video, const std::string& message, const gui::DIALOG_TYPE dialog_type)
{
	int res = 0;

	std::string filename = filename_;

	if (dialog_type == gui::OK_CANCEL){
		gui2::tgame_save dlg(filename, title_);
		dlg.show(video);
		res = dlg.get_retval();
	}
	else if (dialog_type == gui::YES_NO){
		gui2::tgame_save_message dlg(title_, filename, message);
		dlg.show(video);
		res = dlg.get_retval();
	}

	check_filename(filename, video);
	set_filename(filename);

	check_filename(filename, video);
	set_filename(filename);
	return res;
}

bool savegame::check_overwrite(CVideo& video)
{
	std::string filename = filename_;
	if (manager::save_game_exists(filename)) {
		std::stringstream message;
		message << _("Save already exists. Do you want to overwrite it?") << "\n" << _("Name: ") << filename;
		int retval = gui2::show_message(video, _("Overwrite?"), message.str(), gui2::tmessage::yes_no_buttons);
		return retval == gui2::twindow::OK;
	} else {
		return true;
	}
}

void savegame::check_filename(const std::string& filename, CVideo& video)
{
	if (is_gzip_file(filename)) {
		gui2::show_error_message(video, _("Save names should not end on '.gz'. "
			"Please choose a different name."));
		throw illegal_filename_exception();
	}
}

bool savegame::is_illegal_file_char(char c)
{
	return c == '/' || c == '\\' || c == ':'
#ifdef _WIN32
	|| c == '?' || c == '|' || c == '<' || c == '>' || c == '*' || c == '"'
#endif
	;
}

void savegame::set_filename(std::string filename)
{
	filename.erase(std::remove_if(filename.begin(), filename.end(),
	            is_illegal_file_char), filename.end());
	filename_ = filename;
}

void savegame::before_save()
{
	recorder.get_replay_data(gamestate_.replay_data);

	// debug
	gamestate_.replay_data_dbg.clear();
	// recorder.pool_2_config(gamestate_.replay_data_dbg);
}

bool savegame::save_game(CVideo* video, const std::string& filename)
{
	static std::string parent, grandparent;

	try {
		Uint32 start, end;
		start = SDL_GetTicks();

		if (filename_ == "")
			filename_ = filename;

		before_save();

		// The magic moment that does save threading; after
		// each save, the filename of the save file becomes
		// the parent for the next. *Unless* the parent file
		// has the same name as the savefile, in which case we
		// use the grandparent name. When user loads a savegame,
		// we load its correct parent link along with it.
		if (filename_ == parent) {
			gamestate_.classification().parent = grandparent;
		} else {
			gamestate_.classification().parent = parent;
		}
		LOG_SAVE << "Setting parent of '" << filename_<< "' to " << gamestate_.classification().parent << "\n";

		write_game_to_disk(filename_);

		grandparent = parent;
		parent = filename_;

		end = SDL_GetTicks();
		LOG_SAVE << "Milliseconds to save " << filename_ << ": " << end - start << "\n";

		if (video != NULL && show_confirmation_)
			gui2::show_message(*video, _("Saved"), _("The game has been saved"));
		return true;
	} catch(game::save_game_failed& e) {
		ERR_SAVE << error_message_ << e.message;
		if (video != NULL){
			gui2::show_error_message(*video, error_message_ + e.message);
			//do not bother retrying, since the user can just try to save the game again
			//maybe show a yes-no dialog for "disable autosaves now"?
		}

		return false;
	};
}

void savegame::write_game_to_disk(const std::string& filename)
{
	filename_ = filename;
	filename_ += ".sav";

	std::stringstream ss;
	{
		config_writer out(ss, false);
		write_game(out);
	}

	// start scenario data(write it to stream only first, others write stream to file direct)
	if (!gamestate_.start_scenario_ss.str().length()) {
		::write(gamestate_.start_scenario_ss, gamestate_.starting_pos);
	}
	if (!gamestate_.start_hero_data_) {
		gamestate_.start_hero_data_ = (uint8_t*)malloc(resources::heros_start->file_size());
		resources::heros_start->map_to_mem(gamestate_.start_hero_data_);
	}

	config cfg_summary;
	std::stringstream summary_ss;
	extract_summary_data_from_save(cfg_summary);
	::write(summary_ss, cfg_summary);

	// if enable compress, ss.str() is compressed format data.
	posix_file_t fp;
	uint32_t summary_size, scenario_size, side_size, start_scenario_size, start_hero_size, player_data_size, replay_size, bytertd;
	int size;

	// replace_space2underbar(filename_);
	std::string fullfilename = get_saves_dir() + "/" + filename_;
#ifdef _WIN32
	// utf8 ---> utf16
	int wlen = MultiByteToWideChar(CP_UTF8, 0, fullfilename.c_str(), -1, NULL, 0);
	WCHAR *wc = new WCHAR[wlen];
	MultiByteToWideChar(CP_UTF8, 0, fullfilename.c_str(), -1, wc, wlen);
			
	fp = CreateFileW(wc, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			CREATE_ALWAYS, 0, NULL);
	// posix_fopen(wc, GENERIC_WRITE, CREATE_ALWAYS, fp);
	delete [] wc;
#else
	posix_fopen(fullfilename.c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
#endif
	
	if (fp == INVALID_FILE) {
		throw game::save_game_failed(_("Could not write to file"));
	}

	// length of summary data
	summary_size = summary_ss.str().length();
	// length of scenario data
	scenario_size = ss.str().length();
	// length of side data
	side_size = ((unit_segment2*)unit::savegame_cache_)->size_;
	// length of start scenario data
	start_scenario_size = gamestate_.start_scenario_ss.str().length();
	// length of start hero data
	// start_hero_size = gamestate_.start_hero_ss.str().length();
	start_hero_size = resources::heros_start->file_size();
	// length of player data(name,surname,biorigh)
	player_data_size = 16 + rpg::h->name().size() + rpg::h->surname().size();
	// length of replay data
	if (gamestate_.replay_data.pool_pos_vsize()) {
		replay_size = 16 + gamestate_.replay_data.pool_data_gzip_size() + gamestate_.replay_data.pool_pos_vsize() * sizeof(unsigned int);
	} else {
		replay_size = 0;
	}
	
	// [write] length of summary data
	posix_fwrite(fp, &summary_size, sizeof(summary_size), bytertd);
	// [write] length of scenario data
	posix_fwrite(fp, &scenario_size, sizeof(scenario_size), bytertd);
	// [write] length of side data
	posix_fwrite(fp, &side_size, sizeof(side_size), bytertd);
	// [write] length of start scenario data
	posix_fwrite(fp, &start_scenario_size, sizeof(start_scenario_size), bytertd);
	// [write] length of start hero data
	posix_fwrite(fp, &start_hero_size, sizeof(start_hero_size), bytertd);
	// [write] length of player data
	posix_fwrite(fp, &player_data_size, sizeof(player_data_size), bytertd);
	// [write] length of replay data
	posix_fwrite(fp, &replay_size, sizeof(replay_size), bytertd);
	
	// summary data
	posix_fwrite(fp, summary_ss.str().c_str(), summary_ss.str().length(), bytertd);
	// scenario data
	posix_fwrite(fp, ss.str().c_str(), ss.str().length(), bytertd);
	// side data
	if (side_size) {
		posix_fwrite(fp, unit::savegame_cache_, side_size, bytertd);
	}
	// start scenario data
	posix_fwrite(fp, gamestate_.start_scenario_ss.str().c_str(), gamestate_.start_scenario_ss.str().length(), bytertd);
	// start hero data
	posix_fwrite(fp, gamestate_.start_hero_data_, start_hero_size, bytertd);

	// player data(use unit::savegame_cache_ tempereray)
	bytertd = rpg::h->number_;
	memcpy(unit::savegame_cache_, &bytertd, 4);
	size = 4;
	bytertd = rpg::h->name().size();
	memcpy(unit::savegame_cache_ + size, &bytertd, 4);
	size += 4;
	memcpy(unit::savegame_cache_ + size, rpg::h->name().c_str(), rpg::h->name().size());
	size += rpg::h->name().size();
	bytertd = rpg::h->surname().size();
	memcpy(unit::savegame_cache_ + size, &bytertd, 4);
	size += 4;
	memcpy(unit::savegame_cache_ + size, rpg::h->surname().c_str(), rpg::h->surname().size());
	size += rpg::h->surname().size();
	bytertd = 0;
	memcpy(unit::savegame_cache_ + size, &bytertd, 4);
	size += 4;
	posix_fwrite(fp, unit::savegame_cache_, size, bytertd);

	// replay data
	if (replay_size) {
		size = gamestate_.replay_data.pool_data_size();
		posix_fwrite(fp, &size, sizeof(size), bytertd);
		size = gamestate_.replay_data.pool_data_gzip_size();
		posix_fwrite(fp, &size, sizeof(size), bytertd);
		size = gamestate_.replay_data.pool_pos_size();
		posix_fwrite(fp, &size, sizeof(size), bytertd);
		size = gamestate_.replay_data.pool_pos_vsize();
		posix_fwrite(fp, &size, sizeof(size), bytertd);
		// pool data
		posix_fwrite(fp, gamestate_.replay_data.pool_data(), gamestate_.replay_data.pool_data_gzip_size(), bytertd);
		// pool pos
		posix_fwrite(fp, gamestate_.replay_data.pool_pos(), gamestate_.replay_data.pool_pos_vsize() * sizeof(unsigned int), bytertd);
	}

	// hero data
	heros_.map_to_file_fp(fp);
	posix_fclose(fp);
}

void savegame::write_game(config_writer &out) const
{
	out.write_key_val("label", gamestate_.classification().label);
	out.write_key_val("original_label", gamestate_.classification().original_label);
	out.write_key_val("parent", gamestate_.classification().parent);
	out.write_key_val("history", gamestate_.classification().history);
	out.write_key_val("abbrev", gamestate_.classification().abbrev);
	out.write_key_val("mode", gamestate_.classification().mode);
	out.write_key_val("version", game_config::version);
	out.write_key_val("scenario", gamestate_.classification().scenario);
	out.write_key_val("next_scenario", gamestate_.classification().next_scenario);
	out.write_key_val("completion", gamestate_.classification().completion);
	out.write_key_val("campaign", gamestate_.classification().campaign);
	out.write_key_val("campaign_type", gamestate_.classification().campaign_type);
	out.write_key_val("difficulty", gamestate_.classification().difficulty);
	out.write_key_val("campaign_define", gamestate_.classification().campaign_define);
	out.write_key_val("campaign_extra_defines", utils::join(gamestate_.classification().campaign_xtra_defines));
	out.write_key_val("random_seed", lexical_cast<std::string>(gamestate_.rng().get_random_seed()));
	out.write_key_val("random_calls", lexical_cast<std::string>(gamestate_.rng().get_random_calls()));
	// out.write_key_val("recorder_random_seed", lexical_cast<std::string>(recorder.generator().get_random_seed()));
	// out.write_key_val("recorder_random_calls", lexical_cast<std::string>(recorder.generator().get_random_calls()));
	out.write_key_val("next_underlying_unit_id", lexical_cast<std::string>(n_unit::id_manager::instance().get_save_id()));
	out.write_key_val("end_text", gamestate_.classification().end_text);
	out.write_key_val("end_text_duration", str_cast<unsigned int>(gamestate_.classification().end_text_duration));
	// update newest rpg to variables
	// gamestate_.set_variable("player.stratum", lexical_cast<std::string>(rpg::stratum));
	// gamestate_.set_variable("player.forbids", lexical_cast<std::string>(rpg::forbids));
	gamestate_.rpg_2_variable();
	out.write_child("variables", gamestate_.get_variables());

	for (std::map<std::string, wml_menu_item *>::const_iterator j=gamestate_.wml_menu_items.begin();
	    j!=gamestate_.wml_menu_items.end(); ++j) {
		out.open_child("menu_item");
		out.write_key_val("id", j->first);
		out.write_key_val("image", j->second->image);
		out.write_key_val("description", j->second->description);
		out.write_key_val("needs_select", (j->second->needs_select) ? "yes" : "no");
		if(!j->second->show_if.empty())
			out.write_child("show_if", j->second->show_if);
		if(!j->second->filter_location.empty())
			out.write_child("filter_location", j->second->filter_location);
		if(!j->second->command.empty())
			out.write_child("command", j->second->command);
		out.close_child("menu_item");
	}

	out.write_child("snapshot", snapshot_);
/*
	out.open_child("statistics");
	statistics::write_stats(out);
	out.close_child("statistics");
*/
	if (!gamestate_.replay_data_dbg.child("replay12")) {
		out.write_child("replay12", gamestate_.replay_data_dbg);
	}
}

void savegame::extract_summary_data_from_save(config& out)
{
	const bool has_replay = gamestate_.replay_data.pool_pos_vsize()? true: false;
	const bool has_snapshot = snapshot_.child("side") || ((unit_segment2*)unit::savegame_cache_)->size_;

	out["replay"] = has_replay ? "yes" : "no";
	out["snapshot"] = has_snapshot ? "yes" : "no";

	out["label"] = gamestate_.classification().label;
	out["parent"] = gamestate_.classification().parent;
	out["campaign"] = gamestate_.classification().campaign;
	out["campaign_type"] = gamestate_.classification().campaign_type;
	out["scenario"] = gamestate_.classification().scenario;
	out["difficulty"] = gamestate_.classification().difficulty;
	out["version"] = gamestate_.classification().version;
	out["corrupt"] = "";

	if (has_snapshot) {
		out["turn"] = snapshot_["turn_at"];
		if (snapshot_["turns"] != "-1") {
			out["turn"] = out["turn"].str() + "/" + snapshot_["turns"].str();
		}
	}

	// Find the first human leader so we can display their icon in the load menu.

	/** @todo Ideally we should grab all leaders if there's more than 1 human player? */
	std::string leader;

	bool shrouded = false;

	const config& snapshot = has_snapshot ? snapshot_: gamestate_.starting_pos;
	foreach (const config &side, snapshot.child_range("side"))
	{
		if (side["controller"] != "human") {
			continue;
		}
		if (utils::string_bool(side["shroud"])) {
			shrouded = true;
		}
	}

	out["leader"] = leader;
	out["map_data"] = "";

	if(!shrouded) {
		if(has_snapshot) {
			if (!snapshot_.find_child("side", "shroud", "yes")) {
				out["map_data"] = snapshot_["map_data"];
			}
		} else if(has_replay) {
			if (!gamestate_.starting_pos.find_child("side", "shroud", "yes")) {
				out["map_data"] = gamestate_.starting_pos["map_data"];
			}
		}
	}
}

scenariostart_savegame::scenariostart_savegame(hero_map& heros, game_state &gamestate)
	: savegame(heros, gamestate, dummy_snapshot)
{
	memset(unit::savegame_cache_, 0, sizeof(unit_segment2));
	set_filename(gamestate.classification().label);
}

void scenariostart_savegame::before_save()
{
	//Add the player section to the starting position so we can get the correct recall list
	//when loading the replay later on
	// if there is no scenario information in the starting pos, add the (persistent) sides from the snapshot
	// else do nothing, as persistence information was already added at the end of the previous scenario
	if (gamestate().starting_pos["id"].empty()) {
		foreach(const config& snapshot_side, gamestate().snapshot.child_range("side")) {
			//add all side tags (assuming they only contain carryover information)
			gamestate().starting_pos.add_child("side", snapshot_side);
		}
	}
}

replay_savegame::replay_savegame(hero_map& heros, game_state &gamestate)
	: savegame(heros, gamestate, dummy_snapshot, _("Save Replay"))
{
	memset(unit::savegame_cache_, 0, sizeof(unit_segment2));
}

void replay_savegame::create_filename()
{
	std::stringstream stream;

	if (!preferences::eng_file_name()) {
		const std::string ellipsed_name = font::make_text_ellipsis(gamestate().classification().label, font::SIZE_NORMAL, 200);
		stream << ellipsed_name << "_" << _("replay");
	} else {
		const std::string ellipsed_name = font::make_text_ellipsis(gamestate().classification().original_label, font::SIZE_NORMAL, 200);
		stream << ellipsed_name << "_" << "replay";
	}

	set_filename(stream.str());
}

autosave_savegame::autosave_savegame(hero_map& heros, game_state &gamestate, game_display& gui, config& snapshot_cfg)
	: game_savegame(heros, gamestate, gui, snapshot_cfg)
{
	set_error_message(_("Could not auto save the game. Please save the game manually."));
}

void autosave_savegame::autosave(const bool disable_autosave, const int autosave_max, const int infinite_autosaves)
{
	if(disable_autosave)
		return;

	save_game_automatic(gui_.video());

	manager::remove_old_auto_saves(autosave_max, infinite_autosaves);
}

void autosave_savegame::create_filename()
{
	std::string filename;
	if (gamestate().classification().original_label.empty()) {
		if (!preferences::eng_file_name()) {
			filename = _("Auto-Save");
		} else {
			filename = "Auto-Save";
		}
	} else {
		if (!preferences::eng_file_name()) {
			filename = gamestate().classification().label + "-" + _("Auto-Save") + snapshot()["turn_at"];
		} else {
			filename = gamestate().classification().original_label + "-" + "Auto-Save" + snapshot()["turn_at"];
		}
	}

	set_filename(filename);
}

oos_savegame::oos_savegame(hero_map& heros, config& snapshot_cfg)
	: game_savegame(heros, *resources::state_of_game, *resources::screen, snapshot_cfg)
{}

int oos_savegame::show_save_dialog(CVideo& video, const std::string& message, const gui::DIALOG_TYPE /*dialog_type*/)
{
	static bool ignore_all = false;
	int res = 0;

	std::string filename = this->filename();

	if (!ignore_all){
		gui2::tgame_save_oos dlg(ignore_all, title(), filename, message);
		dlg.show(video);
		res = dlg.get_retval();
	}

	check_filename(filename, video);
	set_filename(filename);

	return res;
}

game_savegame::game_savegame(hero_map& heros, game_state &gamestate,
					game_display& gui, config& snapshot_cfg)
	: savegame(heros, gamestate, snapshot_cfg, _("Save Game")),
	gui_(gui)
{
}

void game_savegame::create_filename()
{
	std::stringstream stream;

	if (!preferences::eng_file_name()) {
		const std::string ellipsed_name = font::make_text_ellipsis(gamestate().classification().label, font::SIZE_NORMAL, 200);
		stream << ellipsed_name << "_" << _("Turn") << "_" << snapshot()["turn_at"];
	} else {
		const std::string ellipsed_name = font::make_text_ellipsis(gamestate().classification().original_label, font::SIZE_NORMAL, 200);
		stream << ellipsed_name << "_" << "Turn" << "_" << snapshot()["turn_at"];
	}
	set_filename(stream.str());
}

void game_savegame::before_save()
{
	savegame::before_save();
	write_game_snapshot();
}

void game_savegame::write_game_snapshot()
{

	snapshot()["snapshot"] = "yes";

	std::stringstream buf;
	buf << gui_.playing_team();
	snapshot()["playing_team"] = buf.str();

	write_events(snapshot());

	write_music_play_list(snapshot());

	gamestate().write_snapshot(snapshot());

	gui_.labels().write(snapshot());
}

}

