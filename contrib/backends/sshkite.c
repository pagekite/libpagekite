/* example of setting up a ssh session */

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
        "ssh kite server",   /* What app is this?                       */
        1,                   /* Max number of kites we plan to fly      */
        25,                  /* Max number of simultaneous client conns */
        PK_WITH_DEFAULTS,    /* Use defaults (SSL, IPv4 and IPv6)       */
        PK_LOG_NORMAL);      /* Log verbosity level                     */

    if (pkm == NULL) {
        pagekite_perror(pkm, argv[0]);
        return 2;
    }

    /* Add a kite for the ssh server on localhost:22 */
    if (0 > pagekite_add_kite(pkm,
                              "raw",           /* Kite protocol         */
                              argv[1],         /* Kite name             */
                              22,              /* Public port, 0=any    */
                              argv[2],         /* Kite secret           */
                              "localhost",     /* Origin server host    */
                              22))             /* Origin server port    */
    {
        pagekite_perror(pkm, argv[0]);
        return 3;
    }

    /* Start the worker thread, wait for it to exit. */
    assert(-1 < pagekite_thread_start(pkm));
    assert(-1 < pagekite_thread_wait(pkm));

    /* Not reached: Nothing actually shuts us down in this app. */
    assert(-1 < pagekite_free(pkm));

    return 0;
}
