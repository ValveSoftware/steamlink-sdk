if [ ! -e node_modules ]; then mkdir node_modules; fi
npm install request underscore nopt node-static   #install scxml-test-framework's dependencies

##start the server
#node node-test-server.js &
##keep the pid (so we can kill it later)
#serverpid=$!

sleep 1

#run the client
node scxml-test-framework/lib/test-client.js \
  scxml-test-framework/test/actionSend/*.scxml \
  scxml-test-framework/test/assign-current-small-step/*.scxml \
  scxml-test-framework/test/atom3-basic-tests/*.scxml \
  scxml-test-framework/test/basic/*.scxml \
  scxml-test-framework/test/cond-js/*.scxml \
  scxml-test-framework/test/default-initial-state/*.scxml \
  scxml-test-framework/test/documentOrder/*.scxml \
  scxml-test-framework/test/hierarchy/*.scxml \
  scxml-test-framework/test/hierarchy+documentOrder/*.scxml \
  scxml-test-framework/test/if-else/*.scxml \
  scxml-test-framework/test/in/*.scxml \
  scxml-test-framework/test/internal-transitions/*.scxml \
  scxml-test-framework/test/more-parallel/*.scxml \
  scxml-test-framework/test/multiple-events-per-transition/*.scxml \
  scxml-test-framework/test/parallel/*.scxml \
  scxml-test-framework/test/parallel+interrupt/*.scxml \
  scxml-test-framework/test/script/*.scxml \
  scxml-test-framework/test/script-src/*.scxml \
  scxml-test-framework/test/scxml-prefix-event-name-matching/*.scxml \
  scxml-test-framework/test/targetless-transition/*.scxml 
#  scxml-test-framework/test/history/*.scxml \
#  scxml-test-framework/test/delayedSend/*.scxml \
#  scxml-test-framework/test/foreach/*.scxml \
#  scxml-test-framework/test/send-data/*.scxml \
#  scxml-test-framework/test/send-internal/*.scxml \

status=$?

##kill the server
#kill $serverpid

if [ "$status" = '0' ]; then echo SUCCESS; else echo FAILURE; fi;

exit $status
