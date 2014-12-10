/* mod_pagekite.c, Copyright 2014, The Beanstalks Project ehf.
 *
 * This configures Apache to automatically make chosen VirtualHosts 
 * public using the PageKite tunneled reverse proxy.
 *
 * Apache config directives:
 *
 *   # The basic: configure domains, shared secrets, flags
 *   PageKite .pagekite.me secret +ssl
 *
 *   # Manual configuration
 *   PageKiteNet On
 *   PageKiteDynDNS fmt
 *   PageKiteAddFrontend host port
 *   PageKiteAddKite [host:]port protocol kitename[:port] kitesecret
 */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <pagekite.h>

#include "httpd.h"
#include "http_config.h"
#include "ap_config.h"
#include "apr_global_mutex.h"


/* *** Module configureation ********************************************** */

#define MAX_KITES 100
#define MAX_FRONTENDS 50
#define DISABLE_DYNDNS "Off"

typedef struct {
    char* domain;
    char* secret;
    int   with_e2e_ssl;
} autokite;

typedef struct {
    char* origin_host;
    int   origin_port;
    char* protocol;
    char* kite_name;
    int   kite_port;
    char* secret;
} kite;

typedef struct {
    const char* fe_host;
    int         fe_port;
} frontend;

typedef struct {
    const char* dynDNS;
    int         usePageKiteNet;
    int         autokiteCount;
    autokite    autokites[MAX_KITES];
    int         frontendCount;
    frontend    frontends[MAX_FRONTENDS];
    int         kiteCount;
    kite        kites[MAX_KITES]; 
} pagekite_cfg;

static pagekite_cfg config;
static pagekite_mgr manager;

#define MUTEX_PATH "/tmp/mod_pagekite.lock"
static apr_global_mutex_t* manager_lock;


/* *** Helpers ************************************************************ */

void hostport(
     char* arg,
     char** host,
     int* port)
{
     char* sc;
     if (NULL != (sc = strchr(arg, ':'))) {
         *sc++ = '\0';
         *host = arg;
         *port = atoi(sc);
     }
     else if (isdigit(arg[0])) {
         *port = atoi(arg);
     }
     else {
         *host = arg;
     }
}

void my_assert(const char* message, int condition) {
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}


/* *** Module config handlers ********************************************* */

const char *modpk_pagekite(cmd_parms* cmd, void* cfg,
    int argc,
    char** argv)
{
    my_assert("PageKite: too few arguments", argc >= 2);
    my_assert("PageKite: too many autokites", config.autokiteCount <= MAX_KITES);
    autokite* ak = &config.autokites[config.frontendCount++];

    ak->domain = argv[0];
    ak->secret = argv[1];

    for (int i = 2; i < argc; i++) {
        if ((argv[i][0] && (strcasecmp(argv[i]+1, "ssl") == 0)) ||
                (strcasecmp(argv[i], "ssl") == 0))
        {
            ak->with_e2e_ssl = (argv[i][0] != '-');
        }
    }

    return NULL;
}

const char *modpk_set_pagekitenet(cmd_parms* cmd, void* cfg,
    const char* onoff)
{
    config.usePageKiteNet = ((strcasestr(onoff, "on") != NULL) ||
                             (strcasestr(onoff, "yes") != NULL) ||
                             (strcasestr(onoff, "true") != NULL));
    return NULL;
}

const char *modpk_set_dyndns(cmd_parms* cmd, void* cfg,
    const char* arg)
{
    if ((strcasestr(arg, "on") != NULL) ||
        (strcasestr(arg, "pagekite.net") != NULL))
    {
        config.dynDNS = PAGEKITE_NET_DDNS;
    }
    else if (strcasestr(arg, "off") != NULL) {
        config.dynDNS = DISABLE_DYNDNS;
    }
    else {
        config.dynDNS = arg;
    }
    return NULL;
}

const char *modpk_add_frontend(cmd_parms* cmd, void* cfg,
    const char* fe_host,
    const char* fe_port)
{
    my_assert("PageKiteAddFrontEnd: Too many forontends",
              config.frontendCount <= MAX_FRONTENDS);

    frontend* fe = &config.frontends[config.frontendCount++];
    fe->fe_host = fe_host;
    fe->fe_port = atoi(fe_port);

    return NULL;
}

const char *modpk_add_kite(cmd_parms* cmd, void* cfg,
    int argc,
    char** argv)
{
    my_assert("PageKiteAddKite: Wrong number of arguments", argc == 4);
    my_assert("PageKiteAddKite: Too many kites",
              config.kiteCount <= MAX_KITES);

    kite* k = &config.kites[config.kiteCount++];

    k->origin_host = "localhost";
    k->origin_port = 80;
    hostport(argv[0], &(k->origin_host), &(k->origin_port));

    k->protocol = argv[1];

    k->kite_name = NULL;
    k->kite_port = 0;
    hostport(argv[2], &(k->kite_name), &(k->kite_port));
    my_assert("PageKiteAddKite: Bad kite name", k->kite_name != NULL);

    k->secret = argv[3];

    return NULL;
}

static const command_rec pagekite_directives[] =
{
    AP_INIT_TAKE1("PageKiteNet", modpk_set_pagekitenet, NULL, RSRC_CONF,
                  "Enable or disable use of pagekite.net"),
    AP_INIT_TAKE1("PageKiteDynDNS", modpk_set_dyndns, NULL, RSRC_CONF,
                  "Configure dynamic DNS provider"),
    AP_INIT_TAKE2("PageKiteAddFrontend", modpk_add_frontend, NULL, RSRC_CONF,
                  "Add a frontend relay"),
    AP_INIT_TAKE_ARGV("PageKite", modpk_pagekite, NULL, RSRC_CONF,
                      "Domains, secrets and flags to fly"),
    AP_INIT_TAKE_ARGV("PageKiteAddKite", modpk_add_kite, NULL, RSRC_CONF,
                      "Add a raw kite specification"),
    { NULL }
};


/* *** Module hooks ******************************************************* */

static apr_status_t modpk_stop_pagekite(void* data)
{
    if (manager != NULL) {
        apr_global_mutex_unlock(manager_lock);
        pagekite_stop(manager);
        pagekite_free(manager);
        manager = NULL;
    }
    return OK;
}

static void modpk_start_pagekite(
    apr_pool_t *p,
    server_rec *s)
{
    //apr_global_mutex_child_init(&manager_lock, MUTEX_PATH, p);
    if (APR_STATUS_IS_EBUSY(apr_global_mutex_trylock(manager_lock)))
        return;

    /* We need to discover the following things from the global config:
     *   - What port are we listening on for HTTP?
     *   - Is SSL enabled? What port?
     *   - Which ServerName and ServerAlias directives exist, and which
     *     match our autokite lists?
     */

    int flags = (PK_WITH_IPV4 | PK_WITH_IPV6);
    const char* dyndns = config.dynDNS;

    if (config.usePageKiteNet) {
        flags |= PK_WITH_SERVICE_FRONTENDS;
        if (NULL == config.dynDNS) config.dynDNS = PAGEKITE_NET_DDNS;
    }
    if (0 == strcasecmp(dyndns, DISABLE_DYNDNS)) dyndns = NULL;

    if ((config.kiteCount == 0) ||
            ((config.frontendCount == 0) && !config.usePageKiteNet))
        return;

    fprintf(stderr, "Initializing libpagekite\n");
    manager = pagekite_init(
        "mod_pagekite",
        config.kiteCount,
        config.frontendCount + PAGEKITE_NET_FE_MAX,
        -1, /* Let libpagekite choose */
        config.dynDNS,
        flags,
        PK_LOG_DEBUG | PK_LOG_TRACE);
    my_assert("pagekite_init failed!\n", manager != NULL);

    for (int i = 0; i < config.kiteCount; i++) {
        my_assert("Failed to add kite", -1 < 
            pagekite_add_kite(manager,
                config.kites[i].protocol,
                config.kites[i].kite_name,
                config.kites[i].kite_port,
                config.kites[i].secret,
                config.kites[i].origin_host,
                config.kites[i].origin_port));
        fprintf(stderr, "Added kite %s\n", config.kites[i].kite_name);
    }

    for (int i = 0; i < config.frontendCount; i++) {
        my_assert("Failed to add frontend", -1 <
            pagekite_add_frontend(manager,
                config.frontends[i].fe_host,
                config.frontends[i].fe_port));
        fprintf(stderr, "Added frontend %s\n", config.frontends[i].fe_host);
    }

    pagekite_start(manager);

    apr_pool_cleanup_register(p, s, modpk_stop_pagekite, modpk_stop_pagekite);

    return;
}

static void register_hooks(apr_pool_t *pool) 
{
    manager = NULL;
    config.usePageKiteNet = 1;
    config.dynDNS = NULL;
    config.frontendCount = 0;
    config.kiteCount = 0;
    apr_global_mutex_create(&manager_lock, MUTEX_PATH,
                            APR_LOCK_PROC_PTHREAD, pool);

    ap_hook_child_init(modpk_start_pagekite, NULL, NULL, APR_HOOK_LAST);
}


/* *** Dispatch list for API hooks **************************************** */

module AP_MODULE_DECLARE_DATA pagekite_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,                /* Per-directory configuration handler */
    NULL,                /* Merge handler for per-directory configurations */
    NULL,                /* Per-server configuration handler */
    NULL,                /* Merge handler for per-server configurations */
    pagekite_directives, /* Any directives we may have for httpd */
    register_hooks,      /* Our hook registering function */
};
