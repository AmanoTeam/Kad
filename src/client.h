#if !defined(CLIENT_H)
#define CLIENT_H

#include <stdlib.h>

#include "transferdata.h"

int client_write(transferdata_t* const data, const char* const buffer, const size_t size);

#endif
