//Lets test some sockets.
try {
    var socket = new Socket();
    console.log("Socket object created successfully.");

    //Check the constants
    console.log("SOL_TCP = " + Socket.SOL_TCP);

    //Connect to my website
    //socket.connect({});
    socket.connect({family:Socket.PF_INET, details:{port:80,address:"aoeex.com"}});

    //Make a request
    socket.write("GET / HTTP/1.0\r\nHost: aoeex.com\r\n\r\n");

    //Read the response
    while ((data=socket.read()).length > 0){
        //Read the banner
        console.log(data);
    }

    //Close up shop
    socket.close();
}
catch (e){
    console.log ("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    console.log("Caught exception. ");
    console.log(e);
}
