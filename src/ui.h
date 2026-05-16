#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "state.h"

void InitUI(void);
void DrawUI(void);
void UpdateUIState(void);

int SnapIsSide(SnapPos snap);
int SnapIsBottom(SnapPos snap);
int SnapIsRight(SnapPos snap);

#endif
