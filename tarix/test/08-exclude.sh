#!/usr/bin/env bash

set -xe

[ -f bin/test/data.tgz ]
[ -f bin/test/data.tarix ]
[ -f bin/test/data.x.tar ]

# using -eg means that 'data' won't match 'data.d'
bin/tarix -zxf bin/test/data.tarix -t bin/test/data.tgz -eg data 'data.?/' 'data.?/data[2]' \
  >bin/test/data.exclude.tar
#hexdump -C bin/test/data.exclude.tar
tar -tvf bin/test/data.exclude.tar
cmp bin/test/data.x.tar bin/test/data.exclude.tar
