#pragma once
#include "gw_faces.h"
typedef struct { int n, open, state; char id[GW_TODO_ROWS][12]; char text[GW_TODO_ROWS][124]; bool done[GW_TODO_ROWS]; } gw_todo_list_t;
extern gw_todo_list_t gw_todo;   // state: 0 ok, 1 loading, 2 offline
void GwTodoFetch();
void GwTodoCheck(int row);
void GwTodoSetDoneCb(void (*f)());
