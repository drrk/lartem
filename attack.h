#ifndef ATTACK_H
#define ATTACK_H

struct monst;
struct player;


enum {
	ATTACK_USER,        /* Standard annoying user attack */
	ATTACK_MANAGER      /* Slightly worse annoying manager attack */
};


void attack_player(struct monst *, struct player *);


#endif