var scion = require('scion');

scion.pathToModel('require/require.scxml',function(err,model){
    if(err) throw err;

    var scxml = new scion.SCXML(model);
    var initialConfig = scxml.start();
    console.log("initialConfig",initialConfig);
    var nextConfig = scxml.gen("t");
    console.log("nextConfig",nextConfig);
});
