/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Igor Paliychuk
Copyright © 2012 Stefan Beller
Copyright © 2013 Henrik Andersson

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * class MenuPowers
 */

#include "Menu.h"
#include "FileParser.h"
#include "MenuPowers.h"
#include "SharedResources.h"
#include "PowerManager.h"
#include "Settings.h"
#include "StatBlock.h"
#include "UtilsParsing.h"
#include "WidgetLabel.h"
#include "WidgetSlot.h"
#include "WidgetTooltip.h"

#include <string>
#include <sstream>
#include <iostream>
#include <climits>

using namespace std;
MenuPowers *menuPowers = NULL;
MenuPowers *MenuPowers::getInstance() {
	return menuPowers;
}


MenuPowers::MenuPowers(StatBlock *_stats, PowerManager *_powers, SDL_Surface *_icons) {

	int id;

	stats = _stats;
	powers = _powers;
	icons = _icons;

	overlay_disabled = NULL;

	visible = false;

	points_left = 0;
	tabs_count = 1;
	pressed = false;
	id = 0;

	tabControl = NULL;

	closeButton = new WidgetButton("images/menus/buttons/button_x.png");

	// Read powers data from config file
	FileParser infile;
	if (infile.open("menus/powers.txt")) {
		bool id_line = false;
		while (infile.next()) {
			infile.val = infile.val + ',';

			if (infile.key == "tab_title") {
				tab_titles.push_back(eatFirstString(infile.val, ','));
			}
			else if (infile.key == "tab_tree") {
				tree_image_files.push_back(eatFirstString(infile.val, ','));
			}
			else if (infile.key == "caption") {
				title = eatLabelInfo(infile.val);
			}
			else if (infile.key == "unspent_points") {
				unspent_points = eatLabelInfo(infile.val);
			}
			else if (infile.key == "close") {
				close_pos.x = eatFirstInt(infile.val, ',');
				close_pos.y = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "tab_area") {
				tab_area.x = eatFirstInt(infile.val, ',');
				tab_area.y = eatFirstInt(infile.val, ',');
				tab_area.w = eatFirstInt(infile.val, ',');
				tab_area.h = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "tabs") {
				tabs_count = eatFirstInt(infile.val, ',');
				if (tabs_count < 1) tabs_count = 1;
			}

			if (infile.key == "id") {
				id = eatFirstInt(infile.val, ',');
				id_line = true;
				if (id > 0) {
					power_cell.push_back(Power_Menu_Cell());
					slots.push_back(NULL);
					power_cell.back().id = id;
				}
			}
			else id_line = false;

			if (id < 1) {
				if (id_line) fprintf(stderr, "Power index inside power menu definition out of bounds 1-%d, skipping\n", INT_MAX);
				continue;
			}
			if (id_line) continue;

			if (infile.key == "tab") {
				power_cell.back().tab = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "position") {
				power_cell.back().pos.x = eatFirstInt(infile.val, ',');
				power_cell.back().pos.y = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_physoff") {
				power_cell.back().requires_physoff = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_physdef") {
				power_cell.back().requires_physdef = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_mentoff") {
				power_cell.back().requires_mentoff = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_mentdef") {
				power_cell.back().requires_mentdef = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_defense") {
				power_cell.back().requires_defense = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_offense") {
				power_cell.back().requires_offense = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_physical") {
				power_cell.back().requires_physical = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_mental") {
				power_cell.back().requires_mental = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_point") {
				if (infile.val == "true,")
					power_cell.back().requires_point = true;
			}
			else if (infile.key == "requires_level") {
				power_cell.back().requires_level = eatFirstInt(infile.val, ',');
			}
			else if (infile.key == "requires_power") {
				power_cell.back().requires_power = eatFirstInt(infile.val, ',');
			}

		}
		infile.close();
	}

	loadGraphics();

	// check for errors in config file
	if((tabs_count == 1) && (!tree_image_files.empty() || !tab_titles.empty())) {
		fprintf(stderr, "menu/powers.txt error: you don't have tabs, but tab_tree_image and tab_title counts are not 0\n");
		SDL_Quit();
		exit(1);
	}
	else if((tabs_count > 1) && (tree_image_files.size() != (unsigned)tabs_count || tab_titles.size() != (unsigned)tabs_count)) {
		fprintf(stderr, "menu/powers.txt error: tabs count, tab_tree_image and tab_name counts do not match\n");
		SDL_Quit();
		exit(1);
	}

	menuPowers = this;

	color_bonus = font->getColor("menu_bonus");
	color_penalty = font->getColor("menu_penalty");
}

void MenuPowers::update() {
	for (unsigned i=0; i<power_cell.size(); i++) {
		slots[i]->pos.x = window_area.x + power_cell[i].pos.x;
		slots[i]->pos.y = window_area.y + power_cell[i].pos.y;
	}

	label_powers.set(window_area.x+title.x, window_area.y+title.y, title.justify, title.valign, msg->get("Powers"), font->getColor("menu_normal"), title.font_style);

	closeButton->pos.x = window_area.x+close_pos.x;
	closeButton->pos.y = window_area.y+close_pos.y;

	stat_up.set(window_area.x+unspent_points.x, window_area.y+unspent_points.y, unspent_points.justify, unspent_points.valign, "", font->getColor("menu_bonus"), unspent_points.font_style);

	// If we have more than one tab, create TabControl
	if (tabs_count > 1) {
		tabControl = new WidgetTabControl(tabs_count);

		// Initialize the tab control.
		tabControl->setMainArea(window_area.x+tab_area.x, window_area.y+tab_area.y, tab_area.w, tab_area.h);

		// Define the header.
		for (int i=0; i<tabs_count; i++)
			tabControl->setTabTitle(i, msg->get(tab_titles[i]));
		tabControl->updateHeader();
	}
}

void MenuPowers::loadGraphics() {

	background = loadGraphicSurface("images/menus/powers.png");
	powers_unlock = loadGraphicSurface("images/menus/powers_unlock.png");
	overlay_disabled = loadGraphicSurface("images/menus/disabled.png");

	if (tree_image_files.empty()) {
		tree_surf.push_back(loadGraphicSurface("images/menus/powers_tree.png"));
	}
	else {
		for (unsigned int i = 0; i < tree_image_files.size(); ++i)
			tree_surf.push_back(loadGraphicSurface("images/menus/" + tree_image_files[i]));
	}
	for (unsigned int i=0; i<slots.size(); i++) {

		slots[i] = new WidgetSlot(icons, powers->powers[power_cell[i].id].icon);
		slots[i]->pos.x = power_cell[i].pos.x;
		slots[i]->pos.y = power_cell[i].pos.y;
		tablist.add(slots[i]);
	}
}

short MenuPowers::id_by_powerIndex(short power_index) {
	// Find cell with our power
	for (unsigned i=0; i<power_cell.size(); i++)
		if (power_cell[i].id == power_index)
			return i;

	return -1;
}

bool MenuPowers::baseRequirementsMet(int power_index) {
	int id = id_by_powerIndex(power_index);

	if ((stats->physoff() >= power_cell[id].requires_physoff) &&
			(stats->physdef() >= power_cell[id].requires_physdef) &&
			(stats->mentoff() >= power_cell[id].requires_mentoff) &&
			(stats->mentdef() >= power_cell[id].requires_mentdef) &&
			(stats->get_defense() >= power_cell[id].requires_defense) &&
			(stats->get_offense() >= power_cell[id].requires_offense) &&
			(stats->get_physical() >= power_cell[id].requires_physical) &&
			(stats->get_mental() >= power_cell[id].requires_mental) &&
			(stats->level >= power_cell[id].requires_level) &&
			requirementsMet(power_cell[id].requires_power)) return true;
	return false;
}

/**
 * With great power comes great stat requirements.
 */
bool MenuPowers::requirementsMet(int power_index) {

	// power_index can be 0 during recursive call if requires_power is not defined.
	// Power with index 0 doesn't exist and is always enabled
	if (power_index == 0) return true;

	int id = id_by_powerIndex(power_index);

	// If we didn't find power in power_menu, than it has no requirements
	if (id == -1) return true;

	// If power_id is saved into vector, it's unlocked anyway
	if (find(stats->powers_list.begin(), stats->powers_list.end(), power_index) != stats->powers_list.end()) return true;

	// Check the rest requirements
	if (baseRequirementsMet(power_index) && !power_cell[id].requires_point) return true;
	return false;
}

/**
 * Check if we can unlock power.
 */
bool MenuPowers::powerUnlockable(int power_index) {

	// power_index can be 0 during recursive call if requires_power is not defined.
	// Power with index 0 doesn't exist and is always enabled
	if (power_index == 0) return true;

	// Find cell with our power
	int id = id_by_powerIndex(power_index);

	// If we didn't find power in power_menu, than it has no requirements
	if (id == -1) return true;

	// If we already have a power, don't try to unlock it
	if (requirementsMet(power_index)) return false;

	// Check requirements
	if (baseRequirementsMet(power_index)) return true;
	return false;
}

/**
 * Click-to-drag a power (to the action bar)
 */
int MenuPowers::click(Point mouse) {

	// if we have tabControl
	if (tabs_count > 1) {
		int active_tab = tabControl->getActiveTab();
		for (unsigned i=0; i<power_cell.size(); i++) {
			if (isWithin(slots[i]->pos, mouse) && (power_cell[i].tab == active_tab)) {
				if (requirementsMet(power_cell[i].id) && !powers->powers[power_cell[i].id].passive) return power_cell[i].id;
				else return 0;
			}
		}
		// if have don't have tabs
	}
	else {
		for (unsigned i=0; i<power_cell.size(); i++) {
			if (isWithin(slots[i]->pos, mouse)) {
				if (requirementsMet(power_cell[i].id) && !powers->powers[power_cell[i].id].passive) return power_cell[i].id;
				else return 0;
			}
		}
	}
	return 0;
}

/**
 * Unlock a power
 */
bool MenuPowers::unlockClick(Point mouse) {

	// if we have tabCOntrol
	if (tabs_count > 1) {
		int active_tab = tabControl->getActiveTab();
		for (unsigned i=0; i<power_cell.size(); i++) {
			if (isWithin(slots[i]->pos, mouse)
					&& (powerUnlockable(power_cell[i].id)) && points_left > 0
					&& power_cell[i].requires_point && power_cell[i].tab == active_tab) {
				stats->powers_list.push_back(power_cell[i].id);
				stats->check_title = true;
				return true;
			}
		}
		// if have don't have tabs
	}
	else {
		for (unsigned i=0; i<power_cell.size(); i++) {
			if (isWithin(slots[i]->pos, mouse)
					&& (powerUnlockable(power_cell[i].id))
					&& points_left > 0 && power_cell[i].requires_point) {
				stats->powers_list.push_back(power_cell[i].id);
				stats->check_title = true;
				return true;
			}
		}
	}
	return false;
}

void MenuPowers::logic() {
	short points_used = 0;
	for (unsigned i=0; i<power_cell.size(); i++) {
		if (powers->powers[power_cell[i].id].passive) {
			bool unlocked_power = find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end();
			vector<int>::iterator it = find(stats->powers_passive.begin(), stats->powers_passive.end(), power_cell[i].id);
			if (it != stats->powers_passive.end()) {
				if (!baseRequirementsMet(power_cell[i].id) && power_cell[i].passive_on) {
					stats->powers_passive.erase(it);
					stats->effects.removeEffectPassive(power_cell[i].id);
					power_cell[i].passive_on = false;
					stats->refresh_stats = true;
				}
			} else if (((baseRequirementsMet(power_cell[i].id) && !power_cell[i].requires_point) || unlocked_power) && !power_cell[i].passive_on) {
				stats->powers_passive.push_back(power_cell[i].id);
				power_cell[i].passive_on = true;
				// for passives without special triggers, we need to trigger them here
				if (stats->effects.triggered_others)
					powers->activateSinglePassive(stats, power_cell[i].id);
			}
		}
		if (power_cell[i].requires_point &&
				(find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end()))
			points_used++;
	}
	points_left = (stats->level * stats->power_points_per_level) - points_used;

	if (!visible) return;

	if (NO_MOUSE)
	{
		tablist.logic();
	}

	// make shure keyboard navigation leads us to correct tab
	for (unsigned int i = 0; i < slots.size(); i++) {
		if (slots[i]->in_focus) tabControl->setActiveTab(power_cell[i].tab);
	}

	if (closeButton->checkClick()) {
		visible = false;
		snd->play(sfx_close);
	}
	if (tabs_count > 1) tabControl->logic();
}

void MenuPowers::render() {
	if (!visible) return;

	SDL_Rect src;
	SDL_Rect dest;

	// background
	dest = window_area;
	src.x = 0;
	src.y = 0;
	src.w = window_area.w;
	src.h = window_area.h;
	SDL_BlitSurface(background, &src, screen, &dest);

	if (tabs_count > 1) {
		tabControl->render();
		int active_tab = tabControl->getActiveTab();
		for (int i=0; i<tabs_count; i++) {
			if (active_tab == i) {
				// power tree
				SDL_BlitSurface(tree_surf[i], &src, screen, &dest);
				// power icons
				renderPowers(active_tab);
			}
		}
	}
	else {
		SDL_BlitSurface(tree_surf[0], &src, screen, &dest);
		renderPowers(0);
	}

	// close button
	closeButton->render();

	// text overlay
	if (!title.hidden) label_powers.render();

	// stats
	if (!unspent_points.hidden) {
		stringstream ss;

		ss.str("");
		if (points_left !=0) {
			ss << msg->get("Unspent skill points:") << " " << points_left;
		}
		stat_up.set(ss.str());
		stat_up.render();
	}
}

/**
 * Highlight unlocked powers
 */
void MenuPowers::displayBuild(int power_id) {
	SDL_Rect src_unlock;

	src_unlock.x = 0;
	src_unlock.y = 0;
	src_unlock.w = ICON_SIZE;
	src_unlock.h = ICON_SIZE;

	for (unsigned i=0; i<power_cell.size(); i++) {
		if (power_cell[i].id == power_id) {
			SDL_BlitSurface(powers_unlock, &src_unlock, screen, &slots[i]->pos);
		}
	}
}

/**
 * Show mouseover descriptions of disciplines and powers
 */
TooltipData MenuPowers::checkTooltip(Point mouse) {

	TooltipData tip;

	for (unsigned i=0; i<power_cell.size(); i++) {

		if ((tabs_count > 1) && (tabControl->getActiveTab() != power_cell[i].tab)) continue;

		if (isWithin(slots[i]->pos, mouse)) {
			tip.addText(powers->powers[power_cell[i].id].name);
			if (powers->powers[power_cell[i].id].passive) tip.addText("Passive");
			tip.addText(powers->powers[power_cell[i].id].description);

			if (powers->powers[power_cell[i].id].requires_physical_weapon)
				tip.addText(msg->get("Requires a physical weapon"));
			else if (powers->powers[power_cell[i].id].requires_mental_weapon)
				tip.addText(msg->get("Requires a mental weapon"));
			else if (powers->powers[power_cell[i].id].requires_offense_weapon)
				tip.addText(msg->get("Requires an offense weapon"));


			// add requirement
			if ((power_cell[i].requires_physoff > 0) && (stats->physoff() < power_cell[i].requires_physoff)) {
				tip.addText(msg->get("Requires Physical Offense %d", power_cell[i].requires_physoff), color_penalty);
			}
			else if((power_cell[i].requires_physoff > 0) && (stats->physoff() >= power_cell[i].requires_physoff)) {
				tip.addText(msg->get("Requires Physical Offense %d", power_cell[i].requires_physoff));
			}
			if ((power_cell[i].requires_physdef > 0) && (stats->physdef() < power_cell[i].requires_physdef)) {
				tip.addText(msg->get("Requires Physical Defense %d", power_cell[i].requires_physdef), color_penalty);
			}
			else if ((power_cell[i].requires_physdef > 0) && (stats->physdef() >= power_cell[i].requires_physdef)) {
				tip.addText(msg->get("Requires Physical Defense %d", power_cell[i].requires_physdef));
			}
			if ((power_cell[i].requires_mentoff > 0) && (stats->mentoff() < power_cell[i].requires_mentoff)) {
				tip.addText(msg->get("Requires Mental Offense %d", power_cell[i].requires_mentoff), color_penalty);
			}
			else if ((power_cell[i].requires_mentoff > 0) && (stats->mentoff() >= power_cell[i].requires_mentoff)) {
				tip.addText(msg->get("Requires Mental Offense %d", power_cell[i].requires_mentoff));
			}
			if ((power_cell[i].requires_mentdef > 0) && (stats->mentdef() < power_cell[i].requires_mentdef)) {
				tip.addText(msg->get("Requires Mental Defense %d", power_cell[i].requires_mentdef), color_penalty);
			}
			else if ((power_cell[i].requires_mentdef > 0) && (stats->mentdef() >= power_cell[i].requires_mentdef)) {
				tip.addText(msg->get("Requires Mental Defense %d", power_cell[i].requires_mentdef));
			}
			if ((power_cell[i].requires_offense > 0) && (stats->get_offense() < power_cell[i].requires_offense)) {
				tip.addText(msg->get("Requires Offense %d", power_cell[i].requires_offense), color_penalty);
			}
			else if ((power_cell[i].requires_offense > 0) && (stats->get_offense() >= power_cell[i].requires_offense)) {
				tip.addText(msg->get("Requires Offense %d", power_cell[i].requires_offense));
			}
			if ((power_cell[i].requires_defense > 0) && (stats->get_defense() < power_cell[i].requires_defense)) {
				tip.addText(msg->get("Requires Defense %d", power_cell[i].requires_defense), color_penalty);
			}
			else if ((power_cell[i].requires_defense > 0) && (stats->get_defense() >= power_cell[i].requires_defense)) {
				tip.addText(msg->get("Requires Defense %d", power_cell[i].requires_defense));
			}
			if ((power_cell[i].requires_physical > 0) && (stats->get_physical() < power_cell[i].requires_physical)) {
				tip.addText(msg->get("Requires Physical %d", power_cell[i].requires_physical), color_penalty);
			}
			else if ((power_cell[i].requires_physical > 0) && (stats->get_physical() >= power_cell[i].requires_physical)) {
				tip.addText(msg->get("Requires Physical %d", power_cell[i].requires_physical));
			}
			if ((power_cell[i].requires_mental > 0) && (stats->get_mental() < power_cell[i].requires_mental)) {
				tip.addText(msg->get("Requires Mental %d", power_cell[i].requires_mental), color_penalty);
			}
			else if ((power_cell[i].requires_mental > 0) && (stats->get_mental() >= power_cell[i].requires_mental)) {
				tip.addText(msg->get("Requires Mental %d", power_cell[i].requires_mental));
			}

			// Draw required Level Tooltip
			if ((power_cell[i].requires_level > 0) && stats->level < power_cell[i].requires_level) {
				tip.addText(msg->get("Requires Level %d", power_cell[i].requires_level), color_penalty);
			}
			else if ((power_cell[i].requires_level > 0) && stats->level >= power_cell[i].requires_level) {
				tip.addText(msg->get("Requires Level %d", power_cell[i].requires_level));
			}

			// Draw required Skill Point Tooltip
			if ((power_cell[i].requires_point) &&
					!(find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end()) &&
					(points_left < 1)) {
				tip.addText(msg->get("Requires %d Skill Point", power_cell[i].requires_point), color_penalty);
			}
			else if ((power_cell[i].requires_point) &&
					 !(find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end()) &&
					 (points_left > 0)) {
				tip.addText(msg->get("Requires %d Skill Point", power_cell[i].requires_point));
			}

			// Draw unlock power Tooltip
			if (power_cell[i].requires_point &&
					!(find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end()) &&
					(points_left > 0) &&
					powerUnlockable(power_cell[i].id) && (points_left > 0)) {
				tip.addText(msg->get("Click to Unlock"), color_bonus);
			}


			// Required Power Tooltip
			if ((power_cell[i].requires_power != 0) && !(requirementsMet(power_cell[i].requires_power))) {
				tip.addText(msg->get("Requires Power: %s", powers->powers[power_cell[i].requires_power].name), color_penalty);
			}
			else if ((power_cell[i].requires_power != 0) && (requirementsMet(power_cell[i].requires_power))) {
				tip.addText(msg->get("Requires Power: %s", powers->powers[power_cell[i].requires_power].name));
			}

			// add mana cost
			if (powers->powers[power_cell[i].id].requires_mp > 0) {
				tip.addText(msg->get("Costs %d MP", powers->powers[power_cell[i].id].requires_mp));
			}
			// add health cost
			if (powers->powers[power_cell[i].id].requires_hp > 0) {
				tip.addText(msg->get("Costs %d HP", powers->powers[power_cell[i].id].requires_hp));
			}
			// add cooldown time
			if (powers->powers[power_cell[i].id].cooldown > 0) {
				tip.addText(msg->get("Cooldown: %d seconds", powers->powers[power_cell[i].id].cooldown / MAX_FRAMES_PER_SEC));
			}

			return tip;
		}
	}

	return tip;
}

MenuPowers::~MenuPowers() {
	SDL_FreeSurface(background);
	for (unsigned int i=0; i<tree_surf.size(); i++) SDL_FreeSurface(tree_surf[i]);
	for (unsigned int i=0; i<slots.size(); i++) {
		delete slots.at(i);
	}
	slots.clear();
	SDL_FreeSurface(powers_unlock);
	SDL_FreeSurface(overlay_disabled);

	delete closeButton;
	if (tabs_count > 1) delete tabControl;
	menuPowers = NULL;
}

/**
 * Return true if required stats for power usage are met. Else return false.
 */
bool MenuPowers::meetsUsageStats(unsigned powerid) {

	// Find cell with our power
	int id = id_by_powerIndex(powerid);
	// If we didn't find power in power_menu, than it has no stats requirements
	if (id == -1) return true;

	return stats->physoff() >= power_cell[id].requires_physoff
		   && stats->physdef() >= power_cell[id].requires_physdef
		   && stats->mentoff() >= power_cell[id].requires_mentoff
		   && stats->mentdef() >= power_cell[id].requires_mentdef
		   && stats->get_defense() >= power_cell[id].requires_defense
		   && stats->get_offense() >= power_cell[id].requires_offense
		   && stats->get_mental() >= power_cell[id].requires_mental
		   && stats->get_physical() >= power_cell[id].requires_physical;
}

void MenuPowers::renderPowers(int tab_num) {

	SDL_Rect disabled_src;
	disabled_src.x = disabled_src.y = 0;
	disabled_src.w = disabled_src.h = ICON_SIZE;

	for (unsigned i=0; i<power_cell.size(); i++) {
		bool power_in_vector = false;

		// Continue if slot is not filled with data
		if (power_cell[i].tab != tab_num) continue;

		if (find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end()) power_in_vector = true;

		slots[i]->render();

		// highlighting
		if (power_in_vector || requirementsMet(power_cell[i].id)) {
			displayBuild(power_cell[i].id);
		}
		else {
			if (overlay_disabled != NULL) {
				SDL_BlitSurface(overlay_disabled, &disabled_src, screen, &slots[i]->pos);
			}
		}
		slots[i]->renderSelection();
	}
}
