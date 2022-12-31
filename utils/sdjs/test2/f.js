// Simplified Polyfill from https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/create
function F() {}; // Outside the clonePF-scope to compare instanceof
function clonePF(o) {
    F.prototype = o;
    return new F();
}

// Recursive solution from http://stackoverflow.com/questions/728360/most-elegant-way-to-clone-a-javascript-object
function cloneSO(obj) {
    // Handle the 3 simple types, and null or undefined
    if (null == obj || "object" != typeof obj) return obj;

    // Handle Date
    if (obj instanceof Date) {
        var copy = new Date();
        copy.setTime(obj.getTime());
        return copy;
    }

    // Handle Array
    if (obj instanceof Array) {
        var copy = [];
        for (var i = 0, len = obj.length; i < len; i++) {
            copy[i] = cloneSO(obj[i]);
        }
        return copy;
    }

    // Handle Object
    if (obj instanceof Object) {
        var copy = {};
        for (var attr in obj) {
            if (obj.hasOwnProperty(attr)) copy[attr] = cloneSO(obj[attr]);
        }
        return copy;
    }

    throw new Error("Unable to copy obj! Its type isn't supported.");
}

// Deep circular solution from http://stackoverflow.com/questions/10728412/in-javascript-when-performing-a-deep-copy-how-do-i-avoid-a-cycle-due-to-a-pro
function cloneDR(o) {
    const gdcc = "__getDeepCircularCopy__";
    if (o !== Object(o)) {
        return o; // primitive value
    }
    
    var set = gdcc in o,
        cache = o[gdcc],
        result;
    if (set && typeof cache == "function") {
        return cache();
    }
    // else
    o[gdcc] = function() { return result; }; // overwrite
    if (o instanceof Array) {
        result = [];
        for (var i=0; i<o.length; i++) {
            result[i] = cloneDR(o[i]);
        }
    } else {
        result = {};
        for (var prop in o)
            if (prop != gdcc)
                result[prop] = cloneDR(o[prop]);
            else if (set)
                result[prop] = cloneDR(cache);
    }
    if (set) {
        o[gdcc] = cache; // reset
    } else {
        delete o[gdcc]; // unset again
    }
    return result;
}

// The circular structure
function Circ() {
    this.me = this;
}

// The nested structure
function Nested(y) {
    this.y = y;
}

var a = {
    x: 'a',
    circ: new Circ(),
    nested: new Nested('a')
};

/** Native Solution **/
// Works with modern browsers (>= IE 9) (No deep copying!)
//var b = Object.create(a);

/** JSON **/
// Doesn't work due to the circular structure (TypeError: Converting circular structure to JSON)
var b = JSON.parse( JSON.stringify( a ) );

/** Recursive solution **/
// Doesn't work due to the circular structure (RangeError: Maximum call stack size exceeded)
//var b = cloneSO(a);

/** Polyfill for Object.create **/
// Works, but b instanceof F will return true (No deep copying!)
//var b = clonePF(a);

/** Deep ciruclar copy **/
// Best solution so far
var b = cloneDR(a);

b.x = 'b';
b.nested.y = 'b';

console.log(a, b);
console.log(typeof a, typeof b);
console.log(a instanceof Object, b instanceof Object);
console.log(a instanceof F, b instanceof F);
