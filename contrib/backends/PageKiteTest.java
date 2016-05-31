import net.pagekite.lib.PageKiteAPI;

public class PageKiteTest {
    public static void main(String[] args) throws InterruptedException {
        String Whitelabel_TLD = "pagekite.me";

        if (!PageKiteAPI.libLoaded) {
            System.err.println("libpagekite could not be loaded");
            System.exit(1);
        }

        PageKiteAPI.initWhitelabel(
            "PageKiteTest", 25, 25,
            PageKiteAPI.PK_WITH_DEFAULTS,
            0, Whitelabel_TLD);

        PageKiteAPI.addKite(
            "http", "KITE.pagekite.me", 0, "SECRET",
            "localhost", 80);

        /* Adding a PageKiteAPI.addFrontend for the kite name here would
         * bring the server online quicker after restarts, by avoiding
         * DNS cache/TTL issues. See pagekitec.c for an example. */

        PageKiteAPI.threadStart();

        /* The log is going to stderr, but we can also access recent
         * entries programmatically. */
        Thread.sleep(5000);
        System.out.println("Log so far: " + PageKiteAPI.getLog());

        PageKiteAPI.threadWait();
    }
}
