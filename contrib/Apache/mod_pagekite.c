/* mod_pagekite.c, Copyright 2014-2017, The Beanstalks Project ehf.
 *
 * This configures Apache to automatically make chosen VirtualHosts
 * public using the PageKite tunneled reverse proxy.
 *
 * Apache config directives:
 *
 *   # The basic: configure domains, shared secrets, flags
 *   PageKite .pagekite.me secret +e2e_ssl
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
#include "http_core.h"
#include "http_config.h"
#include "ap_config.h"
#include "apr_global_mutex.h"
#ifdef AP_NEED_SET_MUTEX_PERMS
#include "unixd.h"
#endif


/* *** Module configureation ********************************************** */

#define MAX_KITES 100
#define MAX_FRONTENDS 50
#define DISABLE_DYNDNS "Off"

typedef enum { off=-1, undefined=0, on=1, only=2 } tristate;

typedef struct {
    char* domain;
    char* secret;
    tristate with_e2e_ssl;
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
    tristate    with_pagekitenet;
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
    autokite* ak = &config.autokites[config.autokiteCount++];

    ak->domain = argv[0];
    ak->secret = argv[1];

    ak->with_e2e_ssl = 0;
    for (int i = 2; i < argc; i++) {
        if ((argv[i][0] && (strcasecmp(argv[i]+1, "e2e_ssl") == 0)) ||
                (strcasecmp(argv[i], "e2e_ssl") == 0))
        {
            ak->with_e2e_ssl = (argv[i][0] != '-') || -1;
        }
    }

    return NULL;
}

const char *modpk_set_pagekitenet(cmd_parms* cmd, void* cfg,
    const char* onoff)
{
    config.with_pagekitenet = (
        (strcasestr(onoff, "on") != NULL) ? on :
        (strcasestr(onoff, "yes") != NULL) ? on :
        (strcasestr(onoff, "true") != NULL) ? on :
        (strcasestr(onoff, "auto") != NULL) ? undefined : off);
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
    hostport(strdup(argv[0]), &(k->origin_host), &(k->origin_port));

    k->protocol = argv[1];

    k->kite_name = NULL;
    k->kite_port = 0;
    hostport(strdup(argv[2]), &(k->kite_name), &(k->kite_port));
    my_assert("PageKiteAddKite: Bad kite name", k->kite_name != NULL);

    k->secret = argv[3];
    fprintf(stderr,
            "Added kite: %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3]);

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
        fprintf(stderr, "Stopping PageKite background thread.\n");
        pagekite_stop(manager);
        pagekite_free(manager);
        manager = NULL;
        apr_global_mutex_unlock(manager_lock);
    }
    return OK;
}

static int domains_match(const char* d1, const char* d2) {
    if (d1 == NULL || d2 == NULL) return 0;
    if (0 == strcasecmp(d1, d2)) return 1;
    if (*d2 == '.') {
        char* dot = strchr(d1, '.');
        while (dot != NULL) {
            if (0 == strcasecmp(dot, d2)) return 1;
            dot = strchr(dot+1, '.');
        }
    }
    return 0;
}

static void auto_configure_kites(const char* domain,
                                 int http_port, int https_port) {
    int i;
    char http_host[128], https_host[128];

    fprintf(stderr,
            "Config kite? %s:[%d,%d] akc=%d\n",
            domain, http_port, https_port, config.autokiteCount);

    for (i = 0; i < config.autokiteCount; i++) {
        autokite* ak = &config.autokites[i];
        if (domains_match(domain, ak->domain))
        {
            if ((http_port > 0) && (ak->with_e2e_ssl != only))
            {
                sprintf(http_host, "localhost:%d", http_port);
                const char* args[] =
                    { http_host, "http", domain, ak->secret };
                modpk_add_kite(NULL, NULL, 4, (char**) args);
            }

            if ((https_port > 0) && (ak->with_e2e_ssl != off))
            {
                sprintf(https_host, "localhost:%d", https_port);
                const char* args[] =
                    { https_host, "https", domain, ak->secret };
                modpk_add_kite(NULL, NULL, 4, (char**) args);
            }
            break;
        }
    }
}

static void modpk_start_pagekite(
    apr_pool_t *p,
    server_rec *s)
{
    int flags = (PK_WITH_IPV4 | PK_WITH_IPV6);
    const char* dyndns = config.dynDNS;

    /* We need to discover the following things from the global config:
     *   - Is SSL enabled? What port?
     *   - Do we have IPv6? IPv4? Both?
     *   - What port are we listening on for HTTP?
     *   - Which ServerName and ServerAlias directives exist, and which
     *     match our autokite lists?
     */
    struct server_rec* r;
    for (r = s; r != NULL; r = r->next) {
        /* FIXME: Should we iterate through the addrs list? */
        int http_port = r->addrs[0].host_port;
        auto_configure_kites(r->server_hostname, http_port, -1);
        if (r->names != NULL) {
            char** names = (char **) r->names->elts;
            for (int i = 0; i < r->names->nelts; i++) {
                auto_configure_kites(names[i], http_port, -1);
            }
        }
    }

    if (config.with_pagekitenet != off) {
        flags |= PK_WITH_SERVICE_FRONTENDS;
        if (NULL == dyndns) dyndns = PAGEKITE_NET_DDNS;
    }
    if (0 == strcasecmp(dyndns, DISABLE_DYNDNS)) dyndns = NULL;

    if (((config.frontendCount == 0) && (off == config.with_pagekitenet))
            || ((config.kiteCount == 0)))
        return;

    /* If we get this far, we really do intend to do some PageKite
     * related work. Take the lock so this only happens once. */
    if (APR_STATUS_IS_EBUSY(apr_global_mutex_trylock(manager_lock)))
        return;

    manager = pagekite_init(
        "mod_pagekite",
        config.kiteCount,
        config.frontendCount + PAGEKITE_NET_FE_MAX,
        -1, /* Let libpagekite choose */
        config.dynDNS,
        flags,
        PK_LOG_NORMAL);
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
    }

    for (int i = 0; i < config.frontendCount; i++) {
        my_assert("Failed to add frontend", -1 <
            pagekite_add_frontend(manager,
                config.frontends[i].fe_host,
                config.frontends[i].fe_port));
    }

    apr_pool_cleanup_register(p, s, modpk_stop_pagekite, modpk_stop_pagekite);
    pagekite_start(manager);
    fprintf(stderr, "PageKite started in background thread!\n");
    return;
}

static void register_hooks(apr_pool_t *p)
{
    manager = NULL;
    config.with_pagekitenet = undefined;
    config.dynDNS = NULL;
    config.frontendCount = 0;
    config.kiteCount = 0;

    apr_global_mutex_create(&manager_lock, MUTEX_PATH, APR_LOCK_DEFAULT, p);
#ifdef AP_NEED_SET_MUTEX_PERMS
    ap_unixd_set_global_mutex_perms(manager_lock);
#endif

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
