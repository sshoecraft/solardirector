//Create a log file for use later.
if (!File.exists("runlog")){
    File.create("runlog");
}
else {
    File.touch("runlog");
}

String.prototype.trim = function(){ return this.replace(/^\s*/, '').replace(/\s*$/, ''); }

console.log("Enter a filename: ");
var strfile = Console.read().trim();

console.log("Creating file object for "+strfile);
var file = new File(strfile);

var done = false;
do {
    
    console.log("Do what with "+strfile+"?");
    console.log("1) Check if exists. ");
    console.log("2) Update modifcation time. ");
    console.log("3) Create file. ");
    console.log("4) Quit. ");
    var choice = parseInt(Console.read());
    if (choice == 1){
        if (file.exists()){
            console.log("Yeppers, file exists.");
        }
		else {
			console.log("Nope, it doesn't exist yet.");
		}
    }
    else if (choice == 2){
        try {
            if (file.touch()){
                console.log("File access and modification time update.");
            }
        }
        catch (e){
            console.log("Time update failed, Exception caught: ");
            console.log(e);
        }
    }
    else if (choice == 3){
		try
		{
	        file.create();
		}
		catch (e)
		{
			console.log("Creation failed. Exception caught: ");
			console.log(e);
		}
    }
    else if (choice == 4){
        done = true;
    }
} while (done == false);

