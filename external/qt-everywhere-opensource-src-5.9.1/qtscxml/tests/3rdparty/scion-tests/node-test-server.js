//load statechart from scxml string; return initial state and token id

//send event to statechart with tokenid
//clean up statechart

var scion = require('scion'),
    http = require('http');

var sessionCounter = 0, sessions = {}, timeouts = {}, timeoutMs = 5000;

function loadScxml(scxmlStr){
}

function cleanUp(sessionToken){
    delete sessions[sessionToken];
}

http.createServer(function (req, res) {
    //TODO: set a timeout to clean up if we don't hear back for a while
    var s = "";
    req.on("data",function(data){
        s += data;
    });
    req.on("end",function(){
        var sessionToken;
        try{
            var reqJson = JSON.parse(s);
            if(reqJson.load){
                console.log("Loading new statechart");

                scion.urlToModel(reqJson.load,function(err,model){
                    if(err){
                        console.error(err.stack);
                        res.writeHead(500, {'Content-Type': 'text/plain'});
                        res.end(err.message);
                    }else{
                        var interpreter = new scion.SCXML(model);

                        var sessionToken = sessionCounter;
                        sessionCounter++;
                        sessions[sessionToken] = interpreter; 

                        var conf = interpreter.start(); 

                        res.writeHead(200, {'Content-Type': 'application/json'});
                        res.end(JSON.stringify({
                            sessionToken : sessionToken,
                            nextConfiguration : conf
                        }));

                        timeouts[sessionToken] = setTimeout(function(){cleanUp(sessionToken);},timeoutMs);  
                    }
                });

            }else if(reqJson.event && (typeof reqJson.sessionToken === "number")){
                console.log("sending event to statechart",reqJson.event);
                sessionToken = reqJson.sessionToken;
                var nextConfiguration = sessions[sessionToken].gen(reqJson.event);
                console.log('nextConfiguration',nextConfiguration);
                res.writeHead(200, {'Content-Type': 'application/json'});
                res.end(JSON.stringify({
                    nextConfiguration : nextConfiguration
                }));

                clearTimeout(timeouts[sessionToken]);
                timeouts[sessionToken] = setTimeout(function(){cleanUp(sessionToken);},timeoutMs);  
            }else{
                //unrecognized. send back an error
                res.writeHead(400, {'Content-Type': 'text/plain'});
                res.end("Unrecognized request.\n");
            }
        }catch(e){
            console.error(e.stack);
            console.error(e);
            res.writeHead(500, {'Content-Type': 'text/plain'});
            res.end(e.message);
        }
    });
}).listen(42000, '127.0.0.1');

