mod_pagekite.la: mod_pagekite.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version  mod_pagekite.lo
DISTCLEAN_TARGETS = modules.mk
shared =  mod_pagekite.la
