#!/bin/bash
NEW_COPYRIGHT=2011-$(date +%Y)
for fn in $(rgrep Copyright 2>/dev/null |grep Beanstalks |grep -v 2025 |cut -f1 -d: ); do
    echo -n "$fn "
    perl -pi -e "s/Copyright ([\d-]+)/Copyright $NEW_COPYRIGHT/g;" $fn
    grep 'Copyright' $fn
done
