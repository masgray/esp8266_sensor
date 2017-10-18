#include "application.h"

Application app;

void setup()
{
  app.begin();
}

void loop()
{
  app.loop();
  yield();
}

