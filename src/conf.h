#ifndef GLOBAL_CONF_H
#define GLOBAL_CONF_H

//Dependências
#include "cJSON.h"
#include "string.h"
#include "time.h"
#include <ctime>



// Macros de controle
#define retornaSegundo(x) (1000 * (x))
#define retornaMinuto(x) (60 * 1000 * (x))
#define retornaHora(x) (60 * 60 * 1000 * (x))



// Pin Mapping
#define RelePin 27
#define WiFi_LED 19





#endif // Global Conf.h