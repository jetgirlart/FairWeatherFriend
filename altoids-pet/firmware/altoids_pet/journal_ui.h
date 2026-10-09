#pragma once
#include "display.h"

bool isJournalScreen();
void openJournalScreen(ScreenMode screen);
bool handleJournalButtons(bool aPressed, bool bPressed, bool cPressed);
void updateJournalScreens();

bool drawFieldEventNotification(); // Compose RAM only; caller performs the frame update.
