import net.pagekite.lib.PageKiteAPI;

public class PageKiteTest {
    public static void main(String[] args) {
        System.out.println("Hello world");
        PageKiteAPI.initWhitelabel("PageKiteTest", 25, 25, 
                                   PageKiteAPI.PK_WITH_DEFAULTS, 1,
                                   "pagekite.me");
        PageKiteAPI.addKite("http", "KITE.pagekite.me", 80, "SECRET",
                            "localhost", 80);
        PageKiteAPI.threadStart();
        PageKiteAPI.threadWait();
    }
}
