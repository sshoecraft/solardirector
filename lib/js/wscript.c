/*
Arguments Property
Array of arguments

BuildVersion Property
The BuildVersion property returns an integer value.

FullName Property
The FullName property is a read-only string representing the fully qualified path of the host executable.

Interactive Property
The Interactive Property returns a Boolean value.

Name Property
The Name property is a read-only string.

Path Property
The Path property is a read-only string.

ScriptFullName Property
The ScriptFullName property is a read-only string.

ScriptName Property
The ScriptName property is a read-only string.

StdErr Property
The StdErr property returns an object representing the standard error stream. The StdIn, StdOut, and StdErr streams can be accessed while using CScript.exe only. Attempting to access these streams while using WScript.exe produces an error.

StdIn Property
The StdIn property returns an object representing the standard input stream. The StdIn, StdOut, and StdErr streams can be accessed while using CScript.exe only. Attempting to access these streams while using WScript.exe produces an error.

StdOut Property
The StdOut property returns an object representing the standard output stream. The StdIn, StdOut, and StdErr streams can be accessed while using CScript.exe only. Attempting to access these streams while using WScript.exe produces an error.

Version Property
String.

ConnectObject Method
The ConnectObject method connects the object's outgoing interface to the script file after creating the object. Event functions are a combination of this prefix and the event name.

CreateObject Method
Objects created with the CreateObject method using the strPrefix argument are connected objects. These are useful when you want to sync an object's events. The object's outgoing interface is connected to the script file after the object is created. Event functions are a combination of this prefix and the event name. If you create an object and do not supply the strPrefix argument, you can still sync events on the object by using the ConnectObject method. When the object fires an event, WSH calls a subroutine with strPrefix attached to the beginning of the event name. For example, if strPrefix is MYOBJ and the object fires an event named OnBegin, Windows Script Host calls the MYOBJ_OnBegin subroutine located in the script. The CreateObject method returns a pointer to the object's IDispatch interface.

DisconnectObject Method
Once an object has been "disconnected," WSH will not respond to its events. The object is still capable of firing events, though. Note that the DisconnectObject method does nothing if the specified object is not already connected.

Echo Method
The Echo method behaves differently depending on which WSH engine you are using.

GetObject Method
Use the GetObject method when an instance of the object exists in memory, or when you want to create the object from a file. If no current instance exists and you do not want the object created from a file, use the CreateObject method. The GetObject method can be used with all COM classes, independent of the language used to create the object. If you supply the strPrefix argument, WSH connects the object's outgoing interface to the script file after creating the object. When the object fires an event, WSH calls a subroutine with strPrefix attached to the beginning of the event name. For example, if strPrefix is MYOBJ_ and the object fires an event named OnBegin, WSH calls the MYOBJ_OnBegin subroutine located in the script.

If an object is registered as a single-instance object, only one instance of the object is created (regardless of how many times GetObject is executed). The GetObject method always returns the same instance when called with the zero-length string syntax (""), and it causes an error if you do not supply the path parameter. You cannot use the GetObject method to obtain a reference to a Microsoft Visual Basic class created with Visual Basic 4.0 or earlier.

Quit Method
The Quit method can return an optional error code. If the Quit method is the final instruction in your script (and you have no need to return a non-zero value), you can leave it out, and your script will end normally.

Sleep Method
The thread running the script is suspended, releasing its CPU utilization. Execution resumes as soon as the interval expires. Using the Sleep method can be useful when you are running asynchronous operations, multiple processes, or if your script includes code triggered by an event. To be triggered by an event, a script must be continually active (a script that has finished executing will certainly not detect an event). Events handled by the script will still be executed during a sleep.
*/
