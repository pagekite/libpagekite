import libpagekite
from libpagekite import PK_EV_RESPOND_DEFAULT


class BaseEventHandler(object):
    def __init__(self, pk):
        self.pk = pk
        self._map = {
            libpagekite.PK_EV_CFG_FANCY_URL: self.fancy_rejection_url,
            libpagekite.PK_EV_TUNNEL_REQUEST: self.tunnel_request}

    def get_event_int(self, event_id):
        """Fetch integer data associated with an event"""
        return self.pk.get_event_int(event_id)

    def get_event_string(self, event_id):
        """Fetch string data associated with an event"""
        return self.pk.get_event_str(event_id)

    def debug(self, msg):
        """Override this to collect debugging messages."""
        pass

    def handle_events(self, iterations=-1):
        while iterations:
            if iterations > 0:
                iterations -= 1

            event_id = self.pk.await_event(1)
            event_type = (event_id & libpagekite.PK_EV_TYPE_MASK)

            if (event_type != libpagekite.PK_EV_NONE):
                self.debug("Got event: %x" % event_id)

            if event_type in self._map:
                handler = self._map[event_type]
                result = handler(event_id)
                if isinstance(result, tuple):
                    assert(len(result) == 3)
                    self.pk.event_respond_with_data(event_id, *result)
                elif isinstance(result, int):
                    self.pk.event_respond(event_id, result)
                else:
                    raise TypeError((
                        "Handler %s returned unusable data. "
                        "Should be ints or 3-tuples") % handler)

            if event_type == libpagekite.PK_EV_SHUTDOWN:
                break

    def fancy_rejection_url(self, event_id):
        """
        Override this to handle PK_EV_CFG_FANCY_URL events.

        The event data string will contain the name of the requested HTTP
        Host. If a string is returned in the event response, it will be
        used as the base URL for fancy error pages.
        """
        return PK_EV_RESPOND_DEFAULT

    def tunnel_request(self, event_id):
        """
        Override this to handle PK_EV_TUNNEL_REQUEST events.

        The event data string will contain the raw PageKite tunnel request.

        If the event response code is true (matches PK_EV_RESPOND_TRUE),
        then the tunnel request was parsed and at least one kite request was
        validated, so the connection will stay open as a tunnel. The response
        string must be set and will be parsed for X-PageKite-OK: lines to
        determine which kites to link with the tunnel.

        Any other event response will cause the tunnel request to be rejected
        and the connection closed.

        If the event response string is set, that data will be sent
        unmodified to the client before closing or upgrading to a tunnel.
        When upgrading to a tunnel it is required and should be formatted as
        a valid PageKite response, including the trailing CRLF separator.
        """
        return PK_EV_RESPOND_REJECT
