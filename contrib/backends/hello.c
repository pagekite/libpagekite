/* This is the hello-world example from docs/API.md */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <pagekite.h>


int main(int argc, const char** argv) {
    pagekite_mgr pkm;

    if (argc < 3) {
        fprintf(stderr, "Usage: hello yourkite.pagekite.me password\n");
        return 1;
    }

    pkm = pagekite_init_pagekitenet(
        "Hello World App",   /* What app is this?                       */
        1,                   /* Max number of kites we plan to fly      */
        25,                  /* Max number of simultaneous client conns */
        PK_WITH_DEFAULTS,    /* Use defaults (SSL, IPv4 and IPv6)       */
        PK_LOG_NORMAL);      /* Log verbosity level                     */

    if (pkm == NULL) {
        pagekite_perror(pkm, argv[0]);
        return 2;
    }

    /* Add a kite for the HTTP server on localhost:80 */
    if (0 > pagekite_add_kite(pkm,
                              "http",          /* Kite protocol         */
                              argv[1],         /* Kite name             */
                              0,               /* Public port, 0=any    */
                              argv[2],         /* Kite secret           */
                              "localhost",     /* Origin server host    */
                              80))             /* Origin server port    */
    {
        pagekite_perror(pkm, argv[0]);
        return 3;
    }

    /* Start the worker thread, wait for it to exit. */
    assert(-1 < pagekite_start(pkm));
    assert(-1 < pagekite_wait(pkm));

    /* Not reached: Nothing actually shuts us down in this app. */
    assert(-1 < pagekite_free(pkm));

    return 0;
}
