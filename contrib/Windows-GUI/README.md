Windows GUI
===========

NOTE: This app is still under development!

The project *should* open as a Solution in Visual Studio 2013 without any
issues. Please let us know if you find otherwise!

Requirements:

* The app requires `CookComputing.XmlRpcV2.dll` and `Newtonsoft.Json.dll`.
  Downloaded them and place in the `lib/` subdirectory.
* In order to run the app, you will need to copy `libpagekite.dll` and
  the `img/` folder into whatever the working directory is. The app
  doesn't handle other environments gracefully yet and will crash.
* Currently the app requires a pagekite.net account to run.

Hints:

* Kites won't actually fly until you click the Details button for that
  kite and tick at least one of the protocol's checkboxes.
* There is no built-in web server.

