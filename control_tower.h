#if !defined(_CONTROL_TOWER_H_)
#define _CONTROL_TOWER_H_
#include "shared.h"


void create_thread_arrivals();
void create_thread_departures();
void control_tower();
void handle_pipe();
void parse_request(char *str);

#endif