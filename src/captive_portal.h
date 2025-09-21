#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include "esp_http_server.h"

// Function declarations
void captive_portal_start(void);
void captive_portal_stop(void);
void captive_portal_scan_networks(void);

#endif // CAPTIVE_PORTAL_H