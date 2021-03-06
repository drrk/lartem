#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "player.h"

#include "ask.h"
#include "role.h"
#include "display.h"
#include "map.h"
#include "monst.h"
#include "attack.h"
#include "util.h"


void shadow_scan(unsigned int, int, double, double);


extern struct body humanoid_body;

const struct monst_type ptypes[NUM_ROLES] = {
	{ '@', "sysadmin", &humanoid_body },
	{ '@', "programmer", &humanoid_body }
};

struct symbol_def symbol_defs[] = {
	{ '.', "the floor of a room" },
	{ '-', "a wall or open door" },
	{ '|', "a wall" },
	{ '+', "a closed door" },
	{ '<', "a staircase leading up" },
	{ '>', "a staircase leading down" },

	{ '@', "you" },
	{ 'h', "a humanoid" },
	{ 'i', "an imp or minor demon" },
	{ '&', "a major demon" },
	{ 'D', "a daemon or other program" },
	{ 'S', "a server or other piece of hardware" },
	{ 0, NULL }
};


struct player player;


int player_init()
{
	int role, i;
	struct passwd *pw;
	pw = getpwuid(getuid());

	player.name = ask_str("What is your name?", pw->pw_name);

	role = ask_role();
	if(role == -1) return -1;
	else player.role = (unsigned int) role;

	/* player is placed into a sensible position in player_set_level() */
	player.x = 0;
	player.y = 0;

	player.xp = 0;

	for(i = 0; i < 6; i++) {
		((unsigned int *) &player.stats)[i] = 4 + (random() % 12);
		((int *) &player.stats_exe)[i] = 0;
	}

	player.stats.hpmax = 8 + (random() % 8);
	player.stats.hp = player.stats.hpmax;

	player.turn = 0;

	player.type = &ptypes[player.role];

	return 0;
}



void player_status()
{
	unsigned int level, r;
	const char *rank;

	level = player.xp >> 4;
	r = level >> 2;
	if(r > 7) r = 7;

	rank = get_rank(player.role, r);

	stat_printf("%s the %s\nFloor:%u  HP:%u(%u)  Lvl:%u  St:%u Dx:%u Co:%u In:%u Wi:%u Ch:%u  T:%u",
		    player.name, rank,
		    player.level->floor,
		    player.stats.hp, player.stats.hpmax,
		    level,
		    player.stats.st, player.stats.dx, player.stats.co,
		    player.stats.in, player.stats.wi, player.stats.ch,
		    player.turn);
}


void player_set_level(struct level *l)
{
	struct coord c = find_free_square(l->map);

	player.level = l;
	player.x = c.x;
	player.y = c.y;

	map_square(l->map, c.x, c.y)->monster = (struct monst *) &player;
}


void player_see()
{
	shadow_scan(0, 1, 1, 0);
	shadow_scan(1, 1, 1, 0);
	shadow_scan(2, 1, 1, 0);
	shadow_scan(3, 1, 1, 0);
	shadow_scan(4, 1, 1, 0);
	shadow_scan(5, 1, 1, 0);
	shadow_scan(6, 1, 1, 0);
	shadow_scan(7, 1, 1, 0);

	/* Finally, draw the player */
	main_plot(player.x, player.y, COL_WHITE, '@');

	display_refresh();
}



void shadow_scan(unsigned int octant,
		 int row,
		 double start_slope,
		 double end_slope)
{
	int col, start_col, end_col;
	unsigned int blocking = 1, opaque;
	int x, y;

	double new_start_slope = start_slope;
	double new_end_slope = end_slope;

/* For watching the algorithm in action

	display_refresh();
	usleep(100000);
*/

	start_col = (int) ((start_slope * row) + 0.5);
	end_col = (int) ((end_slope * row) - 0.5);

	if(end_col > start_col) return;


	for(col = start_col;
	    col >= end_col;
	    col--) {

		x = player.x;
		y = player.y;

		switch(octant) {
		case 0:
			x += row;
			y += col;
			break;
		case 1:
			x += col;
			y += row;
			break;
		case 2:
			x -= col;
			y += row;
			break;
		case 3:
			x -= row;
			y += col;
			break;
		case 4:
			x -= row;
			y -= col;
			break;
		case 5:
			x -= col;
			y -= row;
			break;
		case 6:
			x += col;
			y -= row;
			break;
		case 7:
			x += row;
			y -= col;
			break;
		}

		if(x < 0 || x >= MAP_X || y < 0 || y >= MAP_Y)
			opaque = 1;
		else {
			map_plot(player.level->map, x, y);
			opaque = MAP_TILE_IS_OPAQUE(player.level->map,
						    x, y);
		}


		if(blocking && !opaque) {

			if(col != start_col) {
				new_start_slope =
					(((double) col) + 0.5) /
					(((double) row) + 0.5);

				if(new_start_slope < 0)
					new_start_slope = 0;
				else if(new_start_slope > 1)
					new_start_slope = 1;
			}

			blocking = 0;

		} else if(!blocking && opaque) {

			new_end_slope =
				(((double) col) + 0.5) /
				(((double) row) - 0.5);

			if(new_end_slope < 0) new_end_slope = 0;
			else if(new_end_slope > 1) new_end_slope = 1;

			shadow_scan(octant,
				    row + 1,
				    new_start_slope,
				    new_end_slope);

			blocking = 1;
		}
	}

	if(!blocking) shadow_scan(octant, row + 1,
				  new_start_slope, end_slope);
}



/* Anything that happens automatically once a turn for players */
int player_poll()
{
	stats_heal(&player.stats);
	stats_exercise(&player.stats, &player.stats_exe);

	player.turn++;

	return 0;
}



int player_move(int dx, int dy)
{
	struct map_square *sq;
	int xx, yy;

	xx = player.x + dx;
	yy = player.y + dy;

	sq = map_square(player.level->map, xx, yy);
	if(!sq) return 0;

	if(sq->monster) {
		attack_monster(&player, sq->monster);
		return 0;
	}

	if(can_move_into_square(player.level->map, xx, yy)) {
		player.level->map[MAP_OFFSET(player.x,
					     player.y)].monster = NULL;
		player.x = xx;
		player.y = yy;
		sq->monster = (struct monst *) &player;

		return 1;
	}

	return 0;
}



void player_open(int dx, int dy)
{
	struct map_square *sq;
	char *message;

	sq = map_square(player.level->map, player.x + dx, player.y + dy);
	if(!sq) return;

	switch(sq->tile) {
	case TILE_DOOR_CLOSED:
		sq->tile = TILE_DOOR_OPEN;
		message = "You open the door.";
		break;

	case TILE_DOOR_OPEN:
		message = "That door is already open.";
		break;

	case TILE_DOOR_BORKED:
		message = "That door is broken.";
		break;

	default:
		message = "You see no door there.";
		break;
	}

	msg_printf("%s", message);
}



void player_close(int dx, int dy)
{
	struct map_square *sq;
	char *message;

	sq = map_square(player.level->map, player.x + dx, player.y + dy);
	if(!sq) return;

	switch(sq->tile) {
	case TILE_DOOR_CLOSED:
		message = "That door is already closed.";
		break;

	case TILE_DOOR_OPEN:
		sq->tile = TILE_DOOR_CLOSED;
		message = "You close the door.";
		break;

	case TILE_DOOR_BORKED:
		message = "That door is broken.";
		break;

	default:
		message = "You see no door there.";
		break;
	}

	msg_printf("%s", message);
}



void player_kick(int dx, int dy)
{
	struct map_square *sq;

	sq = map_square(player.level->map, player.x + dx, player.y + dy);
	if(!sq) return;

	if(sq->monster) {
		msg_printf("You kick the %s!", monster_name(sq->monster));
		return;
	}

	switch(sq->tile) {
	case TILE_DOOR_CLOSED:
		if(test_stat(&player.stats, STAT_STR, 0)) {
			sq->tile = TILE_DOOR_BORKED;
			msg_printf("The door crashes open!");
		}

		else msg_printf("You kick the door hard, but it won't budge.");

		exercise_stat(&player.stats_exe, STAT_STR, 1);

		break;

	case TILE_WALL_HORIZ:
	case TILE_WALL_VERT:
		msg_printf("You kick the wall.  Ouch!");
		player_hurt(5);
		break;

	default:
		msg_printf("You kick at empty space.");
		exercise_stat(&player.stats_exe, STAT_DEX, -1);
		break;
	}
}



void player_look()
{
	struct map_square *sq;
	char *message;

	sq = map_square(player.level->map, player.x, player.y);

	switch(sq->tile) {
	case TILE_DOOR_OPEN:
		message = "an open door";
		break;
	case TILE_DOOR_BORKED:
		message = "a broken door";
		break;
	case TILE_STAIR_UP:
		message = "a staircase leading up";
		break;
	case TILE_STAIR_DOWN:
		message = "a staircase leading down";
		break;
	default:
		message = "nothing interesting";
	}

	msg_printf("There is %s here.", message);
}



void player_remote_look(unsigned int x, unsigned int y)
{
	char symbol;
	struct symbol_def *def;
	struct map_square *sq;
	const char *descrip = NULL;

	symbol = main_get_character(x, y);

	if(symbol == ' ') {
		msg_printf("You can't see what's there.");
		return;
	}

	for(def = symbol_defs; def->symbol && def->symbol != symbol; def++);


	if(def->symbol == 0) msg_printf("Sorry, I don't know what that is.");
	else {
		sq = map_square(player.level->map, x, y);

		if(sq->monster) descrip = monster_name(sq->monster);
		else {
			switch(sq->tile) {
			case TILE_WALL_HORIZ:
				descrip = "wall";
				break;
			case TILE_DOOR_OPEN:
				descrip = "open door";
				break;
			default:
				break;
			}
		}

		if(descrip) msg_printf("%c       %s (%s)",
				       symbol, def->definition, descrip);
		else msg_printf("%c       %s", symbol, def->definition);
	}
}



void player_hurt(unsigned int damage)
{
	damage = stats_hurt(&player.stats, damage);

	if(player.stats.hp == 0) {
		/* Player is dead, do something about it */
	}

	player_status();
	msg_printf("[%u pts.]", damage);
}



struct coord player_select_square()
{
	struct coord c, dc;
	int k;

	c.x = player.x;
	c.y = player.y;

	do {
		main_move(c.x, c.y);
		display_refresh();

		k = display_getch();

		dc = key_to_direction(k);

		c.x += dc.x;
		c.y += dc.y;

	} while(k != '.');

	return c;
}
